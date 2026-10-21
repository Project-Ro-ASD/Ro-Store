#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QQueue>
#include <QHash>
#include <QSet>
#include <QNetworkAccessManager>
#include <QLockFile>
#include <memory>
#include <QTimer>
#include <QVariantList>

class FlatpakManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* transactionManager READ transactionManager WRITE setTransactionManager NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool outcomeUnknown READ outcomeUnknown NOTIFY changed)
    Q_PROPERTY(bool recoveryChecked READ recoveryChecked NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString operationLog READ operationLog NOTIFY changed)
    Q_PROPERTY(QString operationName READ operationName NOTIFY changed)
    Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY changed)
    Q_PROPERTY(QString operationStage READ operationStage NOTIFY changed)
    Q_PROPERTY(bool verifying READ verifying NOTIFY changed)
    Q_PROPERTY(bool progressAvailable READ progressAvailable NOTIFY changed)
    Q_PROPERTY(int operationProgress READ operationProgress NOTIFY changed)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    Q_PROPERTY(QVariantList installed READ installed NOTIFY changed)
    Q_PROPERTY(QVariantList updates READ updates NOTIFY changed)
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY changed)
    Q_PROPERTY(int metadataRevision READ metadataRevision NOTIFY changed)
    Q_PROPERTY(int inventoryRevision READ inventoryRevision NOTIFY inventoryChanged)

public:
    explicit FlatpakManager(QObject *parent = nullptr);
    ~FlatpakManager() override;

    QObject* transactionManager() const { return m_transactionManager.data(); }
    void setTransactionManager(QObject *manager);
    bool available() const { return m_available; }
    bool loading() const { return m_loading; }
    bool busy() const { return m_busy; }
    bool outcomeUnknown() const { return m_outcomeUnknown; }
    bool recoveryChecked() const { return m_recoveryChecked; }
    QString message() const { return m_message; }
    QString operationLog() const { return m_operationLog; }
    QString operationName() const { return m_operationName; }
    QString operationStatus() const { return m_operationStatus; }
    QString operationStage() const { return m_operationStage; }
    bool verifying() const { return m_verifying; }
    bool progressAvailable() const { return m_progressAvailable; }
    int operationProgress() const { return m_operationProgress; }
    QVariantList sources() const { return m_sources; }
    QVariantList installed() const { return m_installed; }
    QVariantList updates() const { return m_updates; }
    QVariantList catalog() const { return m_catalog; }
    int metadataRevision() const { return m_metadataRevision; }
    int inventoryRevision() const { return m_inventoryRevision; }
    Q_INVOKABLE QVariantMap metadata(const QString &appId) const;
    Q_INVOKABLE void requestMetadata(const QString &appId);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadCatalog(const QString &scope, const QString &remote);
    Q_INVOKABLE bool hasRemote(const QString &scope, const QString &remote) const;
    Q_INVOKABLE bool isInstalled(const QString &scope, const QString &appId) const;
    Q_INVOKABLE bool hasUpdate(const QString &scope, const QString &appId) const;
    Q_INVOKABLE void install(const QString &scope, const QString &remote, const QString &appId);
    Q_INVOKABLE void uninstall(const QString &scope, const QString &appId);
    Q_INVOKABLE void update(const QString &scope, const QString &appId);
    Q_INVOKABLE void addFlathubUser();
    Q_INVOKABLE void launch(const QString &scope, const QString &appId);
    Q_INVOKABLE void checkRecovery();
    Q_INVOKABLE void acknowledgeRecovery();

signals:
    void changed();
    // Inventory updates only: artwork/metadata must not reset the app list.
    void inventoryChanged();

private:
    struct Task {
        QString kind;
        QStringList args;
        int timeoutMs = 20000;
        bool mutation = false;
    };

    static bool safeAppId(const QString &id);
    static bool safeRemote(const QString &remote);
    static bool safeScope(const QString &scope);
    static QStringList lines(const QByteArray &data);
    QString journalPath() const;
    bool writeJournal(const QString &action, const QString &scope, const QString &id);
    bool acquireLock();
    void releaseLock();
    void enqueueRead(const QString &kind, const QStringList &args, int timeoutMs);
    void runNext();
    void runTask(const Task &task);
    void finishTask(QProcess *process, int code, QProcess::ExitStatus status);
    void consumeOutput(QProcess *process);
    void parseResponse(const QString &kind, const QByteArray &output);
    bool startMutation(const QString &kind, const QString &scope,
                       const QString &id, const QStringList &args);
    void fail(const QString &message);
    void completeReads();
    bool verifiedTargetState() const;
    QString installedCommit(const QString &scope, const QString &appId) const;
    void appendOperationLog(const QString &line);
    void startNextMetadataRequest();

    QPointer<QObject> m_transactionManager;
    QPointer<QProcess> m_process;
    QPointer<QTimer> m_timer;
    QQueue<Task> m_queue;
    Task m_current;
    QByteArray m_capture;
    bool m_truncated = false;
    bool m_available = false;
    bool m_loading = false;
    bool m_busy = false;
    bool m_outcomeUnknown = false;
    bool m_recoveryChecked = false;
    bool m_recoveryReadFailed = false;
    QSet<QString> m_successfulReadKinds;
    bool m_locked = false;
    bool m_verifying = false;
    bool m_progressAvailable = false;
    int m_operationProgress = 0;
    QString m_operationName;
    QString m_operationStatus;
    QString m_operationStage;
    QString m_pendingKind;
    QString m_pendingScope;
    QString m_pendingId;
    QString m_pendingBeforeCommit;
    std::unique_ptr<QLockFile> m_crossProcessLock;
    QString m_message;
    QString m_operationLog;
    QVariantList m_sources;
    QVariantList m_installed;
    QVariantList m_updates;
    QVariantList m_catalog;
    QNetworkAccessManager m_metadataNetwork;
    QHash<QString, QVariantMap> m_metadataCache;
    QSet<QString> m_metadataRequested;
    QQueue<QString> m_metadataQueue;
    int m_metadataActive = 0;
    int m_metadataRevision = 0;
    int m_inventoryRevision = 0;
};
