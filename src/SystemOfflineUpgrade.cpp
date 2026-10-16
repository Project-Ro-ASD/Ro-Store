#include "SystemOfflineUpgrade.h"
#include "packages/backends/Dnf5Backend.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QDebug>
#include <limits>
#include <functional>


namespace {
// DNF5 registers org.rpm.dnf.v0.Offline on a SESSION object, not on
// /org/rpm/dnf/v0 (which only exports SessionManager).
constexpr auto kDnfService = "org.rpm.dnf.v0";
constexpr auto kDnfRoot = "/org/rpm/dnf/v0";
constexpr auto kSessionManager = "org.rpm.dnf.v0.SessionManager";
constexpr auto kOfflineInterface = "org.rpm.dnf.v0.Offline";

void closeOfflineSession(const QString &path)
{
    if (path.isEmpty())
        return;
    QDBusInterface manager(QString::fromLatin1(kDnfService),
        QString::fromLatin1(kDnfRoot), QString::fromLatin1(kSessionManager),
        QDBusConnection::systemBus());
    manager.asyncCall(QStringLiteral("close_session"),
                      QVariant::fromValue(QDBusObjectPath(path)));
}

void withOfflineSession(QObject *owner,
                        std::function<void(const QString &, const QString &)> callback)
{
    QDBusInterface manager(QString::fromLatin1(kDnfService),
        QString::fromLatin1(kDnfRoot), QString::fromLatin1(kSessionManager),
        QDBusConnection::systemBus());
    if (!manager.isValid()) {
        callback({}, QStringLiteral("DNF5 arka plan hizmetine bağlanılamadı."));
        return;
    }
    // Only consult offline state: do not load RPM databases or repositories.
    const QVariantMap options{
        {QStringLiteral("load_system_repo"), false},
        {QStringLiteral("load_available_repos"), false}
    };
    auto *watcher = new QDBusPendingCallWatcher(
        manager.asyncCall(QStringLiteral("open_session"), options), owner);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, owner,
                     [watcher, callback]() {
        const QDBusPendingReply<QDBusObjectPath> result = *watcher;
        watcher->deleteLater();
        if (result.isError()) {
            callback({}, QStringLiteral("DNF5 durum oturumu açılamadı: ")
                         + result.error().message());
            return;
        }
        const QString path = result.value().path();
        if (path.isEmpty()) {
            callback({}, QStringLiteral("DNF5 geçerli bir oturum döndürmedi."));
            return;
        }
        callback(path, {});
    });
}

void readOfflineStatus(QObject *owner,
                       std::function<void(const QString &, bool, const QVariantMap &)> callback)
{
    withOfflineSession(owner, [owner, callback](const QString &path, const QString &error) {
        if (!error.isEmpty()) {
            callback(error, false, {});
            return;
        }
        QDBusInterface offline(QString::fromLatin1(kDnfService), path,
            QString::fromLatin1(kOfflineInterface), QDBusConnection::systemBus());
        if (!offline.isValid()) {
            closeOfflineSession(path);
            callback(QStringLiteral("DNF5 çevrimdışı durum arayüzü bu sistemde kullanılamıyor."),
                     false, {});
            return;
        }
        auto *watcher = new QDBusPendingCallWatcher(
            offline.asyncCall(QStringLiteral("get_status")), owner);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, owner,
                         [watcher, callback, path]() {
            const QDBusPendingReply<bool, QVariantMap> result = *watcher;
            watcher->deleteLater();
            closeOfflineSession(path);
            if (result.isError()) {
                callback(QStringLiteral("DNF5 çevrimdışı durumu okunamadı: ")
                             + result.error().message(), false, {});
                return;
            }
            callback({}, result.argumentAt<0>(), result.argumentAt<1>());
        });
    });
}

bool hasPreparedOfflineUpdate(bool pending, const QVariantMap &state)
{
    // Incomplete/unknown records must also block overriding another tool.
    return pending || !state.isEmpty();
}
} // namespace

