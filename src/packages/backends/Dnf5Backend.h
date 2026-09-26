#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
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

    explicit Dnf5Backend(QObject *parent = nullptr);
    ~Dnf5Backend() override;

    bool sessionOpen() const;
    QString sessionPath() const;
    QString lastError() const;
    bool busy() const;
    PackageState packageState() const;

    Q_INVOKABLE bool openSession();
    Q_INVOKABLE bool closeSession();

    Q_INVOKABLE void queryPackage(const QString &packageName);
    Q_INVOKABLE void queryInstalledPackage(const QString &packageName);
    Q_INVOKABLE void queryUpgradePackage(const QString &packageName);
    Q_INVOKABLE void queryPackageState(const QString &packageName);

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

private:
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
