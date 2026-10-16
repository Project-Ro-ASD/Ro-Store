#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QTimer>
#include <memory>

// Persistent user-owned staging area. This component does NOT download,
// install, or assume any RPM file is safe to execute.
class SystemUpdateCache : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path CONSTANT)
    Q_PROPERTY(int rpmCount READ rpmCount NOTIFY changed)
    Q_PROPERTY(qulonglong bytes READ bytes NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY changed)
    Q_PROPERTY(int maximumBatch READ maximumBatch CONSTANT)
    Q_PROPERTY(bool offlineCheckRunning READ offlineCheckRunning NOTIFY changed)
    Q_PROPERTY(QString offlineStatus READ offlineStatus NOTIFY changed)
    Q_PROPERTY(QString downloadStatus READ downloadStatus NOTIFY changed)
    Q_PROPERTY(QString downloadLog READ downloadLog NOTIFY changed)
    Q_PROPERTY(QString downloadFile READ downloadFile NOTIFY changed)
    Q_PROPERTY(int downloadPercent READ downloadPercent NOTIFY changed)
    Q_PROPERTY(qulonglong stagedBytes READ stagedBytes NOTIFY changed)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY changed)
    Q_PROPERTY(bool hasDownloadResult READ hasDownloadResult NOTIFY changed)
    Q_PROPERTY(int lastNewCount READ lastNewCount NOTIFY changed)
    Q_PROPERTY(int lastReusedCount READ lastReusedCount NOTIFY changed)
    Q_PROPERTY(int lastRejectedCount READ lastRejectedCount NOTIFY changed)
public:
    explicit SystemUpdateCache(QObject *parent = nullptr);
    ~SystemUpdateCache() override;
    QString path() const;
    int rpmCount() const { return m_count; }
    qulonglong bytes() const { return m_bytes; }
    QString error() const { return m_error; }
    bool downloading() const { return m_downloading; }
    int maximumBatch() const { return 12; }
    bool offlineCheckRunning() const { return m_offlineCheckRunning; }
    QString offlineStatus() const { return m_offlineStatus; }
    Q_INVOKABLE void checkOfflineStatus();
    Q_INVOKABLE int countCached(const QStringList &nevras) const;
    Q_INVOKABLE qulonglong cachedBytes(const QStringList &nevras) const;
    QString downloadStatus() const { return m_downloadStatus; }
    QString downloadLog() const { return m_downloadLog; }
    QString downloadFile() const { return m_downloadFile; }
    int downloadPercent() const { return m_downloadPercent; }
    qulonglong stagedBytes() const { return m_stagedBytes; }
    int elapsedSeconds() const { return m_elapsedSeconds; }
    bool hasDownloadResult() const { return m_hasDownloadResult; }
    int lastNewCount() const { return m_lastNewCount; }
    int lastReusedCount() const { return m_lastReusedCount; }
    int lastRejectedCount() const { return m_lastRejectedCount; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void downloadSelected(const QStringList &nevras);
    Q_INVOKABLE void cancelDownload();

signals:
    void changed();

private:
    void completeDownload(bool cancelled, int exitCode);
    void appendDownloadOutput(const QByteArray &bytes);
    void pollStagingProgress();
    QString m_root;
    QPointer<QProcess> m_downloadProcess;
    std::unique_ptr<QTemporaryDir> m_staging;
    QStringList m_requested;
    QString m_downloadStatus;
    QString m_downloadLog;
    QString m_downloadFile;
    QString m_pendingOutput;
    int m_downloadPercent = -1; // Per-file percent, not total transaction percent.
    qulonglong m_stagedBytes = 0; // Files currently present in staging; not total network bytes.
    int m_elapsedSeconds = 0;
    bool m_seenPackageDownloads = false;
    bool m_hasDownloadResult = false;
    int m_reusedThisRun = 0;
    int m_lastNewCount = 0;
    int m_lastReusedCount = 0;
    int m_lastRejectedCount = 0;
    bool m_downloading = false;
    bool m_offlineCheckRunning = false;
    QString m_offlineStatus;
    QPointer<QProcess> m_offlineQuery;
    QElapsedTimer m_elapsedTimer;
    QTimer m_progressTimer;
    int m_count = 0;
    qulonglong m_bytes = 0;
    QString m_error;
};
