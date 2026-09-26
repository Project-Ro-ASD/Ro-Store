#include "Dnf5Backend.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDebug>
#include <QMap>
#include <QStringList>
#include <QVariantMap>

namespace {

constexpr auto DNF5_SERVICE = "org.rpm.dnf.v0";
constexpr auto DNF5_ROOT_PATH = "/org/rpm/dnf/v0";
constexpr auto DNF5_SESSION_MANAGER = "org.rpm.dnf.v0.SessionManager";
constexpr auto DNF5_RPM_INTERFACE = "org.rpm.dnf.v0.rpm.Rpm";

QList<QVariantMap> readPackageArray(const QVariant &value)
{
    QList<QVariantMap> packages;

    if (!value.canConvert<QDBusArgument>()) {
        return packages;
    }

    const QDBusArgument argument = value.value<QDBusArgument>();

    argument.beginArray();

    while (!argument.atEnd()) {
        QVariantMap package;

        argument.beginMap();

        while (!argument.atEnd()) {
            QString key;
            QVariant itemValue;

            argument.beginMapEntry();
            argument >> key >> itemValue;
            argument.endMapEntry();

            package.insert(key, itemValue);
        }

        argument.endMap();

        packages.append(package);
    }

    argument.endArray();

    return packages;
}

}

Dnf5Backend::Dnf5Backend(QObject *parent)
    : QObject(parent)
{
}

Dnf5Backend::~Dnf5Backend()
{
    if (sessionOpen()) {
        closeSession();
    }
}

bool Dnf5Backend::sessionOpen() const
{
    return !m_sessionPath.isEmpty();
}

QString Dnf5Backend::sessionPath() const
{
    return m_sessionPath;
}

QString Dnf5Backend::lastError() const
{
    return m_lastError;
}

bool Dnf5Backend::busy() const
{
    return m_busy;
}

Dnf5Backend::PackageState Dnf5Backend::packageState() const
{
    return m_packageState;
}

bool Dnf5Backend::openSession()
{
    if (sessionOpen()) {
        return true;
    }

    QDBusConnection bus = QDBusConnection::systemBus();

    if (!bus.isConnected()) {
        setLastError(QStringLiteral("Sistem D-Bus bağlantısı kurulamadı."));
        return false;
    }

    QDBusInterface manager(
        QString::fromLatin1(DNF5_SERVICE),
        QString::fromLatin1(DNF5_ROOT_PATH),
        QString::fromLatin1(DNF5_SESSION_MANAGER),
        bus
    );

    if (!manager.isValid()) {
        setLastError(
            QStringLiteral("DNF5 SessionManager kullanılamıyor: %1")
                .arg(manager.lastError().message())
        );
        return false;
    }

    QVariantMap options;

    QMap<QString, QString> config;
    config.insert(
        QStringLiteral("skip_if_unavailable"),
        QStringLiteral("1")
    );

    options.insert(
        QStringLiteral("config"),
        QVariant::fromValue(config)
    );

    QDBusReply<QDBusObjectPath> reply =
        manager.call(QStringLiteral("open_session"), options);

    if (!reply.isValid()) {
        setLastError(
            QStringLiteral("DNF5 oturumu açılamadı: %1")
                .arg(reply.error().message())
        );
        return false;
    }

    const QString path = reply.value().path();

    if (path.isEmpty()) {
        setLastError(QStringLiteral("DNF5 boş session path döndürdü."));
        return false;
    }

    m_sessionPath = path;

    emit sessionPathChanged();
    emit sessionOpenChanged();

    setLastError(QString());

    return true;
}

bool Dnf5Backend::closeSession()
{
    if (!sessionOpen()) {
        return true;
    }

    QDBusConnection bus = QDBusConnection::systemBus();

    QDBusInterface manager(
        QString::fromLatin1(DNF5_SERVICE),
        QString::fromLatin1(DNF5_ROOT_PATH),
        QString::fromLatin1(DNF5_SESSION_MANAGER),
        bus
    );

    QDBusReply<bool> reply =
        manager.call(
            QStringLiteral("close_session"),
            QVariant::fromValue(QDBusObjectPath(m_sessionPath))
        );

    if (!reply.isValid()) {
        setLastError(
            QStringLiteral("DNF5 oturumu kapatılamadı: %1")
                .arg(reply.error().message())
        );
        return false;
    }

    if (!reply.value()) {
        setLastError(QStringLiteral("DNF5 oturumu kapatmayı reddetti."));
        return false;
    }

    clearSession();
    setLastError(QString());

    return true;
}

