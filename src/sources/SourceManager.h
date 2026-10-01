#pragma once

#include <QObject>
#include <QString>

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

    Q_INVOKABLE void refresh();

signals:
    void checkingChanged();
    void systemChanged();
    void roAsdStateChanged();

private:
    void detectSystem();
    void detectRoAsdRepository();

    void setChecking(bool value);
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
};
