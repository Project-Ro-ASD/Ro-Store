#include "InstalledRpmModel.h"

#include <QTimer>
#include <algorithm>

InstalledRpmModel::InstalledRpmModel(QObject *parent) : QObject(parent) {}

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
        m_error.clear();
        emit changed();
    });
    QTimer::singleShot(30000, process, [process]() {
        if (process->state() != QProcess::NotRunning) process->kill();
    });
    process->start();
}