void Dnf5Backend::queryPackage(const QString &packageName)
{
    const QString cleanName = packageName.trimmed();

    if (cleanName.isEmpty()) {
        const QString error = QStringLiteral("Paket adı boş.");
        setLastError(error);
        emit packageQueryFailed(error);
        return;
    }

    if (m_busy) {
        const QString error =
            QStringLiteral("DNF5 şu anda başka bir işlem yürütüyor.");

        setLastError(error);
        emit packageQueryFailed(error);
        return;
    }

    if (!sessionOpen() && !openSession()) {
        emit packageQueryFailed(m_lastError);
        return;
    }

    QDBusInterface rpm(
        QString::fromLatin1(DNF5_SERVICE),
        m_sessionPath,
        QString::fromLatin1(DNF5_RPM_INTERFACE),
        QDBusConnection::systemBus()
    );

    if (!rpm.isValid()) {
        const QString error =
            QStringLiteral("DNF5 RPM arayüzü kullanılamıyor: %1")
                .arg(rpm.lastError().message());

        setLastError(error);
        emit packageQueryFailed(error);
        return;
    }

    QVariantMap options;

    options.insert(
        QStringLiteral("patterns"),
        QStringList { cleanName }
    );

    // Şimdilik Ro-Store kataloğundaki Ro-ASD uygulamalarını
    // yalnız Ro-ASD beta deposundan sorguluyoruz.
    options.insert(
        QStringLiteral("repo"),
        QStringList { QStringLiteral("ro-asd-beta") }
    );

    options.insert(
        QStringLiteral("scope"),
        QStringLiteral("all")
    );

    options.insert(
        QStringLiteral("latest-limit"),
        1
    );

    options.insert(
        QStringLiteral("with_src"),
        false
    );

    options.insert(
        QStringLiteral("with_provides"),
        false
    );

    options.insert(
        QStringLiteral("with_filenames"),
        false
    );

    options.insert(
        QStringLiteral("with_binaries"),
        false
    );

    options.insert(
        QStringLiteral("interactive"),
        false
    );

    options.insert(
        QStringLiteral("package_attrs"),
        QStringList {
            QStringLiteral("name"),
            QStringLiteral("epoch"),
            QStringLiteral("version"),
            QStringLiteral("release"),
            QStringLiteral("arch"),
            QStringLiteral("repo_id"),
            QStringLiteral("from_repo_id"),
            QStringLiteral("is_installed"),
            QStringLiteral("install_size"),
            QStringLiteral("download_size"),
            QStringLiteral("summary"),
            QStringLiteral("license")
        }
    );

    setBusy(true);
    setLastError(QString());

    qInfo() << "DNF5 QUERY START:" << cleanName;

    QDBusPendingCall pending =
        rpm.asyncCall(
            QStringLiteral("list"),
            options
        );

    auto *watcher =
        new QDBusPendingCallWatcher(pending, this);

    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, cleanName]() {

            setBusy(false);

            QDBusPendingReply<> reply = *watcher;

            if (reply.isError()) {
                const QString error =
                    QStringLiteral("DNF5 paket sorgusu başarısız: %1")
                        .arg(reply.error().message());

                setLastError(error);

                qWarning()
                    << "DNF5 QUERY ERROR:"
                    << error;

                emit packageQueryFailed(error);

                watcher->deleteLater();
                return;
            }

            const QDBusMessage message = watcher->reply();

            if (message.arguments().isEmpty()) {
                const QString error =
                    QStringLiteral(
                        "DNF5 paket sorgusu boş cevap döndürdü."
                    );

                setLastError(error);
                emit packageQueryFailed(error);

                watcher->deleteLater();
                return;
            }

            const QList<QVariantMap> packages =
                readPackageArray(
                    message.arguments().first()
                );

            qInfo()
                << "DNF5 QUERY RESULT COUNT:"
                << packages.size();

            if (packages.isEmpty()) {
                const QString error =
                    QStringLiteral("Paket bulunamadı: %1")
                        .arg(cleanName);

                setLastError(error);

                qWarning()
                    << "DNF5 PACKAGE NOT FOUND:"
                    << cleanName;

                emit packageQueryFailed(error);

                watcher->deleteLater();
                return;
            }

            const QVariantMap package =
                packages.first();

            qInfo() << "DNF5 PACKAGE";
            qInfo() << " name:" << package.value("name");
            qInfo() << " version:" << package.value("version");
            qInfo() << " release:" << package.value("release");
            qInfo() << " arch:" << package.value("arch");
            qInfo() << " repo:" << package.value("repo_id");
            qInfo() << " installed:" << package.value("is_installed");
            qInfo() << " download size:" << package.value("download_size");
            qInfo() << " install size:" << package.value("install_size");
            qInfo() << " summary:" << package.value("summary");

            setLastError(QString());

            emit packageQueryFinished(package);

            watcher->deleteLater();
        }
    );
}


