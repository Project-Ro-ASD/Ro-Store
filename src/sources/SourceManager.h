#pragma once

#include <QObject>
#include <QString>

class QProcess;

class SourceManager : public QObject
{
    Q_OBJECT

public:
    enum RepositoryState {
        Unknown,
        Missing,
        Disabled,
        Ready,
        Unavailable,
        InvalidConfiguration,
        Unsupported
    };
    Q_ENUM(RepositoryState)

    Q_PROPERTY(
        bool checking
        READ checking
        NOTIFY checkingChanged
    )

    Q_PROPERTY(
        bool supportedSystem
        READ supportedSystem
        NOTIFY systemChanged
    )

    Q_PROPERTY(
        QString distroId
        READ distroId
        NOTIFY systemChanged
    )

    Q_PROPERTY(
        QString distroVersion
        READ distroVersion
        NOTIFY systemChanged
    )

    Q_PROPERTY(
        QString architecture
        READ architecture
        NOTIFY systemChanged
    )

    Q_PROPERTY(
        RepositoryState roAsdState
        READ roAsdState
        NOTIFY roAsdStateChanged
    )

    Q_PROPERTY(
        QString roAsdStatusText
        READ roAsdStatusText
        NOTIFY roAsdStateChanged
    )

    Q_PROPERTY(
        bool roAsdReady
        READ roAsdReady
        NOTIFY roAsdStateChanged
    )

    Q_PROPERTY(
        bool roAsdActionAvailable
        READ roAsdActionAvailable
        NOTIFY roAsdStateChanged
    )

    Q_PROPERTY(
        QString roAsdActionText
        READ roAsdActionText
        NOTIFY roAsdStateChanged
    )

    Q_PROPERTY(
        bool repositoryActionRunning
        READ repositoryActionRunning
        NOTIFY repositoryActionRunningChanged
    )

    Q_PROPERTY(
        QString repositoryActionError
        READ repositoryActionError
        NOTIFY repositoryActionErrorChanged
    )

public:
    explicit SourceManager(QObject *parent = nullptr);

    bool checking() const;
    bool supportedSystem() const;

    QString distroId() const;
    QString distroVersion() const;
    QString architecture() const;

    RepositoryState roAsdState() const;
    QString roAsdStatusText() const;

    bool roAsdReady() const;
    bool roAsdActionAvailable() const;
    QString roAsdActionText() const;

    bool repositoryActionRunning() const;
    QString repositoryActionError() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void executeRoAsdAction();

signals:
    void checkingChanged();
    void systemChanged();
    void roAsdStateChanged();

    void repositoryActionRunningChanged();
    void repositoryActionErrorChanged();

private:
    void detectSystem();
    void detectRoAsdRepository();
    void queryRoAsdEffectiveState();

    void setChecking(bool value);

    void setRepositoryActionRunning(bool value);
    void setRepositoryActionError(
        const QString &message
    );

    void setRoAsdState(
        RepositoryState state,
        const QString &statusText
    );

    static QString readOsReleaseValue(
        const QString &key
    );

    static QString repositoryStateName(
        RepositoryState state
    );

private:
    bool m_checking = false;
    bool m_supportedSystem = false;

    QString m_distroId;
    QString m_distroVersion;
    QString m_architecture;

    RepositoryState m_roAsdState = Unknown;
    QString m_roAsdStatusText =
        QStringLiteral("Depo durumu kontrol edilmedi.");

    QProcess *m_repositoryProcess = nullptr;
    QProcess *m_repositoryStateProcess = nullptr;

    bool m_repositoryStateQueryPending = false;
    bool m_repositoryActionRunning = false;
    QString m_repositoryActionError;
};
