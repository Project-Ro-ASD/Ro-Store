#include "FlatpakManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
constexpr qsizetype MaxCapture = 3 * 1024 * 1024;
const QStringList scopes = {QStringLiteral("user"), QStringLiteral("system")};
QString scoped(const QString &scope) {
    return scope == QStringLiteral("user") ? QStringLiteral("--user") : QStringLiteral("--system");
}
QString clean(const QString &text) {
    QString value = text.trimmed();
    value.remove(QRegularExpression(QStringLiteral("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F]")));
    return value.left(1024);
}
QVariantMap fields(const QStringList &row, const QStringList &keys, const QString &scope) {
    QVariantMap result;
    result.insert(QStringLiteral("scope"), scope);
    for (qsizetype i = 0; i < keys.size(); ++i)
        result.insert(keys.at(i), i < row.size() ? clean(row.at(i)) : QString());
    return result;
}
}

FlatpakManager::FlatpakManager(QObject *parent) : QObject(parent)
{
    m_available = QFileInfo::exists(QStringLiteral("/usr/bin/flatpak"));
    m_outcomeUnknown = QFileInfo::exists(journalPath());
    if (m_outcomeUnknown)
        m_message = QStringLiteral("Önceki Flatpak işlemi belirsiz. İşlemleri durdurduk; önce durumu kontrol edin.");
    else if (!m_available)
        m_message = QStringLiteral("Flatpak yüklü değil. RPM işlemleriniz etkilenmez.");
    else
        m_message = QStringLiteral("Flatpak kaynakları henüz sorgulanmadı.");
}

FlatpakManager::~FlatpakManager()
{
    // Never kill an active mutating Flatpak process or silently clear a journal.
    if (m_process && m_current.mutation) {
        m_process->setParent(nullptr);
        QObject::connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                         m_process, &QObject::deleteLater);
    } else if (m_process) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    releaseLock();
}
void FlatpakManager::setTransactionManager(QObject *manager) {
    // The RPM coordinator must be bound even when a previous Flatpak
    // transaction left a recovery journal. Otherwise acknowledging recovery
    // leaves the next mutation permanently unable to acquire its lock.
    // Never switch coordinator while a Flatpak transaction is active.
    if (m_busy || m_loading || m_process) return;
    if (m_transactionManager == manager) return;
    m_transactionManager = manager;
    emit changed();
}
bool FlatpakManager::safeScope(const QString &v) { return scopes.contains(v); }
bool FlatpakManager::safeRemote(const QString &v) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]{0,127}$"));
    return re.match(v).hasMatch();
}
bool FlatpakManager::safeAppId(const QString &v) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]{2,255}$"));
    return re.match(v).hasMatch() && v.contains(QLatin1Char('.')) && !v.contains(QStringLiteral(".."));
}
QStringList FlatpakManager::lines(const QByteArray &data) {
    return QString::fromUtf8(data).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}
