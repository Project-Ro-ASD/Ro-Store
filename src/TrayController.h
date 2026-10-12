#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class QMenu;
class QAction;

class TrayController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    explicit TrayController(QObject *parent = nullptr);
    bool available() const;
    bool busy() const;
    Q_INVOKABLE void setBusy(bool busy);
    Q_INVOKABLE void notifyBackground();

signals:
    void showRequested();
    void quitRequested();
    void busyChanged();

private:
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_quitAction = nullptr;
    bool m_busy = false;
};
