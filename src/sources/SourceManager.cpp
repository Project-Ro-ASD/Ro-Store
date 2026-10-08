#include "SourceManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
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
    : QObject(parent),
      m_repositoryProcess(new QProcess(this)),
      m_repositoryStateProcess(new QProcess(this))
{
    connect(
        m_repositoryProcess,
        &QProcess::finished,
        this,
        [this](
            int exitCode,
            QProcess::ExitStatus exitStatus
        ) {
            const QByteArray standardOutput =
                m_repositoryProcess
                    ->readAllStandardOutput();

            const QByteArray standardError =
                m_repositoryProcess
                    ->readAllStandardError();

            qInfo()
                << "SOURCE MANAGER REPOSITORY ACTION FINISHED:"
                << "exitCode:" << exitCode
                << "exitStatus:" << exitStatus;

            if (!standardOutput.isEmpty()) {
                qInfo().noquote()
                    << QString::fromUtf8(
                        standardOutput
                    ).trimmed();
            }

            if (exitStatus
                    == QProcess::NormalExit
                && exitCode == 0) {

                setRepositoryActionRunning(false);
                setRepositoryActionError({});

                refresh();
                return;
            }

            setRepositoryActionRunning(false);

            // pkexec: kullanıcı doğrulamayı iptal ettiğinde
            // 126/127 gibi başarısız dönüşler görülebilir.
            if (exitCode == 126
                || exitCode == 127) {

                setRepositoryActionError(
                    QStringLiteral(
                        "Yetkilendirme iptal edildi."
                    )
                );

                return;
            }

            QString error =
                QString::fromUtf8(
                    standardError
                ).trimmed();

            if (error.isEmpty()) {
                error = QStringLiteral(
                    "Ro-ASD deposu yapılandırılamadı."
                );
            }

            setRepositoryActionError(error);
        }
    );

    connect(
        m_repositoryProcess,
        &QProcess::errorOccurred,
        this,
        [this](QProcess::ProcessError error) {
            if (error
                != QProcess::FailedToStart) {
                return;
            }

            setRepositoryActionRunning(false);

            setRepositoryActionError(
                QStringLiteral(
                    "Yetkilendirme işlemi başlatılamadı."
                )
            );
        }
    );
    connect(
        m_repositoryStateProcess,
        &QProcess::finished,
        this,
        [this](
            int exitCode,
            QProcess::ExitStatus exitStatus
        ) {
            m_repositoryStateQueryPending = false;

            const QByteArray standardOutput =
                m_repositoryStateProcess
                    ->readAllStandardOutput();

            const QByteArray standardError =
                m_repositoryStateProcess
                    ->readAllStandardError();

            if (exitStatus != QProcess::NormalExit
                || exitCode != 0) {

                const QString errorText =
                    QString::fromUtf8(
                        standardError
                    ).trimmed();

                if (!errorText.isEmpty()) {
                    qWarning().noquote()
                        << "SOURCE MANAGER DNF5 REPO QUERY:"
                        << errorText;
                }

                setRoAsdState(
                    Unavailable,
                    QStringLiteral(
                        "Ro-ASD deposunun etkinlik durumu "
                        "DNF5 üzerinden kontrol edilemedi."
                    )
                );

                setChecking(false);
                return;
            }

            QJsonParseError parseError;

            const QJsonDocument document =
                QJsonDocument::fromJson(
                    standardOutput,
                    &parseError
                );

            if (parseError.error
                    != QJsonParseError::NoError) {

                qWarning()
                    << "SOURCE MANAGER DNF5 JSON ERROR:"
                    << parseError.errorString();

                setRoAsdState(
                    Unavailable,
                    QStringLiteral(
                        "DNF5 depo durumu geçerli bir "
                        "yanıt döndürmedi."
                    )
                );

                setChecking(false);
                return;
            }

            QJsonArray repositories;

            if (document.isArray()) {
                repositories = document.array();
            } else if (document.isObject()) {
                repositories.append(
                    document.object()
                );
            } else {
                setRoAsdState(
                    Unavailable,
                    QStringLiteral(
                        "DNF5 depo durumu okunamadı."
                    )
                );

                setChecking(false);
                return;
            }

            bool targetFound = false;

            for (const QJsonValue &value : repositories) {
                if (!value.isObject()) {
                    continue;
                }

                const QJsonObject object =
                    value.toObject();

                if (object.value(
                        QStringLiteral("id")
                    ).toString()
                    != QStringLiteral("ro-asd-beta")) {

                    continue;
                }

                targetFound = true;

                const QJsonValue enabledValue =
                    object.value(
                        QStringLiteral("is_enabled")
                    );

                if (!enabledValue.isBool()) {
                    setRoAsdState(
                        Unavailable,
                        QStringLiteral(
                            "DNF5 depo etkinlik durumu "
                            "okunamadı."
                        )
                    );

                    setChecking(false);
                    return;
                }

                const bool enabled =
                    enabledValue.toBool();

                qInfo()
                    << "SOURCE MANAGER EFFECTIVE RO-ASD:"
                    << (enabled
                        ? "enabled"
                        : "disabled");

                if (enabled) {
                    setRoAsdState(
                        Ready,
                        QStringLiteral(
                            "Ro-ASD uygulama deposu hazır."
                        )
                    );
                } else {
                    setRoAsdState(
                        Disabled,
                        QStringLiteral(
                            "Ro-ASD uygulama deposu "
                            "kurulu ancak devre dışı."
                        )
                    );
                }

                setChecking(false);
                return;
            }

            if (!targetFound) {
                setRoAsdState(
                    Unavailable,
                    QStringLiteral(
                        "Ro-ASD deposu DNF5 yapılandırmasında "
                        "bulunamadı."
                    )
                );
            }

            setChecking(false);
        }
    );

    connect(
        m_repositoryStateProcess,
        &QProcess::errorOccurred,
        this,
        [this](QProcess::ProcessError error) {
            if (error
                != QProcess::FailedToStart) {
                return;
            }

            m_repositoryStateQueryPending = false;

            setRoAsdState(
                Unavailable,
                QStringLiteral(
                    "DNF5 depo kontrolü başlatılamadı."
                )
            );

            setChecking(false);
        }
    );
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

bool SourceManager::roAsdReady() const
{
    return m_roAsdState == Ready;
}

bool SourceManager::roAsdActionAvailable() const
{
    return m_roAsdState == Missing
        || m_roAsdState == Disabled
        || m_roAsdState == Unavailable;
}

QString SourceManager::roAsdActionText() const
{
    switch (m_roAsdState) {
    case Missing:
        return QStringLiteral("Ro-ASD Deposunu Ekle");

    case Disabled:
        return QStringLiteral("Depoyu Etkinleştir");

    case Unavailable:
        return QStringLiteral("Tekrar Dene");

    default:
        return {};
    }
}


bool SourceManager::repositoryActionRunning() const
{
    return m_repositoryActionRunning;
}

QString SourceManager::repositoryActionError() const
{
    return m_repositoryActionError;
}

void SourceManager::executeRoAsdAction()
{
    if (m_repositoryActionRunning) {
        return;
    }

    if (!m_supportedSystem) {
        setRepositoryActionError(
            QStringLiteral(
                "Bu sistem Ro-ASD deposu için "
                "desteklenmiyor."
            )
        );

        return;
    }

    QString operation;

    switch (m_roAsdState) {
    case Missing:
        operation =
            QStringLiteral("--install");
        break;

    case Disabled:
        operation =
            QStringLiteral("--enable");
        break;

    case Unavailable:
        refresh();
        return;

    default:
        return;
    }

    const QString pkexecPath =
        QStringLiteral(
            "/usr/bin/pkexec"
        );

    const QString helperPath =
        QStringLiteral(
            "/usr/libexec/ro-store/"
            "ro-store-repo-helper"
        );

    if (!QFileInfo::exists(pkexecPath)) {
        setRepositoryActionError(
            QStringLiteral(
                "Polkit pkexec bulunamadı."
            )
        );

        return;
    }

    if (!QFileInfo::exists(helperPath)) {
        setRepositoryActionError(
            QStringLiteral(
                "Ro-Store repository yardımcısı "
                "sistemde kurulu değil."
            )
        );

        return;
    }

    setRepositoryActionError({});
    setRepositoryActionRunning(true);

    qInfo()
        << "SOURCE MANAGER REPOSITORY ACTION:"
        << operation;

    m_repositoryProcess->setProgram(
        pkexecPath
    );

    m_repositoryProcess->setArguments(
        {
            helperPath,
            operation
        }
    );

    m_repositoryProcess->start();
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

    // detectRoAsdRepository() efektif DNF5 durumunu
    // asenkron sorgulamaya başladıysa checking durumu
    // process tamamlanınca kapatılır.
    if (!m_repositoryStateQueryPending) {
        setChecking(false);
    }
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

    int targetSectionCount = 0;

    bool insideTargetRepo = false;
    bool collectingGpgKey = false;

    QString baseUrl;
    QString gpgCheck;
    QString repoGpgCheck;

    QStringList gpgKeys;

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

        insideTargetRepo = false;
        collectingGpgKey = false;

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

                collectingGpgKey = false;

                if (insideTargetRepo) {
                    ++targetSectionCount;
                }

                continue;
            }

            if (!insideTargetRepo) {
                continue;
            }

            const qsizetype separator =
                line.indexOf('=');

            if (separator < 0) {
                if (collectingGpgKey) {
                    gpgKeys.append(line);
                }

                continue;
            }

            const QString key =
                line.left(separator)
                    .trimmed()
                    .toLower();

            const QString value =
                line.mid(separator + 1)
                    .trimmed();

            collectingGpgKey = false;

            if (key == QStringLiteral("baseurl")) {
                baseUrl = value;
                continue;
            }

            if (key == QStringLiteral("gpgcheck")) {
                gpgCheck = value;
                continue;
            }

            if (key == QStringLiteral("repo_gpgcheck")) {
                repoGpgCheck = value;
                continue;
            }

            if (key == QStringLiteral("gpgkey")) {
                gpgKeys.append(value);
                collectingGpgKey = true;
                continue;
            }
        }
    }

    if (targetSectionCount == 0) {
        setRoAsdState(
            Missing,
            QStringLiteral(
                "Ro-ASD uygulama deposu "
                "sistemde kurulu değil."
            )
        );

        return;
    }

    const auto normalizedUrl =
        [](QString value) {

            value = value.trimmed();

            while (value.endsWith('/')) {
                value.chop(1);
            }

            return value;
        };

    const auto isEnabledValue =
        [](QString value) {

            value =
                value.trimmed().toLower();

            return value == QStringLiteral("1")
                || value == QStringLiteral("true")
                || value == QStringLiteral("yes")
                || value == QStringLiteral("on");
        };

    const QString expectedBaseUrl =
        QStringLiteral(
            "https://repo.ro-asd.org/"
            "rpm/fedora/44/beta/$basearch"
        );

    const QString expectedMetadataKey =
        QStringLiteral(
            "file:///etc/pki/rpm-gpg/"
            "REPODATA-GPG-KEY-ro-asd"
        );

    const QString expectedRpmKey =
        QStringLiteral(
            "file:///etc/pki/rpm-gpg/"
            "RPM-GPG-KEY-ro-asd"
        );

    const QString allGpgKeys =
        gpgKeys.join(
            QStringLiteral(" ")
        );

    const bool configurationValid =
        targetSectionCount == 1
        && normalizedUrl(baseUrl)
            == expectedBaseUrl
        && isEnabledValue(gpgCheck)
        && isEnabledValue(repoGpgCheck)
        && allGpgKeys.contains(
            expectedMetadataKey
        )
        && allGpgKeys.contains(
            expectedRpmKey
        );

    if (!configurationValid) {
        setRoAsdState(
            InvalidConfiguration,
            QStringLiteral(
                "Ro-ASD deposu bulundu ancak "
                "yapılandırması resmi depo ayarlarıyla "
                "eşleşmiyor."
            )
        );

        return;
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

    if (!rpmKeyExists
        || !metadataKeyExists) {

        setRoAsdState(
            Missing,
            QStringLiteral(
                "Ro-ASD deposunun güvenlik "
                "anahtarları eksik."
            )
        );

        return;
    }

    queryRoAsdEffectiveState();
}


