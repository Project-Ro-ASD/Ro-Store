#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QPointer>
#include <QProcess>
#include <QVector>

struct SystemUpdateItem
{
    QString name;
    QString version;
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

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        VersionRole,
        ArchitectureRole,
        RepositoryRole,
        KernelUpdateRole
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

    // Read-only: queries enabled DNF5 repositories; never creates transactions.
    Q_INVOKABLE void refresh();

signals:
    void loadingChanged();
    void updatesChanged();
    void errorChanged();

private:
    void setLoading(bool value);
    void setError(const QString &value);
    void parseUpdates(const QByteArray &output);

    QVector<SystemUpdateItem> m_updates;
    QPointer<QProcess> m_process;
    QByteArray m_stdout;
    QString m_error;
    QDateTime m_lastChecked;
    bool m_loading = false;
    bool m_checked = false;
    bool m_hasKernelUpdates = false;

    static constexpr int MaxOutputBytes = 16 * 1024 * 1024;
};
