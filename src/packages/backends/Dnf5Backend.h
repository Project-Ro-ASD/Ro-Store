#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <functional>
#include <QList>

class Dnf5Backend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool sessionOpen READ sessionOpen NOTIFY sessionOpenChanged)
    Q_PROPERTY(QString sessionPath READ sessionPath NOTIFY sessionPathChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(PackageState packageState READ packageState NOTIFY packageStateChanged)

public:
    enum class PackageState {
        Unknown,
        NotInstalled,
        Installed,
        UpdateAvailable
    };
    Q_ENUM(PackageState)

    enum class TransactionOperation {
        Install,
        Remove,
        Upgrade
    };
    Q_ENUM(TransactionOperation)

    explicit Dnf5Backend(QObject *parent = nullptr);
    ~Dnf5Backend() override;

    bool sessionOpen() const;
    QString sessionPath() const;
    QString lastError() const;
    bool busy() const;
    PackageState packageState() const;

    Q_INVOKABLE bool closeSession();

    Q_INVOKABLE void queryPackageState(const QString &packageName);

    Q_INVOKABLE void resolveTransaction(
        const QString &packageName,
        TransactionOperation operation
    );

    void executeResolvedTransaction(
        bool downloadOnly = false
    );

signals:
    void sessionOpenChanged();
    void sessionPathChanged();
    void lastErrorChanged();
    void busyChanged();
    void packageStateChanged();

    void packageQueryFinished(QVariantMap package);
    void packageQueryFailed(QString error);

    void installedPackageQueryFinished(QVariantMap package);
    void installedPackageQueryFailed(QString error);

    void upgradePackageQueryFinished(QVariantMap package);
    void upgradePackageQueryFailed(QString error);

    void packageStateQueryFinished(
        PackageState state,
        QVariantMap availablePackage,
        QVariantMap installedPackage,
        QVariantMap upgradePackage
    );

    void transactionResolved(
        TransactionOperation operation,
        QString packageName,
        uint result,
        QVariantList resolvedItems,
        qulonglong totalDownloadBytes
    );

    void transactionResolveFailed(
        TransactionOperation operation,
        QString packageName,
        QString error
    );

    void transactionExecutionStarted(bool downloadOnly);

    void transactionExecutionFinished(bool downloadOnly);

    void transactionExecutionFailed(QString error);

private:
    void queryPackage(const QString &packageName);
    void queryInstalledPackage(const QString &packageName);
    void queryUpgradePackage(const QString &packageName);

    void setLastError(const QString &error);
    void setBusy(bool value);
    void setPackageState(PackageState state);
    void ensureSessionAsync(std::function<void(bool)> callback);
    void finishSessionOpen(bool success);
    void clearSession();

    QString m_sessionPath;
    QString m_lastError;
    bool m_busy = false;

    PackageState m_packageState = PackageState::Unknown;
    QVariantMap m_stateAvailablePackage;
    QVariantMap m_stateInstalledPackage;

    bool m_sessionOpening = false;
    QList<std::function<void(bool)>> m_sessionWaiters;
};
