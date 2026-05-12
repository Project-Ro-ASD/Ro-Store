#include "CatalogModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
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

void CatalogModel::load(const QUrl &url)
{
    setLoading(true);
    setError("");

    QNetworkRequest request(url);
    QNetworkReply *reply = m_network.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            setLoading(false);
            setError("Katalog indirilemedi: " + reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();
        parseCatalog(data);

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

void CatalogModel::parseCatalog(const QByteArray &jsonData)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        setError("Katalog JSON hatası: " + parseError.errorString());
        return;
    }

    if (!document.isObject()) {
        setError("Katalog formatı geçersiz.");
        return;
    }

    const QJsonObject root = document.object();
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
        app.iconUrl = obj.value("iconUrl").toString();
        app.featured = obj.value("featured").toBool(false);
        app.priority = obj.value("priority").toInt(999);

        if (!app.id.isEmpty() && !app.packageName.isEmpty() && !app.appName.isEmpty()) {
            loadedApps.append(app);
        }
    }

    std::sort(loadedApps.begin(), loadedApps.end(), [](const AppInfo &a, const AppInfo &b) {
        return a.priority < b.priority;
    });

    m_allApps = loadedApps;

    rebuildCategories();
    applyFilters();

    if (m_allApps.isEmpty()) {
        setError("Katalog okundu ama gösterilecek uygulama bulunamadı.");
    }
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
