#include "PackageTransactionManager.h"

PackageTransactionManager::PackageTransactionManager(
    QObject *parent
)
    : QObject(parent),
      m_model(new PackageTransactionModel(this))
{
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
