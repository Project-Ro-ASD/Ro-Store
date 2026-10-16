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
#include <QDebug>
#include <QProcessEnvironment>
#include <QRegularExpression>

namespace {

// Verify both the exact RPM identity and the trusted signing key. Always use
// the C locale for rpm output: translated "Tamam"/"OK" must not alter trust.
struct VerifiedRpm {
    bool valid = false;
    QString reason;
};

VerifiedRpm verifyStagedRpm(const QString &filePath, const QString &expectedNevra)
{
    const QFileInfo file(filePath);
    if (!file.isFile() || file.isSymLink() || file.size() <= 0)
        return { false, QStringLiteral("Eksik, boş veya güvenli olmayan RPM dosyası") };

    QProcess identify;
    identify.start(QStringLiteral("rpm"), {
        QStringLiteral("-qp"), QStringLiteral("--qf"),
        QStringLiteral("%{NAME}-%{EPOCHNUM}:%{VERSION}-%{RELEASE}.%{ARCH}"),
        filePath
    });
    if (!identify.waitForFinished(8000)
        || identify.exitStatus() != QProcess::NormalExit || identify.exitCode() != 0)
        return { false, QStringLiteral("RPM kimliği okunamadı") };

    QString identity = QString::fromUtf8(identify.readAllStandardOutput()).trimmed();
    if (identity != expectedNevra) {
        // dnf5 repoquery often omits epoch zero but rpm -qp emits "0:".
        identity.replace(QStringLiteral("-0:"), QStringLiteral("-"));
        if (identity != expectedNevra)
            return { false, QStringLiteral("RPM paket kimliği seçimle eşleşmiyor") };
    }

    QProcess signature;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    environment.insert(QStringLiteral("LANGUAGE"), QStringLiteral("C"));
    signature.setProcessEnvironment(environment);
    signature.start(QStringLiteral("rpm"), { QStringLiteral("--checksig"), filePath });
    if (!signature.waitForFinished(8000)
        || signature.exitStatus() != QProcess::NormalExit || signature.exitCode() != 0)
        return { false, QStringLiteral("RPM imza kontrolü başarısız: ")
                 + QString::fromLocal8Bit(signature.readAllStandardOutput()).trimmed().left(140) };

    const QString result = QString::fromLatin1(signature.readAllStandardOutput()).trimmed();
    if (!result.contains(QStringLiteral("signatures OK"), Qt::CaseInsensitive)
        || result.contains(QStringLiteral("NOT OK"), Qt::CaseInsensitive)
        || result.contains(QStringLiteral("NOKEY"), Qt::CaseInsensitive)
        || result.contains(QStringLiteral("NOTTRUSTED"), Qt::CaseInsensitive))
        return { false, QStringLiteral("RPM imzası doğrulanamadı: ") + result.left(140) };
    return { true, {} };
}

} // namespace

