#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>

#include "CatalogModel.h"
#include "PackageStatus.h"
#include "PackageInstaller.h"
#include "AppLauncher.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<CatalogModel>("RoStore", 1, 0, "CatalogModel");
    qmlRegisterType<PackageStatus>("RoStore", 1, 0, "PackageStatus");
    qmlRegisterType<PackageInstaller>("RoStore", 1, 0, "PackageInstaller");
    qmlRegisterType<AppLauncher>("RoStore", 1, 0, "AppLauncher");

    QQmlApplicationEngine engine;

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
