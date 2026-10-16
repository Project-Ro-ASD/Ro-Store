#include "SystemUpdateModel.h"

#include <QSet>
#include <QTimer>
#include <QHash>
#include <algorithm>

namespace {
// QProcess::kill() is asynchronous: deletion must wait for finished().
void retireUpdateQuery(QProcess *process)
{
    if (!process)
        return;
    if (process->state() == QProcess::NotRunning) {
        process->deleteLater();
        return;
    }
    QObject::connect(process, &QProcess::finished,
                     process, &QObject::deleteLater);
    process->kill();
}

bool isKernelUpdate(const QString &name)
{
    return name == QStringLiteral("kernel")
        || name.startsWith(QStringLiteral("kernel-"));
}
} // namespace

SystemUpdateModel::SystemUpdateModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

SystemUpdateModel::~SystemUpdateModel()
{
    // Leaving the application during metadata refresh must not destroy a
    // running QProcess (nor interfere with any RPM transaction).
    for (QProcess *process : findChildren<QProcess *>(QString(), Qt::FindDirectChildrenOnly)) {
        QObject::disconnect(process, nullptr, this, nullptr);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(2000);
        }
    }
}

int SystemUpdateModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_updates.size());
}

QVariant SystemUpdateModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_updates.size())
        return {};

    const SystemUpdateItem &update = m_updates.at(index.row());
    switch (role) {
    case NameRole: return update.name;
    case VersionRole: return update.version;
    case InstalledVersionRole: return update.installedVersion;
    case ArchitectureRole: return update.architecture;
    case RepositoryRole: return update.repository;
    case KernelUpdateRole: return isKernelUpdate(update.name);
    default: return {};
    }
}

QHash<int, QByteArray> SystemUpdateModel::roleNames() const
{
    return {
        { NameRole, "packageName" },
        { VersionRole, "updateVersion" },
        { InstalledVersionRole, "installedVersion" },
        { ArchitectureRole, "architecture" },
        { RepositoryRole, "repository" },
        { KernelUpdateRole, "kernelUpdate" }
    };
}

void SystemUpdateModel::setLoading(bool value)
{
    if (m_loading == value)
        return;
    m_loading = value;
    emit loadingChanged();
}

void SystemUpdateModel::setError(const QString &value)
{
    if (m_error == value)
        return;
    m_error = value;
    emit errorChanged();
}

void SystemUpdateModel::refresh()
{
    // No concurrent refreshes, and no writes to DNF5 package state.
    if (m_loading)
        return;

    setError({});
    m_stdout.clear();
    setLoading(true);

    auto *process = new QProcess(this);
    m_process = process;
    process->setProgram(QStringLiteral("dnf5"));
    process->setArguments({
        QStringLiteral("repoquery"),
        QStringLiteral("--upgrades"),
        QStringLiteral("--latest-limit=1"),
        QStringLiteral("--queryformat=%{name}\t%{evr}\t%{arch}\t%{repoid}\\n")
    });
    process->setProcessChannelMode(QProcess::SeparateChannels);

    qInfo() << "SYSTEM UPDATE CHECK START (read-only DNF5 repoquery)";

    connect(process, &QProcess::readyReadStandardOutput, this,
            [this, process]() {
        if (m_process != process)
            return;
        m_stdout += process->readAllStandardOutput();
        if (m_stdout.size() > MaxOutputBytes) {
            m_process = nullptr;
            retireUpdateQuery(process);
            setLoading(false);
            setError(QStringLiteral("Güncelleme listesi çok büyük. DNF5 çıktısı sınırı aşıldı."));
        }
    });

    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (m_process != process || error != QProcess::FailedToStart)
            return;
        m_process = nullptr;
        process->deleteLater();
        setLoading(false);
        setError(QStringLiteral("DNF5 başlatılamadı. Sistemde dnf5 kurulu olmalı."));
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus status) {
        if (m_process != process)
            return;

        m_stdout += process->readAllStandardOutput();
        const QString stderrText = QString::fromLocal8Bit(
            process->readAllStandardError()).trimmed();
        m_process = nullptr;
        process->deleteLater();
        setLoading(false);

        if (status != QProcess::NormalExit || exitCode != 0) {
            qWarning() << "SYSTEM UPDATE CHECK FAILED:" << exitCode << stderrText;
            setError(stderrText.isEmpty()
                ? QStringLiteral("DNF5 güncelleme sorgusu başarısız oldu.")
                : QStringLiteral("DNF5: ") + stderrText.left(450));
            return;
        }
        if (m_stdout.size() > MaxOutputBytes) {
            setError(QStringLiteral("Güncelleme listesi çok büyük. DNF5 çıktısı sınırı aşıldı."));
            return;
        }

        parseUpdates(m_stdout);
    });

    QTimer::singleShot(120000, process, [this, process]() {
        if (m_process != process || process->state() == QProcess::NotRunning)
            return;
        m_process = nullptr;
        retireUpdateQuery(process);
        setLoading(false);
        setError(QStringLiteral("DNF5 sorgusu zaman aşımına uğradı. Ağ ve depoları kontrol edin."));
    });

    process->start();
}

