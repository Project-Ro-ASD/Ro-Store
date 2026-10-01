#include "SourceManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QStringList>

namespace {

QString cleanOsReleaseValue(QString value)
{
    value = value.trimmed();

    if (value.size() >= 2) {
        const bool doubleQuoted =
            value.startsWith('"')
            && value.endsWith('"');

        const bool singleQuoted =
            value.startsWith('\'')
            && value.endsWith('\'');

        if (doubleQuoted || singleQuoted) {
            value = value.mid(
                1,
                value.size() - 2
            );
        }
    }

    return value;
}

}

SourceManager::SourceManager(QObject *parent)
    : QObject(parent)
{
}

bool SourceManager::checking() const
{
    return m_checking;
}

bool SourceManager::supportedSystem() const
{
    return m_supportedSystem;
}

QString SourceManager::distroId() const
{
    return m_distroId;
}

QString SourceManager::distroVersion() const
{
    return m_distroVersion;
}

QString SourceManager::architecture() const
{
    return m_architecture;
}

SourceManager::RepositoryState
SourceManager::roAsdState() const
{
    return m_roAsdState;
}

QString SourceManager::roAsdStatusText() const
{
    return m_roAsdStatusText;
}

void SourceManager::refresh()
{
    setChecking(true);

    detectSystem();

    if (!m_supportedSystem) {
        setRoAsdState(
            Unsupported,
            QStringLiteral(
                "Bu sistem Ro-ASD RPM deposu için "
                "henüz desteklenmiyor."
            )
        );

        setChecking(false);
        return;
    }

    detectRoAsdRepository();

    setChecking(false);
}

void SourceManager::detectSystem()
{
    const QString newDistroId =
        readOsReleaseValue(
            QStringLiteral("ID")
        ).toLower();

    const QString newVersion =
        readOsReleaseValue(
            QStringLiteral("VERSION_ID")
        );

    const QString idLike =
        readOsReleaseValue(
            QStringLiteral("ID_LIKE")
        ).toLower();

    const QString newArchitecture =
        QSysInfo::currentCpuArchitecture();

    const QStringList idLikeParts =
        idLike.split(
            ' ',
            Qt::SkipEmptyParts
        );

    const bool fedoraLike =
        newDistroId == QStringLiteral("fedora")
        || newDistroId.startsWith(
            QStringLiteral("ro-asd")
        )
        || idLikeParts.contains(
            QStringLiteral("fedora")
        );

    const bool supportedVersion =
        newVersion == QStringLiteral("44")
        || newVersion.startsWith(
            QStringLiteral("44.")
        );

    const bool supportedArchitecture =
        newArchitecture == QStringLiteral("x86_64")
        || newArchitecture == QStringLiteral("aarch64");

    const bool newSupportedSystem =
        fedoraLike
        && supportedVersion
        && supportedArchitecture;

    const bool changed =
        m_distroId != newDistroId
        || m_distroVersion != newVersion
        || m_architecture != newArchitecture
        || m_supportedSystem != newSupportedSystem;

    m_distroId = newDistroId;
    m_distroVersion = newVersion;
    m_architecture = newArchitecture;
    m_supportedSystem = newSupportedSystem;

    if (changed) {
        emit systemChanged();
    }

    qInfo()
        << "SOURCE MANAGER SYSTEM:"
        << "id:" << m_distroId
        << "version:" << m_distroVersion
        << "arch:" << m_architecture
        << "supported:" << m_supportedSystem;
}

