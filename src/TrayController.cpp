#include "TrayController.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>

TrayController::TrayController(QObject *parent) : QObject(parent)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_menu = new QMenu();
    QAction *openAction = m_menu->addAction(QStringLiteral("Ro-Store'u aç"));
    m_quitAction = m_menu->addAction(QStringLiteral("Ro-Store'dan çık"));

    connect(openAction, &QAction::triggered, this, &TrayController::showRequested);
    connect(m_quitAction, &QAction::triggered, this, &TrayController::quitRequested);

    m_tray = new QSystemTrayIcon(this);
    QIcon icon = QIcon::fromTheme(QStringLiteral("ro-store"));
    if (icon.isNull())
        icon = QIcon::fromTheme(QStringLiteral("system-software-install"));
    m_tray->setIcon(icon);
    m_tray->setToolTip(QStringLiteral("Ro-Store"));
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
        [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger
                || reason == QSystemTrayIcon::DoubleClick)
                emit showRequested();
        });
    m_tray->show();
}

bool TrayController::available() const
{
    return m_tray != nullptr;
}

bool TrayController::busy() const
{
    return m_busy;
}

void TrayController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    if (m_tray)
        m_tray->setToolTip(busy ? QStringLiteral("Ro-Store — paket işlemi sürüyor")
                                : QStringLiteral("Ro-Store"));
    emit busyChanged();
}

void TrayController::notifyBackground()
{
    if (m_tray && m_tray->supportsMessages()) {
        m_tray->showMessage(QStringLiteral("Ro-Store"),
            m_busy ? QStringLiteral("Paket işlemi arka planda devam ediyor.")
                   : QStringLiteral("Ro-Store sistem tepsisinde çalışıyor."),
            QSystemTrayIcon::Information, 3000);
    }
}
