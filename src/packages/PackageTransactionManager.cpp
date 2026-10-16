#include "PackageTransactionManager.h"
#include "backends/Dnf5Backend.h"

#include <QDebug>
#include <QLocale>
#include <QStandardPaths>
#include <QDir>
#include <QSaveFile>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QTimeZone>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDirIterator>

namespace {
QString readableSize(qulonglong bytes)
{
    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mib < 1024.0)
        return QLocale::c().toString(mib, 'f', 2) + QStringLiteral(" MB");
    return QLocale::c().toString(mib / 1024.0, 'f', 2) + QStringLiteral(" GB");
}

Dnf5Backend::TransactionOperation toBackendOperation(
    PackageTransaction::Operation operation
)
{
    switch (operation) {
    case PackageTransaction::Operation::Install:
        return Dnf5Backend::TransactionOperation::Install;

    case PackageTransaction::Operation::Remove:
        return Dnf5Backend::TransactionOperation::Remove;

    case PackageTransaction::Operation::Upgrade:
        return Dnf5Backend::TransactionOperation::Upgrade;
    }

    return Dnf5Backend::TransactionOperation::Install;
}

}

PackageTransactionManager::PackageTransactionManager(
    QObject *parent
)
    : QObject(parent),
      m_model(new PackageTransactionModel(this)),
      m_backend(new Dnf5Backend(this))
{
    if (QFile::exists(journalPath())) {
        activateRecoveryHold(QStringLiteral("Önceki Ro-Store işlemi kesin sonuç alınmadan sonlandı. Yeni paket işlemleri güvenlik amacıyla engellendi."));
        QTimer::singleShot(0, this, &PackageTransactionManager::recheckRecovery);
    }
    connect(
        m_backend,
        &Dnf5Backend::transactionResolved,
        this,
        [this](
            Dnf5Backend::TransactionOperation operation,
            const QString &packageName,
            uint result,
            const QVariantList &resolvedItems,
            qulonglong totalDownloadBytes
        ) {
            if (!m_activeTransaction) {
                return;
            }

            if (m_activeTransaction->packageName() != packageName) {
                return;
            }

            if (toBackendOperation(
                    m_activeTransaction->operation()
                ) != operation) {
                return;
            }

            m_activeTransaction->appendTechnicalLog(QStringLiteral("Çözümleme başarılı: %1 işlem, %2, sonuç kodu %3.")
                .arg(resolvedItems.size()).arg(readableSize(totalDownloadBytes)).arg(result));
            for (const QVariant &itemValue : resolvedItems) {
                const QVariantMap item = itemValue.toMap();
                const QVariantMap pkg = item.value(QStringLiteral("object")).toMap();
                m_activeTransaction->appendTechnicalLog(QStringLiteral("RPM: %1 %2 %3 [%4]")
                    .arg(item.value(QStringLiteral("action")).toString(),
                         pkg.value(QStringLiteral("name")).toString(),
                         pkg.value(QStringLiteral("version")).toString(),
                         pkg.value(QStringLiteral("repo_id")).toString()));
            }
            m_activeTransaction->setResolvedItems(
                resolvedItems
            );

            m_activeTransaction->setTotalBytes(
                totalDownloadBytes
            );

            m_activeTransaction->setDownloadedBytes(0);
            m_activeTransaction->setProgress(0);

            if (!m_downloadSpeedTimer.isValid()) {
                m_lastSpeedSampleBytes = 0;
                m_smoothedDownloadSpeed = 0.0;

                m_activeTransaction
                    ->setDownloadSpeedBytesPerSecond(0);

                m_downloadSpeedTimer.start();
            }

            m_activeTransaction->setState(
                PackageTransaction::State::Ready
            );
            if (m_cancelRequested) {
                cancelActiveFinished();
                return;
            }

            qInfo()
                << "TRANSACTION MANAGER READY:"
                << packageName
                << "items:"
                << resolvedItems.size()
                << "download:"
                << totalDownloadBytes
                << "resolve result:"
                << result;

            emit transactionResolved(
                m_activeTransaction
            );
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::transactionResolveFailed,
        this,
        [this](
            Dnf5Backend::TransactionOperation operation,
            const QString &packageName,
            const QString &error
        ) {
            if (!m_activeTransaction) {
                return;
            }

            if (m_activeTransaction->packageName() != packageName) {
                return;
            }

            if (toBackendOperation(
                    m_activeTransaction->operation()
                ) != operation) {
                return;
            }

            qWarning()
                << "TRANSACTION MANAGER RESOLVE FAILED:"
                << packageName
                << error;

            m_activeTransaction->appendTechnicalLog(QStringLiteral("ÇÖZÜMLEME HATASI: ") + error);
            if (m_cancelRequested) {
                cancelActiveFinished();
                return;
            }
            failActive(error);
        }
    );

    connect(m_backend, &Dnf5Backend::localRpmTransactionPrepared, this, [this]() {
        if (!m_activeTransaction || m_cancelRequested || m_outcomeUncertain)
            return;
        m_rpmPhaseStarted = true;
        m_activeTransaction->setState(PackageTransaction::State::Running);
        m_activeTransaction->appendTechnicalLog(QStringLiteral(
            "Paketler yerel dosyalardan doğrulandı. RPM uygulanıyor (internet indirmesi kapalı)."));
        m_backend->executeResolvedTransaction(false);
    });
    connect(m_backend, &Dnf5Backend::localRpmTransactionPreparationFailed,
            this, [this](const QString &error) {
        if (m_activeTransaction && !m_outcomeUncertain)
            failActive(QStringLiteral("Yerel paket doğrulama başarısız: ") + error);
    });

    connect(
        m_backend,
        &Dnf5Backend::transactionExecutionStarted,
        this,
        [this](bool downloadOnly) {
            if (!m_activeTransaction || m_outcomeUncertain) {
                return;
            }

            qInfo()
                << "TRANSACTION MANAGER EXECUTION STARTED:"
                << m_activeTransaction->packageName()
                << "downloadOnly:"
                << downloadOnly;
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::transactionExecutionFinished,
        this,
        [this](bool downloadOnly) {
            if (!m_activeTransaction) {
                return;
            }

            qInfo()
                << "TRANSACTION MANAGER EXECUTION FINISHED:"
                << m_activeTransaction->packageName()
                << "downloadOnly:"
                << downloadOnly;

            if (downloadOnly) {
                // The public download-only test must never implicitly install.
                if (!m_downloadOnlyPhase) {
                    m_activeTransaction->setState(PackageTransaction::State::Ready);
                    return;
                }
                m_downloadOnlyPhase = false;
                if (m_cancelRequested || m_outcomeUncertain) {
                    m_activeTransaction->appendTechnicalLog(QStringLiteral(
                        "İptal veya belirsiz sonuç: RPM kurulumu BAŞLATILMADI."));
                    cancelActiveFinished();
                    return;
                }
                m_activeTransaction->appendTechnicalLog(QStringLiteral(
                    "İndirme tamamlandı, şimdi RPM aşamasına geçiliyor."));
                m_rpmPhaseStarted = true;
                m_activeTransaction->setState(PackageTransaction::State::Running);
                m_backend->executeResolvedTransaction(false);
                return;


            }

            // An accepted cancel request is not evidence of cancellation;
            // successful DNF completion remains authoritative.
            finishActive();
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::transactionExecutionFailed,
        this,
        [this](const QString &error) {
            if (!m_activeTransaction || m_outcomeUncertain) {
                return;
            }

            qWarning()
                << "TRANSACTION MANAGER EXECUTION FAILED:"
                << m_activeTransaction->packageName()
                << error;

            if (m_cancelRequested && m_downloadOnlyPhase && !m_rpmPhaseStarted) {
                qInfo()
                    << "TRANSACTION MANAGER CANCELLED:"
                    << m_activeTransaction->packageName();

                cancelActiveFinished();
                return;
            }

            m_activeTransaction->appendTechnicalLog(QStringLiteral("ÇALIŞTIRMA HATASI: ") + error);
            failActive(error);
        }
    );

    connect(m_backend, &Dnf5Backend::transactionExecutionOutcomeUnknown,
            this, [this](const QString &error) {
        if (!m_activeTransaction || m_outcomeUncertain)
            return;
        activateRecoveryHold(error);
        const QString warning = QStringLiteral(
            "DNF5 yanıtı alınamadı. İşlem arka planda devam etmiş olabilir. "
            "Yeni işlemler güvenlik nedeniyle durduruldu. "
            "DNF5 servis durumunu ve RPM sonucunu kontrol edin.\n") + error;
        m_activeTransaction->appendTechnicalLog(QStringLiteral("SONUÇ BELİRSİZ: ") + warning);
        m_activeTransaction->markOutcomeUnknown(warning);
        m_activeTransaction->setCancelRequested(false);
        for (PackageTransaction *queued : m_queue) {
            queued->appendTechnicalLog(QStringLiteral(
                "Önceki DNF5 işleminin sonucu belirsiz olduğundan bu istek çalıştırılmadı."));
            queued->setErrorMessage(QStringLiteral(
                "İşlem DNF5'e gönderilmedi; önceki işlem belirsizleşince kuyruktan kaldırıldı."));
            queued->setState(PackageTransaction::State::Cancelled);
        }
        m_queue.clear();
        emit queuedCountChanged();
        // Deliberately do NOT reset/close the daemon session here:
        // it may still be running the original transaction.
        emit activeTransactionChanged();
    });

    connect(
        m_backend,
        &Dnf5Backend::downloadStarted,
        this,
        [this](
            const QString &downloadId,
            const QString &description,
            qlonglong totalBytes
        ) {
            if (m_outcomeUncertain || !m_activeTransaction ||
                !downloadId.startsWith(
                    QStringLiteral("package:")
                )) {
                return;
            }

            const qulonglong safeTotal =
                totalBytes > 0
                    ? static_cast<qulonglong>(totalBytes)
                    : 0;

            m_downloadTotalById.insert(
                downloadId,
                safeTotal
            );

            m_downloadedById.insert(
                downloadId,
                0
            );

            qulonglong totalDownloadBytes = 0;

            for (auto it = m_downloadTotalById.cbegin();
                 it != m_downloadTotalById.cend();
                 ++it) {
                totalDownloadBytes += it.value();
            }

            m_activeTransaction->setTotalBytes(
                totalDownloadBytes
            );

            m_activeTransaction->setDownloadedBytes(0);
            m_activeTransaction->setProgress(0);

            m_activeTransaction->setState(
                PackageTransaction::State::Downloading
            );
            m_activeTransaction->appendTechnicalLog(QStringLiteral("İndirme başladı: %1 (%2)")
                .arg(description, readableSize(safeTotal)));

            qInfo()
                << "TRANSACTION PACKAGE DOWNLOAD START:"
                << downloadId
                << description
                << safeTotal;
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::downloadProgressChanged,
        this,
        [this](
            const QString &downloadId,
            qlonglong totalBytes,
            qlonglong downloadedBytes
        ) {
            if (!m_activeTransaction ||
                !downloadId.startsWith(
                    QStringLiteral("package:")
                )) {
                return;
            }

            const qulonglong safeTotal =
                totalBytes > 0
                    ? static_cast<qulonglong>(totalBytes)
                    : 0;

            const qulonglong safeDownloaded =
                downloadedBytes > 0
                    ? static_cast<qulonglong>(downloadedBytes)
                    : 0;

            if (safeTotal > 0) {
                m_downloadTotalById.insert(
                    downloadId,
                    safeTotal
                );
            }

            m_downloadedById.insert(
                downloadId,
                safeDownloaded
            );

            qulonglong downloadedSum = 0;

            for (auto it = m_downloadedById.cbegin();
                 it != m_downloadedById.cend();
                 ++it) {
                downloadedSum += it.value();
            }

            m_activeTransaction->setDownloadedBytes(
                downloadedSum
            );

            updateDownloadSpeed(downloadedSum);

            const qulonglong transactionTotal =
                m_activeTransaction->totalBytes();

            if (transactionTotal > 0) {
                const int percent =
                    static_cast<int>(
                        qMin<qulonglong>(
                            100,
                            downloadedSum * 100 /
                                transactionTotal
                        )
                    );

                m_activeTransaction->setProgress(
                    percent
                );
            }
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::downloadFinished,
        this,
        [this](
            const QString &downloadId,
            uint status,
            const QString &message
        ) {
            if (!m_activeTransaction ||
                !downloadId.startsWith(
                    QStringLiteral("package:")
                )) {
                return;
            }

            if (status == 2) {
                qWarning()
                    << "TRANSACTION PACKAGE DOWNLOAD FAILED:"
                    << downloadId
                    << message;

                return;
            }

            const qulonglong total =
                m_downloadTotalById.value(
                    downloadId,
                    0
                );

            if (total > 0) {
                m_downloadedById.insert(
                    downloadId,
                    total
                );
            }

            qulonglong downloadedSum = 0;

            for (auto it = m_downloadedById.cbegin();
                 it != m_downloadedById.cend();
                 ++it) {
                downloadedSum += it.value();
            }

            m_activeTransaction->setDownloadedBytes(
                downloadedSum
            );

            updateDownloadSpeed(downloadedSum);

            const qulonglong transactionTotal =
                m_activeTransaction->totalBytes();

            if (transactionTotal > 0) {
                const int percent =
                    static_cast<int>(
                        qMin<qulonglong>(
                            100,
                            downloadedSum * 100 /
                                transactionTotal
                        )
                    );

                m_activeTransaction->setProgress(
                    percent
                );
            }

            qInfo()
                << "TRANSACTION PACKAGE DOWNLOAD END:"
                << downloadId
                << "status:"
                << status
                << "downloaded:"
                << downloadedSum
                << "/"
                << transactionTotal;
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::rpmActionStarted,
        this,
        [this](
            const QString &nevra,
            uint action,
            qulonglong total
        ) {
            if (m_outcomeUncertain || !m_activeTransaction) {
                return;
            }

            m_activeTransaction
                ->setDownloadSpeedBytesPerSecond(0);

            resetDownloadSpeedTracking();

            m_activeTransaction->setState(
                PackageTransaction::State::Running
            );

            qInfo()
                << "TRANSACTION RPM START:"
                << nevra
                << action
                << total;
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::rpmActionProgressChanged,
        this,
        [this](
            const QString &nevra,
            qulonglong processed,
            qulonglong total
        ) {
            if (!m_activeTransaction ||
                total == 0) {
                return;
            }

            const int percent =
                static_cast<int>(
                    qMin<qulonglong>(
                        100,
                        processed * 100 / total
                    )
                );

            m_activeTransaction->setProgress(
                percent
            );

            qInfo()
                << "TRANSACTION RPM PROGRESS:"
                << nevra
                << percent;
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::rpmTransactionFinished,
        this,
        [this](bool success) {
            if (!m_activeTransaction) {
                return;
            }

            qInfo()
                << "TRANSACTION RPM COMPLETE:"
                << success;

            // Terminal sonucu do_transaction() cevabı belirler.
            // Burada yalnızca RPM sonucunu gözlemliyoruz.
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::transactionResetFinished,
        this,
        [this]() {
            qInfo()
                << "TRANSACTION MANAGER SESSION REFRESH FINISHED";

            releaseActiveAndStartNext();
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::transactionResetFailed,
        this,
        [this](const QString &error) {
            qWarning()
                << "TRANSACTION MANAGER SESSION REFRESH FAILED:"
                << error;

            // Backend başarısız reset durumunda session'ı temizliyor.
            // Sonraki işlem yeni session açabilir.
            releaseActiveAndStartNext();
        }
    );

    connect(m_backend, &Dnf5Backend::transactionCancelOutcomeUnknown,
            this, [this](const QString &error) {
        if (!m_activeTransaction || m_outcomeUncertain)
            return;
        m_activeTransaction->appendTechnicalLog(
            QStringLiteral("İptal isteğinin sonucu belirsiz; DNF5 işlemi devam edebilir: ") + error);
        activateRecoveryHold(QStringLiteral(
            "DNF5 iptal isteğine kesin yanıt alınamadı. İndirme veya RPM işlemi devam edebilir. "
            "Yeni paket işlemleri güvenlik nedeniyle engellendi. ") + error);
        m_activeTransaction->markOutcomeUnknown(m_recoveryDetails);
        for (PackageTransaction *queued : m_queue) {
            queued->appendTechnicalLog(QStringLiteral("Önceki işlem belirsiz: isteğiniz DNF5'e gönderilmedi."));
            queued->setState(PackageTransaction::State::Cancelled);
        }
        m_queue.clear();
        emit queuedCountChanged();
        emit activeTransactionChanged();
    });

    connect(
        m_backend,
        &Dnf5Backend::transactionCancelFinished,
        this,
        [this](
            bool success,
            const QString &error
        ) {
            if (!m_activeTransaction) {
                return;
            }

            if (!success) {
                m_cancelRequested = false;
                m_activeTransaction->setCancelRequested(false);
                m_activeTransaction->appendTechnicalLog(QStringLiteral(
                    "DNF5 iptal isteğini reddetti. Mevcut işlem takip ediliyor: ") + error);

                qWarning()
                    << "TRANSACTION MANAGER CANCEL REJECTED:"
                    << error;
                return;
            }

            m_activeTransaction->appendTechnicalLog(QStringLiteral("DNF5 iptal isteğini kabul etti; işlem sonucu bekleniyor."));
            qInfo()
                << "TRANSACTION MANAGER CANCEL ACCEPTED:"
                << m_activeTransaction->packageName();
        }
    );


}


QString PackageTransactionManager::journalPath() const
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return directory + QStringLiteral("/pending-dnf5-transaction.json");
}

void PackageTransactionManager::setRecoveryDetails(const QString &details)
{
    if (m_recoveryDetails == details)
        return;
    m_recoveryDetails = details;
    emit recoveryDetailsChanged();
}

QString PackageTransactionManager::recoveryDetails() const
{
    return m_recoveryDetails;
}

void PackageTransactionManager::activateRecoveryHold(const QString &reason)
{
    if (!m_outcomeUncertain) {
        m_outcomeUncertain = true;
        emit outcomeUncertainChanged();
    }
    setRecoveryDetails(reason);
}

void PackageTransactionManager::persistPendingExecution()
{
    if (!m_activeTransaction)
        return;
    const QFileInfo destination(journalPath());
    if (!QDir().mkpath(destination.absolutePath())) {
        activateRecoveryHold(QStringLiteral("DNF5 güvenlik günlüğü için dizin oluşturulamadı; paket işlemi başlatılmadı."));
        return;
    }
    QSaveFile file(journalPath());
    if (!file.open(QIODevice::WriteOnly)) {
        activateRecoveryHold(QStringLiteral("DNF5 güvenlik günlüğü yazılamadı; paket işlemi başlatılmadı."));
        return;
    }
    m_pendingPackage = m_activeTransaction->packageName();
    m_pendingOperation = QString::number(static_cast<int>(m_activeTransaction->operation()));
    m_pendingStartedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const QJsonObject data {
        {QStringLiteral("package"), m_pendingPackage},
        {QStringLiteral("operation"), m_pendingOperation},
        {QStringLiteral("started_at"), m_pendingStartedAt},
        {QStringLiteral("state"), QStringLiteral("dispatched_or_pending")}
    };
    const QByteArray bytes = QJsonDocument(data).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        activateRecoveryHold(QStringLiteral("Güvenlik günlüğü diske yazılamadı; işlem başlatılmadı."));
    }
}

void PackageTransactionManager::clearPendingExecution()
{
    if (!QFile::exists(journalPath()))
        return;
    if (!QFile::remove(journalPath())) {
        activateRecoveryHold(QStringLiteral("Tamamlanan işleme ait güvenlik kaydı temizlenemedi."));
    } else {
        m_pendingPackage.clear();
        m_pendingOperation.clear();
        m_pendingStartedAt.clear();
    }
}

void PackageTransactionManager::recheckRecovery()
{
    if (m_recheckBusy || !m_outcomeUncertain)
        return;
    QFile file(journalPath());
    if (!file.open(QIODevice::ReadOnly)) {
        setRecoveryDetails(QStringLiteral("Kurtarma kaydı okunamadı. Yeni işlem engellendi."));
        return;
    }
    const QJsonDocument data = QJsonDocument::fromJson(file.readAll());
    if (!data.isObject()) {
        setRecoveryDetails(QStringLiteral("Kurtarma kaydı geçersiz. Yeni işlem engellendi."));
        return;
    }
    const QJsonObject obj = data.object();
    m_pendingPackage = obj.value(QStringLiteral("package")).toString();
    m_pendingOperation = obj.value(QStringLiteral("operation")).toString();
    m_pendingStartedAt = obj.value(QStringLiteral("started_at")).toString();
    if (m_pendingPackage.isEmpty()) {
        setRecoveryDetails(QStringLiteral("Kurtarma kaydında paket bilgisi eksik. Yeni işlem engellendi."));
        return;
    }
    m_recoveryVerified = false;
    m_recheckBusy = true;
    auto *process = new QProcess(this);
    process->setProgram(QStringLiteral("rpm"));
    process->setArguments({QStringLiteral("-q"), QStringLiteral("--"), m_pendingPackage});
    connect(process, &QProcess::finished, this,
        [this, process](int exitCode, QProcess::ExitStatus status) {
            m_recheckBusy = false;
            const bool rpmQueryValid = status == QProcess::NormalExit && (exitCode == 0 || exitCode == 1);
            const bool installed = status == QProcess::NormalExit && exitCode == 0;
            const QString rpmResult = installed ? QStringLiteral("Kurulu") : QStringLiteral("Kurulu görünmüyor");
            const QString message = QStringLiteral(
                "Önceki paket: %1 | İşlem kodu: %2 | Başlangıç (UTC): %3\n"
                "RPM durumu: %4. Bu kontrol tek başına DNF5 işleminin tamamlandığını "
                "veya artık çalışmadığını kanıtlamaz.\n"
                "İşlem geçmişini ve dnf5daemon-server durumunu kontrol edip "
                "önceki işlemin bittiğinden emin olmadan kilidi kaldırmayın.")
                .arg(m_pendingPackage, m_pendingOperation, m_pendingStartedAt, rpmResult);
            process->deleteLater();
            // Cross-check DNF5's actual history. This stays read-only,
            // and never silently reenqueues a recovered mutation.
            auto *history = new QProcess(this);
            history->setProgram(QStringLiteral("dnf5"));
            // Compare wall-clock timestamps in one unambiguous timezone.
            // The durable Ro-Store journal records ISO 8601 UTC.
            QProcessEnvironment historyEnvironment = QProcessEnvironment::systemEnvironment();
            historyEnvironment.insert(QStringLiteral("TZ"), QStringLiteral("UTC"));
            history->setProcessEnvironment(historyEnvironment);
            history->setArguments({QStringLiteral("history"), QStringLiteral("list")});
            connect(history, &QProcess::finished, this,
                [this, history, message, installed, rpmQueryValid](int code, QProcess::ExitStatus result) {
                    const QString output = QString::fromLocal8Bit(history->readAllStandardOutput());
                    if (result != QProcess::NormalExit || code != 0) {
                        setRecoveryDetails(message + QStringLiteral("\nDNF5 geçmişi okunamadı. Kilit korunuyor."));
                        history->deleteLater();
                        return;
                    }
                    // Do not inspect only the newest history row: it can
                    // belong to another package or a much older transaction.
                    const QDateTime expectedStart = QDateTime::fromString(
                        m_pendingStartedAt, Qt::ISODate).toUTC();
                    const QRegularExpression entryPattern(
                        QStringLiteral("^\\s*(\\d+)\\s+dnf5daemon-server\\s+"
                                       "(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})"),
                        QRegularExpression::MultilineOption);
                    auto iterator = entryPattern.globalMatch(output);
                    QString id;
                    while (iterator.hasNext()) {
                        const auto entry = iterator.next();
                        QDateTime candidate = QDateTime::fromString(
                            entry.captured(2), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                        candidate.setTimeZone(QTimeZone::UTC);
                        if (candidate.isValid() && expectedStart.isValid()
                            && candidate >= expectedStart.addSecs(-20)) {
                            id = entry.captured(1);
                            break;
                        }
                    }
                    if (id.isEmpty()) {
                        setRecoveryDetails(message + QStringLiteral(
                            "\nİşlem başlangıcından sonraya ait DNF5 geçmiş kaydı bulunamadı. "
                            "Eski işlemler eşleştirilmeyecek; kilit korunuyor."));
                        history->deleteLater();
                        return;
                    }
                    history->deleteLater();
                    auto *details = new QProcess(this);
                    details->setProgram(QStringLiteral("dnf5"));
                    QProcessEnvironment detailsEnvironment = QProcessEnvironment::systemEnvironment();
                    detailsEnvironment.insert(QStringLiteral("TZ"), QStringLiteral("UTC"));
                    details->setProcessEnvironment(detailsEnvironment);
                    details->setArguments({QStringLiteral("history"), QStringLiteral("info"), id});
                    connect(details, &QProcess::finished, this,
                        [this, details, message, id, installed, rpmQueryValid](int exit, QProcess::ExitStatus state) {
                            const QString text = QString::fromLocal8Bit(details->readAllStandardOutput());
                            const QRegularExpression beginPattern(QStringLiteral("Begin time\\s*:\\s*(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})"));
                            const auto startMatch = beginPattern.match(text);
                            QDateTime recordedStart = QDateTime::fromString(
                                startMatch.captured(1), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                            recordedStart.setTimeZone(QTimeZone::UTC);
                            const QDateTime expectedStart = QDateTime::fromString(
                                m_pendingStartedAt, Qt::ISODate).toUTC();
                            const bool matchingDate = recordedStart.isValid()
                                && expectedStart.isValid()
                                && recordedStart >= expectedStart.addSecs(-20);
                            const bool correctInstalledState = m_pendingOperation == QStringLiteral("1")
                                ? !installed : installed;
                            const bool completed = state == QProcess::NormalExit && exit == 0
                                && text.contains(QRegularExpression(QStringLiteral("Status\\s*:\\s*Ok"), QRegularExpression::CaseInsensitiveOption))
                                && text.contains(QStringLiteral("Ro-Store"))
                                && text.contains(m_pendingPackage, Qt::CaseInsensitive)
                                && matchingDate
                                && correctInstalledState && rpmQueryValid;
                            m_recoveryVerified = completed;
                            // This is supporting evidence only. An older entry with
                            // the same package name is not necessarily our transaction.
                            setRecoveryDetails(message + QStringLiteral("\nEşleşen DNF işlemi #%1: %2\n")
                                .arg(id, completed ? QStringLiteral("Ro-Store kaydı bulundu.")
                                                   : QStringLiteral("Eşleşen başarı kaydı doğrulanamadı."))
                                + text.left(4000)
                                + (completed
                                   ? QStringLiteral("\nDoğrulama geçti. Yeni paket işlemlerinin kilidini kaldırmak için açık onayın gerekiyor.")
                                   : QStringLiteral("\nDoğrulama tamamlanmadı; güvenlik kilidi korunuyor.")));
                            details->deleteLater();
                        });
                    details->start();
                });
            history->start();
        });
    process->start();
}

void PackageTransactionManager::acknowledgeRecovery()
{
    // Never release a running/ambiguous in-process DNF session.
    // Only a recovered history entry with matching time, package, successful
    // outcome and installed state can be acknowledged by the user.
    if (m_activeTransaction || !m_recoveryVerified || !m_outcomeUncertain)
        return;
    if (!QFile::remove(journalPath())) {
        setRecoveryDetails(QStringLiteral("Doğrulanmış güvenlik kaydı temizlenemedi."));
        return;
    }
    m_pendingPackage.clear();
    m_pendingOperation.clear();
    m_pendingStartedAt.clear();
    m_outcomeUncertain = false;
    m_recoveryVerified = false;
    emit outcomeUncertainChanged();
    setRecoveryDetails(QString());
    // Intentionally never restore or replay old queued operations.
}

PackageTransactionModel *
PackageTransactionManager::model() const
{
    return m_model;
}

PackageTransaction *
PackageTransactionManager::activeTransaction() const
{
    return m_activeTransaction;
}

int PackageTransactionManager::queuedCount() const
{
    return m_queue.size();
}

bool PackageTransactionManager::busy() const
{
    return m_activeTransaction != nullptr || m_externalOwner != nullptr;
}

bool PackageTransactionManager::acquireExternalOperation(QObject *owner)
{
    if (!owner || busy() || m_outcomeUncertain || !m_queue.isEmpty())
        return false;
    m_externalOwner = owner;
    emit activeTransactionChanged();
    return true;
}

void PackageTransactionManager::releaseExternalOperation(QObject *owner)
{
    if (!owner || m_externalOwner != owner)
        return;
    m_externalOwner = nullptr;
    emit activeTransactionChanged();
    startNext();
}

bool PackageTransactionManager::outcomeUncertain() const
{
    return m_outcomeUncertain;
}

bool PackageTransactionManager::canCancelActive() const
{
    if (!m_activeTransaction || m_outcomeUncertain || m_cancelRequested)
        return false;
    const auto state = m_activeTransaction->state();
    return state == PackageTransaction::State::Resolving
        || state == PackageTransaction::State::Ready
        || (state == PackageTransaction::State::Downloading && m_downloadOnlyPhase);
}

QObject *PackageTransactionManager::enqueueInstall(
    const QString &packageName,
    const QString &repositoryId
)
{
    return enqueue(
        packageName,
        PackageTransaction::Operation::Install,
        repositoryId
    );
}

QObject *PackageTransactionManager::enqueueRemove(
    const QString &packageName
)
{
    return enqueue(
        packageName,
        PackageTransaction::Operation::Remove,
        QString()
    );
}

QObject *PackageTransactionManager::enqueueUpgrade(
    const QString &packageName,
    const QString &repositoryId
)
{
    return enqueue(
        packageName,
        PackageTransaction::Operation::Upgrade,
        repositoryId
    );
}

PackageTransaction *
PackageTransactionManager::enqueue(
    const QString &packageName,
    PackageTransaction::Operation operation,
    const QString &repositoryId
)
{
    const QString cleanName = packageName.trimmed();

    if (cleanName.isEmpty() || m_outcomeUncertain || m_externalOwner) {
        qWarning() << "TRANSACTION BLOCKED: unresolved previous DNF5 outcome";
        return nullptr;
    }

    if (auto *existing = findPendingTransaction(cleanName)) {
        qInfo()
            << "TRANSACTION MANAGER REUSING PENDING:"
            << cleanName
            << "existing operation:"
            << static_cast<int>(existing->operation())
            << "requested operation:"
            << static_cast<int>(operation);

        return existing;
    }

    auto *transaction =
        new PackageTransaction(
            cleanName,
            operation,
            repositoryId,
            this
        );

    transaction->appendTechnicalLog(QStringLiteral("Paket: %1 | Kaynak: %2 | İşlem: %3")
        .arg(cleanName, repositoryId.isEmpty() ? QStringLiteral("sistem") : repositoryId,
             QString::number(static_cast<int>(operation))));
    m_model->appendTransaction(transaction);

    m_queue.enqueue(transaction);

    emit queuedCountChanged();
    emit transactionEnqueued(transaction);

    startNext();

    return transaction;
}

PackageTransaction *
PackageTransactionManager::findPendingTransaction(
    const QString &packageName
) const
{
    const QString cleanName = packageName.trimmed();

    if (cleanName.isEmpty()) {
        return nullptr;
    }

    if (m_activeTransaction
        && !m_activeTransaction->isFinished()
        && m_activeTransaction->packageName() == cleanName) {

        return m_activeTransaction;
    }

    for (PackageTransaction *transaction : m_queue) {
        if (!transaction) {
            continue;
        }

        if (!transaction->isFinished()
            && transaction->packageName() == cleanName) {

            return transaction;
        }
    }

    return nullptr;
}


QObject *PackageTransactionManager::pendingTransaction(
    const QString &packageName
) const
{
    return findPendingTransaction(packageName);
}


void PackageTransactionManager::startNext()
{
    if (m_activeTransaction || m_externalOwner || m_outcomeUncertain ||
        m_queue.isEmpty()) {
        return;
    }

    m_activeTransaction = m_queue.dequeue();
    connect(m_activeTransaction, &PackageTransaction::stateChanged, this,
            &PackageTransactionManager::activeTransactionChanged);
    connect(m_activeTransaction, &PackageTransaction::cancelRequestedChanged, this,
            &PackageTransactionManager::activeTransactionChanged);

    m_downloadedById.clear();
    m_downloadTotalById.clear();
    resetDownloadSpeedTracking();
    m_cancelRequested = false;
    m_resetInProgress = false;
    m_downloadOnlyPhase = false;
    m_rpmPhaseStarted = false;

    emit queuedCountChanged();
    emit activeTransactionChanged();

    m_activeTransaction->setState(
        PackageTransaction::State::Resolving
    );

    emit transactionActivated(
        m_activeTransaction
    );

    resolveActive();
}

void PackageTransactionManager::resolveActive()
{
    if (!m_activeTransaction) {
        return;
    }

    qInfo()
        << "TRANSACTION MANAGER RESOLVING:"
        << m_activeTransaction->packageName();

    m_activeTransaction->appendTechnicalLog(QStringLiteral("DNF5 bağımlılık çözümlemesi başlatıldı."));
    m_backend->resolveTransaction(
        m_activeTransaction->packageName(),
        toBackendOperation(m_activeTransaction->operation()),
        m_activeTransaction->repositoryId()
    );
}



void PackageTransactionManager::cleanupControlledDownload()
{
    if (m_downloadProgressTimer)
        m_downloadProgressTimer->stop();
    if (m_controlledDownload) {
        m_controlledDownload->disconnect(this);
        if (m_controlledDownload->state() != QProcess::NotRunning)
            m_controlledDownload->kill();
        m_controlledDownload->deleteLater();
        m_controlledDownload = nullptr;
    }
    m_downloadDirectory.reset();
    m_approvedPlan.clear();
    m_controlledProcessKilled = false;
}

void PackageTransactionManager::refreshControlledDownloadProgress()
{
    if (!m_activeTransaction || !m_downloadDirectory)
        return;
    qulonglong bytes = 0;
    QDirIterator it(m_downloadDirectory->path(), {QStringLiteral("*.rpm"), QStringLiteral("*.part")},
                    QDir::Files);
    while (it.hasNext()) {
        it.next();
        bytes += static_cast<qulonglong>(it.fileInfo().size());
    }
    m_activeTransaction->setDownloadedBytes(bytes);
    const auto total = m_activeTransaction->totalBytes();
    if (total)
        m_activeTransaction->setProgress(static_cast<int>(qMin<qulonglong>(99, bytes * 100 / total)));
    updateDownloadSpeed(bytes);
}

void PackageTransactionManager::startControlledDownload()
{
    if (!m_activeTransaction || m_outcomeUncertain)
        return;
    const QString package = m_activeTransaction->packageName();
    // Isolated, non-root RPM downloading. The resulting files are never
    // passed directly to rpm; a new DNF5 solver run must validate them.
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!QDir().mkpath(cacheRoot)) {
        failActive(QStringLiteral("İndirme önbelleği klasörü oluşturulamadı."));
        return;
    }
    m_downloadDirectory = std::make_unique<QTemporaryDir>(
        QDir(cacheRoot).filePath(QStringLiteral("ro-store-download-XXXXXX")));
    if (!m_downloadDirectory->isValid()) {
        failActive(QStringLiteral("Geçici indirme klasörü oluşturulamadı."));
        return;
    }
    m_approvedPlan = m_activeTransaction->resolvedItems();
    m_controlledProcessKilled = false;
    m_controlledDownload = new QProcess(this);
    QProcess *const process = m_controlledDownload;
    QStringList args {
        QStringLiteral("download"),
        QStringLiteral("--destdir"), m_downloadDirectory->path()
    };
    // Download EXACTLY the solver-approved inbound NEVRAs, including weak
    // dependencies. A second independent --resolve could choose other RPMs
    // or omit optional dependencies and unexpectedly consume extra data.
    for (const QVariant &entry : std::as_const(m_approvedPlan)) {
        const QVariantMap item = entry.toMap();
        const QString action = item.value(QStringLiteral("action")).toString();
        if (action == QStringLiteral("Remove") || action == QStringLiteral("Replaced")
            || action == QStringLiteral("Reason Change"))
            continue;
        const QVariantMap pkg = item.value(QStringLiteral("object")).toMap();
        const QString repo = pkg.value(QStringLiteral("repo_id")).toString();
        if (repo == QStringLiteral("@System"))
            continue;
        const QString nevra = pkg.value(QStringLiteral("full_nevra")).toString();
        if (nevra.isEmpty()) {
            failActive(QStringLiteral("İşlem planında tam RPM kimliği eksik; güvenli indirme başlatılmadı."));
            return;
        }
        args << nevra;
    }
    if (args.size() == 3) {
        failActive(QStringLiteral("Plan indirilmesi gereken RPM içermiyor; güvenli işlem başlatılmadı."));
        return;
    }
    process->setProgram(QStringLiteral("/usr/bin/dnf5"));
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::MergedChannels);
    m_activeTransaction->appendTechnicalLog(QStringLiteral("Bağımsız DNF5 indirici: ") + args.join(' '));
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        if (!m_activeTransaction || process != m_controlledDownload)
            return;
        const QString log = QString::fromLocal8Bit(process->readAllStandardOutput());
        if (!log.isEmpty())
            m_activeTransaction->appendTechnicalLog(log.trimmed().left(1500));
        refreshControlledDownloadProgress();
    });
    connect(process, &QProcess::finished, this,
            [this, process](int code, QProcess::ExitStatus status) {
        if (m_controlledDownload != process)
            return;
        refreshControlledDownloadProgress();
        m_controlledDownload = nullptr;
        process->deleteLater();
        finishControlledDownload(code, status);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (process != m_controlledDownload || !m_activeTransaction)
            return;
        if (error == QProcess::FailedToStart)
            failActive(QStringLiteral("dnf5 indirme aracı başlatılamadı: ") + process->errorString());
    });
    if (!m_downloadProgressTimer) {
        m_downloadProgressTimer = new QTimer(this);
        m_downloadProgressTimer->setInterval(300);
        connect(m_downloadProgressTimer, &QTimer::timeout,
                this, &PackageTransactionManager::refreshControlledDownloadProgress);
    }
    if (!QFileInfo::exists(QStringLiteral("/usr/bin/dnf5"))) {
        failActive(QStringLiteral("/usr/bin/dnf5 bulunamadı."));
        return;
    }
    process->start();
    m_downloadProgressTimer->start();
}

void PackageTransactionManager::finishControlledDownload(int exitCode, QProcess::ExitStatus status)
{
    if (m_downloadProgressTimer)
        m_downloadProgressTimer->stop();
    if (!m_activeTransaction || m_outcomeUncertain)
        return;
    m_downloadOnlyPhase = false;
    if (m_cancelRequested || m_controlledProcessKilled) {
        m_activeTransaction->appendTechnicalLog(QStringLiteral(
            "İndirme işlemi durduruldu; RPM kurulumuna geçilmeyecek."));
        cancelActiveFinished();
        return;
    }
    if (status != QProcess::NormalExit || exitCode != 0) {
        failActive(QStringLiteral(
            "İndirme başarısız (kod %1). dnf5 download komutunun mevcut olduğundan "
            "ve paket depolarının erişilebilir olduğundan emin olun.").arg(exitCode));
        return;
    }
    QStringList paths;
    QDirIterator it(m_downloadDirectory->path(), {QStringLiteral("*.rpm")}, QDir::Files);
    while (it.hasNext()) {
        it.next();
        paths << it.filePath();
    }
    if (paths.isEmpty()) {
        failActive(QStringLiteral("İndirici hiç RPM dosyası üretmedi; kurulum başlatılmadı."));
        return;
    }
    m_activeTransaction->appendTechnicalLog(QStringLiteral(
        "İndirme tamamlandı (%1 RPM). Yerel paketler doğrulanıyor.").arg(paths.size()));
    persistPendingExecution();
    if (m_outcomeUncertain)
        return;
    m_backend->prepareLocalRpmTransaction(paths, m_approvedPlan);
}


void PackageTransactionManager::testDownloadOnlyActive()
{
    if (!m_activeTransaction) {
        qWarning()
            << "TRANSACTION MANAGER:"
            << "no active transaction";
        return;
    }

    if (m_activeTransaction->state() !=
        PackageTransaction::State::Ready) {

        qWarning()
            << "TRANSACTION MANAGER:"
            << "transaction is not ready";
        return;
    }

    qInfo()
        << "TRANSACTION MANAGER DOWNLOAD-ONLY TEST:"
        << m_activeTransaction->packageName();

    m_activeTransaction->setState(
        PackageTransaction::State::Downloading
    );

    m_backend->executeResolvedTransaction(true);
}



void PackageTransactionManager::executeActive()
{
    if (!m_activeTransaction) {
        qWarning()
            << "TRANSACTION MANAGER:"
            << "no active transaction";
        return;
    }

    if (m_activeTransaction->state() !=
        PackageTransaction::State::Ready) {

        qWarning()
            << "TRANSACTION MANAGER:"
            << "transaction is not ready";
        return;
    }

    qInfo()
        << "TRANSACTION MANAGER EXECUTE:"
        << m_activeTransaction->packageName();

    m_activeTransaction->setProgress(0);

    m_activeTransaction->setState(
        PackageTransaction::State::Downloading
    );

    m_activeTransaction->appendTechnicalLog(QStringLiteral(
        "İndirme ayrı süreçte başlatılıyor. İptal, bu süreci anında durdurur."));
    // Do not create the RPM-execution journal until downloaded packages
    // have passed the second verified, offline-only dependency resolution.
    m_downloadOnlyPhase = true;
    m_rpmPhaseStarted = false;
    emit activeTransactionChanged();
    if (m_activeTransaction->operation() == PackageTransaction::Operation::Remove) {
        m_downloadOnlyPhase = false;
        persistPendingExecution();
        if (m_outcomeUncertain)
            return;
        m_rpmPhaseStarted = true;
        m_activeTransaction->setState(PackageTransaction::State::Running);
        m_backend->executeResolvedTransaction(false);
    } else {
        startControlledDownload();
    }
}



void PackageTransactionManager::cancelActive()
{
    if (!m_activeTransaction || m_outcomeUncertain)
        return;
    cancelTransaction(m_activeTransaction);
}

void PackageTransactionManager::cancelTransaction(QObject *transactionObject)
{
    auto *transaction = qobject_cast<PackageTransaction *>(transactionObject);
    if (!transaction || transaction->isFinished() || transaction->cancelRequested())
        return;

    if (transaction != m_activeTransaction) {
        if (transaction->state() != PackageTransaction::State::Queued)
            return;
        if (!m_queue.removeOne(transaction))
            return;
        transaction->appendTechnicalLog(QStringLiteral("Bekleyen işlem kullanıcı tarafından iptal edildi."));
        transaction->setState(PackageTransaction::State::Cancelled);
        emit queuedCountChanged();
        return;
    }

    if (m_resetInProgress || m_cancelRequested)
        return;

    // RPM's Running phase must never be interrupted. During Resolving we
    // remember the request and cancel before any RPM execution starts.
    const auto state = transaction->state();
    if (state == PackageTransaction::State::Resolving
        || state == PackageTransaction::State::Ready) {
        m_cancelRequested = true;
        transaction->setCancelRequested(true);
        transaction->appendTechnicalLog(QStringLiteral("İptal istendi; çözümleme/oturumun güvenli bitişi bekleniyor."));
        if (state == PackageTransaction::State::Ready)
            cancelActiveFinished();
        return;
    }

    if (state != PackageTransaction::State::Downloading)
        return;

    if (!m_downloadOnlyPhase || m_rpmPhaseStarted)
        return;
    m_cancelRequested = true;
    transaction->setCancelRequested(true);
    transaction->appendTechnicalLog(QStringLiteral(
        "İptal edildi: indirme sürecine hemen sonlandırma sinyali gönderiliyor."));
    if (m_controlledDownload && m_controlledDownload->state() != QProcess::NotRunning) {
        m_controlledProcessKilled = true;
        m_controlledDownload->kill();
        return;
    }
    // A race with an already completed download is safe: local RPM
    // execution has not yet begun, so cancellation still wins.
    cancelActiveFinished();
}


void PackageTransactionManager::updateDownloadSpeed(
    qulonglong downloadedBytes
)
{
    if (!m_activeTransaction) {
        return;
    }

    if (!m_downloadSpeedTimer.isValid()) {
        m_lastSpeedSampleBytes = downloadedBytes;
        m_downloadSpeedTimer.start();
        return;
    }

    if (downloadedBytes < m_lastSpeedSampleBytes) {
        m_lastSpeedSampleBytes = downloadedBytes;
        m_downloadSpeedTimer.restart();
        return;
    }

    const qulonglong deltaBytes =
        downloadedBytes - m_lastSpeedSampleBytes;

    if (deltaBytes == 0) {
        return;
    }

    const qint64 elapsedNanoseconds =
        m_downloadSpeedTimer.nsecsElapsed();

    if (elapsedNanoseconds <= 0) {
        return;
    }

    const double instantaneousSpeed =
        static_cast<double>(deltaBytes)
        * 1000000000.0
        / static_cast<double>(elapsedNanoseconds);

    if (m_smoothedDownloadSpeed <= 0.0) {
        m_smoothedDownloadSpeed = instantaneousSpeed;
    } else {
        // Ani dalgalanmaları azaltmak için hafif exponential smoothing.
        constexpr double newSampleWeight = 0.30;

        m_smoothedDownloadSpeed =
            (m_smoothedDownloadSpeed
             * (1.0 - newSampleWeight))
            + (instantaneousSpeed
               * newSampleWeight);
    }

    m_activeTransaction->setDownloadSpeedBytesPerSecond(
        static_cast<qulonglong>(
            m_smoothedDownloadSpeed
        )
    );

    m_lastSpeedSampleBytes = downloadedBytes;
    m_downloadSpeedTimer.restart();
}


void PackageTransactionManager::resetDownloadSpeedTracking()
{
    m_downloadSpeedTimer.invalidate();
    m_lastSpeedSampleBytes = 0;
    m_smoothedDownloadSpeed = 0.0;
}


void PackageTransactionManager::cancelActiveFinished()
{
    if (!m_activeTransaction) {
        return;
    }

    cleanupControlledDownload();
    clearPendingExecution();
    m_activeTransaction->appendTechnicalLog(QStringLiteral("İşlem iptal edildi."));
    m_activeTransaction->setState(
        PackageTransaction::State::Cancelled
    );
    m_activeTransaction->setCancelRequested(false);

    resetAndReleaseActive();
}


void PackageTransactionManager::resetAndReleaseActive()
{
    if (!m_activeTransaction ||
        m_resetInProgress) {
        return;
    }

    m_resetInProgress = true;

    qInfo()
        << "TRANSACTION MANAGER REFRESHING DNF5 SESSION";

    m_backend->resetTransaction();
}


void PackageTransactionManager::releaseActiveAndStartNext()
{
    m_resetInProgress = false;
    m_cancelRequested = false;
    m_downloadOnlyPhase = false;
    m_rpmPhaseStarted = false;

    m_downloadedById.clear();
    m_downloadTotalById.clear();
    resetDownloadSpeedTracking();

    cleanupControlledDownload();
    m_activeTransaction = nullptr;

    emit activeTransactionChanged();

    startNext();
}


void PackageTransactionManager::finishActive()
{
    if (!m_activeTransaction) {
        return;
    }

    const QString packageName =
        m_activeTransaction->packageName();

    m_cancelRequested = false;
    m_activeTransaction->setCancelRequested(false);
    m_activeTransaction->setProgress(100);

    clearPendingExecution();
    // A definitive reply to do_transaction outweighs an earlier
    // unanswered cancellation request, provided the journal was removed.
    if (m_outcomeUncertain && !QFile::exists(journalPath())) {
        m_outcomeUncertain = false;
        emit outcomeUncertainChanged();
        setRecoveryDetails(QString());
    }
    m_activeTransaction->appendTechnicalLog(QStringLiteral("Paket işlemi başarıyla tamamlandı."));
    m_activeTransaction->setState(
        PackageTransaction::State::Finished
    );

    emit packageStateChanged(packageName);

    resetAndReleaseActive();
}

void PackageTransactionManager::failActive(
    const QString &errorMessage
)
{
    if (!m_activeTransaction) {
        return;
    }

    // A confirmed local/pre-dispatch failure cannot leave an RPM mutation running.
    // Remote do_transaction reply errors take OutcomeUnknown instead.
    clearPendingExecution();
    m_activeTransaction->appendTechnicalLog(QStringLiteral("İşlem başarısız: ") + errorMessage);
    m_activeTransaction->fail(errorMessage);

    resetAndReleaseActive();
}
