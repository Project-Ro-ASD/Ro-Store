#include "SystemUpdateModel.h"

#include <QSet>
#include <QTimer>
#include <QHash>
#include <QProcessEnvironment>
#include <QRegularExpression>
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
    case SelectedRole: return m_selected.contains(updateKey(update));
    case DownloadBytesRole: return QVariant::fromValue(update.downloadBytes);
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
        { KernelUpdateRole, "kernelUpdate" },
        { SelectedRole, "updateSelected" },
        { DownloadBytesRole, "downloadBytes" }
    };
}

QString SystemUpdateModel::updateKey(const SystemUpdateItem &item)
{
    return item.name + QChar('|') + item.architecture;
}

QStringList SystemUpdateModel::selectedNevras() const
{
    // Exact package candidates from the last repoquery, not loose names.
    // This list is used for staging only, never trusted as an install plan.
    QStringList result;
    static const QRegularExpression safe(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.+\\-:~]{0,180}$"));
    for (const SystemUpdateItem &item : m_updates) {
        if (!m_selected.contains(updateKey(item)))
            continue;
        const QString nevra = item.name + QChar('-') + item.version
                            + QChar('.') + item.architecture;
        if (safe.match(nevra).hasMatch())
            result.append(nevra);
    }
    return result;
}

qulonglong SystemUpdateModel::selectedDownloadBytes() const
{
    qulonglong total = 0;
    for (const auto &item : m_updates)
        if (m_selected.contains(updateKey(item))) total += item.downloadBytes;
    return total;
}

int SystemUpdateModel::selectedUnknownSizes() const
{
    int count = 0;
    for (const auto &item : m_updates)
        if (m_selected.contains(updateKey(item)) && item.downloadBytes == 0) ++count;
    return count;
}

int SystemUpdateModel::selectedKernelCount() const
{
    int count = 0;
    for (const SystemUpdateItem &item : m_updates) {
        if (isKernelUpdate(item.name) && m_selected.contains(updateKey(item)))
            ++count;
    }
    return count;
}

void SystemUpdateModel::setSelected(const QString &name, const QString &arch, bool selected)
{
    for (int i = 0; i < m_updates.size(); ++i) {
        const SystemUpdateItem &item = m_updates.at(i);
        if (item.name != name || item.architecture != arch)
            continue;
        const QString key = updateKey(item);
        if (m_selected.contains(key) == selected)
            return;
        if (selected)
            m_selected.insert(key);
        else
            m_selected.remove(key);
        invalidatePreview();
        emit dataChanged(index(i, 0), index(i, 0), {SelectedRole});
        emit selectionChanged();
        return;
    }
}

void SystemUpdateModel::selectAll()
{
    if (m_loading || !m_checked)
        return;
    if (m_selected.size() == m_updates.size())
        return;
    invalidatePreview();
    m_selected.clear();
    for (const SystemUpdateItem &item : m_updates)
        m_selected.insert(updateKey(item));
    if (!m_updates.isEmpty())
        emit dataChanged(index(0, 0), index(m_updates.size() - 1, 0), {SelectedRole});
    emit selectionChanged();
}

void SystemUpdateModel::clearSelection()
{
    if (m_selected.isEmpty())
        return;
    invalidatePreview();
    m_selected.clear();
    if (!m_updates.isEmpty())
        emit dataChanged(index(0, 0), index(m_updates.size() - 1, 0), {SelectedRole});
    emit selectionChanged();
}

void SystemUpdateModel::setPreviewRunning(bool value)
{
    if (m_previewRunning == value)
        return;
    m_previewRunning = value;
    emit previewChanged();
}

void SystemUpdateModel::invalidatePreview()
{
    if (m_previewProcess) {
        QProcess *previous = m_previewProcess;
        m_previewProcess = nullptr;
        retireUpdateQuery(previous);
    }
    m_previewBuffer.clear();
    m_previewText.clear();
    m_previewError.clear();
    m_previewSelectionCount = 0;
    m_previewAllUpdates = false;
    setPreviewRunning(false);
    emit previewChanged();
}

void SystemUpdateModel::cancelPreview()
{
    if (!m_previewRunning)
        return;
    invalidatePreview();
    m_previewError = QStringLiteral("Güncelleme planı incelemesi iptal edildi. Sistem paketleri değiştirilmedi.");
    emit previewChanged();
}