QString FlatpakManager::journalPath() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QStringLiteral("/flatpak-transaction-journal.json");
}
bool FlatpakManager::writeJournal(const QString &action, const QString &scope, const QString &id) {
    const QString path = journalPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray bytes = QJsonDocument(QJsonObject{
        {QStringLiteral("operation"), action},
        {QStringLiteral("scope"), scope},
        {QStringLiteral("id"), id},
        {QStringLiteral("started"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}
    }).toJson(QJsonDocument::Compact);
    return file.write(bytes) == bytes.size() && file.commit();
}
bool FlatpakManager::acquireLock() {
    if (m_locked) return true;
    if (!m_transactionManager) return false;
    m_crossProcessLock = std::make_unique<QLockFile>(journalPath() + QStringLiteral(".lock"));
    if (!QDir().mkpath(QFileInfo(journalPath()).absolutePath()) || !m_crossProcessLock->tryLock(0)) {
        m_crossProcessLock.reset();
        return false;
    }
    bool locked = false;
    if (!QMetaObject::invokeMethod(m_transactionManager.data(), "acquireExternalOperation",
                                   Qt::DirectConnection, Q_RETURN_ARG(bool, locked),
                                   Q_ARG(QObject*, this)) || !locked)
        { m_crossProcessLock.reset(); return false; }
    m_locked = true;
    return true;
}
void FlatpakManager::releaseLock() {
    if (m_locked && m_transactionManager)
        QMetaObject::invokeMethod(m_transactionManager.data(), "releaseExternalOperation",
                                  Qt::DirectConnection, Q_ARG(QObject*, this));
    m_locked = false;
    if (m_crossProcessLock) { m_crossProcessLock->unlock(); m_crossProcessLock.reset(); }
}
void FlatpakManager::fail(const QString &message) {
    m_message = message;
    emit changed();
}
void FlatpakManager::enqueueRead(const QString &kind, const QStringList &args, int timeoutMs) {
    m_queue.enqueue(Task{kind, args, timeoutMs, false});
}
void FlatpakManager::refresh() {
    if (!m_available) { fail(QStringLiteral("Flatpak kurulu değil.")); return; }
    if (m_busy || m_loading || m_process) return;
    m_loading = true;
    m_recoveryChecked = false;
    m_recoveryReadFailed = false;
    m_successfulReadKinds.clear();
    m_queue.clear();
    m_sources.clear();
    m_installed.clear();
    m_updates.clear();
    for (const QString &scope : scopes) {
        enqueueRead(QStringLiteral("remotes:") + scope,
                    {scoped(scope), QStringLiteral("remotes"), QStringLiteral("--columns=name,url")}, 25000);
        enqueueRead(QStringLiteral("installed:") + scope,
                    {scoped(scope), QStringLiteral("list"), QStringLiteral("--app"),
                     QStringLiteral("--columns=application,name,version,origin,branch")}, 30000);
        enqueueRead(QStringLiteral("updates:") + scope,
                    {scoped(scope), QStringLiteral("remote-ls"), QStringLiteral("--updates"),
                     QStringLiteral("--columns=application,name,version,origin")}, 45000);
    }
    m_message = QStringLiteral("Flatpak kaynakları ve kurulu uygulamalar okunuyor...");
    // The inventory is incomplete until all read commands finish. Do not
    // signal a stable new revision for transiently cleared lists.
    emit changed();
    runNext();
}
void FlatpakManager::loadCatalog(const QString &scope, const QString &remote) {
    if (!safeScope(scope) || !safeRemote(remote) || m_busy || m_loading || m_process) return;
    if (!hasRemote(scope, remote)) { fail(QStringLiteral("Seçilen Flatpak deposu bu kapsamda bulunamadı.")); return; }
    m_catalog.clear();
    m_loading = true;
    enqueueRead(QStringLiteral("catalog:") + scope + QLatin1Char(':') + remote,
                {scoped(scope), QStringLiteral("remote-ls"), QStringLiteral("--app"),
                 QStringLiteral("--columns=application,name,version,branch"), remote}, 90000);
    m_message = QStringLiteral("Flatpak kataloğu yükleniyor...");
    emit changed();
    runNext();
}
bool FlatpakManager::hasRemote(const QString &scope, const QString &remote) const {
    for (const QVariant &v : m_sources) {
        const auto row = v.toMap();
        if (row.value(QStringLiteral("scope")) == scope && row.value(QStringLiteral("name")) == remote)
            return true;
    }
    return false;
}
bool FlatpakManager::isCatalogApp(const QString &scope, const QString &remote, const QString &id) const {
    if (!safeScope(scope) || !safeRemote(remote) || !safeAppId(id)) return false;
    for (const QVariant &v : m_catalog) {
        const auto row = v.toMap();
        if (row.value(QStringLiteral("scope")) == scope
            && row.value(QStringLiteral("origin")) == remote
            && row.value(QStringLiteral("id")) == id) return true;
    }
    return false;
}
bool FlatpakManager::isInstalled(const QString &scope, const QString &id) const {
    for (const QVariant &v : m_installed) {
        const auto row = v.toMap();
        if (row.value(QStringLiteral("scope")) == scope && row.value(QStringLiteral("id")) == id) return true;
    }
    return false;
}
bool FlatpakManager::hasUpdate(const QString &scope, const QString &id) const {
    for (const QVariant &v : m_updates) {
        const auto row = v.toMap();
        if (row.value(QStringLiteral("scope")) == scope && row.value(QStringLiteral("id")) == id) return true;
    }
    return false;
}
bool FlatpakManager::startMutation(const QString &kind, const QString &scope,
                                   const QString &id, const QStringList &args) {
    if (!m_available || !safeScope(scope) || m_busy || m_loading || m_process || m_outcomeUnknown
        || !m_queue.isEmpty() || QFile::exists(journalPath())) { fail(QStringLiteral("İşlem başlatılamadı: önceki işlem veya güvenlik kilidi var.")); return false; }
    if (!acquireLock()) { fail(QStringLiteral("RPM işlemi etkin veya güvenlik kilidi var; Flatpak işlemi engellendi.")); return false; }
    // The journal may have been created by another Ro-Store instance while
    // this one was waiting for the cross-process lock. Recheck under lock
    // before writing: never overwrite another operation's recovery record.
    if (QFile::exists(journalPath())) {
        releaseLock();
        fail(QStringLiteral("Başka bir Flatpak işleminin güvenlik kaydı bulundu; işlem engellendi."));
        m_outcomeUnknown = true;
        emit changed();
        return false;
    }
    // Resolve the pre-update commit BEFORE persisting a mutation journal.
    // No Flatpak mutation has started yet: failure must not create a
    // confusing journal or silently discard an unremovable journal.
    const QString beforeCommit = kind == QStringLiteral("update")
        ? installedCommit(scope, id) : QString();
    if (kind == QStringLiteral("update") && beforeCommit.isEmpty()) {
        releaseLock();
        fail(QStringLiteral("Mevcut uygulama commit kimliği doğrulanamadı; güncelleme başlatılmadı."));
        return false;
    }
    if (!writeJournal(kind, scope, id)) {
        releaseLock();
        fail(QStringLiteral("Kalıcı Flatpak güvenlik kaydı yazılamadı; işlem başlatılmadı."));
        return false;
    }
    m_busy = true;
    m_verifying = false;
    m_pendingKind = kind;
    m_pendingScope = scope;
    m_pendingId = id;
    m_pendingBeforeCommit = beforeCommit;
    m_operationName = kind + QStringLiteral(" · ") + id;
    m_operationStatus = QStringLiteral("Çalışıyor");
    m_operationStage = QStringLiteral("İşlem başlatıldı");
    m_operationProgress = 0;
    m_progressAvailable = false;
    m_recoveryChecked = false;
    m_outcomeUnknown = true;
    m_operationLog.clear();
    m_message = QStringLiteral("Flatpak işlemi yürütülüyor. Uygulamayı kapatmayın.");
    emit changed();
    runTask(Task{kind, args, 0, true});
    return true;
}
void FlatpakManager::install(const QString &scope, const QString &remote, const QString &id) {
    if (!safeScope(scope) || !safeRemote(remote) || !safeAppId(id) || !hasRemote(scope, remote) || !isCatalogApp(scope, remote, id)) {
        fail(QStringLiteral("Geçersiz kapsam, uygulama kimliği veya depo.")); return;
    }
    if (isInstalled(scope, id)) {
        fail(QStringLiteral("Uygulama bu kapsamda zaten kurulu. Yeniden kurma işlemi başlatılmadı.")); return;
    }
    startMutation(QStringLiteral("install"), scope, id,
                  {scoped(scope), QStringLiteral("install"), QStringLiteral("--noninteractive"),
                   QStringLiteral("--assumeyes"), remote, id});
}
void FlatpakManager::uninstall(const QString &scope, const QString &id) {
    if (!safeScope(scope) || !safeAppId(id) || !isInstalled(scope, id)) {
        fail(QStringLiteral("Seçilen uygulama bu kapsamda kurulu değil.")); return;
    }
    startMutation(QStringLiteral("uninstall"), scope, id,
                  {scoped(scope), QStringLiteral("uninstall"), QStringLiteral("--noninteractive"),
                   QStringLiteral("--assumeyes"), id});
}
void FlatpakManager::update(const QString &scope, const QString &id) {
    // remote-ls --updates also reports runtime/driver extensions (NVIDIA, VA-API).
    // v0.7 only permits upgrading apps confirmed in the --app installation list.
    // Runtime and driver updates remain read-only until separately reviewed.
    if (!safeScope(scope) || !safeAppId(id) || !isInstalled(scope, id) || !hasUpdate(scope, id)) {
        fail(QStringLiteral("Bu güncelleme kurulu bir uygulama olarak doğrulanamadı. Sürücü/çalışma ortamı güncellemeleri v0.7 test aşamasında engellenmiştir."));
        return;
    }
    startMutation(QStringLiteral("update"), scope, id,
                  {scoped(scope), QStringLiteral("update"), QStringLiteral("--noninteractive"),
                   QStringLiteral("--assumeyes"), id});
}
void FlatpakManager::addFlathubUser() {
    if (hasRemote(QStringLiteral("user"), QStringLiteral("flathub"))) {
        fail(QStringLiteral("Flathub kullanıcı deposu zaten kurulu.")); return;
    }
    // flatpak remote-add does not accept --noninteractive. Use the official
    // .flatpakrepo descriptor (which includes the GPG key); never turn off
    // repository signature verification.
    startMutation(QStringLiteral("add-flathub"), QStringLiteral("user"), QStringLiteral("flathub"),
                  {QStringLiteral("--user"), QStringLiteral("remote-add"),
                   QStringLiteral("--if-not-exists"),
                   QStringLiteral("flathub"), QStringLiteral("https://dl.flathub.org/repo/flathub.flatpakrepo")});
}
void FlatpakManager::launch(const QString &scope, const QString &id) {
    if (!safeScope(scope) || !safeAppId(id) || !isInstalled(scope,id) || m_busy) return;
    // Launch is user-initiated, detached, and does not run through a shell.
    const bool ok = QProcess::startDetached(QStringLiteral("/usr/bin/flatpak"),
                                {scoped(scope), QStringLiteral("run"), id});
    if (!ok) fail(QStringLiteral("Uygulama başlatılamadı."));
}
void FlatpakManager::runNext() {
    if (m_process || m_busy) return;
    if (m_queue.isEmpty()) { completeReads(); return; }
    runTask(m_queue.dequeue());
}
void FlatpakManager::runTask(const Task &task) {
    m_current = task;
    m_capture.clear();
    m_truncated = false;
    auto *process = new QProcess(this);
    m_process = process;
    process->setProgram(QStringLiteral("/usr/bin/flatpak"));
    process->setArguments(task.args);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() { consumeOutput(process); });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finishTask(process, -1, QProcess::CrashExit);
        }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process](int code, QProcess::ExitStatus status) {
                finishTask(process, code, status);
            });
    if (!task.mutation && task.timeoutMs > 0) {
        auto *timer = new QTimer(process);
        m_timer = timer;
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, process, [process]() {
            if (process->state() != QProcess::NotRunning) process->kill();
        });
        timer->start(task.timeoutMs);
    }
    process->start();
}
void FlatpakManager::consumeOutput(QProcess *process) {
    if (!process || process != m_process) return;
    QByteArray chunk = process->readAllStandardOutput();
    const auto remaining = MaxCapture - m_capture.size();
    if (remaining > 0) m_capture.append(chunk.left(remaining));
    if (chunk.size() > remaining) m_truncated = true;
    if (m_current.mutation) {
        // Preserve the bounded live transaction log. clean() is intended for
        // short labels and truncates at 1024 characters, so do not use it here.
        m_operationLog = QString::fromUtf8(m_capture.right(12000));
        m_operationLog.remove(QRegularExpression(QStringLiteral("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F]")));
        const QString normalized = QString::fromUtf8(m_capture.right(4096));
        static const QRegularExpression percent(QStringLiteral("(?:^|[^0-9])([0-9]{1,3})%"));
        auto it = percent.globalMatch(normalized);
        while (it.hasNext()) {
            const auto match = it.next();
            const int value = match.captured(1).toInt();
            if (value <= 100) { m_progressAvailable = true; m_operationProgress = value; }
        }
        const QStringList segments = normalized.split(QRegularExpression(QStringLiteral("[\\r\\n]")), Qt::SkipEmptyParts);
        if (!segments.isEmpty()) m_operationStage = clean(segments.last()).left(160);
        emit changed();
    }
}
void FlatpakManager::finishTask(QProcess *process, int code, QProcess::ExitStatus status) {
    if (!process || process != m_process) return;
    consumeOutput(process);
    if (m_timer) m_timer->stop();
    m_timer.clear();
    const Task completed = m_current;
    const QByteArray output = m_capture;
    const bool successful = status == QProcess::NormalExit && code == 0 && !m_truncated;
    m_process.clear();
    process->deleteLater();
    if (completed.mutation) {
        m_busy = false;
        m_verifying = successful;
        m_operationStatus = successful ? QStringLiteral("Doğrulanıyor") : QStringLiteral("Başarısız / sonuç belirsiz");
        appendOperationLog(successful ? QStringLiteral("Flatpak komutu başarıyla tamamlandı; hedef durum doğrulanıyor.") : QStringLiteral("Flatpak komutu başarısız veya çıktı belirsiz; güvenlik kaydı korundu."));
        // Always re-check actual installed/source state before clearing the journal.
        m_message = successful
            ? QStringLiteral("Flatpak komutu tamamlandı; sonuç doğrulaması gerekiyor.")
            : QStringLiteral("Flatpak işlem sonucu belirsiz veya başarısız. Güvenlik kilidi korundu.");
        m_outcomeUnknown = true;
        releaseLock();
        emit changed();
        refresh();
        return;
    }
    if (successful) {
        m_successfulReadKinds.insert(completed.kind);
        parseResponse(completed.kind, output);
    } else {
        m_recoveryReadFailed = true;
        m_recoveryChecked = false;
        m_message = QStringLiteral("Flatpak sorgusu başarısız: ") + clean(QString::fromUtf8(output.right(500)));
    }
    emit changed();
    runNext();
}
void FlatpakManager::parseResponse(const QString &kind, const QByteArray &output) {
    const QStringList parts = kind.split(QLatin1Char(':'));
    const QString section = parts.value(0);
    const QString scope = parts.value(1);
    const QString remote = parts.value(2);
    QStringList keys;
    if (section == QStringLiteral("remotes")) keys = {QStringLiteral("name"), QStringLiteral("url")};
    if (section == QStringLiteral("installed")) keys = {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("version"), QStringLiteral("origin"), QStringLiteral("branch")};
    if (section == QStringLiteral("updates")) keys = {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("version"), QStringLiteral("origin")};
    if (section == QStringLiteral("catalog")) keys = {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("version"), QStringLiteral("branch")};
    for (const QString &line : lines(output)) {
        const auto columns = line.split(QLatin1Char('\t'));
        if (keys.isEmpty() || columns.size() != keys.size()) continue;
        QVariantMap row = fields(columns, keys, scope);
        if (section == QStringLiteral("catalog")) {
            row.insert(QStringLiteral("origin"), remote);
            if (safeAppId(row.value(QStringLiteral("id")).toString())) m_catalog.append(row);
        } else if (section == QStringLiteral("remotes")) {
            if (safeRemote(row.value(QStringLiteral("name")).toString())) m_sources.append(row);
        } else if (section == QStringLiteral("installed")) {
            if (safeAppId(row.value(QStringLiteral("id")).toString())) m_installed.append(row);
        } else if (section == QStringLiteral("updates")) {
            if (safeAppId(row.value(QStringLiteral("id")).toString())) m_updates.append(row);
        }
    }
}
void FlatpakManager::completeReads() {
    m_loading = false;
    if (m_verifying) {
        m_verifying = false;
        if (verifiedTargetState()) {
            if (QFile::remove(journalPath())) {
                m_outcomeUnknown = false;
                m_recoveryChecked = false;
                m_operationStatus = QStringLiteral("Tamamlandı");
                m_operationStage = QStringLiteral("Gerçek Flatpak durumu doğrulandı");
                m_progressAvailable = true;
                m_operationProgress = 100;
                appendOperationLog(QStringLiteral("Doğrulama başarılı. Güvenlik kaydı otomatik kapatıldı."));
            } else {
                m_operationStatus = QStringLiteral("Doğrulandı, kayıt temizlenemedi");
                appendOperationLog(QStringLiteral("Güvenlik kaydı silinemedi; manuel inceleme gerekiyor."));
            }
        } else {
            m_operationStatus = QStringLiteral("Doğrulanamadı");
            appendOperationLog(QStringLiteral("Hedef durum doğrulanamadı; güvenlik kilidi korunuyor."));
        }
    }
    if (m_outcomeUnknown) {
        m_recoveryChecked = !m_recoveryReadFailed;
        m_message = m_recoveryReadFailed
            ? QStringLiteral("Flatpak sorgularından biri başarısız. Güvenlik kilidi korunuyor; tekrar deneyin.")
            : QStringLiteral("Flatpak bilgileri yüklendi. Önceki işlem sonucunu ayrıca kontrol edip güvenlik kaydını onaylayın.");
    } else {
        m_message = m_recoveryReadFailed
            ? QStringLiteral("Bazı Flatpak bilgileri okunamadı; listeler eksik olabilir. Yeniden deneyin.")
            : QStringLiteral("Flatpak bilgileri güncellendi.");
    }
    // This revision is consumed by QML install/update state bindings.
    // Without a new revision here, reinstalling leaves the button at "Kur"
    // even after the verified installed list is populated.
    ++m_inventoryRevision;
    emit changed();
    emit inventoryChanged();
}
bool FlatpakManager::verifiedTargetState() const
{
    if (m_pendingScope != QStringLiteral("user") && m_pendingScope != QStringLiteral("system"))
        return false;
    const QString installedRead = QStringLiteral("installed:") + m_pendingScope;
    const QString remoteRead = QStringLiteral("remotes:") + m_pendingScope;
    if (m_pendingKind == QStringLiteral("install"))
        return m_successfulReadKinds.contains(installedRead) && isInstalled(m_pendingScope, m_pendingId);
    if (m_pendingKind == QStringLiteral("uninstall"))
        return m_successfulReadKinds.contains(installedRead) && !isInstalled(m_pendingScope, m_pendingId);
    if (m_pendingKind == QStringLiteral("add-flathub"))
        return m_pendingScope == QStringLiteral("user") && m_successfulReadKinds.contains(remoteRead)
               && hasRemote(QStringLiteral("user"), QStringLiteral("flathub"));
    if (m_pendingKind == QStringLiteral("update")) {
        if (!m_successfulReadKinds.contains(installedRead) || !isInstalled(m_pendingScope, m_pendingId)
            || m_pendingBeforeCommit.isEmpty()) return false;
        const QString after = installedCommit(m_pendingScope, m_pendingId);
        return !after.isEmpty() && after != m_pendingBeforeCommit;
    }
    return false;
}
QString FlatpakManager::installedCommit(const QString &scope, const QString &appId) const
{
    if (!safeScope(scope) || !safeAppId(appId)) return {};
    QProcess check;
    check.setProgram(QStringLiteral("/usr/bin/flatpak"));
    check.setArguments({scoped(scope), QStringLiteral("info"), QStringLiteral("--show-commit"), appId});
    check.setStandardInputFile(QProcess::nullDevice());
    check.start();
    if (!check.waitForStarted(2000)) return {};
    if (!check.waitForFinished(6000)) {
        check.kill();
        check.waitForFinished(1000);
        return {};
    }
    if (check.exitStatus() != QProcess::NormalExit || check.exitCode() != 0) return {};
    const QString hash = QString::fromUtf8(check.readAllStandardOutput()).trimmed();
    static const QRegularExpression validHash(QStringLiteral("^[0-9a-fA-F]{40,64}$"));
    return validHash.match(hash).hasMatch() ? hash : QString();
}
void FlatpakManager::appendOperationLog(const QString &line)
{
    if (!m_operationLog.isEmpty()) m_operationLog.append(QLatin1Char('\n'));
    m_operationLog.append(line);
    if (m_operationLog.size() > 12000) m_operationLog = m_operationLog.right(12000);
}
void FlatpakManager::checkRecovery() {
    if (m_busy || m_loading || m_process || !m_outcomeUnknown) return;
    // Verification is user-assisted: a successful read does NOT prove the requested
    // mutation succeeded. The user must inspect the actual lists and operation log.
    refresh();
}
void FlatpakManager::acknowledgeRecovery() {
    if (!m_outcomeUnknown || !m_recoveryChecked || m_busy || m_loading || m_process) return;
    if (!QFile::remove(journalPath())) {
        fail(QStringLiteral("İşlem güvenlik kaydı silinemedi; kilit devam ediyor."));
        return;
    }
    m_outcomeUnknown = false;
    m_recoveryChecked = false;
    m_message = QStringLiteral("Kullanıcı onayıyla Flatpak güvenlik kilidi kaldırıldı.");
    emit changed();
}


