#pragma once
#include <QObject>
#include <QPointer>
#include <QHash>
#include <QProcess>
#include <QVariantList>
#include <QStringList>

class Dnf5Backend;

// One DNF5 solver goal for the entire selection. Never execute RPM changes
// in the running desktop; only prepare an offline transaction upon approval.
class SystemOfflineUpgrade : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* transactionManager READ transactionManager WRITE setTransactionManager NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool preparing READ preparing NOTIFY changed)
    Q_PROPERTY(bool checkingPreparation READ checkingPreparation NOTIFY changed)
    Q_PROPERTY(bool outcomeUnknown READ outcomeUnknown NOTIFY changed)
    Q_PROPERTY(bool recoveryVerified READ recoveryVerified NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString planText READ planText NOTIFY changed)
    Q_PROPERTY(int planCount READ planCount NOTIFY changed)
    Q_PROPERTY(int removalCount READ removalCount NOTIFY changed)
    Q_PROPERTY(bool kernelAffected READ kernelAffected NOTIFY changed)
    Q_PROPERTY(qulonglong planBytes READ planBytes NOTIFY changed)
    Q_PROPERTY(qulonglong downloadedBytes READ downloadedBytes NOTIFY changed)
    Q_PROPERTY(int progressPercent READ progressPercent NOTIFY changed)
    Q_PROPERTY(bool offlineReady READ offlineReady NOTIFY changed)
    Q_PROPERTY(bool rebootRequestRunning READ rebootRequestRunning NOTIFY changed)
    Q_PROPERTY(bool clearingPrepared READ clearingPrepared NOTIFY changed)
public:
    explicit SystemOfflineUpgrade(QObject* parent = nullptr);
    ~SystemOfflineUpgrade() override;
    QObject* transactionManager() const { return m_manager.data(); }
    void setTransactionManager(QObject* manager);
    bool busy() const { return m_busy; }
    bool ready() const { return m_ready; }
    bool preparing() const { return m_preparing; }
    bool checkingPreparation() const { return m_preflightRunning; }
    bool outcomeUnknown() const { return m_outcomeUnknown; }
    bool recoveryVerified() const { return m_recoveryVerified; }
    bool offlineReady() const { return m_offlineReady; }
    bool rebootRequestRunning() const { return m_rebootRequestRunning; }
    bool clearingPrepared() const { return m_clearingPrepared; }
    QString message() const { return m_message; }
    QString planText() const { return m_planText; }
    int planCount() const { return m_planCount; }
    int removalCount() const { return m_removalCount; }
    bool kernelAffected() const { return m_kernelAffected; }
    qulonglong planBytes() const { return m_planBytes; }
    qulonglong downloadedBytes() const { return m_downloadedBytes; }
    int progressPercent() const { return m_progressPercent; }
    Q_INVOKABLE void resolve(const QStringList &selectedNevras);
    Q_INVOKABLE void stageOffline();
    Q_INVOKABLE void abandonPlan();
    Q_INVOKABLE void requestCancellation();
    Q_INVOKABLE void recheckRecovery();
    Q_INVOKABLE void acknowledgeRecovery();
    Q_INVOKABLE void requestConfirmedReboot();
    Q_INVOKABLE void discardPreparedOffline();
    Q_INVOKABLE void invalidateIfSelectionChanged(const QStringList &selectedNevras);
signals:
    void changed();
private:
    void fail(const QString &message);
    void releaseLock();
    void verifyOfflineStatus();
    void beginOfflineStage();
    QString journalPath() const;
    bool writeJournal();
    QPointer<QObject> m_manager;
    Dnf5Backend* m_backend = nullptr;
    QStringList m_requested;
    QString m_message;
    QString m_planText;
    int m_planCount = 0;
    int m_removalCount = 0;
    bool m_kernelAffected = false;
    bool m_rebootRequestRunning = false;
    bool m_clearingPrepared = false;
    QPointer<QProcess> m_rebootProcess;
    qulonglong m_planBytes = 0;
    qulonglong m_downloadedBytes = 0;
    int m_progressPercent = 0;
    QHash<QString, qulonglong> m_downloadById;
    bool m_busy = false;
    bool m_ready = false;
    bool m_preparing = false;
    bool m_preflightRunning = false;
    bool m_outcomeUnknown = false;
    bool m_offlineReady = false;
    bool m_locked = false;
    bool m_recoveryVerified = false;
    bool m_selectionInvalidated = false;
};