void Dnf5Backend::queryInstalledPackage(const QString &packageName)
{
    const QString cleanName = packageName.trimmed();

    if (cleanName.isEmpty()) {
        const QString error = QStringLiteral("Paket adı boş.");
        setLastError(error);
        emit installedPackageQueryFailed(error);
        return;
    }

    if (m_busy) {
        const QString error =
            QStringLiteral("DNF5 şu anda başka bir işlem yürütüyor.");

        setLastError(error);
        emit installedPackageQueryFailed(error);
        return;
    }

    if (!sessionOpen() && !openSession()) {
        emit installedPackageQueryFailed(m_lastError);
        return;
    }

    QDBusInterface rpm(
        QString::fromLatin1(DNF5_SERVICE),
        m_sessionPath,
        QString::fromLatin1(DNF5_RPM_INTERFACE),
        QDBusConnection::systemBus()
    );

    if (!rpm.isValid()) {
        const QString error =
            QStringLiteral("DNF5 RPM arayüzü kullanılamıyor: %1")
                .arg(rpm.lastError().message());

        setLastError(error);
        emit installedPackageQueryFailed(error);
        return;
    }

    QVariantMap options;

    options.insert(
        QStringLiteral("patterns"),
        QStringList { cleanName }
    );

    options.insert(
        QStringLiteral("scope"),
        QStringLiteral("installed")
    );

    options.insert(
        QStringLiteral("latest-limit"),
        1
    );

    options.insert(
        QStringLiteral("with_src"),
        false
    );

    options.insert(
        QStringLiteral("with_provides"),
        false
    );

    options.insert(
        QStringLiteral("with_filenames"),
        false
    );

    options.insert(
        QStringLiteral("with_binaries"),
        false
    );

    options.insert(
        QStringLiteral("interactive"),
        false
    );

    options.insert(
        QStringLiteral("package_attrs"),
        QStringList {
            QStringLiteral("name"),
            QStringLiteral("epoch"),
            QStringLiteral("version"),
            QStringLiteral("release"),
            QStringLiteral("arch"),
            QStringLiteral("repo_id"),
            QStringLiteral("from_repo_id"),
            QStringLiteral("is_installed"),
            QStringLiteral("install_size"),
            QStringLiteral("summary")
        }
    );

    setBusy(true);
    setLastError(QString());

    qInfo() << "DNF5 INSTALLED QUERY START:" << cleanName;

    QDBusPendingCall pending =
        rpm.asyncCall(
            QStringLiteral("list"),
            options
        );

    auto *watcher =
        new QDBusPendingCallWatcher(pending, this);

    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, cleanName]() {

            setBusy(false);

            QDBusPendingReply<> reply = *watcher;

            if (reply.isError()) {
                const QString error =
                    QStringLiteral("DNF5 kurulu paket sorgusu başarısız: %1")
                        .arg(reply.error().message());

                setLastError(error);

                qWarning()
                    << "DNF5 INSTALLED QUERY ERROR:"
                    << error;

                emit installedPackageQueryFailed(error);

                watcher->deleteLater();
                return;
            }

            const QDBusMessage message = watcher->reply();

            if (message.arguments().isEmpty()) {
                const QString error =
                    QStringLiteral(
                        "DNF5 kurulu paket sorgusu boş cevap döndürdü."
                    );

                setLastError(error);
                emit installedPackageQueryFailed(error);

                watcher->deleteLater();
                return;
            }

            const QList<QVariantMap> packages =
                readPackageArray(
                    message.arguments().first()
                );

            QVariantMap installedPackage;

            for (const QVariantMap &package : packages) {
                if (package.value(QStringLiteral("name")).toString() ==
                        cleanName &&
                    package.value(QStringLiteral("is_installed")).toBool()) {

                    installedPackage = package;
                    break;
                }
            }

            if (installedPackage.isEmpty()) {
                qInfo()
                    << "DNF5 INSTALLED:"
                    << cleanName
                    << "NOT INSTALLED";

                setLastError(QString());

                emit installedPackageQueryFinished(QVariantMap{});

                watcher->deleteLater();
                return;
            }

            qInfo() << "DNF5 INSTALLED PACKAGE";
            qInfo()
                << " name:"
                << installedPackage.value("name");
            qInfo()
                << " version:"
                << installedPackage.value("version");
            qInfo()
                << " release:"
                << installedPackage.value("release");
            qInfo()
                << " arch:"
                << installedPackage.value("arch");
            qInfo()
                << " repo:"
                << installedPackage.value("repo_id");
            qInfo()
                << " from repo:"
                << installedPackage.value("from_repo_id");
            qInfo()
                << " installed:"
                << installedPackage.value("is_installed");

            setLastError(QString());

            emit installedPackageQueryFinished(installedPackage);

            watcher->deleteLater();
        }
    );
}


