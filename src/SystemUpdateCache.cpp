#include "SystemUpdateCache.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QSet>
#include <QTimer>

SystemUpdateCache::SystemUpdateCache(QObject *parent) : QObject(parent)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    // Keep new system-update assets away from the v0.5 temporary download
    // folders, where cancellation currently deletes in-flight RPMs.
    m_root = QDir(base).filePath(QStringLiteral("system-updates/rpms"));
    refresh();
}

SystemUpdateCache::~SystemUpdateCache()
{
    if (m_downloadProcess && m_downloadProcess->state() != QProcess::NotRunning) {
        m_downloadProcess->kill();
        m_downloadProcess->waitForFinished(3000);
    }
}

QString SystemUpdateCache::path() const { return m_root; }

void SystemUpdateCache::refresh()
{
    m_error.clear();
    m_count = 0;
    m_bytes = 0;
    QDir folder(m_root);
    if (!folder.exists()) {
        emit changed();
        return;
    }
    if (QFileInfo(m_root).isSymLink()) {
        m_error = QStringLiteral("Önbellek klasörü sembolik bağlantı olamaz.");
        emit changed();
        return;
    }
    QDirIterator iterator(m_root, {QStringLiteral("*.rpm")}, QDir::Files | QDir::NoSymLinks);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo file = iterator.fileInfo();
        if (!file.isFile() || file.isSymLink())
            continue;
        ++m_count;
        m_bytes += static_cast<qulonglong>(file.size());
    }
    emit changed();
}

void SystemUpdateCache::clear()
{
    if (m_downloading) {
        m_error = QStringLiteral("İndirme sürerken önbellek temizlenemez.");
        emit changed();
        return;
    }
    m_error.clear();
    const QFileInfo folderInfo(m_root);
    if (!folderInfo.exists()) {
        refresh();
        return;
    }
    if (!folderInfo.isDir() || folderInfo.isSymLink()) {
        m_error = QStringLiteral("Önbellek klasörü güvenli değil; silme engellendi.");
        emit changed();
        return;
    }
    // Only remove direct child RPMs in this dedicated application cache.
    // No recursion, wildcards outside our directory, or external dnf cache.
    QDir folder(m_root);
    const QFileInfoList items = folder.entryInfoList(
        {QStringLiteral("*.rpm")}, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &item : items) {
        if (!item.isFile() || item.isSymLink())
            continue;
        if (!QFile::remove(item.absoluteFilePath()))
            m_error = QStringLiteral("Bazı RPM dosyaları silinemedi.");
    }
    const QString result = m_error;
    refresh();
    if (!result.isEmpty()) {
        m_error = result;
        emit changed();
    }
}


void SystemUpdateCache::downloadSelected(const QStringList &nevras)
{
    if (m_downloading || nevras.isEmpty())
        return;
    if (nevras.size() > 3) {
        m_error = QStringLiteral("İlk güvenlik testinde en fazla 3 paket indirilebilir.");
        emit changed();
        return;
    }
    static const QRegularExpression safe(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.+\\-:~]{0,240}$"));
    QSet<QString> unique;
    for (const QString &name : nevras) {
        if (!safe.match(name).hasMatch() || unique.contains(name)) {
            m_error = QStringLiteral("Paket kimliği güvenli biçimde doğrulanamadı.");
            emit changed();
            return;
        }
        unique.insert(name);
    }
    const QFileInfo rootInfo(m_root);
    if ((rootInfo.exists() && (rootInfo.isSymLink() || !rootInfo.isDir()))
        || !QDir().mkpath(m_root)) {
        m_error = QStringLiteral("Güvenli RPM önbelleği oluşturulamadı.");
        emit changed();
        return;
    }
    m_staging = std::make_unique<QTemporaryDir>(QDir(m_root).filePath(QStringLiteral("staging-XXXXXX")));
    if (!m_staging->isValid()) {
        m_error = QStringLiteral("Geçici indirme dizini açılamadı.");
        emit changed();
        return;
    }
    m_requested = nevras;
    m_error.clear();
    m_downloadStatus = QStringLiteral("%1 RPM için indirme başlatıldı.").arg(nevras.size());
    auto *process = new QProcess(this);
    m_downloadProcess = process;
    m_downloading = true;
    process->setProgram(QStringLiteral("/usr/bin/dnf5"));
    QStringList args { QStringLiteral("download"), QStringLiteral("--destdir"), m_staging->path() };
    args.append(nevras);
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        if (m_downloadProcess != process)
            return;
        const QString log = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
        if (!log.isEmpty()) {
            m_downloadStatus = log.right(300);
            emit changed();
        }
    });
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        if (m_downloadProcess != process)
            return;
        m_downloadProcess = nullptr;
        process->deleteLater();
        completeDownload(status != QProcess::NormalExit, code);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (m_downloadProcess != process || error != QProcess::FailedToStart)
            return;
        m_error = QStringLiteral("DNF5 indirici başlatılamadı.");
        m_downloadProcess = nullptr;
        process->deleteLater();
        completeDownload(true, -1);
    });
    process->start();
    QTimer::singleShot(300000, process, [this, process]() {
        if (m_downloadProcess == process && process->state() != QProcess::NotRunning) {
            m_downloadStatus = QStringLiteral("İndirme 5 dakikalık zaman sınırına ulaştı; durduruluyor.");
            process->kill();
            emit changed();
        }
    });
    emit changed();
}