SystemUpdateCache::SystemUpdateCache(QObject *parent) : QObject(parent)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    // Keep new system-update assets away from the v0.5 temporary download
    // folders, where cancellation currently deletes in-flight RPMs.
    m_root = QDir(base).filePath(QStringLiteral("system-updates/rpms"));
    m_progressTimer.setInterval(350);
    connect(&m_progressTimer, &QTimer::timeout, this, &SystemUpdateCache::pollStagingProgress);
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
    if (m_downloading || nevras.isEmpty()) {
        qWarning() << "SYSTEM UPDATE DOWNLOAD SKIPPED: active or empty selection";
        return;
    }
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
    // Reuse verified completed RPMs rather than wasting bandwidth.
    QStringList missing;
    int reused = 0;
    for (const QString &nevra : nevras) {
        const QString cachedPath = QDir(m_root).filePath(nevra + QStringLiteral(".rpm"));
        if (QFileInfo::exists(cachedPath)) {
            const VerifiedRpm checked = verifyStagedRpm(cachedPath, nevra);
            if (checked.valid) {
                ++reused;
                continue;
            }
            // Never overwrite or delete an untrusted cached file automatically.
            m_error = QStringLiteral("Önbellekte doğrulanamayan RPM var: ")
                      + nevra + QStringLiteral(" — ") + checked.reason
                      + QStringLiteral(". Dosyayı inceleyin veya önbelleği temizleyin.");
            emit changed();
            return;
        }
        missing.append(nevra);
    }
    if (missing.isEmpty()) {
        m_hasDownloadResult = true;
        m_lastNewCount = 0;
        m_lastReusedCount = reused;
        m_lastRejectedCount = 0;
        m_staging.reset();
        m_error.clear();
        m_downloadStatus = QStringLiteral("%1 doğrulanmış RPM zaten önbellekte; indirme gerekmedi.").arg(reused);
        refresh();
        qInfo() << "SYSTEM UPDATE DOWNLOAD CACHE HIT:" << reused;
        return;
    }
    m_requested = missing;
    m_reusedThisRun = reused;
    m_hasDownloadResult = false;
    m_lastNewCount = 0;
    m_lastReusedCount = 0;
    m_lastRejectedCount = 0;
    m_error.clear();
    m_downloadLog.clear();
    m_pendingOutput.clear();
    m_downloadFile.clear();
    m_downloadPercent = -1;
    m_stagedBytes = 0;
    m_elapsedSeconds = 0;
    m_seenPackageDownloads = false;
    m_elapsedTimer.start();
    m_progressTimer.start();
    qInfo() << "SYSTEM UPDATE DOWNLOAD START (download only):" << missing
            << "staging directory:" << m_staging->path();
    m_downloadStatus = QStringLiteral("%1 RPM indiriliyor; %2 doğrulanmış dosya yeniden kullanılacak.").arg(missing.size()).arg(reused);
    auto *process = new QProcess(this);
    m_downloadProcess = process;
    m_downloading = true;
    process->setProgram(QStringLiteral("/usr/bin/dnf5"));
    QStringList args { QStringLiteral("download"), QStringLiteral("--destdir"), m_staging->path() };
    args.append(missing);
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        if (m_downloadProcess != process)
            return;
        appendDownloadOutput(process->readAllStandardOutput());
    });
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        if (m_downloadProcess != process)
            return;
        appendDownloadOutput(process->readAllStandardOutput());
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
    m_downloadStatus = QStringLiteral("İndirme iptal ediliyor; doğrulanan tamamlanmış dosyalar korunacak.");
    qInfo() << "SYSTEM UPDATE DOWNLOAD CANCEL REQUESTED";
    m_downloadProcess->kill(); // Only the standalone dnf5 download worker; no RPM transaction.
    emit changed();
}

