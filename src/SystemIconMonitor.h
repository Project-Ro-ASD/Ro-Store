#pragma once

#include <QDBusConnection>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

// KDE broadcasts icon changes on /KIconLoader. Also watch kdeglobals,
// as some themes/platform integrations update the config without notifying
// an already-running Qt Quick Controls page.
class SystemIconMonitor final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit SystemIconMonitor(QObject *parent = nullptr)
        : QObject(parent)
        , m_configPath(QStandardPaths::writableLocation(
              QStandardPaths::GenericConfigLocation) + QStringLiteral("/kdeglobals"))
        , m_themeName(readTheme())
    {
        m_debounce.setSingleShot(true);
        m_debounce.setInterval(100);

        connect(&m_debounce, &QTimer::timeout,
                this, &SystemIconMonitor::checkTheme);
        connect(&m_watcher, &QFileSystemWatcher::fileChanged,
                this, [this]() { scheduleCheck(false); });
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
                this, [this]() { scheduleCheck(false); });

        watchConfig();

        // KIconLoader::iconChanged(int) is emitted by KDE when the active
        // icon theme is refreshed, including during a live theme switch.
        QDBusConnection::sessionBus().connect(
            QString(), QStringLiteral("/KIconLoader"),
            QStringLiteral("org.kde.KIconLoader"),
            QStringLiteral("iconChanged"),
            this, SLOT(onKdeIconChanged(int)));
    }

    int revision() const { return m_revision; }

signals:
    void revisionChanged();

private slots:
    void onKdeIconChanged(int)
    {
        // Even if the theme name does not change, KDE can refresh its icons.
        scheduleCheck(true);
    }

private:
    QString readTheme() const
    {
        QSettings config(m_configPath, QSettings::IniFormat);
        return config.value(QStringLiteral("Icons/Theme")).toString();
    }

    void watchConfig()
    {
        const QString directory = QFileInfo(m_configPath).absolutePath();

        if (QFileInfo::exists(directory)
            && !m_watcher.directories().contains(directory)) {
            m_watcher.addPath(directory);
        }

        // KDE may atomically replace kdeglobals, dropping the file watch.
        if (QFileInfo::exists(m_configPath)
            && !m_watcher.files().contains(m_configPath)) {
            m_watcher.addPath(m_configPath);
        }
    }

    void scheduleCheck(bool forceRefresh)
    {
        m_forceRefresh = m_forceRefresh || forceRefresh;
        m_debounce.start();
    }

    void checkTheme()
    {
        watchConfig();
        const QString current = readTheme();
        if (current != m_themeName || m_forceRefresh) {
            m_themeName = current;
            ++m_revision;
            emit revisionChanged();
        }
        m_forceRefresh = false;
    }

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    const QString m_configPath;
    QString m_themeName;
    int m_revision = 0;
    bool m_forceRefresh = false;
};
