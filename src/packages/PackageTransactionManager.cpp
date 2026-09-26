#include "PackageTransactionManager.h"
#include "backends/Dnf5Backend.h"

#include <QDebug>

namespace {

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

            failActive(error);
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::transactionExecutionStarted,
        this,
        [this](bool downloadOnly) {
            if (!m_activeTransaction) {
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

            finishActive();
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::transactionExecutionFailed,
        this,
        [this](const QString &error) {
            if (!m_activeTransaction) {
                return;
            }

            qWarning()
                << "TRANSACTION MANAGER EXECUTION FAILED:"
                << m_activeTransaction->packageName()
                << error;

            if (m_cancelRequested) {
                cancelActiveFinished();
                return;
            }

            if (m_cancelRequested) {
                qInfo()
                    << "TRANSACTION MANAGER CANCELLED:"
                    << m_activeTransaction->packageName();

                cancelActiveFinished();
                return;
            }

            failActive(error);
        }
    );

    connect(
        m_backend,
        &Dnf5Backend::downloadStarted,
        this,
        [this](
            const QString &downloadId,
            const QString &description,
            qlonglong totalBytes
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
            if (!m_activeTransaction) {
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

                qWarning()
                    << "TRANSACTION MANAGER CANCEL REJECTED:"
                    << error;
                return;
            }

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

QObject *PackageTransactionManager::enqueueInstall(
    const QString &packageName
)
{
    return enqueue(
        packageName,
        PackageTransaction::Operation::Install
    );
}

QObject *PackageTransactionManager::enqueueRemove(
    const QString &packageName
)
{
    return enqueue(
        packageName,
        PackageTransaction::Operation::Remove
    );
}

QObject *PackageTransactionManager::enqueueUpgrade(
    const QString &packageName
)
{
    return enqueue(
        packageName,
        PackageTransaction::Operation::Upgrade
    );
}

PackageTransaction *
PackageTransactionManager::enqueue(
    const QString &packageName,
    PackageTransaction::Operation operation
)
{
    const QString cleanName = packageName.trimmed();

    if (cleanName.isEmpty()) {
        return nullptr;
    }

    auto *transaction =
        new PackageTransaction(
            cleanName,
            operation,
            this
        );

    m_model->appendTransaction(transaction);

    m_queue.enqueue(transaction);

    emit queuedCountChanged();
    emit transactionEnqueued(transaction);

    startNext();

    return transaction;
}

void PackageTransactionManager::startNext()
{
    if (m_activeTransaction ||
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

    m_backend->resolveTransaction(
        m_activeTransaction->packageName(),
        toBackendOperation(
            m_activeTransaction->operation()
        )
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

    m_backend->executeResolvedTransaction(false);
}



void PackageTransactionManager::cancelActive()
{
    if (!m_activeTransaction) {
        qWarning()
            << "TRANSACTION MANAGER:"
            << "no active transaction";
        return;
    }

    if (m_activeTransaction->state() !=
        PackageTransaction::State::Downloading) {

        qWarning()
            << "TRANSACTION MANAGER CANCEL:"
            << "cancellation is only allowed while downloading";
        return;
    }

    if (m_cancelRequested) {
        return;
    }

    m_cancelRequested = true;

    qInfo()
        << "TRANSACTION MANAGER CANCEL REQUEST:"
        << m_activeTransaction->packageName();

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

    m_activeTransaction->setProgress(100);

    m_activeTransaction->setState(
        PackageTransaction::State::Finished
    );

    resetAndReleaseActive();
}

void PackageTransactionManager::failActive(
    const QString &errorMessage
)
{
    if (!m_activeTransaction) {
        return;
    }

    m_activeTransaction->fail(errorMessage);

    resetAndReleaseActive();
}