SystemOfflineUpgrade::SystemOfflineUpgrade(QObject *parent) : QObject(parent)
{
    m_backend = new Dnf5Backend(this);
    if (QFile::exists(journalPath())) {
        m_outcomeUnknown = true;
        m_message = QStringLiteral(
            "Önceki çevrimdışı hazırlama işleminin sonucu henüz doğrulanmadı. "
            "Otomatik tekrar ve yeni hazırlama güvenlik için engellendi.");
    }

    connect(m_backend, &Dnf5Backend::systemUpdatesResolved, this,
        [this](const QVariantList &items, qulonglong bytes) {
        if (!m_busy || m_preparing || m_outcomeUnknown)
            return;
        if (m_selectionInvalidated) {
            fail(QStringLiteral("Paket seçimi plan hazırlanırken değişti. DNF5 planını yeniden oluşturun."));
            return;
        }
        m_planCount = items.size();
        m_planBytes = bytes;
        QStringList summary;
        int removals = 0;
        bool kernel = false;
        bool unsafe = false;
        int unknownBytes = 0;
        for (const QVariant &row : items) {
            const QVariantMap item = row.toMap();
            const QVariantMap pkg = item.value(QStringLiteral("object")).toMap();
            const QString action = item.value(QStringLiteral("action")).toString();
            const QString name = pkg.value(QStringLiteral("name")).toString();
            // For a first live system test, refuse unrecognized operations,
            // removals, downgrades or missing package identities.
            const QStringList allowed{QStringLiteral("Upgrade"), QStringLiteral("Install"),
                QStringLiteral("Reinstall"), QStringLiteral("Replaced"),
                QStringLiteral("Reason Change")};
            if (name.isEmpty() || !allowed.contains(action))
                unsafe = true;
            if (name.startsWith(QStringLiteral("kernel"), Qt::CaseInsensitive))
                kernel = true;
            if (action == QStringLiteral("Remove"))
                ++removals;
            else if (action != QStringLiteral("Replaced")
                     && action != QStringLiteral("Reason Change")
                     && !pkg.value(QStringLiteral("download_size")).toULongLong()
                     && !pkg.value(QStringLiteral("package_size")).toULongLong())
                ++unknownBytes;
            summary << QStringLiteral("%1 — %2 %3.%4")
                .arg(action, pkg.value(QStringLiteral("name")).toString(),
                     pkg.value(QStringLiteral("version")).toString(),
                     pkg.value(QStringLiteral("arch")).toString());
        }
        m_planText = summary.join(QChar('\n'));
        m_removalCount = removals;
        m_unsafePlan = unsafe;
        m_kernelAffected = kernel;
        m_ready = !unsafe;
        m_message = unsafe
            ? QStringLiteral("Güvenlik engeli: Plan kaldırma, düşürme, bilinmeyen işlem "
                             "veya eksik paket kimliği içeriyor. Hazırlama kapatıldı.")
            : QStringLiteral(
            "Tek DNF5 işlem planı çözüldü: %1 değişiklik, %2 kaldırma. "
            "Yaklaşık indirme: %3 MiB; %4 paketin boyutu belirsiz. "
            "Bu toplam mevcut Ro-Store bağımsız RPM önbelleğini düşmez.")
            .arg(items.size()).arg(removals)
            .arg(QString::number(bytes / 1048576.0, 'f', 1)).arg(unknownBytes);
        qInfo() << "SYSTEM OFFLINE GOAL READY:" << items.size()
                << "removals:" << removals << "unknown sizes:" << unknownBytes;
        emit changed();
    });
    connect(m_backend, &Dnf5Backend::systemUpdatesResolveFailed, this,
        [this](const QString &error) { fail(error); });
    connect(m_backend, &Dnf5Backend::downloadStarted, this,
        [this](const QString &id, const QString &, qlonglong) {
            if (!m_preparing) return;
            m_downloadById.insert(id, 0);
            emit changed();
        });
    connect(m_backend, &Dnf5Backend::downloadProgressChanged, this,
        [this](const QString &id, qlonglong, qlonglong downloaded) {
            if (!m_preparing) return;
            m_downloadById.insert(id, downloaded > 0
                ? static_cast<qulonglong>(downloaded) : 0);
            qulonglong total = 0;
            for (auto it = m_downloadById.cbegin(); it != m_downloadById.cend(); ++it)
                total += it.value();
            m_downloadedBytes = total;
            if (m_planBytes > 0)
                m_progressPercent = static_cast<int>(
                    qMin<qulonglong>(99, total * 100 / m_planBytes));
            emit changed();
        });
    connect(m_backend, &Dnf5Backend::downloadFinished, this,
        [this](const QString &, uint, const QString &) {
            if (m_preparing) emit changed();
        });
    connect(m_backend, &Dnf5Backend::transactionExecutionStarted, this,
        [this](bool) { m_message = QStringLiteral("DNF5 çevrimdışı işlem dosyalarını hazırlıyor. Yeniden başlatma yapılmayacak."); emit changed(); });
    connect(m_backend, &Dnf5Backend::transactionExecutionFinished, this,
        [this](bool) { verifyOfflineStatus(); });
    connect(m_backend, &Dnf5Backend::transactionExecutionFailed, this,
        [this](const QString &error) {
            m_outcomeUnknown = true; m_preparing = false;
            m_message = QStringLiteral("İşlem başlamış olabilir, otomatik tekrar engellendi: ") + error;
            emit changed();
        });
    connect(m_backend, &Dnf5Backend::transactionExecutionOutcomeUnknown, this,
        [this](const QString &error) {
            m_outcomeUnknown = true; m_preparing = false;
            m_message = QStringLiteral("DNF5 işlem sonucu belirsiz. Güvenlik kaydı korundu: ") + error;
            emit changed();
        });
    connect(m_backend, &Dnf5Backend::transactionCancelFinished, this,
        [this](bool accepted, const QString &error) {
            m_message = accepted
                ? QStringLiteral("İptal isteği kabul edildi; DNF5'in gerçek sonucu bekleniyor.")
                : QStringLiteral("İptal doğrulanamadı; işlem devam ediyor olabilir: ") + error;
            emit changed();
        });
}

