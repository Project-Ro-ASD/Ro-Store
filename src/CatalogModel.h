#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QStringList>
#include <QVector>

struct AppInfo
{
    QString id;
    QString packageName;
    QString appName;
    QString summary;
    QString description;
    QString category;
    QString developer;
    QString source;
    QString latestVersion;
    QString installPackage;
    QString icon;
    QString iconUrl;
    bool featured = false;
    int priority = 999;
};

class CatalogModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY categoryFilterChanged)
    Q_PROPERTY(QStringList categories READ categories NOTIFY categoriesChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        PackageNameRole,
        AppNameRole,
        SummaryRole,
        DescriptionRole,
        CategoryRole,
        DeveloperRole,
        SourceRole,
        LatestVersionRole,
        InstallPackageRole,
        IconRole,
        IconUrlRole,
        FeaturedRole,
        PriorityRole
    };

    explicit CatalogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const;
    QString error() const;

    QString searchText() const;
    void setSearchText(const QString &text);

    QString categoryFilter() const;
    void setCategoryFilter(const QString &category);

    QStringList categories() const;

    Q_INVOKABLE void load(const QUrl &url);

signals:
    void loadingChanged();
    void errorChanged();
    void searchTextChanged();
    void categoryFilterChanged();
    void categoriesChanged();

private:
    void setLoading(bool value);
    void setError(const QString &message);
    void parseCatalog(const QByteArray &jsonData);
    void rebuildCategories();
    void applyFilters();

private:
    QVector<AppInfo> m_allApps;
    QVector<AppInfo> m_apps;

    QNetworkAccessManager m_network;

    bool m_loading = false;
    QString m_error;

    QString m_searchText;
    QString m_categoryFilter = "Tümü";
    QStringList m_categories = { "Tümü" };
};