void Dnf5Backend::queryUpgradePackage(const QString &packageName)
{
    const QString cleanName = packageName.trimmed();

    if (cleanName.isEmpty()) {
        const QString error = QStringLiteral("Paket adı boş.");
        setLastError(error);
        emit upgradePackageQueryFailed(error);
        return;
    }

    if (m_busy) {
        const QString error =
            QStringLiteral("DNF5 şu anda başka bir işlem yürütüyor.");

        setLastError(error);
        emit upgradePackageQueryFailed(error);
        return;
    }

    if (!sessionOpen() && !openSession()) {
        emit upgradePackageQueryFailed(m_lastError);
        return;
    }

    QDBusInterface rpm(
        QString::fromLatin1(DNF5_SERVICE),
        m_sessionPath,
        QString::fromLatin1(DNF5_RPM_INTERFACE),
        QDBusConnection::systemBus()
    );

    if (!rpm.isValid()) {
        const QString error =
            QStringLiteral("DNF5 RPM arayüzü kullanılamıyor: %1")
                .arg(rpm.lastError().message());

        setLastError(error);
        emit upgradePackageQueryFailed(error);
        return;
    }

    QVariantMap options;

    options.insert(
        QStringLiteral("patterns"),
        QStringList { cleanName }
    );

    options.insert(
        QStringLiteral("scope"),
        QStringLiteral("upgrades")
    );

    options.insert(
        QStringLiteral("repo"),
        QStringList { QStringLiteral("ro-asd-beta") }
    );

    options.insert(
        QStringLiteral("latest-limit"),
        1
    );

    options.insert(
        QStringLiteral("with_src"),
        false
    );

    options.insert(
        QStringLiteral("with_provides"),
        false
    );

    options.insert(
        QStringLiteral("with_filenames"),
        false
    );

    options.insert(
        QStringLiteral("with_binaries"),
        false
    );

    options.insert(
        QStringLiteral("interactive"),
        false
    );

    options.insert(
        QStringLiteral("package_attrs"),
        QStringList {
            QStringLiteral("name"),
            QStringLiteral("epoch"),
            QStringLiteral("version"),
            QStringLiteral("release"),
            QStringLiteral("arch"),
            QStringLiteral("repo_id"),
            QStringLiteral("download_size"),
            QStringLiteral("install_size"),
            QStringLiteral("summary")
        }
    );

    setBusy(true);
    setLastError(QString());

    qInfo() << "DNF5 UPGRADE QUERY START:" << cleanName;

    QDBusPendingCall pending =
        rpm.asyncCall(
            QStringLiteral("list"),
            options
        );

    auto *watcher =
        new QDBusPendingCallWatcher(pending, this);

    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, cleanName]() {

            setBusy(false);

            QDBusPendingReply<> reply = *watcher;

            if (reply.isError()) {
                const QString error =
                    QStringLiteral("DNF5 güncelleme sorgusu başarısız: %1")
                        .arg(reply.error().message());

                setLastError(error);

                qWarning()
                    << "DNF5 UPGRADE QUERY ERROR:"
                    << error;

                emit upgradePackageQueryFailed(error);

                watcher->deleteLater();
                return;
            }

            const QDBusMessage message = watcher->reply();

            if (message.arguments().isEmpty()) {
                const QString error =
                    QStringLiteral(
                        "DNF5 güncelleme sorgusu boş cevap döndürdü."
                    );

                setLastError(error);
                emit upgradePackageQueryFailed(error);

                watcher->deleteLater();
                return;
            }

            const QList<QVariantMap> packages =
                readPackageArray(
                    message.arguments().first()
                );

            if (packages.isEmpty()) {
                qInfo()
                    << "DNF5 UPGRADE:"
                    << cleanName
                    << "NO UPDATE";

                setLastError(QString());

                emit upgradePackageQueryFinished(QVariantMap{});

                watcher->deleteLater();
                return;
            }

            const QVariantMap package = packages.first();

            qInfo() << "DNF5 UPGRADE AVAILABLE";
            qInfo() << " name:" << package.value("name");
            qInfo() << " version:" << package.value("version");
            qInfo() << " release:" << package.value("release");
            qInfo() << " arch:" << package.value("arch");
            qInfo() << " repo:" << package.value("repo_id");

            setLastError(QString());

            emit upgradePackageQueryFinished(package);

            watcher->deleteLater();
        }
    );
}