void SourceManager::detectRoAsdRepository()
{
    const QDir repoDirectory(
        QStringLiteral("/etc/yum.repos.d")
    );

    const QStringList repoFiles =
        repoDirectory.entryList(
            {
                QStringLiteral("*.repo")
            },
            QDir::Files
        );

    bool repoFound = false;
    bool repoEnabled = false;

    for (const QString &fileName : repoFiles) {
        QFile file(
            repoDirectory.filePath(fileName)
        );

        if (!file.open(
                QIODevice::ReadOnly
                | QIODevice::Text
            )) {
            continue;
        }

        bool insideTargetRepo = false;

        while (!file.atEnd()) {
            const QString line =
                QString::fromUtf8(
                    file.readLine()
                ).trimmed();

            if (line.isEmpty()
                || line.startsWith('#')
                || line.startsWith(';')) {
                continue;
            }

            if (line.startsWith('[')
                && line.endsWith(']')) {

                const QString section =
                    line.mid(
                        1,
                        line.size() - 2
                    ).trimmed();

                insideTargetRepo =
                    section.compare(
                        QStringLiteral(
                            "ro-asd-beta"
                        ),
                        Qt::CaseInsensitive
                    ) == 0;

                if (insideTargetRepo) {
                    repoFound = true;

                    // DNF'de enabled belirtilmemişse
                    // repository varsayılan olarak etkin
                    // kabul edilir.
                    repoEnabled = true;
                }

                continue;
            }

            if (!insideTargetRepo) {
                continue;
            }

            const qsizetype separator =
                line.indexOf('=');

            if (separator < 0) {
                continue;
            }

            const QString key =
                line.left(separator)
                    .trimmed()
                    .toLower();

            const QString value =
                line.mid(separator + 1)
                    .trimmed()
                    .toLower();

            if (key == QStringLiteral("enabled")) {
                repoEnabled =
                    value != QStringLiteral("0")
                    && value != QStringLiteral("false")
                    && value != QStringLiteral("no");
            }
        }

        if (repoFound) {
            break;
        }
    }

    const bool rpmKeyExists =
        QFileInfo::exists(
            QStringLiteral(
                "/etc/pki/rpm-gpg/"
                "RPM-GPG-KEY-ro-asd"
            )
        );

    const bool metadataKeyExists =
        QFileInfo::exists(
            QStringLiteral(
                "/etc/pki/rpm-gpg/"
                "REPODATA-GPG-KEY-ro-asd"
            )
        );

    if (!repoFound
        || !rpmKeyExists
        || !metadataKeyExists) {

        setRoAsdState(
            Missing,
            QStringLiteral(
                "Ro-ASD uygulama deposu "
                "sistemde kurulu değil."
            )
        );

        return;
    }

    if (!repoEnabled) {
        setRoAsdState(
            Disabled,
            QStringLiteral(
                "Ro-ASD uygulama deposu "
                "kurulu ancak devre dışı."
            )
        );

        return;
    }

    setRoAsdState(
        Ready,
        QStringLiteral(
            "Ro-ASD uygulama deposu hazır."
        )
    );
}

void SourceManager::setChecking(bool value)
{
    if (m_checking == value) {
        return;
    }

    m_checking = value;
    emit checkingChanged();
}

void SourceManager::setRoAsdState(
    RepositoryState state,
    const QString &statusText
)
{
    if (m_roAsdState == state
        && m_roAsdStatusText == statusText) {
        return;
    }

    m_roAsdState = state;
    m_roAsdStatusText = statusText;

    emit roAsdStateChanged();

    qInfo()
        << "SOURCE MANAGER RO-ASD:"
        << repositoryStateName(state)
        << "-"
        << statusText;
}

QString SourceManager::readOsReleaseValue(
    const QString &key
)
{
    QFile file(
        QStringLiteral("/etc/os-release")
    );

    if (!file.open(
            QIODevice::ReadOnly
            | QIODevice::Text
        )) {
        return {};
    }

    while (!file.atEnd()) {
        const QString line =
            QString::fromUtf8(
                file.readLine()
            ).trimmed();

        if (line.isEmpty()
            || line.startsWith('#')) {
            continue;
        }

        const qsizetype separator =
            line.indexOf('=');

        if (separator < 0) {
            continue;
        }

        const QString currentKey =
            line.left(separator).trimmed();

        if (currentKey != key) {
            continue;
        }

        return cleanOsReleaseValue(
            line.mid(separator + 1)
        );
    }

    return {};
}

QString SourceManager::repositoryStateName(
    RepositoryState state
)
{
    switch (state) {
    case Unknown:
        return QStringLiteral("Unknown");

    case Missing:
        return QStringLiteral("Missing");

    case Disabled:
        return QStringLiteral("Disabled");

    case Ready:
        return QStringLiteral("Ready");

    case Unavailable:
        return QStringLiteral("Unavailable");

    case Unsupported:
        return QStringLiteral("Unsupported");
    }

    return QStringLiteral("Unknown");
}