SystemOfflineUpgrade::~SystemOfflineUpgrade()
{
    // Never forcibly kill a DNF5 RPM transaction. The daemon owns the work.
    releaseLock();
}

void SystemOfflineUpgrade::setTransactionManager(QObject *manager)
{
    if (m_busy)
        return;
    m_manager = manager;
    if (m_outcomeUnknown && manager) {
        const bool acquired = QMetaObject::invokeMethod(manager, "acquireExternalOperation",
            Q_RETURN_ARG(bool, m_locked), Q_ARG(QObject*, this));
        if (!acquired) m_locked = false;
    }
    emit changed();
}

QString SystemOfflineUpgrade::journalPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QStringLiteral("/pending-offline-system-upgrade.json");
}

void SystemOfflineUpgrade::releaseLock()
{
    if (m_locked && m_manager)
        QMetaObject::invokeMethod(m_manager, "releaseExternalOperation",
                                  Q_ARG(QObject*, this));
    m_locked = false;
}

void SystemOfflineUpgrade::resolve(const QStringList &selectedNevras)
{
    if (m_busy || m_outcomeUnknown || !m_manager || selectedNevras.isEmpty())
        return;
    if (!QMetaObject::invokeMethod(m_manager, "acquireExternalOperation",
           Q_RETURN_ARG(bool, m_locked), Q_ARG(QObject*, this)) || !m_locked) {
        m_message = QStringLiteral("Başka bir RPM işlemi aktif; sistem planı başlatılmadı.");
        emit changed();
        return;
    }
    m_busy = true;
    m_ready = false;
    m_offlineReady = false;
    m_preparing = false;
    m_requested = selectedNevras;
    m_selectionInvalidated = false;
    m_planText.clear();
    m_planBytes = 0;
    m_planCount = 0;
    m_removalCount = 0;
    m_unsafePlan = false;
    m_kernelAffected = false;
    m_message = QStringLiteral("DNF5 tüm seçilen RPM güncellemelerini tek işlemde çözüyor...");
    emit changed();
    // Check the authoritative Offline interface on a short-lived DNF5 session.
    // A failed check must never allow a new transaction to be prepared.
    readOfflineStatus(this, [this, selectedNevras](const QString &error, bool pending,
                                                  const QVariantMap &state) {
        if (!m_busy || m_outcomeUnknown)
            return;
        if (!error.isEmpty()) {
            fail(error);
            return;
        }
        const QString status = state.value(QStringLiteral("status")).toString();
        if (hasPreparedOfflineUpdate(pending, state)) {
            fail(QStringLiteral("Önceden hazırlanmış bir sistem güncellemesi var (%1). "
                                "Güvenlik için üzerine yazılmadı.").arg(status));
            return;
        }
        if (m_selectionInvalidated) {
            fail(QStringLiteral("Paket seçimi değişti. Güncel seçim için tekrar plan oluşturun."));
            return;
        }
        m_backend->resolveSystemUpdates(selectedNevras);
    });

}