void Dnf5Backend::queryPackageState(const QString &packageName)
{
    const QString cleanName = packageName.trimmed();

    if (cleanName.isEmpty()) {
        const QString error = QStringLiteral("Paket adı boş.");
        setLastError(error);
        return;
    }

    if (m_busy) {
        setLastError(
            QStringLiteral("DNF5 şu anda başka bir işlem yürütüyor.")
        );
        return;
    }

    m_stateAvailablePackage.clear();
    m_stateInstalledPackage.clear();

    setPackageState(PackageState::Unknown);

    auto *availableConnection =
        new QMetaObject::Connection;

    *availableConnection = connect(
        this,
        &Dnf5Backend::packageQueryFinished,
        this,
        [this, cleanName, availableConnection](const QVariantMap &package) {

            disconnect(*availableConnection);
            delete availableConnection;

            m_stateAvailablePackage = package;

            auto *installedConnection =
                new QMetaObject::Connection;

            *installedConnection = connect(
                this,
                &Dnf5Backend::installedPackageQueryFinished,
                this,
                [this, cleanName, installedConnection](
                    const QVariantMap &installedPackage
                ) {

                    disconnect(*installedConnection);
                    delete installedConnection;

                    m_stateInstalledPackage = installedPackage;

                    if (installedPackage.isEmpty()) {
                        setPackageState(PackageState::NotInstalled);

                        qInfo()
                            << "DNF5 PACKAGE STATE:"
                            << "NOT_INSTALLED";

                        emit packageStateQueryFinished(
                            PackageState::NotInstalled,
                            m_stateAvailablePackage,
                            QVariantMap{},
                            QVariantMap{}
                        );

                        return;
                    }

                    auto *upgradeConnection =
                        new QMetaObject::Connection;

                    *upgradeConnection = connect(
                        this,
                        &Dnf5Backend::upgradePackageQueryFinished,
                        this,
                        [this, upgradeConnection](
                            const QVariantMap &upgradePackage
                        ) {

                            disconnect(*upgradeConnection);
                            delete upgradeConnection;

                            if (upgradePackage.isEmpty()) {
                                setPackageState(
                                    PackageState::Installed
                                );

                                qInfo()
                                    << "DNF5 PACKAGE STATE:"
                                    << "INSTALLED";

                                emit packageStateQueryFinished(
                                    PackageState::Installed,
                                    m_stateAvailablePackage,
                                    m_stateInstalledPackage,
                                    QVariantMap{}
                                );
                            } else {
                                setPackageState(
                                    PackageState::UpdateAvailable
                                );

                                qInfo()
                                    << "DNF5 PACKAGE STATE:"
                                    << "UPDATE_AVAILABLE";

                                emit packageStateQueryFinished(
                                    PackageState::UpdateAvailable,
                                    m_stateAvailablePackage,
                                    m_stateInstalledPackage,
                                    upgradePackage
                                );
                            }
                        }
                    );

                    queryUpgradePackage(cleanName);
                }
            );

            queryInstalledPackage(cleanName);
        }
    );

    ensureSessionAsync(
        [this, cleanName](bool success) {
            if (!success) {
                qWarning()
                    << "DNF5 PACKAGE STATE ERROR:"
                    << m_lastError;
                return;
            }

            queryPackage(cleanName);
        }
    );
}


