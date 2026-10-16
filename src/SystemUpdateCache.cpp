#include "SystemUpdateCache.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

SystemUpdateCache::SystemUpdateCache(QObject *parent) : QObject(parent)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    // Keep new system-update assets away from the v0.5 temporary download
    // folders, where cancellation currently deletes in-flight RPMs.
    m_root = QDir(base).filePath(QStringLiteral("system-updates/rpms"));
    refresh();
}

QString SystemUpdateCache::path() const { return m_root; }

void SystemUpdateCache::refresh()
{
    m_error.clear();
    m_count = 0;
    m_bytes = 0;
    QDir folder(m_root);
    if (!folder.exists()) {
        emit changed();
        return;
    }
    if (QFileInfo(m_root).isSymLink()) {
        m_error = QStringLiteral("Önbellek klasörü sembolik bağlantı olamaz.");
        emit changed();
        return;
    }
    QDirIterator iterator(m_root, {QStringLiteral("*.rpm")}, QDir::Files | QDir::NoSymLinks);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo file = iterator.fileInfo();
        if (!file.isFile() || file.isSymLink())
            continue;
        ++m_count;
        m_bytes += static_cast<qulonglong>(file.size());
    }
    emit changed();
}

void SystemUpdateCache::clear()
{
    m_error.clear();
    const QFileInfo folderInfo(m_root);
    if (!folderInfo.exists()) {
        refresh();
        return;
    }
    if (!folderInfo.isDir() || folderInfo.isSymLink()) {
        m_error = QStringLiteral("Önbellek klasörü güvenli değil; silme engellendi.");
        emit changed();
        return;
    }
    // Only remove direct child RPMs in this dedicated application cache.
    // No recursion, wildcards outside our directory, or external dnf cache.
    QDir folder(m_root);
    const QFileInfoList items = folder.entryInfoList(
        {QStringLiteral("*.rpm")}, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &item : items) {
        if (!item.isFile() || item.isSymLink())
            continue;
        if (!QFile::remove(item.absoluteFilePath()))
            m_error = QStringLiteral("Bazı RPM dosyaları silinemedi.");
    }
    const QString result = m_error;
    refresh();
    if (!result.isEmpty()) {
        m_error = result;
        emit changed();
    }
}
