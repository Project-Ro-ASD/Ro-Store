#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QVariantList>

class SoftwareHistoryModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList rpmHistory READ rpmHistory NOTIFY changed)
    Q_PROPERTY(QVariantList flatpakHistory READ flatpakHistory NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString details READ details NOTIFY changed)
    Q_PROPERTY(bool detailsLoading READ detailsLoading NOTIFY changed)

public:
    explicit SoftwareHistoryModel(QObject *parent = nullptr);
    ~SoftwareHistoryModel() override;
    QVariantList rpmHistory() const { return m_rpmHistory; }
    QVariantList flatpakHistory() const { return m_flatpakHistory; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    QString details() const { return m_details; }
    bool detailsLoading() const { return m_detailsLoading; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadDetails(qint64 id);

signals:
    void changed();

private:
    void runNext();
    QPointer<QProcess> m_process;
    QPointer<QProcess> m_detailProcess;
    QVariantList m_rpmHistory;
    QVariantList m_flatpakHistory;
    QString m_error;
    QString m_details;
    bool m_loading = false;
    bool m_detailsLoading = false;
    int m_step = 0;
};
