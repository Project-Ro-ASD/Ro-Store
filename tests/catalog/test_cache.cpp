#include "CatalogModel.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdio>

class CatalogCacheTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_directory;
    const QUrl m_url = QUrl(QStringLiteral("https://127.0.0.1:1/catalog.json"));

    QString cachePath() const
    {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/catalog-")
            + QString::fromLatin1(QCryptographicHash::hash(
                m_url.toString(QUrl::FullyEncoded).toUtf8(), QCryptographicHash::Sha256).toHex())
            + QStringLiteral(".json");
    }

    void writeCache(const QByteArray &content)
    {
        QVERIFY(QDir().mkpath(QFileInfo(cachePath()).absolutePath()));
        QFile file(cachePath());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(content), content.size());
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        qputenv("XDG_CACHE_HOME", m_directory.path().toUtf8());
    }

    void cachedCatalogSurvivesNetworkFailure()
    {
        writeCache(R"({"schemaVersion":2,"apps":[{"id":"app.test.Test","packageName":"test-app","name":"Test App","summary":"Example"}]})");
        CatalogModel model;
        model.load(m_url);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.searchSnapshot(QStringLiteral("Test")).size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 5000);
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(model.error().contains(QStringLiteral("son kayıtlı")));
    }

    void malformedCacheIsRejected()
    {
        writeCache("invalid catalog");
        CatalogModel model;
        model.load(m_url);
        QCOMPARE(model.rowCount(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 5000);
        QCOMPARE(model.rowCount(), 0);
    }

    void staleCacheIsRejected()
    {
        writeCache(R"({"schemaVersion":2,"apps":[{"id":"app.test.Test","packageName":"test-app","name":"Test App"}]})");
        QFile file(cachePath());
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTime().addDays(-8),
                                 QFileDevice::FileModificationTime));
        file.close();
        CatalogModel model;
        model.load(m_url);
        QCOMPARE(model.rowCount(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 5000);
        QCOMPARE(model.rowCount(), 0);
    }

    void warmCatalogAndSearchLatency()
    {
        QJsonArray apps;
        for (int i = 0; i < 2000; ++i) {
            apps.append(QJsonObject{
                {QStringLiteral("id"), QStringLiteral("app.test.%1").arg(i)},
                {QStringLiteral("packageName"), QStringLiteral("test-app-%1").arg(i)},
                {QStringLiteral("name"), QStringLiteral("Application %1").arg(i)},
                {QStringLiteral("summary"), QStringLiteral("Catalog search fixture")}
            });
        }
        writeCache(QJsonDocument(QJsonObject{
            {QStringLiteral("schemaVersion"), 2},
            {QStringLiteral("apps"), apps}
        }).toJson(QJsonDocument::Compact));

        QElapsedTimer timer;
        timer.start();
        CatalogModel model;
        model.load(m_url);
        const qint64 cacheLoadUs = timer.nsecsElapsed() / 1000;
        QCOMPARE(model.rowCount(), 2000);
        timer.restart();
        for (int i = 0; i < 100; ++i)
            QVERIFY(!model.searchSnapshot(QStringLiteral("Application 19")).isEmpty());
        const qint64 search100Us = timer.nsecsElapsed() / 1000;
        fprintf(stdout, "Warm 2000-app catalog: load %lld us; 100 searches %lld us\n",
                static_cast<long long>(cacheLoadUs), static_cast<long long>(search100Us));
        fflush(stdout);
        QVERIFY(cacheLoadUs < 2000000);
        QVERIFY(search100Us < 2000000);
    }
};

QTEST_GUILESS_MAIN(CatalogCacheTest)
#include "test_cache.moc"
