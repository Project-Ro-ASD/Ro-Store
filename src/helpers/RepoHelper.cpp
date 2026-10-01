#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>

#include <unistd.h>

namespace {

constexpr auto repoSection = "ro-asd-beta";

const QString repoDirectory =
    QStringLiteral("/etc/yum.repos.d");

const QString keyDirectory =
    QStringLiteral("/etc/pki/rpm-gpg");

const QString defaultRepoPath =
    repoDirectory
    + QStringLiteral("/ro-asd.repo");

const QString rpmKeyPath =
    keyDirectory
    + QStringLiteral("/RPM-GPG-KEY-ro-asd");

const QString metadataKeyPath =
    keyDirectory
    + QStringLiteral("/REPODATA-GPG-KEY-ro-asd");


void printError(const QString &message)
{
    QTextStream(stderr)
        << "ro-store-repo-helper: "
        << message
        << '\n';
}


void printInfo(const QString &message)
{
    QTextStream(stdout)
        << "ro-store-repo-helper: "
        << message
        << '\n';
}


QByteArray readResource(
    const QString &resourcePath
)
{
    QFile file(resourcePath);

    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    return file.readAll();
}


bool writeSystemFile(
    const QString &path,
    const QByteArray &data,
    QString *error
)
{
    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error =
                QStringLiteral(
                    "Dosya açılamadı: %1"
                ).arg(path);
        }

        return false;
    }

    if (file.write(data) != data.size()) {
        if (error) {
            *error =
                QStringLiteral(
                    "Dosya tam olarak yazılamadı: %1"
                ).arg(path);
        }

        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        if (error) {
            *error =
                QStringLiteral(
                    "Dosya kaydedilemedi: %1"
                ).arg(path);
        }

        return false;
    }

    QFile::setPermissions(
        path,
        QFileDevice::ReadOwner
            | QFileDevice::WriteOwner
            | QFileDevice::ReadGroup
            | QFileDevice::ReadOther
    );

    return true;
}


bool fileContainsRepositorySection(
    const QString &path
)
{
    QFile file(path);

    if (!file.open(
            QIODevice::ReadOnly
            | QIODevice::Text
        )) {
        return false;
    }

    while (!file.atEnd()) {
        const QString line =
            QString::fromUtf8(
                file.readLine()
            ).trimmed();

        if (!line.startsWith('[')
            || !line.endsWith(']')) {
            continue;
        }

        const QString section =
            line.mid(
                1,
                line.size() - 2
            ).trimmed();

        if (section.compare(
                QString::fromLatin1(repoSection),
                Qt::CaseInsensitive
            ) == 0) {
            return true;
        }
    }

    return false;
}


QString findRepositoryFile()
{
    const QDir directory(repoDirectory);

    const QStringList files =
        directory.entryList(
            {
                QStringLiteral("*.repo")
            },
            QDir::Files
        );

    for (const QString &name : files) {
        const QString path =
            directory.filePath(name);

        if (fileContainsRepositorySection(path)) {
            return path;
        }
    }

    return {};
}


bool ensureDirectory(
    const QString &path,
    QString *error
)
{
    QDir directory;

    if (directory.mkpath(path)) {
        return true;
    }

    if (error) {
        *error =
            QStringLiteral(
                "Dizin oluşturulamadı: %1"
            ).arg(path);
    }

    return false;
}


bool installRepository(QString *error)
{
    if (!ensureDirectory(
            repoDirectory,
            error
        )) {
        return false;
    }

    if (!ensureDirectory(
            keyDirectory,
            error
        )) {
        return false;
    }

    const QByteArray rpmKey =
        readResource(
            QStringLiteral(
                ":/repository/"
                "RPM-GPG-KEY-ro-asd"
            )
        );

    const QByteArray metadataKey =
        readResource(
            QStringLiteral(
                ":/repository/"
                "REPODATA-GPG-KEY-ro-asd"
            )
        );

    const QByteArray repoData =
        readResource(
            QStringLiteral(
                ":/repository/ro-asd.repo"
            )
        );

    if (rpmKey.isEmpty()
        || metadataKey.isEmpty()
        || repoData.isEmpty()) {

        if (error) {
            *error =
                QStringLiteral(
                    "Gömülü repository dosyaları okunamadı."
                );
        }

        return false;
    }

    if (!writeSystemFile(
            rpmKeyPath,
            rpmKey,
            error
        )) {
        return false;
    }

    if (!writeSystemFile(
            metadataKeyPath,
            metadataKey,
            error
        )) {
        return false;
    }

    // Aynı repo başka bir .repo dosyasında zaten
    // tanımlıysa ikinci bir tanım oluşturmayalım.
    if (findRepositoryFile().isEmpty()) {
        if (!writeSystemFile(
                defaultRepoPath,
                repoData,
                error
            )) {
            return false;
        }
    }

    return true;
}


