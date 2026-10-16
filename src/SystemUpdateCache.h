#pragma once

#include <QObject>
#include <QString>
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
public:
    explicit SystemUpdateCache(QObject *parent = nullptr);
    QString path() const;
    int rpmCount() const { return m_count; }
    qulonglong bytes() const { return m_bytes; }
    QString error() const { return m_error; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    QString m_root;
    int m_count = 0;
    qulonglong m_bytes = 0;
    QString m_error;
};
