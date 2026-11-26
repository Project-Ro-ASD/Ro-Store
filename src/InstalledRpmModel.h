#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QHash>
#include <QStringList>
#include <QVariantList>

class InstalledRpmModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList packages READ packages NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool applicationMetadataReady READ applicationMetadataReady NOTIFY changed)

public:
    explicit InstalledRpmModel(QObject *parent = nullptr);
    ~InstalledRpmModel() override;

    QVariantList packages() const { return m_packages; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    bool applicationMetadataReady() const { return m_applicationMetadataReady; }
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    QPointer<QProcess> m_process;
    QVariantList m_packages;
    QString m_error;
    QHash<QString, QString> m_applicationNames;
    QHash<QString, QStringList> m_applicationIds;
    bool m_applicationMetadataReady = false;
    bool m_loading = false;
    void annotateApplications();
};