void SystemOfflineUpgrade::fail(const QString &error)
{
    m_message = error;
    m_ready = false;
    m_busy = false;
    m_preparing = false;
    m_preflightRunning = false;
    releaseLock();
    emit changed();
}

bool SystemOfflineUpgrade::writeJournal()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir().mkpath(dir))
        return false;
    QSaveFile file(journalPath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QJsonObject obj;
    QFile bootIdFile(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    if (!bootIdFile.open(QIODevice::ReadOnly))
        return false;
    const QString bootId = QString::fromLatin1(bootIdFile.readAll()).trimmed();
    if (bootId.isEmpty())
        return false;
    obj.insert(QStringLiteral("boot_id"), bootId);
    obj.insert(QStringLiteral("started_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    obj.insert(QStringLiteral("selection_count"), m_requested.size());
    obj.insert(QStringLiteral("plan_count"), m_planCount);
    obj.insert(QStringLiteral("type"), QStringLiteral("offline-system-upgrade"));
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    return file.write(payload) == payload.size() && file.commit();
}

void SystemOfflineUpgrade::stageOffline()
{
    if (!m_ready || m_unsafePlan || !m_busy || m_preparing || m_preflightRunning
        || m_outcomeUnknown || !m_locked)
        return;
    m_preflightRunning = true;
    m_message = QStringLiteral("Mevcut çevrimdışı güncellemeler yeniden kontrol ediliyor...");
    emit changed();
    readOfflineStatus(this, [this](const QString &error, bool pending,
                                   const QVariantMap &state) {
        if (!m_preflightRunning)
            return;
        m_preflightRunning = false;
        if (!error.isEmpty()) {
            fail(QStringLiteral("Güncellemeyi hazırlamadan önce DNF5 durumu doğrulanamadı: ")
                 + error);
            return;
        }
        if (hasPreparedOfflineUpdate(pending, state)) {
            fail(QStringLiteral("Başka bir çevrimdışı güncelleme hazır durumda. "
                                "Mevcut işlem korunuyor; hazırlama başlatılmadı."));
            return;
        }
        if (m_selectionInvalidated) {
            fail(QStringLiteral("Seçim değişti; eski güncelleme planı geçersiz."));
            return;
        }
        beginOfflineStage();
    });
}

void SystemOfflineUpgrade::beginOfflineStage()
{
    if (!m_ready || m_unsafePlan || !m_busy || m_preparing || m_preflightRunning
        || m_outcomeUnknown || !m_locked)
        return;
    if (QFile::exists(journalPath())) {
        m_outcomeUnknown = true;
        m_message = QStringLiteral("Önceki çevrimdışı işlem güvenlik kaydı var. Yeni işlem engellendi.");
        emit changed();
        return;
    }
    // Be conservative about disk space for downloads, dependency changes
    // and offline transaction metadata. DNF5 still performs its own checks.
    const QStorageInfo storage(QStringLiteral("/var"));
    const qulonglong minimumReserve = 512ull * 1024ull * 1024ull;
    const qulonglong extra = 300ull * 1024ull * 1024ull;
    if (!storage.isValid() || !storage.isReady() || storage.isReadOnly()) {
        fail(QStringLiteral("DNF5 indirme depolaması doğrulanamadı veya salt okunur; işlem başlatılmadı."));
        return;
    }
    const qulonglong freeBytes = static_cast<qulonglong>(
        qMax<qint64>(0, storage.bytesAvailable()));
    const qulonglong required = qMax(minimumReserve,
        m_planBytes <= (std::numeric_limits<qulonglong>::max() - extra) / 2
            ? m_planBytes * 2 + extra
            : std::numeric_limits<qulonglong>::max());
    if (freeBytes < required) {
        fail(QStringLiteral(
            "Yeterli disk alanı doğrulanamadı. /var boş: %1 MiB, "
            "güvenli hazırlık için yaklaşık %2 MiB gerekli.")
            .arg(freeBytes / 1048576).arg(required / 1048576));
        return;
    }
    if (!writeJournal()) {
        fail(QStringLiteral("Kalıcı güvenlik kaydı yazılamadı; DNF5 işlemi başlatılmadı."));
        return;
    }
    m_preparing = true;
    m_ready = false;
    m_downloadedBytes = 0;
    m_progressPercent = 0;
    m_downloadById.clear();
    m_message = QStringLiteral(
        "Açık onay alındı. DNF5 işlem yalnızca çevrimdışı yürütme için hazırlanıyor; şimdi RPM kurulmaz.");
    emit changed();
    // Documented DNF5 Goal option: offline=true; no automatic reboot.
    m_backend->executeResolvedTransaction(false, true);
}

void SystemOfflineUpgrade::verifyOfflineStatus()
{
    readOfflineStatus(this, [this](const QString &error, bool pending,
                                   const QVariantMap &state) {
        m_preparing = false;
        if (!error.isEmpty()) {
            m_outcomeUnknown = true;
            m_message = QStringLiteral("Hazırlamanın sonucu doğrulanamadı. Güvenlik kaydı korundu: ")
                        + error;
            emit changed();
            return;
        }
        const QString status = state.value(QStringLiteral("status")).toString();
        if (!hasPreparedOfflineUpdate(pending, state)) {
            m_outcomeUnknown = true;
            m_message = QStringLiteral("DNF5 hazırlama sonucunu doğrulayamadı (%1). "
                                      "Yeniden hazırlama engellendi.").arg(status);
            emit changed();
            return;
        }
        m_offlineReady = true;
        m_busy = false;
        m_message = QStringLiteral("Güncelleme dosyaları hazır (%1). Paketler henüz kurulmadı. "
                                  "Uygulamak için ayrıca yeniden başlatma onayı gerekir.")
                    .arg(status);
        // Keep the safety journal until post-boot recovery review.
        releaseLock();
        emit changed();
    });
}

void SystemOfflineUpgrade::abandonPlan()
{
    if (m_preparing || m_preflightRunning || m_outcomeUnknown)
        return;
    m_ready = false;
    m_busy = false;
    m_requested.clear();
    m_planText.clear();
    m_planCount = 0;
    m_planBytes = 0;
    m_unsafePlan = false;
    m_message = QStringLiteral("Plan iptal edildi; sistemde hiçbir RPM işlemi uygulanmadı.");
    releaseLock();
    emit changed();
}

void SystemOfflineUpgrade::requestCancellation()
{
    if (!m_preparing)
        return;
    m_message = QStringLiteral("DNF5 indirme aşamasında iptal isteği gönderildi. Sonuç bekleniyor...");
    // Never kill the daemon or an RPM transaction; DNF5 decides if safe.
    m_backend->cancelTransaction();
    emit changed();
}


void SystemOfflineUpgrade::recheckRecovery()
{
    if (!m_outcomeUnknown || !QFile::exists(journalPath()))
        return;
    m_recoveryVerified = false;
    QFile journal(journalPath());
    if (!journal.open(QIODevice::ReadOnly)) {
        m_message = QStringLiteral("Güvenlik kaydı okunamadı; kilit korunuyor.");
        emit changed();
        return;
    }
    const QJsonDocument json = QJsonDocument::fromJson(journal.readAll());
    const QString previousBoot = json.object().value(QStringLiteral("boot_id")).toString();
    QFile bootFile(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    if (!bootFile.open(QIODevice::ReadOnly) || previousBoot.isEmpty()) {
        m_message = QStringLiteral("Önceki ve mevcut sistem açılışı doğrulanamadı; kilit korunuyor.");
        emit changed();
        return;
    }
    const QString nowBoot = QString::fromLatin1(bootFile.readAll()).trimmed();
    if (nowBoot == previousBoot) {
        m_message = QStringLiteral(
            "Önceki işlemle aynı sistem açılışındasınız. "
            "Çevrimdışı hazırlama sonucu kesin olarak doğrulanmadığı için "
            "önce sistem günlüklerini kontrol edin; kilit korunuyor.");
        emit changed();
        return;
    }
    readOfflineStatus(this, [this](const QString &error, bool pending,
                                   const QVariantMap &state) {
        if (!error.isEmpty()) {
            m_message = QStringLiteral("DNF5 durumu denetlenemedi; güvenlik kilidi korunuyor: ")
                + error;
            emit changed();
            return;
        }
        if (hasPreparedOfflineUpdate(pending, state)) {
            m_message = QStringLiteral("DNF5 güncellemesi hâlâ bekliyor. "
                                      "Güvenlik kilidi kaldırılamaz.");
            emit changed();
            return;
        }
        m_recoveryVerified = true;
        m_message = QStringLiteral("Sistem yeniden başlatılmış ve bekleyen işlem bulunmamış. "
                                  "Bu, güncellemenin başarılı olduğunu göstermez. "
                                  "Önce 'dnf5 offline log' ve 'dnf5 history' kayıtlarını inceleyin; "
                                  "sonra güvenlik kaydını kaldırmayı onaylayabilirsiniz.");
        emit changed();
    });

}

void SystemOfflineUpgrade::acknowledgeRecovery()
{
    if (!m_recoveryVerified || !m_outcomeUnknown || !QFile::exists(journalPath()))
        return;
    if (!QFile::remove(journalPath())) {
        m_message = QStringLiteral("Güvenlik kaydı silinemedi; kilit korunuyor.");
        emit changed();
        return;
    }
    m_outcomeUnknown = false;
    m_offlineReady = false;
    m_recoveryVerified = false;
    m_message = QStringLiteral(
        "Kullanıcı onayıyla kurtarma kaydı temizlendi. "
        "Bu onay güncellemelerin başarıyla kurulduğu anlamına gelmez.");
    releaseLock();
    emit changed();
}


void SystemOfflineUpgrade::invalidateIfSelectionChanged(const QStringList &selectedNevras)
{
    if (!m_busy || m_preparing || m_requested == selectedNevras)
        return;
    if (m_preflightRunning) {
        m_selectionInvalidated = true;
        m_message = QStringLiteral("Seçim değişti. Hazırlama güvenlik kontrolünün sonucu kullanılmayacak.");
        emit changed();
        return;
    }
    if (m_ready) {
        abandonPlan();
    } else {
        // The solver is still running. Do not release the lock until it
        // finishes; then discard the no-longer-approved goal.
        m_selectionInvalidated = true;
        m_message = QStringLiteral("Seçim değişti. Çözümleme bittiğinde bu plan kullanılmayacak.");
        emit changed();
    }
}


void SystemOfflineUpgrade::requestConfirmedReboot()
{
    // Do not reboot based solely on a stale in-memory state.
    if (!m_offlineReady || m_preparing || m_outcomeUnknown || m_rebootRequestRunning
        || m_clearingPrepared || !QFile::exists(journalPath()))
        return;
    m_rebootRequestRunning = true;
    emit changed();
    readOfflineStatus(this, [this](const QString &error, bool pending,
                                   const QVariantMap &state) {
        if (!m_rebootRequestRunning)
            return;
        if (!error.isEmpty() || !pending ||
            state.value(QStringLiteral("status")).toString() != QStringLiteral("ready")) {
            m_rebootRequestRunning = false;
            m_message = QStringLiteral("DNF5 güncellemesi yeniden başlatmaya hazır olarak "
                                      "doğrulanamadı. Yeniden başlatma engellendi. ") + error;
            emit changed();
            return;
        }
        beginConfirmedReboot();
    });
}

void SystemOfflineUpgrade::beginConfirmedReboot()
{
    if (!m_rebootRequestRunning || !m_offlineReady || !QFile::exists(journalPath()))
        return;
    if (!QFileInfo::exists(QStringLiteral("/usr/bin/pkexec"))
        || !QFileInfo::exists(QStringLiteral("/usr/bin/dnf5"))) {
        m_rebootRequestRunning = false;
        m_message = QStringLiteral(
            "pkexec veya dnf5 bulunamadı. Yönetici olarak terminalden "
            "'sudo dnf5 offline reboot' komutunu ayrı onayla çalıştırabilirsiniz.");
        emit changed();
        return;
    }
    auto *process = new QProcess(this);
    m_rebootProcess = process;
    process->setProgram(QStringLiteral("/usr/bin/pkexec"));
    process->setArguments({QStringLiteral("/usr/bin/dnf5"),
                           QStringLiteral("-y"),
                           QStringLiteral("offline"),
                           QStringLiteral("reboot")});
    process->setProcessChannelMode(QProcess::MergedChannels);
    m_message = QStringLiteral(
        "Açık onayla DNF5 çevrimdışı yeniden başlatma isteği gönderiliyor. "
        "Kimlik doğrulama penceresi açılabilir. Çalışmalarınızı kaydetmiş olmalısınız.");
    qInfo() << "SYSTEM OFFLINE REBOOT REQUEST: explicitly approved by user";
    connect(process, &QProcess::finished, this,
            [this, process](int code, QProcess::ExitStatus status) {
        if (m_rebootProcess != process)
            return;
        const QString output =
            QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed().left(1200);
        m_rebootProcess = nullptr;
        m_rebootRequestRunning = false;
        process->deleteLater();
        m_message = status == QProcess::NormalExit && code == 0
            ? QStringLiteral("DNF5 yeniden başlatma isteği tamamlandı. "
                             "Sistem açıldıktan sonra kurtarma kaydını denetleyin.")
            : QStringLiteral("Yeniden başlatma gerçekleşmemiş olabilir (kod %1): %2. "
                             "Çevrimdışı işlem kaydı korunuyor.").arg(code).arg(output);
        emit changed();
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (m_rebootProcess != process || error != QProcess::FailedToStart)
            return;
        m_rebootProcess = nullptr;
        m_rebootRequestRunning = false;
        m_message = QStringLiteral(
            "Yönetici yetkisiyle yeniden başlatma başlatılamadı. "
            "Çevrimdışı kayıt korunuyor.");
        process->deleteLater();
        emit changed();
    });
    process->start();
    emit changed();
}


void SystemOfflineUpgrade::discardPreparedOffline()
{
    if (!m_offlineReady || m_preparing || m_outcomeUnknown || m_clearingPrepared
        || m_rebootRequestRunning || !QFile::exists(journalPath()))
        return;
    m_clearingPrepared = true;
    m_message = QStringLiteral("Hazırlanmış güncelleme dosyaları DNF5 ile temizleniyor...");
    emit changed();
    withOfflineSession(this, [this](const QString &path, const QString &error) {
        if (!error.isEmpty()) {
            m_clearingPrepared = false;
            m_message = error + QStringLiteral(" Hazırlanmış işlem korunuyor.");
            emit changed();
            return;
        }
        QDBusInterface offline(QString::fromLatin1(kDnfService), path,
            QString::fromLatin1(kOfflineInterface), QDBusConnection::systemBus());
        if (!offline.isValid()) {
            closeOfflineSession(path);
            m_clearingPrepared = false;
            m_message = QStringLiteral("DNF5 çevrimdışı arayüzü açılamadı. İşlem korunuyor.");
            emit changed();
            return;
        }
        auto *watcher = new QDBusPendingCallWatcher(
            offline.asyncCall(QStringLiteral("clean_with_options"),
                              QVariantMap{{QStringLiteral("interactive"), true}}), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, path]() {
            const QDBusPendingReply<bool, QString> result = *watcher;
            watcher->deleteLater();
            closeOfflineSession(path);
            if (result.isError() || !result.argumentAt<0>()) {
                m_clearingPrepared = false;
                m_message = QStringLiteral("DNF5 temizleme işlemi doğrulanamadı: ")
                    + (result.isError() ? result.error().message() : result.argumentAt<1>());
                emit changed();
                return;
            }
            readOfflineStatus(this, [this](const QString &error, bool pending,
                                           const QVariantMap &state) {
                m_clearingPrepared = false;
                if (!error.isEmpty() || hasPreparedOfflineUpdate(pending, state)) {
                    m_outcomeUnknown = true;
                    m_message = QStringLiteral("DNF5 son temizleme durumu kesin doğrulanamadı. "
                                              "Güvenlik kaydı korundu. ") + error;
                    emit changed();
                    return;
                }
                if (!QFile::remove(journalPath())) {
                    m_outcomeUnknown = true;
                    m_message = QStringLiteral("DNF5 dosyaları temizlendi ama Ro-Store "
                                              "güvenlik kaydı silinemedi.");
                    emit changed();
                    return;
                }
                m_offlineReady = false;
                m_ready = false;
                m_busy = false;
                m_message = QStringLiteral("Hazırlanan güncelleme iptal edildi. "
                                          "Kurulu sistem paketleri değiştirilmedi.");
                releaseLock();
                emit changed();
            });
        });
    });

}