bool enableRepository(QString *error)
{
    const QString path =
        findRepositoryFile();

    if (path.isEmpty()) {
        if (error) {
            *error =
                QStringLiteral(
                    "ro-asd-beta repository tanımı bulunamadı."
                );
        }

        return false;
    }

    QFile input(path);

    if (!input.open(
            QIODevice::ReadOnly
            | QIODevice::Text
        )) {

        if (error) {
            *error =
                QStringLiteral(
                    "Repository dosyası okunamadı: %1"
                ).arg(path);
        }

        return false;
    }

    QStringList lines;

    while (!input.atEnd()) {
        lines.append(
            QString::fromUtf8(
                input.readLine()
            )
        );
    }

    input.close();

    bool inTargetSection = false;
    bool targetFound = false;
    bool enabledWritten = false;

    QStringList output;

    for (QString line : lines) {
        const QString trimmed =
            line.trimmed();

        if (trimmed.startsWith('[')
            && trimmed.endsWith(']')) {

            if (inTargetSection
                && !enabledWritten) {

                output.append(
                    QStringLiteral(
                        "enabled=1\n"
                    )
                );

                enabledWritten = true;
            }

            const QString section =
                trimmed.mid(
                    1,
                    trimmed.size() - 2
                ).trimmed();

            inTargetSection =
                section.compare(
                    QString::fromLatin1(
                        repoSection
                    ),
                    Qt::CaseInsensitive
                ) == 0;

            if (inTargetSection) {
                targetFound = true;
            }

            output.append(line);
            continue;
        }

        if (inTargetSection) {
            const qsizetype separator =
                trimmed.indexOf('=');

            if (separator >= 0) {
                const QString key =
                    trimmed.left(separator)
                        .trimmed()
                        .toLower();

                if (key
                    == QStringLiteral(
                        "enabled"
                    )) {

                    output.append(
                        QStringLiteral(
                            "enabled=1\n"
                        )
                    );

                    enabledWritten = true;
                    continue;
                }
            }
        }

        output.append(line);
    }

    if (!targetFound) {
        if (error) {
            *error =
                QStringLiteral(
                    "ro-asd-beta repository bölümü bulunamadı."
                );
        }

        return false;
    }

    if (inTargetSection
        && !enabledWritten) {

        output.append(
            QStringLiteral(
                "enabled=1\n"
            )
        );
    }

    return writeSystemFile(
        path,
        output.join(QString()).toUtf8(),
        error
    );
}


void printUsage()
{
    QTextStream(stdout)
        << "Usage:\n"
        << "  ro-store-repo-helper --install\n"
        << "  ro-store-repo-helper --enable\n";
}

}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QStringList arguments =
        app.arguments();

    if (arguments.contains(
            QStringLiteral("--help")
        )
        || arguments.size() != 2) {

        printUsage();

        return arguments.size() == 2
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }

    if (geteuid() != 0) {
        printError(
            QStringLiteral(
                "Bu işlem yönetici yetkisi gerektiriyor."
            )
        );

        return EXIT_FAILURE;
    }

    QString error;

    if (arguments.at(1)
        == QStringLiteral("--install")) {

        if (!installRepository(&error)) {
            printError(error);
            return EXIT_FAILURE;
        }

        printInfo(
            QStringLiteral(
                "Ro-ASD repository dosyaları kuruldu."
            )
        );

        return EXIT_SUCCESS;
    }

    if (arguments.at(1)
        == QStringLiteral("--enable")) {

        if (!enableRepository(&error)) {
            printError(error);
            return EXIT_FAILURE;
        }

        printInfo(
            QStringLiteral(
                "Ro-ASD beta repository etkinleştirildi."
            )
        );

        return EXIT_SUCCESS;
    }

    printError(
        QStringLiteral(
            "Geçersiz işlem."
        )
    );

    printUsage();
    return EXIT_FAILURE;
}
