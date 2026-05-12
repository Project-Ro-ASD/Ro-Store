#include "PackageStatus.h"

#include <QProcess>

PackageStatus::PackageStatus(QObject *parent)
    : QObject(parent)
{
}

bool PackageStatus::checking() const
{
    return m_checking;
}

bool PackageStatus::installed() const
{
    return m_installed;
}

bool PackageStatus::updateAvailable() const
{
    return m_updateAvailable;
}

QString PackageStatus::statusText() const
{
    return m_statusText;
}

QString PackageStatus::installedVersion() const
{
    return m_installedVersion;
}

QString PackageStatus::latestVersion() const
{
    return m_latestVersion;
}

void PackageStatus::checkInstalled(const QString &packageName, const QString &latestVersion)
{
    const QString cleanPackageName = packageName.trimmed();
    const QString cleanLatestVersion = latestVersion.trimmed();

    setLatestVersion(cleanLatestVersion);

    if (cleanPackageName.isEmpty()) {
        setInstalled(false);
        setInstalledVersion("");
        setUpdateAvailable(false);
        setStatusText("Paket adı bulunamadı");
        return;
    }

    setChecking(true);
    setStatusText("Paket durumu kontrol ediliyor...");

    QProcess *process = new QProcess(this);
    process->setProgram("rpm");

    // Paket kuruluysa sadece VERSION bilgisini döndürür.
    process->setArguments({ "-q", "--qf", "%{VERSION}", cleanPackageName });

    connect(process, &QProcess::errorOccurred, this, [this, process]() {
        setChecking(false);
        setInstalled(false);
        setInstalledVersion("");
        setUpdateAvailable(false);
        setStatusText("Paket durumu kontrol edilemedi");
        process->deleteLater();
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        const QByteArray output = process->readAllStandardOutput();
        const QString version = QString::fromLocal8Bit(output).trimmed();

        const bool isInstalled =
            exitStatus == QProcess::NormalExit && exitCode == 0 && !version.isEmpty();

        setInstalled(isInstalled);

        if (isInstalled) {
            setInstalledVersion(version);
            evaluateUpdateState();
        } else {
            setInstalledVersion("");
            setUpdateAvailable(false);
            setStatusText("Bu uygulama sistemde kurulu değil");
        }

        setChecking(false);
        process->deleteLater();
    });

    process->start();
}

void PackageStatus::evaluateUpdateState()
{
    if (!m_installed) {
        setUpdateAvailable(false);
        setStatusText("Bu uygulama sistemde kurulu değil");
        return;
    }

    if (m_latestVersion.isEmpty()) {
        setUpdateAvailable(false);
        setStatusText("Bu uygulama sistemde kurulu");
        return;
    }

    if (m_installedVersion != m_latestVersion) {
        setUpdateAvailable(true);
        setStatusText("Güncelleme mevcut");
    } else {
        setUpdateAvailable(false);
        setStatusText("Bu uygulama güncel");
    }
}

void PackageStatus::setChecking(bool value)
{
    if (m_checking == value) {
        return;
    }

    m_checking = value;
    emit checkingChanged();
}

void PackageStatus::setInstalled(bool value)
{
    if (m_installed == value) {
        return;
    }

    m_installed = value;
    emit installedChanged();
}

void PackageStatus::setUpdateAvailable(bool value)
{
    if (m_updateAvailable == value) {
        return;
    }

    m_updateAvailable = value;
    emit updateAvailableChanged();
}

void PackageStatus::setStatusText(const QString &text)
{
    if (m_statusText == text) {
        return;
    }

    m_statusText = text;
    emit statusTextChanged();
}

void PackageStatus::setInstalledVersion(const QString &version)
{
    if (m_installedVersion == version) {
        return;
    }

    m_installedVersion = version;
    emit installedVersionChanged();
}

void PackageStatus::setLatestVersion(const QString &version)
{
    if (m_latestVersion == version) {
        return;
    }

    m_latestVersion = version;
    emit latestVersionChanged();
}
