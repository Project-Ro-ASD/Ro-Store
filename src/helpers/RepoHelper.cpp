#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
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
    // Önce gerçekten ro-asd-beta tanımının sistemde
    // bulunduğunu doğrula.
    if (findRepositoryFile().isEmpty()) {
        if (error) {
            *error =
                QStringLiteral(
                    "ro-asd-beta repository tanımı bulunamadı."
                );
        }

        return false;
    }

    const QString dnf5Path =
        QStringLiteral("/usr/bin/dnf5");

    if (!QFileInfo::exists(dnf5Path)) {
        if (error) {
            *error =
                QStringLiteral(
                    "DNF5 sistemde bulunamadı."
                );
        }

        return false;
    }

    QProcess process;

    process.setProgram(
        dnf5Path
    );

    // Shell kullanılmaz ve kullanıcıdan hiçbir argüman
    // alınmaz. Repository kimliği sabittir.
    process.setArguments(
        {
            QStringLiteral("config-manager"),
            QStringLiteral("setopt"),
            QStringLiteral(
                "ro-asd-beta.enabled=1"
            )
        }
    );

    process.start();

    if (!process.waitForStarted(5000)) {
        if (error) {
            *error =
                QStringLiteral(
                    "DNF5 repository etkinleştirme "
                    "işlemi başlatılamadı."
                );
        }

        return false;
    }

    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();

        if (error) {
            *error =
                QStringLiteral(
                    "DNF5 repository etkinleştirme "
                    "işlemi zaman aşımına uğradı."
                );
        }

        return false;
    }

    if (process.exitStatus()
            == QProcess::NormalExit
        && process.exitCode() == 0) {

        return true;
    }

    QString processError =
        QString::fromUtf8(
            process.readAllStandardError()
        ).trimmed();

    if (processError.isEmpty()) {
        processError =
            QString::fromUtf8(
                process.readAllStandardOutput()
            ).trimmed();
    }

    if (processError.isEmpty()) {
        processError =
            QStringLiteral(
                "DNF5 repository etkinleştirme "
                "işlemi başarısız oldu."
            );
    }

    if (error) {
        *error = processError;
    }

    return false;
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