void SystemUpdateCache::cancelDownload()
{
    if (!m_downloading || !m_downloadProcess)
        return;
    m_downloadStatus = QStringLiteral("İndirme iptal ediliyor, tamamlanmış RPM dosyaları korunacak.");
    m_downloadProcess->kill(); // Only the standalone dnf5 download worker; no RPM transaction.
    emit changed();
}

void SystemUpdateCache::completeDownload(bool cancelled, int exitCode)
{
    int kept = 0;
    // Fail closed: only retain RPMs verified by the local RPM signature checker
    // AND matching one of the exact requested NEVRAs. A file extension alone
    // is not evidence of correctness.
    if (m_staging && m_staging->isValid()) {
        QDir folder(m_staging->path());
        const QFileInfoList files = folder.entryInfoList(
            {QStringLiteral("*.rpm")}, QDir::Files | QDir::NoSymLinks);
        for (const QFileInfo &file : files) {
            if (file.isSymLink() || !file.isFile() || file.size() <= 0)
                continue;
            QProcess identify;
            identify.start(QStringLiteral("rpm"),
                {QStringLiteral("-qp"), QStringLiteral("--qf"),
                 QStringLiteral("%{NAME}-%{EPOCHNUM}:%{VERSION}-%{RELEASE}.%{ARCH}"),
                 file.absoluteFilePath()});
            if (!identify.waitForFinished(8000) || identify.exitCode() != 0)
                continue;
            QString identity = QString::fromUtf8(identify.readAllStandardOutput()).trimmed();
            if (!m_requested.contains(identity)) {
                // DNF candidate EVR commonly omits an epoch of zero.
                identity.replace(QStringLiteral("-0:"), QStringLiteral("-"));
                if (!m_requested.contains(identity))
                    continue;
            }
            QProcess signature;
            signature.start(QStringLiteral("rpm"), {QStringLiteral("--checksig"), file.absoluteFilePath()});
            if (!signature.waitForFinished(8000) || signature.exitCode() != 0)
                continue;
            const QString check = QString::fromLocal8Bit(signature.readAllStandardOutput());
            if (!check.contains(QStringLiteral("OK"), Qt::CaseInsensitive)
                || check.contains(QStringLiteral("NOT OK"), Qt::CaseInsensitive)
                || check.contains(QStringLiteral("NOKEY"), Qt::CaseInsensitive))
                continue;
            const QString destination = QDir(m_root).filePath(file.fileName());
            if (QFileInfo::exists(destination)) {
                // Never overwrite a previously staged RPM based on filename.
                continue;
            }
            if (QFile::rename(file.absoluteFilePath(), destination))
                ++kept;
        }
    }
    m_staging.reset(); // Discard incomplete/unverified files only.
    m_requested.clear();
    m_downloading = false;
    m_downloadStatus = cancelled
        ? QStringLiteral("İndirme durduruldu; %1 doğrulanmış RPM korundu.").arg(kept)
        : QStringLiteral("İndirme bitti (kod %1); %2 doğrulanmış RPM korundu.").arg(exitCode).arg(kept);
    refresh();
}