void SystemUpdateCache::completeDownload(bool cancelled, int exitCode)
{
    m_progressTimer.stop();
    pollStagingProgress();
    int kept = 0;
    int rejected = 0;
    // Fail closed: only retain RPMs verified by the local RPM signature checker
    // AND matching one of the exact requested NEVRAs. A file extension alone
    // is not evidence of correctness.
    if (m_staging && m_staging->isValid()) {
        QDir folder(m_staging->path());
        const QFileInfoList files = folder.entryInfoList(
            {QStringLiteral("*.rpm")}, QDir::Files | QDir::NoSymLinks);
        for (const QFileInfo &file : files) {
            if (file.isSymLink() || !file.isFile() || file.size() <= 0) {
                ++rejected;
                continue;
            }
            // Match every completed RPM against the requested NEVRAs and
            // verify the signing key in a fixed locale before preservation.
            QString matchedNevra;
            QString rejectionReason;
            for (const QString &nevra : m_requested) {
                const VerifiedRpm checked = verifyStagedRpm(file.absoluteFilePath(), nevra);
                if (checked.valid) {
                    matchedNevra = nevra;
                    break;
                }
                rejectionReason = checked.reason;
            }
            if (matchedNevra.isEmpty()) {
                ++rejected;
                qWarning() << "SYSTEM UPDATE DOWNLOAD VERIFICATION REJECTED:"
                           << file.fileName() << rejectionReason;
                continue;
            }
            const QString destination = QDir(m_root).filePath(file.fileName());
            if (QFileInfo::exists(destination)) {
                // Never overwrite a previously staged RPM based on filename.
                qInfo() << "SYSTEM UPDATE DOWNLOAD ALREADY CACHED:" << file.fileName();
                continue;
            }
            if (QFile::rename(file.absoluteFilePath(), destination))
                ++kept;
            else {
                ++rejected;
                qWarning() << "SYSTEM UPDATE DOWNLOAD CACHE MOVE FAILED:" << file.fileName();
            }
        }
    }
    m_staging.reset(); // Discard incomplete/unverified files only.
    m_hasDownloadResult = true;
    m_lastNewCount = kept;
    m_lastReusedCount = m_reusedThisRun;
    m_lastRejectedCount = rejected;
    m_reusedThisRun = 0;
    m_requested.clear();
    m_downloading = false;
    m_downloadStatus = cancelled
        ? QStringLiteral("İndirme iptal edildi; %1 yeni RPM korundu, %2 önceden önbellekteydi.")
              .arg(kept).arg(m_lastReusedCount)
        : QStringLiteral("İndirme bitti (kod %1); %2 yeni, %3 önbellekten, %4 doğrulanamayan RPM.")
              .arg(exitCode).arg(kept).arg(m_lastReusedCount).arg(rejected);
    refresh();
    if (!cancelled && (exitCode != 0 || rejected > 0 || (kept == 0 && m_lastReusedCount == 0))) {
        m_error = QStringLiteral("İndirme tamamlanamadı veya RPM doğrulaması başarısız. "
                                 "Ayrıntılar için terminalde SYSTEM UPDATE DOWNLOAD kayıtlarını kontrol edin.");
        emit changed();
    }
    qInfo() << "SYSTEM UPDATE DOWNLOAD FINISHED:"
            << "cancelled:" << cancelled << "exit:" << exitCode
            << "kept:" << kept << "rejected:" << rejected
            << "cache count:" << m_count;
}


void SystemUpdateCache::appendDownloadOutput(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return;
    // DNF5 may write CR-only terminal updates. Keep only a bounded tail.
    m_pendingOutput += QString::fromLocal8Bit(bytes);
    m_pendingOutput.replace(QChar('\r'), QChar('\n'));
    const QStringList rows = m_pendingOutput.split(QChar('\n'));
    m_pendingOutput = rows.constLast().right(1000);
    static const QRegularExpression percentPattern(
        QStringLiteral(R"((?:^|\s)(100|[1-9]?[0-9])%)"));
    for (int i = 0; i + 1 < rows.size(); ++i) {
        const QString row = rows.at(i).trimmed();
        if (row.isEmpty())
            continue;
        qInfo().noquote() << "SYSTEM UPDATE DOWNLOAD OUTPUT:" << row.right(300);
        m_downloadLog += row.left(600) + QChar('\n');
        if (m_downloadLog.size() > 14000)
            m_downloadLog = m_downloadLog.right(14000);
        m_downloadStatus = row.left(320);
        if (row.contains(QStringLiteral("Downloading Packages:"), Qt::CaseInsensitive))
            m_seenPackageDownloads = true;
        if (m_seenPackageDownloads) {
            const auto match = percentPattern.match(row);
            if (match.hasMatch()) {
                m_downloadPercent = match.captured(1).toInt();
                m_downloadFile = row.left(match.capturedStart(0)).trimmed().left(150);
            }
        }
    }
    emit changed();
}

void SystemUpdateCache::pollStagingProgress()
{
    if (m_elapsedTimer.isValid())
        m_elapsedSeconds = static_cast<int>(m_elapsedTimer.elapsed() / 1000);
    m_stagedBytes = 0;
    if (m_staging && m_staging->isValid()) {
        const QDir folder(m_staging->path());
        const QFileInfoList files = folder.entryInfoList(QDir::Files | QDir::NoSymLinks);
        for (const QFileInfo &file : files)
            m_stagedBytes += static_cast<qulonglong>(qMax<qint64>(0, file.size()));
    }
    emit changed();
}
