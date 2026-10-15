#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QHash>
#include <QProcess>
#include <QStringList>
#include <QVector>

struct FedoraPackage
{
    QString name;
    QString version;
    QString architecture;
    QString repository;
    QString summary;
};

class FedoraPackageModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString activeQuery READ activeQuery NOTIFY activeQueryChanged)
    Q_PROPERTY(QStringList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QString sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY sourceFilterChanged)
    Q_PROPERTY(QStringList categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY categoryFilterChanged)
    Q_PROPERTY(int totalResults READ totalResults NOTIFY totalResultsChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(bool truncated READ truncated NOTIFY totalResultsChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        VersionRole,
        ArchitectureRole,
        RepositoryRole,
        SummaryRole,
        DisplayNameRole,
        DisplaySummaryRole,
        AppCategoryRole,
        HasAppStreamRole,
        AppIconUrlRole,
        AppDescriptionRole,
        AppScreenshotUrlsRole
    };

    explicit FedoraPackageModel(QObject *parent = nullptr);
    ~FedoraPackageModel() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    QString activeQuery() const { return m_query; }
    QStringList sources() const { return m_sources; }
    QString sourceFilter() const { return m_sourceFilter; }
    QStringList categories() const { return m_categories; }
    QString categoryFilter() const { return m_categoryFilter; }
    int totalResults() const { return m_matching.size(); }
    bool hasMore() const { return m_visibleCount < m_matching.size(); }
    bool truncated() const { return m_truncated; }

    Q_INVOKABLE void search(const QString &name);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    void setSourceFilter(const QString &source);
    void setCategoryFilter(const QString &category);

signals:
    void loadingChanged();
    void errorChanged();
    void activeQueryChanged();
    void sourcesChanged();
    void sourceFilterChanged();
    void categoriesChanged();
    void categoryFilterChanged();
    void totalResultsChanged();
    void hasMoreChanged();

private:
    void runQuery(const QString &name);
    void parseOutput(const QByteArray &data);
    void rebuildResults();
    void setLoading(bool value);
    void setError(const QString &text);
    void startMetadataLoad();
    void notifyMetadataUpdated();
    struct AppMetadata {
        QString title;
        QString summary;
        QString category;
        QString iconUrl;
        QString description;
        QStringList screenshotUrls;
    };

    QVector<FedoraPackage> m_packages;
    QHash<QString, AppMetadata> m_metadataByPackage;
    QVector<int> m_matching;
    QPointer<QProcess> m_process;
    QStringList m_sources = { QStringLiteral("Tümü") };
    QString m_sourceFilter = QStringLiteral("Tümü");
    QStringList m_categories = { QStringLiteral("Tümü") };
    QString m_categoryFilter = QStringLiteral("Tümü");
    QString m_query;
    QString m_error;
    bool m_loading = false;
    bool m_truncated = false;
    int m_visibleCount = 0;
    static constexpr int PageSize = 48;
    static constexpr int MaxResults = 3000;
};