void SourceManager::queryRoAsdEffectiveState()
{
    if (m_repositoryStateProcess->state()
            != QProcess::NotRunning) {

        m_repositoryStateQueryPending = true;
        return;
    }

    const QString dnf5Path =
        QStringLiteral("/usr/bin/dnf5");

    if (!QFileInfo::exists(dnf5Path)) {
        m_repositoryStateQueryPending = false;

        setRoAsdState(
            Unavailable,
            QStringLiteral(
                "DNF5 sistemde bulunamadı."
            )
        );

        return;
    }

    m_repositoryStateQueryPending = true;

    m_repositoryStateProcess->setProgram(
        dnf5Path
    );

    m_repositoryStateProcess->setArguments(
        {
            QStringLiteral("repo"),
            QStringLiteral("list"),
            QStringLiteral("--all"),
            QStringLiteral("--json"),
            QStringLiteral("ro-asd-beta")
        }
    );

    qInfo()
        << "SOURCE MANAGER DNF5 REPO QUERY START";

    m_repositoryStateProcess->start();
}


void SourceManager::setChecking(bool value)
{
    if (m_checking == value) {
        return;
    }

    m_checking = value;
    emit checkingChanged();
}

void SourceManager::setRepositoryActionRunning(
    bool value
)
{
    if (m_repositoryActionRunning == value) {
        return;
    }

    m_repositoryActionRunning = value;
    emit repositoryActionRunningChanged();
}

void SourceManager::setRepositoryActionError(
    const QString &message
)
{
    if (m_repositoryActionError == message) {
        return;
    }

    m_repositoryActionError = message;
    emit repositoryActionErrorChanged();

    if (!message.isEmpty()) {
        qWarning()
            << "SOURCE MANAGER REPOSITORY ERROR:"
            << message;
    }
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

    case InvalidConfiguration:
        return QStringLiteral(
            "InvalidConfiguration"
        );

    case Unsupported:
        return QStringLiteral("Unsupported");
    }

    return QStringLiteral("Unknown");
}
