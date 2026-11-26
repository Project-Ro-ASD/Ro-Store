#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QVariantList>

class InstalledRpmModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList packages READ packages NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    explicit InstalledRpmModel(QObject *parent = nullptr);
    ~InstalledRpmModel() override;

    QVariantList packages() const { return m_packages; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    QPointer<QProcess> m_process;
    QVariantList m_packages;
    QString m_error;
    bool m_loading = false;
};
