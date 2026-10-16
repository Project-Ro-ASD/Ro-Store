#include "SystemOfflineUpgrade.h"
#include "packages/backends/Dnf5Backend.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDebug>

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
        m_planCount = items.size();
        m_planBytes = bytes;
        QStringList summary;
        int removals = 0;
        int unknownBytes = 0;
        for (const QVariant &row : items) {
            const QVariantMap item = row.toMap();
            const QVariantMap pkg = item.value(QStringLiteral("object")).toMap();
            const QString action = item.value(QStringLiteral("action")).toString();
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
        m_ready = true;
        m_message = QStringLiteral(
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
    m_planText.clear();
    m_planBytes = 0;
    m_planCount = 0;
    m_message = QStringLiteral("DNF5 tüm seçilen RPM güncellemelerini tek işlemde çözüyor...");
    emit changed();
    // Never overwrite an offline transaction prepared by DNF5, Discover,
    // or another application. Query the authoritative daemon interface,
    // not just the /system-update marker.
    QDBusInterface offline(QStringLiteral("org.rpm.dnf.v0"),
        QStringLiteral("/org/rpm/dnf/v0"),
        QStringLiteral("org.rpm.dnf.v0.Offline"), QDBusConnection::systemBus());
    if (!offline.isValid()) {
        fail(QStringLiteral("DNF5 çevrimdışı işlem durumu doğrulanamadı; plan başlatılmadı."));
        return;
    }
    auto *watcher = new QDBusPendingCallWatcher(
        offline.asyncCall(QStringLiteral("get_status")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, selectedNevras]() {
        QDBusPendingReply<bool, QVariantMap> result = *watcher;
        watcher->deleteLater();
        if (!m_busy || m_outcomeUnknown)
            return;
        if (result.isError()) {
            fail(QStringLiteral("DNF5 çevrimdışı durumu okunamadı: ")
                 + result.error().message());
            return;
        }
        const QString status = result.argumentAt<1>()
            .value(QStringLiteral("status")).toString();
        if (result.argumentAt<0>()
            || status == QStringLiteral("ready")
            || status == QStringLiteral("download-complete")) {
            fail(QStringLiteral(
                "Önceden hazırlanmış DNF5 çevrimdışı işlemi bulundu (")
                + status + QStringLiteral("). Üzerine yeni işlem yazılmadı."));
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
    obj.insert(QStringLiteral("started_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    obj.insert(QStringLiteral("selection_count"), m_requested.size());
    obj.insert(QStringLiteral("plan_count"), m_planCount);
    obj.insert(QStringLiteral("type"), QStringLiteral("offline-system-upgrade"));
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return file.commit();
}

void SystemOfflineUpgrade::stageOffline()
{
    if (!m_ready || !m_busy || m_preparing || m_outcomeUnknown || !m_locked)
        return;
    if (QFile::exists(journalPath())) {
        m_outcomeUnknown = true;
        m_message = QStringLiteral("Önceki çevrimdışı işlem güvenlik kaydı var. Yeni işlem engellendi.");
        emit changed();
        return;
    }
    if (!writeJournal()) {
        fail(QStringLiteral("Kalıcı güvenlik kaydı yazılamadı; DNF5 işlemi başlatılmadı."));
        return;
    }
    m_preparing = true;
    m_ready = false;
    m_message = QStringLiteral(
        "Açık onay alındı. DNF5 işlem yalnızca çevrimdışı yürütme için hazırlanıyor; şimdi RPM kurulmaz.");
    emit changed();
    // Documented DNF5 Goal option: offline=true; no automatic reboot.
    m_backend->executeResolvedTransaction(false, true);
}

void SystemOfflineUpgrade::verifyOfflineStatus()
{
    QDBusInterface offline(QStringLiteral("org.rpm.dnf.v0"),
        QStringLiteral("/org/rpm/dnf/v0"),
        QStringLiteral("org.rpm.dnf.v0.Offline"), QDBusConnection::systemBus());
    if (!offline.isValid()) {
        m_outcomeUnknown = true;
        m_preparing = false;
        m_message = QStringLiteral(
            "DNF5 hazırlama tamamlandığını bildirdi fakat çevrimdışı durum arayüzü açılamadı. Güvenlik kaydı korundu.");
        emit changed();
        return;
    }
    auto *watcher = new QDBusPendingCallWatcher(
        offline.asyncCall(QStringLiteral("get_status")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        QDBusPendingReply<bool, QVariantMap> result = *watcher;
        watcher->deleteLater();
        m_preparing = false;
        if (result.isError()) {
            m_outcomeUnknown = true;
            m_message = QStringLiteral(
                "Hazırlama sonucu çevrimdışı durum kaydıyla doğrulanamadı; otomatik tekrar engellendi: ")
                + result.error().message();
            emit changed();
            return;
        }
        const QVariantMap state = result.argumentAt<1>();
        const QString status = state.value(QStringLiteral("status")).toString();
        // A transaction can be downloaded but not scheduled for reboot yet.
        const bool ready = result.argumentAt<0>()
            || status == QStringLiteral("ready")
            || status == QStringLiteral("download-complete");
        if (!ready) {
            m_outcomeUnknown = true;
            m_message = QStringLiteral(
                "DNF5 işlemi döndü ancak çevrimdışı hazırlama durumu doğrulanamadı (")
                + status + QStringLiteral("). Güvenlik kaydı korundu.");
            emit changed();
            return;
        }
        m_offlineReady = true;
        m_busy = false;
        m_message = QStringLiteral(
            "DNF5 çevrimdışı işlem dosyaları hazır (%1). "
            "Ro-Store yeniden başlatmadı. Güncellemeleri uygulamak için "
            "ayrı bir yeniden başlatma onayı gereklidir.").arg(status);
        // Keep the journal until recovery checks after the next boot.
        releaseLock();
        emit changed();
    });
}

void SystemOfflineUpgrade::abandonPlan()
{
    if (m_preparing || m_outcomeUnknown)
        return;
    m_ready = false;
    m_busy = false;
    m_requested.clear();
    m_planText.clear();
    m_planCount = 0;
    m_planBytes = 0;
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
