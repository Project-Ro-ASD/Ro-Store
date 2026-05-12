#pragma once

#include <QObject>
#include <QString>

class PackageStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool installed READ installed NOTIFY installedChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString installedVersion READ installedVersion NOTIFY installedVersionChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)

public:
    explicit PackageStatus(QObject *parent = nullptr);

    bool checking() const;
    bool installed() const;
    bool updateAvailable() const;

    QString statusText() const;
    QString installedVersion() const;
    QString latestVersion() const;

    Q_INVOKABLE void checkInstalled(const QString &packageName, const QString &latestVersion);

signals:
    void checkingChanged();
    void installedChanged();
    void updateAvailableChanged();
    void statusTextChanged();
    void installedVersionChanged();
    void latestVersionChanged();

private:
    void setChecking(bool value);
    void setInstalled(bool value);
    void setUpdateAvailable(bool value);
    void setStatusText(const QString &text);
    void setInstalledVersion(const QString &version);
    void setLatestVersion(const QString &version);
    void evaluateUpdateState();

private:
    bool m_checking = false;
    bool m_installed = false;
    bool m_updateAvailable = false;

    QString m_statusText = "Durum kontrol edilmedi";
    QString m_installedVersion;
    QString m_latestVersion;
};
