#include "PackageTransactionManager.h"
#include "backends/Dnf5Backend.h"

#include <QDebug>
#include <QLocale>

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
                m_activeTransaction->setDownloadedBytes(
                    m_activeTransaction->totalBytes()
                );

                // Henüz gerçek RPM işlemi yapılmadı.
                // Transaction tekrar çalıştırılmaya hazır.
                m_activeTransaction->setProgress(0);

                m_activeTransaction->setState(
                    PackageTransaction::State::Ready
                );

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

            if (m_cancelRequested) {
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
        m_outcomeUncertain = true;
        emit outcomeUncertainChanged();
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
            queued->markOutcomeUnknown(QStringLiteral(
                "Güvenlik nedeniyle gönderilmedi. Önceki işlemin sonucu doğrulanmalı."));
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
                m_activeTransaction->appendTechnicalLog(QStringLiteral("DNF5 iptal isteğini reddetti: ") + error);

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
    return m_activeTransaction != nullptr;
}

bool PackageTransactionManager::outcomeUncertain() const
{
    return m_outcomeUncertain;
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

    if (cleanName.isEmpty() || m_outcomeUncertain) {
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
    if (m_activeTransaction || m_outcomeUncertain ||
        m_queue.isEmpty()) {
        return;
    }

    m_activeTransaction = m_queue.dequeue();

    m_downloadedById.clear();
    m_downloadTotalById.clear();
    resetDownloadSpeedTracking();
    m_cancelRequested = false;
    m_resetInProgress = false;

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

    m_activeTransaction->appendTechnicalLog(QStringLiteral("DNF5 işlemi yürütülüyor."));
    m_backend->executeResolvedTransaction(false);
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

    m_cancelRequested = true;
    transaction->setCancelRequested(true);
    transaction->appendTechnicalLog(QStringLiteral("DNF5 indirme iptali istendi."));
    m_backend->cancelTransaction();
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

    m_activeTransaction->appendTechnicalLog(QStringLiteral("İşlem iptal edildi."));
    m_activeTransaction->setState(
        PackageTransaction::State::Cancelled
    );

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

    m_downloadedById.clear();
    m_downloadTotalById.clear();
    resetDownloadSpeedTracking();

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

    m_activeTransaction->appendTechnicalLog(QStringLiteral("İşlem başarısız: ") + errorMessage);
    m_activeTransaction->fail(errorMessage);

    resetAndReleaseActive();
}
