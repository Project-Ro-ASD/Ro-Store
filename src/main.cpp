#include <QApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include <QDBusMetaType>
#include <QMap>
#include <QIcon>
#include <QQuickImageProvider>
#include <QRegularExpression>

#include "TrayController.h"
#include "CatalogModel.h"
#include "FedoraPackageModel.h"
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

int main(int argc, char *argv[])
{
    qDBusRegisterMetaType<QMap<QString, QString>>();

    QApplication app(argc, argv);
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
