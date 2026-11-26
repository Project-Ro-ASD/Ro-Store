#include "InstalledRpmModel.h"

#include <QTimer>
#include <algorithm>

#ifdef RO_STORE_HAS_APPSTREAM
#include <AppStreamQt/pool.h>
#include <AppStreamQt/component.h>
#endif

InstalledRpmModel::InstalledRpmModel(QObject *parent) : QObject(parent)
{
#ifdef RO_STORE_HAS_APPSTREAM
    auto *pool = new AppStream::Pool(this);
    pool->setFlags(AppStream::Pool::Flags(AppStream::Pool::FlagLoadOsCatalog)
                   | AppStream::Pool::FlagLoadOsMetainfo);
    connect(pool, &AppStream::Pool::loadFinished, this, [this, pool](bool success) {
        if (!success) return;
        for (const auto &component : pool->components()) {
            if (component.kind() != AppStream::Component::KindDesktopApp) continue;
            const QString title = component.name().trimmed();
            QString appId = component.id().trimmed();
            if (appId.endsWith(QStringLiteral(".desktop")))
                appId.chop(8);
            for (const QString &name : component.packageNames()) {
                if (name.isEmpty()) continue;
                if (!m_applicationNames.contains(name))
                    m_applicationNames.insert(name, title.isEmpty() ? name : title);
                if (!appId.isEmpty() && !m_applicationIds[name].contains(appId))
                    m_applicationIds[name].append(appId);
            }
        }
        m_applicationMetadataReady = !m_applicationNames.isEmpty();
        annotateApplications();
        emit changed();
    });
    pool->loadAsync();
#endif
}

void InstalledRpmModel::annotateApplications()
{
    for (QVariant &value : m_packages) {
        QVariantMap row = value.toMap();
        const QString name = row.value(QStringLiteral("name")).toString();
        const auto it = m_applicationNames.constFind(name);
        row.insert(QStringLiteral("isApplication"), it != m_applicationNames.cend());
        row.insert(QStringLiteral("displayName"), it == m_applicationNames.cend() ? name : it.value());
        row.insert(QStringLiteral("appIds"), m_applicationIds.value(name));
        value = row;
    }
}

InstalledRpmModel::~InstalledRpmModel()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

void InstalledRpmModel::refresh()
{
    if (m_loading) return;
    m_loading = true;
    m_error.clear();
    emit changed();

    auto *process = new QProcess(this);
    m_process = process;
    process->setProgram(QStringLiteral("/usr/bin/rpm"));
    process->setArguments({QStringLiteral("--query"), QStringLiteral("--all"),
                           QStringLiteral("--queryformat"),
                           QStringLiteral("%{NAME}\t%{VERSION}-%{RELEASE}\t%{ARCH}\t%{SIZE}\n")});

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (m_process != process || error != QProcess::FailedToStart) return;
        m_loading = false;
        m_error = QStringLiteral("Kurulu RPM paketleri okunamadı: rpm başlatılamadı.");
        m_process = nullptr;
        process->deleteLater();
        emit changed();
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process](int code, QProcess::ExitStatus status) {
        if (m_process != process) return;
        m_process = nullptr;
        m_loading = false;
        const QByteArray output = process->readAllStandardOutput();
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0 || output.size() > 8 * 1024 * 1024) {
            m_error = QStringLiteral("Kurulu RPM paketleri okunamadı. Önceki liste korunuyor.");
            emit changed();
            return;
        }

        QVariantList next;
        const QList<QByteArray> lines = output.split('\n');
        for (const QByteArray &line : lines) {
            if (line.isEmpty()) continue;
            const QList<QByteArray> columns = line.split('\t');
            if (columns.size() != 4) continue;
            const QString name = QString::fromUtf8(columns.at(0)).trimmed();
            bool sizeOk = false;
            const qlonglong size = columns.at(3).toLongLong(&sizeOk);
            if (name.isEmpty() || !sizeOk || size < 0) continue;
            next.append(QVariantMap{
                {QStringLiteral("name"), name},
                {QStringLiteral("version"), QString::fromUtf8(columns.at(1)).trimmed()},
                {QStringLiteral("architecture"), QString::fromUtf8(columns.at(2)).trimmed()},
                {QStringLiteral("installedSize"), size}
            });
        }
        std::sort(next.begin(), next.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("name")).toString().compare(
                       b.toMap().value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
        });
        m_packages = next;
        annotateApplications();
        m_error.clear();
        emit changed();
    });
    QTimer::singleShot(30000, process, [process]() {
        if (process->state() != QProcess::NotRunning) process->kill();
    });
    process->start();
}
