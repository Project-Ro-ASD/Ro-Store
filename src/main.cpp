#include <QApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include <QDBusMetaType>
#include <QMap>
#include <QIcon>
#include <QQuickImageProvider>
#include <QRegularExpression>
#include <QBuffer>
#include <QEventLoop>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "TrayController.h"
#include "CatalogModel.h"
#include "FedoraPackageModel.h"
#include "SystemUpdateModel.h"
#include "SystemUpdateCache.h"
#include "SystemOfflineUpgrade.h"
#include "flatpak/FlatpakManager.h"
#include "PackageStatus.h"
#include "AppLauncher.h"
#include "SystemFontMonitor.h"
#include "SystemIconMonitor.h"
#include "sources/SourceManager.h"
#include "packages/backends/Dnf5Backend.h"
#include "packages/PackageTransaction.h"
#include "packages/PackageTransactionModel.h"
#include "packages/PackageTransactionManager.h"

// QML Image does not resolve stock icon names on its own. Qt's QIcon theme
// lookup preserves Plasma/KDE theme selection and supplies a normal pixmap.
class ThemeIconProvider final : public QQuickImageProvider
{
public:
    ThemeIconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

    QPixmap requestPixmap(const QString &id, QSize *size,
                          const QSize &requestedSize) override
    {
        static const QRegularExpression safeName(QStringLiteral("^[A-Za-z0-9._-]{1,128}$"));
        if (!safeName.match(id).hasMatch())
            return {};

        const QSize target = requestedSize.isValid() ? requestedSize : QSize(64, 64);
        const QPixmap result = QIcon::fromTheme(id).pixmap(target);
        if (size)
            *size = result.size();
        return result;
    }
};

// Run only when explicitly requested; useful for diagnosing image servers
// which return a JPEG to curl but non-image content to Qt Quick.
static int checkImageWithQt(const QString &rawUrl)
{
    const QUrl url(rawUrl);
    if (!url.isValid() || (url.scheme() != QStringLiteral("https")
                           && !url.isLocalFile())) {
        qWarning() << "QA IMAGE: supply an HTTPS or file:/// URL";
        return 2;
    }

    const auto formats = QImageReader::supportedImageFormats();
    qInfo() << "QA IMAGE JPEG DECODER:"
            << (formats.contains(QByteArrayLiteral("jpeg"))
                || formats.contains(QByteArrayLiteral("jpg")));
    qInfo() << "QA IMAGE URL:" << url;

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout.start(20000);
    loop.exec();
    timeout.stop();

    const QByteArray bytes = reply->readAll();
    qInfo() << "QA IMAGE HTTP:"
            << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
            << "Content-Type:" << reply->header(QNetworkRequest::ContentTypeHeader)
            << "Network error:" << reply->errorString();
    qInfo() << "QA IMAGE BYTES:" << bytes.size()
            << "FIRST HEX:" << bytes.left(24).toHex()
            << "FIRST TEXT:" << bytes.left(80);

    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const QImage image = reader.read();
    qInfo() << "QA IMAGE FORMAT:" << reader.format()
            << "SIZE:" << image.size()
            << "DECODE ERROR:" << reader.errorString();
    reply->deleteLater();
    return image.isNull() ? 1 : 0;
}

int main(int argc, char *argv[])
{
    qDBusRegisterMetaType<QMap<QString, QString>>();

    QApplication app(argc, argv);
    const QStringList args = app.arguments();
    const int qaIndex = args.indexOf(QStringLiteral("--qa-image-check"));
    if (qaIndex >= 0) {
        if (qaIndex + 1 >= args.size()) {
            qWarning() << "Usage: ro-store --qa-image-check https://example.org/image.jpg";
            return 2;
        }
        return checkImageWithQt(args.at(qaIndex + 1));
    }
    app.setQuitOnLastWindowClosed(false);

    TrayController trayController;
    qmlRegisterSingletonInstance(
        "RoStore", 1, 0, "TrayController", &trayController
    );

    SystemFontMonitor systemFontMonitor;
    qmlRegisterSingletonInstance(
        "RoStore", 1, 0, "SystemFontMonitor", &systemFontMonitor
    );

    SystemIconMonitor systemIconMonitor;
    qmlRegisterSingletonInstance(
        "RoStore", 1, 0, "SystemIconMonitor", &systemIconMonitor
    );

    qmlRegisterType<CatalogModel>("RoStore", 1, 0, "CatalogModel");
    qmlRegisterType<FedoraPackageModel>("RoStore", 1, 0, "FedoraPackageModel");
    qmlRegisterType<SystemUpdateModel>("RoStore", 1, 0, "SystemUpdateModel");
    qmlRegisterType<SystemUpdateCache>("RoStore", 1, 0, "SystemUpdateCache");
    qmlRegisterType<SystemOfflineUpgrade>("RoStore", 1, 0, "SystemOfflineUpgrade");
    qmlRegisterType<FlatpakManager>("RoStore", 1, 0, "FlatpakManager");
    qmlRegisterType<PackageStatus>("RoStore", 1, 0, "PackageStatus");
    qmlRegisterType<AppLauncher>("RoStore", 1, 0, "AppLauncher");
    qmlRegisterType<SourceManager>("RoStore", 1, 0, "SourceManager");
    qmlRegisterType<Dnf5Backend>("RoStore", 1, 0, "Dnf5Backend");

    qmlRegisterType<PackageTransactionManager>(
        "RoStore",
        1,
        0,
        "PackageTransactionManager"
    );

    qmlRegisterUncreatableType<PackageTransaction>(
        "RoStore",
        1,
        0,
        "PackageTransaction",
        "Created by PackageTransactionManager"
    );

    qmlRegisterUncreatableType<PackageTransactionModel>(
        "RoStore",
        1,
        0,
        "PackageTransactionModel",
        "Owned by PackageTransactionManager"
    );

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("rostore-theme"), new ThemeIconProvider);
    // Explicit QA-only UI preview: never creates a DNF5 transaction or
    // touches the persistent recovery journal.
    engine.setInitialProperties({
        {QStringLiteral("qaSafetyBannerPreview"),
         app.arguments().contains(QStringLiteral("--qa-safety-banner"))}
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    engine.loadFromModule("RoStore", "Main");

    return app.exec();
}
