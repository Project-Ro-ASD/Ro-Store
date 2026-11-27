#include "CatalogModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

CatalogModel::CatalogModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CatalogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_apps.size();
}

QVariant CatalogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_apps.size()) {
        return {};
    }

    const AppInfo &app = m_apps.at(index.row());

    switch (role) {
    case IdRole:
        return app.id;
    case PackageNameRole:
        return app.packageName;
    case AppNameRole:
        return app.appName;
    case SummaryRole:
        return app.summary;
    case DescriptionRole:
        return app.description;
    case CategoryRole:
        return app.category;
    case DeveloperRole:
        return app.developer;
    case SourceRole:
        return app.source;
    case LatestVersionRole:
        return app.latestVersion;
    case InstallPackageRole:
        return app.installPackage;
    case IconRole:
        return app.icon;
    case IconUrlRole:
        return app.iconUrl;
    case FeaturedRole:
        return app.featured;
    case PriorityRole:
        return app.priority;
    default:
        return {};
    }
}

QHash<int, QByteArray> CatalogModel::roleNames() const
{
    return {
        { IdRole, "appId" },
        { PackageNameRole, "packageName" },
        { AppNameRole, "appName" },
        { SummaryRole, "summary" },
        { DescriptionRole, "description" },
        { CategoryRole, "category" },
        { DeveloperRole, "developer" },
        { SourceRole, "source" },
        { LatestVersionRole, "latestVersion" },
        { InstallPackageRole, "installPackage" },
        { IconRole, "icon" },
        { IconUrlRole, "iconUrl" },
        { FeaturedRole, "featured" },
        { PriorityRole, "priority" }
    };
}

bool CatalogModel::loading() const
{
    return m_loading;
}

QString CatalogModel::error() const
{
    return m_error;
}

QString CatalogModel::searchText() const
{
    return m_searchText;
}

void CatalogModel::setSearchText(const QString &text)
{
    if (m_searchText == text) {
        return;
    }

    m_searchText = text;
    emit searchTextChanged();

    applyFilters();
}

QString CatalogModel::categoryFilter() const
{
    return m_categoryFilter;
}

void CatalogModel::setCategoryFilter(const QString &category)
{
    if (m_categoryFilter == category) {
        return;
    }

    m_categoryFilter = category;
    emit categoryFilterChanged();

    applyFilters();
}

QStringList CatalogModel::categories() const
{
    return m_categories;
}

QVariantList CatalogModel::searchSnapshot(const QString &query) const
{
    QVariantList result;
    const QString needle = query.trimmed();
    if (needle.size() < 2) return result;
    for (const AppInfo &app : m_allApps) {
        if (!app.appName.contains(needle, Qt::CaseInsensitive)
            && !app.packageName.contains(needle, Qt::CaseInsensitive)
            && !app.summary.contains(needle, Qt::CaseInsensitive))
            continue;
        result.append(QVariantMap{
            {QStringLiteral("id"), app.id},
            {QStringLiteral("packageName"), app.packageName},
            {QStringLiteral("name"), app.appName},
            {QStringLiteral("summary"), app.summary},
            {QStringLiteral("description"), app.description},
            {QStringLiteral("category"), app.category},
            {QStringLiteral("version"), app.latestVersion},
            {QStringLiteral("source"), app.source},
            {QStringLiteral("iconUrl"), app.iconUrl}
        });
        if (result.size() >= 80) break;
    }
    return result;
}

void CatalogModel::load(const QUrl &url)
{
    if (m_loading)
        return;
    if (!url.isValid() || url.scheme() != QStringLiteral("https")
        || url.host().isEmpty() || !url.userInfo().isEmpty()) {
        setError(QStringLiteral("Katalog adresi geçersiz veya güvenli değil."));
        return;
    }
    constexpr qint64 maxBytes = 2 * 1024 * 1024;
    const QString cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString cachePath = cacheDirectory + QStringLiteral("/catalog-")
        + QString::fromLatin1(QCryptographicHash::hash(url.toString(QUrl::FullyEncoded).toUtf8(),
                                                      QCryptographicHash::Sha256).toHex())
        + QStringLiteral(".json");
    bool cached = false;
    const QFileInfo cacheInfo(cachePath);
    const qint64 cacheAge = cacheInfo.lastModified().secsTo(QDateTime::currentDateTime());
    if (cacheInfo.isFile() && cacheInfo.size() > 0 && cacheInfo.size() <= maxBytes
        && cacheAge >= 0 && cacheAge < 7 * 24 * 60 * 60) {
        QFile file(cachePath);
        if (file.open(QIODevice::ReadOnly))
            cached = parseCatalog(file.readAll(), url);
    }
    setLoading(true);
    setError("");

    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = m_network.get(request);
    reply->setReadBufferSize(maxBytes + 1);
    connect(reply, &QIODevice::readyRead, this, [reply, maxBytes]() {
        if (reply->bytesAvailable() > maxBytes) {
            reply->setProperty("catalogTooLarge", true);
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, url, cachePath, cacheDirectory, cached]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            setLoading(false);
            setError(reply->property("catalogTooLarge").toBool()
                ? QStringLiteral("Katalog boyutu güvenlik sınırını aştı.")
                : (cached ? QStringLiteral("Katalog yenilenemedi; son kayıtlı liste gösteriliyor.")
                          : QStringLiteral("Katalog indirilemedi: ") + reply->errorString()));
            return;
        }

        const QByteArray data = reply->readAll();
        const bool validResponse = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
            && reply->url() == url && reply->url().scheme() == QStringLiteral("https");
        if (!validResponse || data.isEmpty() || data.size() > maxBytes
            || !parseCatalog(data, reply->url())) {
            if (!validResponse || data.isEmpty())
                setError(cached ? QStringLiteral("Katalog yenilenemedi; son kayıtlı liste gösteriliyor.")
                                : QStringLiteral("Katalog yanıtı geçersiz."));
            else if (data.size() > maxBytes)
                setError(QStringLiteral("Katalog boyutu güvenlik sınırını aştı."));
            setLoading(false);
            return;
        }
        if (QDir().mkpath(cacheDirectory)) {
            QSaveFile file(cachePath);
            if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size())
                file.commit();
        }

        setLoading(false);
    });
}

