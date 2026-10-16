#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>
#include <memory>
#include <QVariantList>

// Persistent user-owned staging area. This component does NOT download,
// install, or assume any RPM file is safe to execute.
class SystemUpdateCache : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path CONSTANT)
    Q_PROPERTY(int rpmCount READ rpmCount NOTIFY changed)
    Q_PROPERTY(qulonglong bytes READ bytes NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY changed)
    Q_PROPERTY(QString downloadStatus READ downloadStatus NOTIFY changed)
public:
    explicit SystemUpdateCache(QObject *parent = nullptr);
    ~SystemUpdateCache() override;
    QString path() const;
    int rpmCount() const { return m_count; }
    qulonglong bytes() const { return m_bytes; }
    QString error() const { return m_error; }
    bool downloading() const { return m_downloading; }
    QString downloadStatus() const { return m_downloadStatus; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void downloadSelected(const QStringList &nevras);
    Q_INVOKABLE void cancelDownload();

signals:
    void changed();

private:
    void completeDownload(bool cancelled, int exitCode);
    QString m_root;
    QPointer<QProcess> m_downloadProcess;
    std::unique_ptr<QTemporaryDir> m_staging;
    QStringList m_requested;
    QString m_downloadStatus;
    bool m_downloading = false;
    int m_count = 0;
    qulonglong m_bytes = 0;
    QString m_error;
};
