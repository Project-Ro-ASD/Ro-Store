#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QVector>

struct SystemUpdateItem
{
    QString name;
    QString version;
    QString installedVersion;
    QString architecture;
    QString repository;
};

class SystemUpdateModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool checked READ checked NOTIFY updatesChanged)
    Q_PROPERTY(int updateCount READ updateCount NOTIFY updatesChanged)
    Q_PROPERTY(bool hasKernelUpdates READ hasKernelUpdates NOTIFY updatesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QDateTime lastChecked READ lastChecked NOTIFY updatesChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectedKernelCount READ selectedKernelCount NOTIFY selectionChanged)
    Q_PROPERTY(bool previewRunning READ previewRunning NOTIFY previewChanged)
    Q_PROPERTY(QString previewText READ previewText NOTIFY previewChanged)
    Q_PROPERTY(QString previewError READ previewError NOTIFY previewChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        VersionRole,
        InstalledVersionRole,
        ArchitectureRole,
        RepositoryRole,
        KernelUpdateRole,
        SelectedRole
    };

    explicit SystemUpdateModel(QObject *parent = nullptr);
    ~SystemUpdateModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const { return m_loading; }
    bool checked() const { return m_checked; }
    int updateCount() const { return m_updates.size(); }
    bool hasKernelUpdates() const { return m_hasKernelUpdates; }
    QString error() const { return m_error; }
    QDateTime lastChecked() const { return m_lastChecked; }
    int selectedCount() const { return m_selected.size(); }
    int selectedKernelCount() const;
    bool previewRunning() const { return m_previewRunning; }
    QString previewText() const { return m_previewText; }
    QString previewError() const { return m_previewError; }

    // Read-only: queries enabled DNF5 repositories; never creates transactions.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setSelected(const QString &name, const QString &architecture, bool selected);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void previewPlan();
    Q_INVOKABLE void cancelPreview();

signals:
    void loadingChanged();
    void updatesChanged();
    void errorChanged();
    void selectionChanged();
    void previewChanged();

private:
    void setLoading(bool value);
    void setError(const QString &value);
    void parseUpdates(const QByteArray &output);
    void loadInstalledVersions();
    void applyInstalledVersions(const QByteArray &output);
    void invalidatePreview();
    void setPreviewRunning(bool value);

    static QString updateKey(const SystemUpdateItem &item);

    QVector<SystemUpdateItem> m_updates;
    QSet<QString> m_selected;
    QPointer<QProcess> m_process;
    QPointer<QProcess> m_previewProcess;
    QByteArray m_previewBuffer;
    QString m_previewText;
    QString m_previewError;
    bool m_previewRunning = false;
    QByteArray m_stdout;
    QString m_error;
    QDateTime m_lastChecked;
    bool m_loading = false;
    bool m_checked = false;
    bool m_hasKernelUpdates = false;

    static constexpr int MaxOutputBytes = 16 * 1024 * 1024;
};