void SystemUpdateModel::previewPlan()
{
    // One solver goal for the whole selection; never invoke a series of
    // independent per-package RPM mutations. This command is deliberately
    // non-privileged, cache-only and declines all transactions.
    if (m_loading || !m_checked || m_selected.isEmpty() || m_previewRunning)
        return;

    static const QRegularExpression safeName(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.+\\-]{0,127}$"));
    QStringList names;
    names.reserve(m_selected.size());
    const bool allSelected = m_selected.size() == m_updates.size();
    if (!allSelected) {
        for (const SystemUpdateItem &item : m_updates) {
            if (!m_selected.contains(updateKey(item)))
                continue;
            if (!safeName.match(item.name).hasMatch()
                || !safeName.match(item.architecture).hasMatch()) {
                m_previewError = QStringLiteral("Güvenli DNF5 paket adı doğrulanamadı; plan oluşturulmadı.");
                emit previewChanged();
                return;
            }
            names.append(item.name + QChar('.') + item.architecture);
        }
        if (names.size() != m_selected.size()) {
            m_previewError = QStringLiteral("Paket seçimi geçersiz; yeniden denetleyin.");
            emit previewChanged();
            return;
        }
    }

    invalidatePreview();
    m_previewSelectionCount = m_selected.size();
    m_previewAllUpdates = allSelected;
    auto *process = new QProcess(this);
    m_previewProcess = process;
    process->setProgram(QStringLiteral("dnf5"));
    QStringList args {
        QStringLiteral("--cacheonly"),
        QStringLiteral("--assumeno"),
        QStringLiteral("upgrade")
    };
    if (!allSelected)
        args.append(names);
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    env.insert(QStringLiteral("CLICOLOR"), QStringLiteral("0"));
    process->setProcessEnvironment(env);

    setPreviewRunning(true);
    qInfo() << "SYSTEM UPDATE PLAN PREVIEW (cache-only, assumeno):"
            << (allSelected ? QStringLiteral("all upgrades") : QString::number(names.size()));

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        if (m_previewProcess != process)
            return;
        m_previewBuffer += process->readAllStandardOutput();
        if (m_previewBuffer.size() > MaxOutputBytes) {
            invalidatePreview();
            m_previewError = QStringLiteral("DNF5 plan çıktısı boyut sınırını aştı.");
            emit previewChanged();
        }
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (m_previewProcess != process || error != QProcess::FailedToStart)
            return;
        m_previewProcess = nullptr;
        process->deleteLater();
        setPreviewRunning(false);
        m_previewError = QStringLiteral("DNF5 plan sorgusu başlatılamadı.");
        emit previewChanged();
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int code, QProcess::ExitStatus status) {
        if (m_previewProcess != process)
            return;
        m_previewBuffer += process->readAllStandardOutput();
        m_previewProcess = nullptr;
        process->deleteLater();
        setPreviewRunning(false);

        if (m_previewBuffer.size() > MaxOutputBytes) {
            m_previewError = QStringLiteral("DNF5 plan çıktısı boyut sınırını aştı.");
        } else {
            m_previewText = QString::fromLocal8Bit(m_previewBuffer).trimmed();
            if (m_previewText.isEmpty())
                m_previewError = QStringLiteral("DNF5 plan çıktısı boş. Herhangi bir işlem yapılmadı.");
            // --assumeno commonly exits non-zero after declining the plan.
            // Do not mistake that exit code for a successful installation.
            else if (status != QProcess::NormalExit)
                m_previewError = QStringLiteral("DNF5 plan sorgusu beklenmedik biçimde sonlandı.");
            qInfo() << "SYSTEM UPDATE PLAN PREVIEW EXIT:" << code
                    << "output bytes:" << m_previewBuffer.size();
        }
        emit previewChanged();
    });
    QTimer::singleShot(90000, process, [this, process]() {
        if (m_previewProcess != process || process->state() == QProcess::NotRunning)
            return;
        invalidatePreview();
        m_previewError = QStringLiteral("DNF5 plan sorgusu 90 saniyede tamamlanamadı. Önbellek ve depoları kontrol edin.");
        emit previewChanged();
    });
    process->start();
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
    invalidatePreview();
    clearSelection(); // A refreshed dependency graph invalidates old selections.
    m_stdout.clear();
    setLoading(true);

    auto *process = new QProcess(this);
    m_process = process;
    process->setProgram(QStringLiteral("dnf5"));
    process->setArguments({
        QStringLiteral("repoquery"),
        QStringLiteral("--upgrades"),
        QStringLiteral("--latest-limit=1"),
        QStringLiteral("--queryformat=%{name}\t%{evr}\t%{arch}\t%{repoid}\t%{downloadsize}\\n")
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
        if (columns.size() != 5)
            continue;

        SystemUpdateItem update {
            QString::fromUtf8(columns.at(0)).trimmed(),
            QString::fromUtf8(columns.at(1)).trimmed(),
            {},
            QString::fromUtf8(columns.at(2)).trimmed(),
            QString::fromUtf8(columns.at(3)).trimmed(),
            QString::fromUtf8(columns.at(4)).trimmed().toULongLong()
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
    m_selected.clear();
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
    for (const QByteArray &line : output.split('\n')) {
        const QList<QByteArray> parts = line.trimmed().split('\t');
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
