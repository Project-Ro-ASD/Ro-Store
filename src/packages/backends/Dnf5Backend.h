#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class Dnf5Backend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool sessionOpen READ sessionOpen NOTIFY sessionOpenChanged)
    Q_PROPERTY(QString sessionPath READ sessionPath NOTIFY sessionPathChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit Dnf5Backend(QObject *parent = nullptr);
    ~Dnf5Backend() override;

    bool sessionOpen() const;
    QString sessionPath() const;
    QString lastError() const;
    bool busy() const;

    Q_INVOKABLE bool openSession();
    Q_INVOKABLE bool closeSession();

    Q_INVOKABLE void queryPackage(const QString &packageName);
    Q_INVOKABLE void queryInstalledPackage(const QString &packageName);
    Q_INVOKABLE void queryUpgradePackage(const QString &packageName);

signals:
    void sessionOpenChanged();
    void sessionPathChanged();
    void lastErrorChanged();
    void busyChanged();

    void packageQueryFinished(QVariantMap package);
    void packageQueryFailed(QString error);

    void installedPackageQueryFinished(QVariantMap package);
    void installedPackageQueryFailed(QString error);

    void upgradePackageQueryFinished(QVariantMap package);
    void upgradePackageQueryFailed(QString error);

private:
    void setLastError(const QString &error);
    void setBusy(bool value);
    void clearSession();

    QString m_sessionPath;
    QString m_lastError;
    bool m_busy = false;
};