QVariantMap FlatpakManager::metadata(const QString &appId) const
{
    return m_metadataCache.value(appId);
}

void FlatpakManager::requestMetadata(const QString &appId)
{
    if (!safeAppId(appId) || m_metadataRequested.contains(appId)
        || m_metadataRequested.size() >= 250)
        return;
    // Metadata is display-only, never trusted for a Flatpak command.
    m_metadataRequested.insert(appId);
    m_metadataCache.insert(appId, QVariantMap{{QStringLiteral("metadataStatus"), QStringLiteral("loading")}});
    ++m_metadataRevision;
    emit changed();
    m_metadataQueue.enqueue(appId);
    startNextMetadataRequest();
}

void FlatpakManager::startNextMetadataRequest()
{
    if (m_metadataActive >= 3 || m_metadataQueue.isEmpty())
        return;
    const QString id = m_metadataQueue.dequeue();
    ++m_metadataActive;
    const QUrl url(QStringLiteral("https://flathub.org/api/v2/appstream/")
                   + QString::fromLatin1(QUrl::toPercentEncoding(id)));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Ro-Store/0.7"));
    request.setTransferTimeout(12000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    auto *reply = m_metadataNetwork.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
        --m_metadataActive;
        const bool okay = reply->error() == QNetworkReply::NoError
                        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
                        && reply->url().scheme() == QStringLiteral("https")
                        && reply->bytesAvailable() <= 256 * 1024;
        const QByteArray content = okay ? reply->readAll() : QByteArray();
        QVariantMap meta;
        if (!content.isEmpty()) {
            const QJsonDocument document = QJsonDocument::fromJson(content);
            const QJsonObject obj = document.object();
            meta.insert(QStringLiteral("summary"), obj.value(QStringLiteral("summary")).toString().left(300));
            QString desc = obj.value(QStringLiteral("description")).toString();
            desc.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QStringLiteral(" "));
            desc.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
            desc.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
            desc.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
            desc.replace(QStringLiteral("&quot;"), QStringLiteral("\\\""));
            meta.insert(QStringLiteral("description"), desc.simplified().left(700));
            const QJsonValue icon = obj.value(QStringLiteral("icon"));
            QString iconUrl;
            if (icon.isString()) iconUrl = icon.toString();
            if (icon.isObject()) {
                const QJsonObject icons = icon.toObject();
                iconUrl = icons.value(QStringLiteral("128")).toString();
                if (iconUrl.isEmpty()) iconUrl = icons.value(QStringLiteral("64")).toString();
            }
            const QUrl iconLink(iconUrl);
            if (iconLink.scheme() == QStringLiteral("https")
                && iconLink.host() == QStringLiteral("dl.flathub.org"))
                meta.insert(QStringLiteral("iconUrl"), iconUrl);
            const QJsonArray screenshots = obj.value(QStringLiteral("screenshots")).toArray();
            QStringList images;
            auto usableImage = [](const QString &urlString) {
                const QUrl url(urlString);
                return url.isValid() && url.scheme() == QStringLiteral("https")
                       && !url.host().isEmpty() && url.userInfo().isEmpty()
                       && url.port(-1) == -1;
            };
            for (const QJsonValue &value : screenshots) {
                if (images.size() >= 4) break;
                QStringList candidates;
                if (value.isString()) {
                    candidates.append(value.toString());
                } else if (value.isObject()) {
                    const QJsonObject shot = value.toObject();
                    // Flathub API v2 uses screenshots[].sizes[] with {width,height,src}.
                    const QJsonValue sizesValue = shot.value(QStringLiteral("sizes"));
                    if (sizesValue.isArray()) {
                        const QJsonArray sizes = sizesValue.toArray();
                        for (const QJsonValue &sizeValue : sizes) {
                            const QJsonObject variant = sizeValue.toObject();
                            candidates.append(variant.value(QStringLiteral("src")).toString());
                        }
                    } else if (sizesValue.isObject()) {
                        const QJsonObject sizes = sizesValue.toObject();
                        candidates.append(sizes.value(QStringLiteral("624x351")).toString());
                        candidates.append(sizes.value(QStringLiteral("1248x702")).toString());
                    }
                    for (const QString &key : {QStringLiteral("624x351"), QStringLiteral("1248x702"),
                                               QStringLiteral("imgDesktopUrl"), QStringLiteral("url"),
                                               QStringLiteral("src"), QStringLiteral("source")}) {
                        candidates.append(shot.value(key).toString());
                    }
                }
                for (const QString &candidate : candidates) {
                    if (usableImage(candidate) && !images.contains(candidate)) {
                        images.append(candidate);
                        break;
                    }
                }
            }
            meta.insert(QStringLiteral("screenshots"), images);
        }
        meta.insert(QStringLiteral("metadataStatus"), content.isEmpty() ? QStringLiteral("error") : QStringLiteral("ready"));
        m_metadataCache.insert(id, meta);
        ++m_metadataRevision;
        emit changed();
        reply->deleteLater();
        startNextMetadataRequest();
    });
    startNextMetadataRequest();
}