void Dnf5Backend::ensureSessionAsync(std::function<void(bool)> callback)
{
    if (sessionOpen()) {
        callback(true);
        return;
    }

    m_sessionWaiters.append(std::move(callback));

    if (m_sessionOpening) {
        return;
    }

    m_sessionOpening = true;

    QDBusConnection bus = QDBusConnection::systemBus();

    if (!bus.isConnected()) {
        setLastError(
            QStringLiteral("Sistem D-Bus bağlantısı kurulamadı.")
        );

        finishSessionOpen(false);
        return;
    }

    QDBusInterface manager(
        QString::fromLatin1(DNF5_SERVICE),
        QString::fromLatin1(DNF5_ROOT_PATH),
        QString::fromLatin1(DNF5_SESSION_MANAGER),
        bus
    );

    if (!manager.isValid()) {
        setLastError(
            QStringLiteral("DNF5 SessionManager kullanılamıyor: %1")
                .arg(manager.lastError().message())
        );

        finishSessionOpen(false);
        return;
    }

    QVariantMap options;

    QMap<QString, QString> config;
    config.insert(
        QStringLiteral("skip_if_unavailable"),
        QStringLiteral("1")
    );

    options.insert(
        QStringLiteral("config"),
        QVariant::fromValue(config)
    );

    qInfo() << "DNF5 SESSION OPEN START";

    QDBusPendingCall pending =
        manager.asyncCall(
            QStringLiteral("open_session"),
            options
        );

    auto *watcher =
        new QDBusPendingCallWatcher(pending, this);

    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher]() {

            QDBusPendingReply<QDBusObjectPath> reply = *watcher;

            if (reply.isError()) {
                setLastError(
                    QStringLiteral("DNF5 oturumu açılamadı: %1")
                        .arg(reply.error().message())
                );

                qWarning()
                    << "DNF5 SESSION OPEN ERROR:"
                    << m_lastError;

                watcher->deleteLater();
                finishSessionOpen(false);
                return;
            }

            const QString path = reply.value().path();

            if (path.isEmpty()) {
                setLastError(
                    QStringLiteral(
                        "DNF5 boş session path döndürdü."
                    )
                );

                watcher->deleteLater();
                finishSessionOpen(false);
                return;
            }

            m_sessionPath = path;

            emit sessionPathChanged();
            emit sessionOpenChanged();

            setLastError(QString());

            qInfo()
                << "DNF5 SESSION OPEN:"
                << m_sessionPath;

            watcher->deleteLater();

            finishSessionOpen(true);
        }
    );
}

void Dnf5Backend::finishSessionOpen(bool success)
{
    m_sessionOpening = false;

    const auto waiters = std::move(m_sessionWaiters);
    m_sessionWaiters.clear();

    for (const auto &callback : waiters) {
        callback(success);
    }
}

void Dnf5Backend::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }

    m_lastError = error;
    emit lastErrorChanged();
}

void Dnf5Backend::setBusy(bool value)
{
    if (m_busy == value) {
        return;
    }

    m_busy = value;
    emit busyChanged();
}

void Dnf5Backend::setPackageState(PackageState state)
{
    if (m_packageState == state) {
        return;
    }

    m_packageState = state;
    emit packageStateChanged();
}

void Dnf5Backend::clearSession()
{
    if (m_sessionPath.isEmpty()) {
        return;
    }

    m_sessionPath.clear();

    emit sessionPathChanged();
    emit sessionOpenChanged();
}
