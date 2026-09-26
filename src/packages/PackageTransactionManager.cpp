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

            failActive(error);
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
        PackageTransaction::State::Running
    );

    m_backend->executeResolvedTransaction(false);
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

    m_activeTransaction = nullptr;

    emit activeTransactionChanged();

    startNext();
}

void PackageTransactionManager::failActive(
    const QString &errorMessage
)
{
    if (!m_activeTransaction) {
        return;
    }

    m_activeTransaction->fail(errorMessage);

    m_activeTransaction = nullptr;

    emit activeTransactionChanged();

    startNext();
}
