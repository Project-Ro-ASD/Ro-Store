#pragma once

#include <QObject>
#include <QQueue>
#include <QHash>
#include <QElapsedTimer>
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
    Q_INVOKABLE void cancelActive();

    Q_INVOKABLE QObject *pendingTransaction(
        const QString &packageName
    ) const;

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

    PackageTransaction *findPendingTransaction(
        const QString &packageName
    ) const;

    void startNext();
    void resolveActive();

    void updateDownloadSpeed(qulonglong downloadedBytes);
    void resetDownloadSpeedTracking();

    void resetAndReleaseActive();
    void releaseActiveAndStartNext();
    void cancelActiveFinished();

    PackageTransactionModel *m_model = nullptr;
    Dnf5Backend *m_backend = nullptr;

    QQueue<PackageTransaction *> m_queue;

    PackageTransaction *m_activeTransaction = nullptr;

    QHash<QString, qulonglong> m_downloadedById;
    QHash<QString, qulonglong> m_downloadTotalById;

    QElapsedTimer m_downloadSpeedTimer;
    qulonglong m_lastSpeedSampleBytes = 0;
    double m_smoothedDownloadSpeed = 0.0;

    bool m_cancelRequested = false;
    bool m_resetInProgress = false;
};
