#pragma once

#include "flatpak/FlatpakManager.h"
#include "FakeCoordinator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class FlatpakBackendTest : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_directory;
    QString m_program;
    QString m_state;
    QString m_journal;

    static void writeFile(const QString &path, const QByteArray &content)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(content), content.size());
    }

    void readInventory(FlatpakManager &manager)
    {
        manager.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!manager.loading(), 10000);
    }

    void readCatalog(FlatpakManager &manager)
    {
        manager.loadCatalog(QStringLiteral("user"), QStringLiteral("flathub"));
        QTRY_VERIFY_WITH_TIMEOUT(!manager.loading(), 10000);
        QVERIFY(manager.isCatalogApp(QStringLiteral("user"), QStringLiteral("flathub"),
                                     QStringLiteral("app.test.Test")));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        qputenv("XDG_DATA_HOME", (m_directory.path() + QStringLiteral("/data")).toUtf8());
        m_program = m_directory.path() + QStringLiteral("/flatpak-fixture");
        m_state = m_directory.path() + QStringLiteral("/installed");
        qputenv("RO_STORE_TEST_STATE", m_state.toUtf8());
        QVERIFY(QFile::copy(QStringLiteral(RO_STORE_TEST_FIXTURE_PATH), m_program));
        QVERIFY(QFile::setPermissions(m_program, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                | QFileDevice::ExeOwner));
        m_journal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                    + QStringLiteral("/flatpak-transaction-journal.json");
        QVERIFY(m_journal.startsWith(m_directory.path()));
        QVERIFY(QDir().mkpath(QFileInfo(m_journal).absolutePath()));
    }

    void init()
    {
        QFile::remove(m_state);
        QFile::remove(m_journal);
        QFile::remove(m_journal + QStringLiteral(".lock"));
        qputenv("RO_STORE_TEST_MODE", "normal");
    }

    void rejectsUnsafeRequests()
    {
        FakeCoordinator coordinator;
        FlatpakManager manager(m_program, nullptr);
        manager.setTransactionManager(&coordinator);
        readInventory(manager);
        readCatalog(manager);
        manager.install(QStringLiteral("user"), QStringLiteral("flathub"), QStringLiteral("../unsafe"));
        QVERIFY(!QFile::exists(m_journal));
        manager.install(QStringLiteral("system"), QStringLiteral("flathub"), QStringLiteral("app.test.Test"));
        manager.update(QStringLiteral("user"), QStringLiteral("app.test.Test"));
        QVERIFY(!QFile::exists(m_journal));
        QVERIFY(!QFile::exists(m_state));
    }

    void successfulInstallClearsJournalOnlyAfterRead()
    {
        FakeCoordinator coordinator;
        FlatpakManager manager(m_program, nullptr);
        manager.setTransactionManager(&coordinator);
        readInventory(manager);
        readCatalog(manager);
        manager.install(QStringLiteral("user"), QStringLiteral("flathub"), QStringLiteral("app.test.Test"));
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy() && !manager.loading(), 10000);
        QVERIFY(QFile::exists(m_state));
        QVERIFY(manager.isInstalled(QStringLiteral("user"), QStringLiteral("app.test.Test")));
        QVERIFY(!manager.outcomeUnknown());
        QVERIFY(!QFile::exists(m_journal));
        QVERIFY(!coordinator.occupied);
    }

    void failureKeepsJournalUntilReviewed()
    {
        FakeCoordinator coordinator;
        FlatpakManager manager(m_program, nullptr);
        manager.setTransactionManager(&coordinator);
        readInventory(manager);
        readCatalog(manager);
        qputenv("RO_STORE_TEST_MODE", "mutation-fail");
        manager.install(QStringLiteral("user"), QStringLiteral("flathub"), QStringLiteral("app.test.Test"));
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy() && !manager.loading(), 10000);
        QVERIFY(manager.outcomeUnknown());
        QVERIFY(QFile::exists(m_journal));
        QVERIFY(!QFile::exists(m_state));
        QVERIFY(!coordinator.occupied);
        manager.acknowledgeRecovery();
        QVERIFY(!QFile::exists(m_journal));
    }

    void failedVerificationKeepsJournal()
    {
        FakeCoordinator coordinator;
        FlatpakManager manager(m_program, nullptr);
        manager.setTransactionManager(&coordinator);
        readInventory(manager);
        readCatalog(manager);
        qputenv("RO_STORE_TEST_MODE", "read-fail-after-install");
        manager.install(QStringLiteral("user"), QStringLiteral("flathub"), QStringLiteral("app.test.Test"));
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy() && !manager.loading(), 10000);
        QVERIFY(QFile::exists(m_state));
        QVERIFY(manager.outcomeUnknown());
        QVERIFY(!manager.recoveryChecked());
        QVERIFY(QFile::exists(m_journal));
        manager.acknowledgeRecovery();
        QVERIFY(QFile::exists(m_journal));
        qputenv("RO_STORE_TEST_MODE", "normal");
        manager.checkRecovery();
        QTRY_VERIFY_WITH_TIMEOUT(!manager.loading(), 10000);
        QVERIFY(manager.recoveryChecked());
        manager.acknowledgeRecovery();
        QVERIFY(!QFile::exists(m_journal));
    }

    void lockAndExistingJournalFailClosed()
    {
        FakeCoordinator coordinator;
        FlatpakManager manager(m_program, nullptr);
        manager.setTransactionManager(&coordinator);
        readInventory(manager);
        readCatalog(manager);
        coordinator.occupied = true;
        manager.install(QStringLiteral("user"), QStringLiteral("flathub"), QStringLiteral("app.test.Test"));
        QVERIFY(!QFile::exists(m_journal));
        coordinator.occupied = false;
        writeFile(m_journal, "broken journal");
        manager.install(QStringLiteral("user"), QStringLiteral("flathub"), QStringLiteral("app.test.Test"));
        QVERIFY(QFile::exists(m_journal));
        QVERIFY(!QFile::exists(m_state));
    }

    void externalFailuresKeepRecoveryRecord_data()
    {
        QTest::addColumn<QByteArray>("mode");
        QTest::newRow("disk-full") << QByteArray("disk-full");
        QTest::newRow("process-crash") << QByteArray("crash");
    }

    void externalFailuresKeepRecoveryRecord()
    {
        QFETCH(QByteArray, mode);
        FakeCoordinator coordinator;
        FlatpakManager manager(m_program, nullptr);
        manager.setTransactionManager(&coordinator);
        readInventory(manager);
        readCatalog(manager);
        qputenv("RO_STORE_TEST_MODE", mode);
        manager.install(QStringLiteral("user"), QStringLiteral("flathub"), QStringLiteral("app.test.Test"));
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy() && !manager.loading(), 10000);
        QVERIFY(manager.outcomeUnknown());
        QVERIFY(QFile::exists(m_journal));
        QVERIFY(!QFile::exists(m_state));
        QVERIFY(!coordinator.occupied);
    }

    void parallelInstancesDoNotBothMutate()
    {
        FakeCoordinator firstCoordinator;
        FakeCoordinator secondCoordinator;
        FlatpakManager first(m_program, nullptr);
        FlatpakManager second(m_program, nullptr);
        first.setTransactionManager(&firstCoordinator);
        second.setTransactionManager(&secondCoordinator);
        readInventory(first);
        readCatalog(first);
        readInventory(second);
        readCatalog(second);
        qputenv("RO_STORE_TEST_MODE", "slow-install");
        first.install(QStringLiteral("user"), QStringLiteral("flathub"), QStringLiteral("app.test.Test"));
        QVERIFY(first.busy());
        second.install(QStringLiteral("user"), QStringLiteral("flathub"), QStringLiteral("app.test.Test"));
        QVERIFY(!second.busy());
        QVERIFY(!secondCoordinator.occupied);
        QTRY_VERIFY_WITH_TIMEOUT(!first.busy() && !first.loading(), 10000);
        QVERIFY(QFile::exists(m_state));
        QVERIFY(!QFile::exists(m_journal));
    }

    void readsPermissionsBeforeInstallation()
    {
        FlatpakManager manager(m_program, nullptr);
        readInventory(manager);
        readCatalog(manager);
        manager.requestPermissions(QStringLiteral("user"), QStringLiteral("flathub"),
                                   QStringLiteral("app.test.Test"));
        QTRY_COMPARE_WITH_TIMEOUT(manager.permissionInfo(QStringLiteral("app.test.Test"))
                                      .value(QStringLiteral("status")).toString(),
                                  QStringLiteral("ready"), 10000);
        QVERIFY(manager.permissionInfo(QStringLiteral("app.test.Test"))
                    .value(QStringLiteral("text")).toString().contains(QStringLiteral("shared=network")));
        manager.requestPermissions(QStringLiteral("system"), QStringLiteral("flathub"),
                                   QStringLiteral("app.test.Test"));
        QVERIFY(!QFile::exists(m_journal));
    }

    void permissionReadCanBeRetried()
    {
        FlatpakManager manager(m_program, nullptr);
        readInventory(manager);
        readCatalog(manager);
        qputenv("RO_STORE_TEST_MODE", "permission-fail");
        manager.requestPermissions(QStringLiteral("user"), QStringLiteral("flathub"),
                                   QStringLiteral("app.test.Test"));
        QTRY_COMPARE_WITH_TIMEOUT(manager.permissionInfo(QStringLiteral("app.test.Test"))
                                      .value(QStringLiteral("status")).toString(),
                                  QStringLiteral("error"), 10000);
        qputenv("RO_STORE_TEST_MODE", "normal");
        manager.requestPermissions(QStringLiteral("user"), QStringLiteral("flathub"),
                                   QStringLiteral("app.test.Test"));
        QTRY_COMPARE_WITH_TIMEOUT(manager.permissionInfo(QStringLiteral("app.test.Test"))
                                      .value(QStringLiteral("status")).toString(),
                                  QStringLiteral("ready"), 10000);
    }

    void closingDuringPermissionReadIsSafe()
    {
        auto *manager = new FlatpakManager(m_program, nullptr);
        readInventory(*manager);
        readCatalog(*manager);
        qputenv("RO_STORE_TEST_MODE", "permission-slow");
        manager->requestPermissions(QStringLiteral("user"), QStringLiteral("flathub"),
                                    QStringLiteral("app.test.Test"));
        QCOMPARE(manager->permissionInfo(QStringLiteral("app.test.Test"))
                     .value(QStringLiteral("status")).toString(), QStringLiteral("loading"));
        delete manager;
        QVERIFY(!QFile::exists(m_journal));
    }
};
