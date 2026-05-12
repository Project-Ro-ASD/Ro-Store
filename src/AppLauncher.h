#pragma once

#include <QObject>
#include <QString>

class AppLauncher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit AppLauncher(QObject *parent = nullptr);

    QString statusText() const;

    Q_INVOKABLE bool launch(const QString &command);

signals:
    void statusTextChanged();

private:
    void setStatusText(const QString &text);

private:
    QString m_statusText = "Hazır";
};
