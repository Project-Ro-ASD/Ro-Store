#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class PackageTransaction : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString packageName READ packageName CONSTANT)
    Q_PROPERTY(Operation operation READ operation CONSTANT)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(qulonglong downloadedBytes READ downloadedBytes NOTIFY downloadedBytesChanged)
    Q_PROPERTY(qulonglong totalBytes READ totalBytes NOTIFY totalBytesChanged)
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
        Cancelled
    };
    Q_ENUM(State)

    explicit PackageTransaction(
        const QString &packageName,
        Operation operation,
        QObject *parent = nullptr
    );

    QString packageName() const;
    Operation operation() const;
    State state() const;

    int progress() const;

    qulonglong downloadedBytes() const;
    qulonglong totalBytes() const;

    QString errorMessage() const;

    QVariantList resolvedItems() const;
    int resolvedItemCount() const;

    bool isFinished() const;

    void setState(State state);
    void setProgress(int progress);

    void setDownloadedBytes(qulonglong bytes);
    void setTotalBytes(qulonglong bytes);

    void setErrorMessage(const QString &message);
    void setResolvedItems(const QVariantList &items);

    void fail(const QString &message);

signals:
    void stateChanged();
    void progressChanged();
    void downloadedBytesChanged();
    void totalBytesChanged();
    void errorMessageChanged();
    void resolvedItemsChanged();

private:
    QString m_packageName;
    Operation m_operation;

    State m_state = State::Queued;

    int m_progress = 0;

    qulonglong m_downloadedBytes = 0;
    qulonglong m_totalBytes = 0;

    QString m_errorMessage;
    QVariantList m_resolvedItems;
};