void CatalogModel::setLoading(bool value)
{
    if (m_loading == value) {
        return;
    }

    m_loading = value;
    emit loadingChanged();
}

void CatalogModel::setError(const QString &message)
{
    if (m_error == message) {
        return;
    }

    m_error = message;
    emit errorChanged();
}

bool CatalogModel::parseCatalog(const QByteArray &jsonData, const QUrl &catalogUrl)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        setError("Katalog JSON hatası: " + parseError.errorString());
        return false;
    }

    if (!document.isObject()) {
        setError("Katalog formatı geçersiz.");
        return false;
    }

    const QJsonObject root = document.object();

    if (root.value("schemaVersion").toInt(-1) != 2) {
        setError("Desteklenmeyen katalog sürümü.");
        return false;
    }

    if (!root.value("apps").isArray()) {
        setError("Katalog uygulama listesi geçersiz.");
        return false;
    }

    const QJsonArray appsArray = root.value("apps").toArray();

    QVector<AppInfo> loadedApps;

    for (const QJsonValue &value : appsArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject obj = value.toObject();

        AppInfo app;
        app.id = obj.value("id").toString();
        app.packageName = obj.value("packageName").toString();
        app.appName = obj.value("name").toString();
        app.summary = obj.value("summary").toString();
        app.description = obj.value("description").toString();
        app.category = obj.value("category").toString();
        app.developer = obj.value("developer").toString();
        app.source = obj.value("source").toString();
        app.latestVersion = obj.value("latestVersion").toString();
        app.installPackage = obj.value("installPackage").toString();
        app.icon = obj.value("icon").toString();

        if (!app.icon.isEmpty()) {
            app.iconUrl = catalogUrl.resolved(QUrl(app.icon)).toString();
        }
        app.featured = obj.value("featured").toBool(false);
        app.priority = obj.value("priority").toInt(999);

        if (!app.id.isEmpty() && !app.packageName.isEmpty() && !app.appName.isEmpty()) {
            loadedApps.append(app);
        }
    }

    std::sort(loadedApps.begin(), loadedApps.end(), [](const AppInfo &a, const AppInfo &b) {
        return a.priority < b.priority;
    });

    if (loadedApps.isEmpty()) {
        setError("Katalog okundu ama gösterilecek uygulama bulunamadı.");
        return false;
    }

    m_allApps = loadedApps;

    rebuildCategories();
    applyFilters();

    return true;
}

void CatalogModel::rebuildCategories()
{
    QStringList newCategories;
    newCategories << "Tümü";

    for (const AppInfo &app : m_allApps) {
        if (!app.category.isEmpty() && !newCategories.contains(app.category)) {
            newCategories << app.category;
        }
    }

    std::sort(newCategories.begin() + 1, newCategories.end());

    if (m_categories == newCategories) {
        return;
    }

    m_categories = newCategories;
    emit categoriesChanged();
}

void CatalogModel::applyFilters()
{
    QVector<AppInfo> filteredApps;

    const QString search = m_searchText.trimmed();

    for (const AppInfo &app : m_allApps) {
        const bool categoryMatches =
            m_categoryFilter.isEmpty()
            || m_categoryFilter == "Tümü"
            || app.category == m_categoryFilter;

        const bool searchMatches =
            search.isEmpty()
            || app.appName.contains(search, Qt::CaseInsensitive)
            || app.summary.contains(search, Qt::CaseInsensitive)
            || app.description.contains(search, Qt::CaseInsensitive)
            || app.packageName.contains(search, Qt::CaseInsensitive)
            || app.category.contains(search, Qt::CaseInsensitive);

        if (categoryMatches && searchMatches) {
            filteredApps.append(app);
        }
    }

    beginResetModel();
    m_apps = filteredApps;
    endResetModel();
}