void SystemUpdateModel::parseUpdates(const QByteArray &output)
{
    QVector<SystemUpdateItem> parsed;
    QSet<QString> seen;
    parsed.reserve(256);

    for (const QByteArray &line : output.split('\n')) {
        const QList<QByteArray> columns = line.trimmed().split('\t');
        if (columns.size() != 4)
            continue;

        SystemUpdateItem update {
            QString::fromUtf8(columns.at(0)).trimmed(),
            QString::fromUtf8(columns.at(1)).trimmed(),
            {},
            QString::fromUtf8(columns.at(2)).trimmed(),
            QString::fromUtf8(columns.at(3)).trimmed()
        };
        if (update.name.isEmpty() || update.version.isEmpty()
            || update.architecture.isEmpty() || update.repository.isEmpty())
            continue;
        const QString key = update.name + QChar('|') + update.architecture;
        if (seen.contains(key))
            continue;
        seen.insert(key);
        parsed.append(std::move(update));
    }

    std::sort(parsed.begin(), parsed.end(), [](const SystemUpdateItem &a,
                                               const SystemUpdateItem &b) {
        const int byName = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        if (byName != 0)
            return byName < 0;
        return a.architecture < b.architecture;
    });

    const bool anyKernel = std::any_of(parsed.cbegin(), parsed.cend(),
        [](const SystemUpdateItem &item) { return isKernelUpdate(item.name); });

    beginResetModel();
    m_updates = std::move(parsed);
    endResetModel();

    m_hasKernelUpdates = anyKernel;
    m_checked = true;
    m_lastChecked = QDateTime::currentDateTime();
    emit updatesChanged();
    loadInstalledVersions();

    qInfo() << "SYSTEM UPDATE CHECK FINISHED:" << m_updates.size()
            << "RPM upgrades, kernel:" << m_hasKernelUpdates;
}

void SystemUpdateModel::loadInstalledVersions()
{
    if (m_updates.isEmpty())
        return;

    // Query the local RPM database without refreshing repositories or
    // performing transactions. Missing data is shown as "unknown".
    auto *process = new QProcess(this);
    process->setProgram(QStringLiteral("rpm"));
    process->setArguments({
        QStringLiteral("-qa"),
        QStringLiteral("--qf"),
        QStringLiteral("%{NAME}\\t%{EPOCHNUM}:%{VERSION}-%{RELEASE}\\t%{ARCH}\\n")
    });
    process->setProcessChannelMode(QProcess::SeparateChannels);

    auto *output = new QByteArray();
    connect(process, &QProcess::readyReadStandardOutput, this,
            [process, output]() {
        *output += process->readAllStandardOutput();
        if (output->size() > MaxOutputBytes && process->state() != QProcess::NotRunning)
            process->kill();
    });

    connect(process, &QProcess::errorOccurred, this,
            [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            qWarning() << "SYSTEM UPDATE LOCAL RPM QUERY FAILED:" << process->errorString();
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, output](int code, QProcess::ExitStatus status) {
        *output += process->readAllStandardOutput();
        if (status == QProcess::NormalExit && code == 0
            && output->size() <= MaxOutputBytes)
            applyInstalledVersions(*output);
        else
            qWarning() << "SYSTEM UPDATE LOCAL RPM QUERY UNAVAILABLE:"
                       << process->errorString();
        process->deleteLater();
        delete output;
    });

    // No updates to apply: a hung rpm query is stopped after ten seconds.
    QTimer::singleShot(10000, process, [process]() {
        if (process->state() != QProcess::NotRunning)
            process->kill();
    });
    process->start();
}

void SystemUpdateModel::applyInstalledVersions(const QByteArray &output)
{
    QHash<QString, QString> versions;
    for (const QByteArray &line : output.split('\\n')) {
        const QList<QByteArray> parts = line.trimmed().split('\\t');
        if (parts.size() != 3)
            continue;
        const QString name = QString::fromUtf8(parts.at(0));
        const QString arch = QString::fromUtf8(parts.at(2));
        QString version = QString::fromUtf8(parts.at(1));
        if (version.startsWith(QStringLiteral("0:")))
            version.remove(0, 2);
        const QString key = name + QChar('|') + arch;
        if (!name.isEmpty() && !arch.isEmpty())
            versions.insert(key, version);
    }

    bool changed = false;
    for (SystemUpdateItem &update : m_updates) {
        const QString key = update.name + QChar('|') + update.architecture;
        const QString installed = versions.value(key);
        if (update.installedVersion != installed) {
            update.installedVersion = installed;
            changed = true;
        }
    }
    if (changed && !m_updates.isEmpty())
        emit dataChanged(index(0, 0), index(m_updates.size() - 1, 0),
                         {InstalledVersionRole});
}
