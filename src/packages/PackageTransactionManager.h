#pragma once

#include <QObject>
#include <QQueue>
#include <QString>

#include "PackageTransaction.h"
#include "PackageTransactionModel.h"

class Dnf5Backend;

class PackageTransactionManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(
        PackageTransactionModel* model
        READ model
        CONSTANT
    )

    Q_PROPERTY(
        PackageTransaction* activeTransaction
        READ activeTransaction
        NOTIFY activeTransactionChanged
    )

    Q_PROPERTY(
        int queuedCount
        READ queuedCount
        NOTIFY queuedCountChanged
    )

    Q_PROPERTY(
        bool busy
        READ busy
        NOTIFY activeTransactionChanged
    )

public:
    explicit PackageTransactionManager(
        QObject *parent = nullptr
    );

    PackageTransactionModel *model() const;

    PackageTransaction *activeTransaction() const;

    int queuedCount() const;
    bool busy() const;

    Q_INVOKABLE QObject *enqueueInstall(
        const QString &packageName
    );

    Q_INVOKABLE QObject *enqueueRemove(
        const QString &packageName
    );

    Q_INVOKABLE QObject *enqueueUpgrade(
        const QString &packageName
    );

    Q_INVOKABLE void testDownloadOnlyActive();
    Q_INVOKABLE void executeActive();

    void finishActive();
    void failActive(const QString &errorMessage);

signals:
    void activeTransactionChanged();
    void queuedCountChanged();

    void transactionEnqueued(
        PackageTransaction *transaction
    );

    void transactionActivated(
        PackageTransaction *transaction
    );

    void transactionResolved(
        PackageTransaction *transaction
    );

private:
    PackageTransaction *enqueue(
        const QString &packageName,
        PackageTransaction::Operation operation
    );

    void startNext();
    void resolveActive();

    PackageTransactionModel *m_model = nullptr;
    Dnf5Backend *m_backend = nullptr;

    QQueue<PackageTransaction *> m_queue;

    PackageTransaction *m_activeTransaction = nullptr;
};
