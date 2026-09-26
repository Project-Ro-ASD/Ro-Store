#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include <QDBusMetaType>
#include <QMap>

#include "CatalogModel.h"
#include "PackageStatus.h"
#include "AppLauncher.h"
#include "packages/backends/Dnf5Backend.h"
#include "packages/PackageTransaction.h"
#include "packages/PackageTransactionModel.h"
#include "packages/PackageTransactionManager.h"

int main(int argc, char *argv[])
{
    qDBusRegisterMetaType<QMap<QString, QString>>();

    QGuiApplication app(argc, argv);

    qmlRegisterType<CatalogModel>("RoStore", 1, 0, "CatalogModel");
    qmlRegisterType<PackageStatus>("RoStore", 1, 0, "PackageStatus");
    qmlRegisterType<AppLauncher>("RoStore", 1, 0, "AppLauncher");
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
