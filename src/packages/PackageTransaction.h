#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class PackageTransaction : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString packageName READ packageName CONSTANT)
    Q_PROPERTY(QString repositoryId READ repositoryId CONSTANT)
    Q_PROPERTY(QString technicalLog READ technicalLog NOTIFY technicalLogChanged)
    Q_PROPERTY(bool cancelRequested READ cancelRequested NOTIFY cancelRequestedChanged)
    Q_PROPERTY(Operation operation READ operation CONSTANT)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(qulonglong downloadedBytes READ downloadedBytes NOTIFY downloadedBytesChanged)
    Q_PROPERTY(qulonglong totalBytes READ totalBytes NOTIFY totalBytesChanged)
    Q_PROPERTY(qulonglong downloadSpeedBytesPerSecond READ downloadSpeedBytesPerSecond NOTIFY downloadSpeedBytesPerSecondChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QVariantList resolvedItems READ resolvedItems NOTIFY resolvedItemsChanged)
    Q_PROPERTY(int resolvedItemCount READ resolvedItemCount NOTIFY resolvedItemsChanged)

public:
    enum class Operation {
        Install,
        Remove,
        Upgrade
    };
    Q_ENUM(Operation)

    enum class State {
        Queued,
        Resolving,
        Ready,
        Downloading,
        Running,
        Finished,
        Failed,
        OutcomeUnknown,
        Cancelled
    };
    Q_ENUM(State)

    explicit PackageTransaction(
        const QString &packageName,
        Operation operation,
        const QString &repositoryId,
        QObject *parent = nullptr
    );

    QString packageName() const;
    QString repositoryId() const;
    QString technicalLog() const;
    void appendTechnicalLog(const QString &message);
    bool cancelRequested() const;
    void setCancelRequested(bool requested);
    Q_INVOKABLE void copyTechnicalLog(const QString &text) const;
    Operation operation() const;
    State state() const;

    int progress() const;

    qulonglong downloadedBytes() const;
    qulonglong totalBytes() const;
    qulonglong downloadSpeedBytesPerSecond() const;

    QString errorMessage() const;

    QVariantList resolvedItems() const;
    int resolvedItemCount() const;

    bool isFinished() const;

    void setState(State state);
    void setProgress(int progress);

    void setDownloadedBytes(qulonglong bytes);
    void setTotalBytes(qulonglong bytes);
    void setDownloadSpeedBytesPerSecond(qulonglong bytesPerSecond);

    void setErrorMessage(const QString &message);
    void setResolvedItems(const QVariantList &items);

    void fail(const QString &message);
    void markOutcomeUnknown(const QString &message);

signals:
    void stateChanged();
    void progressChanged();
    void downloadedBytesChanged();
    void totalBytesChanged();
    void downloadSpeedBytesPerSecondChanged();
    void errorMessageChanged();
    void technicalLogChanged();
    void cancelRequestedChanged();
    void resolvedItemsChanged();

private:
    QString m_packageName;
    QString m_repositoryId;
    QString m_technicalLog;
    bool m_cancelRequested = false;
    Operation m_operation;

    State m_state = State::Queued;

    int m_progress = 0;

    qulonglong m_downloadedBytes = 0;
    qulonglong m_totalBytes = 0;
    qulonglong m_downloadSpeedBytesPerSecond = 0;

    QString m_errorMessage;
    QVariantList m_resolvedItems;
};
