#include "FedoraPackageModel.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTimer>
#include <QSet>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>
#ifdef RO_STORE_HAS_APPSTREAM
#include <AppStreamQt/pool.h>
#include <AppStreamQt/component.h>
#include <AppStreamQt/icon.h>
#endif

namespace {
// Keep a cancelled repoquery alive until the operating system reports exit.
// kill() is asynchronous; deleting a still-running QProcess triggers a Qt warning.
void retireQuery(QProcess *process)
{
    if (!process)
        return;
    if (process->state() == QProcess::NotRunning) {
        process->deleteLater();
        return;
    }
    QObject::connect(process, &QProcess::finished,
                     process, &QObject::deleteLater);
    process->kill();
}
}

FedoraPackageModel::FedoraPackageModel(QObject *parent)
    : QAbstractListModel(parent)
{
    startMetadataLoad();
}

FedoraPackageModel::~FedoraPackageModel()
{
    // The owner may be destroyed while queries are still pending, for example
    // when Ro-Store is closed just after opening the Fedora catalog.
    for (QProcess *process : findChildren<QProcess *>(QString(), Qt::FindDirectChildrenOnly)) {
        QObject::disconnect(process, nullptr, this, nullptr);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(2000);
        }
    }
}

int FedoraPackageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visibleCount;
}

QVariant FedoraPackageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleCount)
        return {};

    const FedoraPackage &pkg = m_packages.at(m_matching.at(index.row()));
    switch (role) {
    case NameRole: return pkg.name;
    case VersionRole: return pkg.version;
    case ArchitectureRole: return pkg.architecture;
    case RepositoryRole: return pkg.repository;
    case SummaryRole: return pkg.summary;
    case DisplayNameRole: return m_metadataByPackage.contains(pkg.name)
        ? m_metadataByPackage.value(pkg.name).title : pkg.name;
    case DisplaySummaryRole: {
        const auto metadata = m_metadataByPackage.value(pkg.name);
        return metadata.summary.isEmpty() ? pkg.summary : metadata.summary;
    }
    case AppCategoryRole: return m_metadataByPackage.value(pkg.name).category;
    case HasAppStreamRole: return m_metadataByPackage.contains(pkg.name);
    case AppIconUrlRole: return m_metadataByPackage.value(pkg.name).iconUrl;
    default: return {};
    }
}

QHash<int, QByteArray> FedoraPackageModel::roleNames() const
{
    return {
        { NameRole, "packageName" },
        { VersionRole, "version" },
        { ArchitectureRole, "architecture" },
        { RepositoryRole, "repository" },
        { SummaryRole, "summary" },
        { DisplayNameRole, "displayName" },
        { DisplaySummaryRole, "displaySummary" },
        { AppCategoryRole, "appCategory" },
        { HasAppStreamRole, "hasAppStream" },
        { AppIconUrlRole, "appIconUrl" }
    };
}

void FedoraPackageModel::setLoading(bool value)
{
    if (m_loading == value)
        return;
    m_loading = value;
    emit loadingChanged();
}

void FedoraPackageModel::setError(const QString &text)
{
    if (m_error == text)
        return;
    m_error = text;
    emit errorChanged();
}

void FedoraPackageModel::search(const QString &name)
{
    const QString query = name.trimmed();
    // Repoquery receives each argument separately, never through a shell.
    // Only literal RPM package-name characters are accepted; no glob injection.
    static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9_.+\\-]{2,64}$"));
    if (!query.isEmpty() && !allowed.match(query).hasMatch()) {
        setError(QStringLiteral("En az 2 karakter içeren bir paket adı girin (harf, rakam, ., _, + veya -)."));
        return;
    }
    runQuery(query);
}

void FedoraPackageModel::refresh()
{
    runQuery(m_query);
}

void FedoraPackageModel::runQuery(const QString &name)
{
    if (m_process) {
        QProcess *old = m_process;
        m_process = nullptr;
        retireQuery(old);
    }

    if (m_query != name) {
        m_query = name;
        emit activeQueryChanged();
    }

    setError({});
    setLoading(true);

    auto *process = new QProcess(this);
    m_process = process;
    process->setProgram(QStringLiteral("dnf5"));

    QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == QStringLiteral("arm64"))
        arch = QStringLiteral("aarch64");
    if (arch == QStringLiteral("amd64"))
        arch = QStringLiteral("x86_64");

    // The queryformat uses actual tab separators and DNF5's escaped newline.
    QStringList arguments = {
        QStringLiteral("repoquery"),
        QStringLiteral("--available"),
        QStringLiteral("--latest-limit=1"),
        QStringLiteral("--arch=") + arch + QStringLiteral(",noarch"),
        QStringLiteral("--queryformat=%{name}\t%{version}\t%{arch}\t%{repoid}\t%{summary}\\n")
    };

    if (name.isEmpty()) {
        // A useful, quick-to-load starting selection. AppStream's full
        // application metadata/catalog is a planned later milestone.
        arguments.append({
            QStringLiteral("firefox"), QStringLiteral("thunderbird"),
            QStringLiteral("libreoffice-writer"), QStringLiteral("gimp"),
            QStringLiteral("inkscape"), QStringLiteral("krita"),
            QStringLiteral("blender"), QStringLiteral("kdenlive"),
            QStringLiteral("audacity"), QStringLiteral("filezilla"),
            QStringLiteral("htop"), QStringLiteral("kate"),
            QStringLiteral("okular"), QStringLiteral("konsole"),
            QStringLiteral("virt-manager"), QStringLiteral("keepassxc"),
            QStringLiteral("obs-studio")
        });
    } else {
        arguments.append(QStringLiteral("*") + name + QStringLiteral("*"));
    }

    process->setArguments(arguments);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (m_process != process || error != QProcess::FailedToStart)
            return;
        m_process = nullptr;
        setLoading(false);
        setError(QStringLiteral("DNF5 başlatılamadı. Fedora'da dnf5 kurulu olmalı."));
        process->deleteLater();
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int code, QProcess::ExitStatus status) {
        if (m_process != process)
            return;
        m_process = nullptr;
        const QByteArray output = process->readAllStandardOutput();
        const QString stderrText = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
        process->deleteLater();
        setLoading(false);
        if (status != QProcess::NormalExit || code != 0) {
            setError(stderrText.isEmpty()
                     ? QStringLiteral("DNF5 paket sorgusu başarısız oldu.")
                     : QStringLiteral("DNF5: ") + stderrText.left(350));
            return;
        }
        if (output.size() > 16 * 1024 * 1024) {
            setError(QStringLiteral("Sonuç çok büyük. Daha özel bir paket adıyla arayın."));
            return;
        }
        parseOutput(output);
    });

    QTimer::singleShot(90000, process, [this, process]() {
        if (m_process == process && process->state() != QProcess::NotRunning) {
            m_process = nullptr;
            retireQuery(process);
            setLoading(false);
            setError(QStringLiteral("DNF5 sorgusu zaman aşımına uğradı. Depo bağlantısını kontrol edip tekrar deneyin."));
        }
    });

    process->start();
}

void FedoraPackageModel::parseOutput(const QByteArray &data)
{
    QVector<FedoraPackage> parsed;
    parsed.reserve(1000);
    QSet<QString> seen;
    bool truncated = false;

    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &line : lines) {
        const QList<QByteArray> fields = line.trimmed().split('\t');
        if (fields.size() < 5)
            continue;

        FedoraPackage pkg;
        pkg.name = QString::fromUtf8(fields.at(0)).trimmed();
        pkg.version = QString::fromUtf8(fields.at(1)).trimmed();
        pkg.architecture = QString::fromUtf8(fields.at(2)).trimmed();
        pkg.repository = QString::fromUtf8(fields.at(3)).trimmed();
        pkg.summary = QString::fromUtf8(fields.mid(4).join("\t")).simplified();
        if (pkg.name.isEmpty() || pkg.version.isEmpty() || pkg.repository.isEmpty())
            continue;
        // Existing Ro-ASD catalog already supplies rich records for this repo.
        if (pkg.repository.startsWith(QStringLiteral("ro-asd"), Qt::CaseInsensitive))
            continue;
        const QString key = pkg.name + QChar('|') + pkg.repository;
        if (seen.contains(key))
            continue;
        if (parsed.size() >= MaxResults) {
            truncated = true;
            break;
        }
        seen.insert(key);
        parsed.append(pkg);
    }

    std::sort(parsed.begin(), parsed.end(), [](const FedoraPackage &a, const FedoraPackage &b) {
        const int byName = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        if (byName != 0)
            return byName < 0;
        return a.repository < b.repository;
    });

    QStringList sources = { QStringLiteral("Tümü") };
    for (const FedoraPackage &pkg : parsed) {
        if (!sources.contains(pkg.repository))
            sources.append(pkg.repository);
    }
    std::sort(sources.begin() + 1, sources.end());

    const bool oldHasMore = hasMore();
    beginResetModel();
    m_packages = std::move(parsed);
    m_matching.clear();
    m_visibleCount = 0;
    m_truncated = truncated;
    endResetModel();

    if (m_sources != sources) {
        m_sources = sources;
        emit sourcesChanged();
    }
    if (!m_sources.contains(m_sourceFilter)) {
        m_sourceFilter = QStringLiteral("Tümü");
        emit sourceFilterChanged();
    }

    rebuildResults();
    emit totalResultsChanged();
    if (oldHasMore != hasMore())
        emit hasMoreChanged();
}

void FedoraPackageModel::rebuildResults()
{
    const bool oldHasMore = hasMore();
    beginResetModel();
    m_matching.clear();
    for (int i = 0; i < m_packages.size(); ++i) {
        if (m_sourceFilter == QStringLiteral("Tümü")
            || m_packages.at(i).repository == m_sourceFilter)
            m_matching.append(i);
    }
    m_visibleCount = std::min(PageSize, int(m_matching.size()));
    endResetModel();
    emit totalResultsChanged();
    if (oldHasMore != hasMore())
        emit hasMoreChanged();
}

void FedoraPackageModel::setSourceFilter(const QString &source)
{
    if (m_sourceFilter == source || !m_sources.contains(source))
        return;
    m_sourceFilter = source;
    emit sourceFilterChanged();
    rebuildResults();
}

void FedoraPackageModel::loadMore()
{
    if (!hasMore())
        return;
    const int extra = std::min(PageSize, int(m_matching.size()) - m_visibleCount);
    const int first = m_visibleCount;
    beginInsertRows({}, first, first + extra - 1);
    m_visibleCount += extra;
    endInsertRows();
    emit hasMoreChanged();
}

void FedoraPackageModel::notifyMetadataUpdated()
{
    if (m_visibleCount > 0)
        emit dataChanged(index(0, 0), index(m_visibleCount - 1, 0),
                         {DisplayNameRole, DisplaySummaryRole, AppCategoryRole, HasAppStreamRole});
}

#ifdef RO_STORE_HAS_APPSTREAM
namespace {
// Prefer locally installed/cached icon files. Remote AppStream icons are not
// fetched implicitly: an unavailable image must fall back to the text avatar.
QString appStreamIconSource(const AppStream::Component &component)
{
    const auto icons = component.icons();
    for (const auto &icon : icons) {
        if (icon.kind() != AppStream::Icon::KindCached
            && icon.kind() != AppStream::Icon::KindLocal)
            continue;

        const QUrl url = icon.url();
        QString filePath;
        if (url.isLocalFile())
            filePath = url.toLocalFile();
        if (filePath.isEmpty())
            filePath = icon.filename();
        const QFileInfo file(filePath);
        if (file.isAbsolute() && file.isFile() && file.isReadable())
            return QUrl::fromLocalFile(file.absoluteFilePath()).toString();
    }
    for (const auto &icon : icons) {
        if (icon.kind() != AppStream::Icon::KindStock)
            continue;
        const QString name = icon.name().trimmed();
        static const QRegularExpression safeName(QStringLiteral("^[A-Za-z0-9._-]{1,128}$"));
        if (safeName.match(name).hasMatch())
            return QStringLiteral("image://rostore-theme/") + name;
    }
    return {};
}
}
#endif

void FedoraPackageModel::startMetadataLoad()
{
#ifdef RO_STORE_HAS_APPSTREAM
    // AppStream metadata is advisory only. RPM identity, repository and version
    // always come exclusively from DNF5 repoquery results.
    auto *pool = new AppStream::Pool(this);
    pool->setFlags(AppStream::Pool::Flags(AppStream::Pool::FlagLoadOsCatalog)
                   | AppStream::Pool::FlagLoadOsMetainfo);
    qInfo() << "APPSTREAM METADATA LOAD START";
    connect(pool, &AppStream::Pool::loadFinished, this, [this, pool](bool success) {
        if (!success) {
            qWarning() << "APPSTREAM METADATA UNAVAILABLE:" << pool->lastError();
            return; // Existing Fedora RPM catalog remains fully functional.
        }
        QHash<QString, AppMetadata> next;
        const auto components = pool->components();
        for (const auto &component : components) {
            if (component.kind() != AppStream::Component::KindDesktopApp)
                continue;
            const QString title = component.name().trimmed();
            if (title.isEmpty())
                continue;
            const QString summary = component.summary().simplified();
            const QString category = component.categories().value(0);
            const QString iconUrl = appStreamIconSource(component);
            for (const auto &packageName : component.packageNames()) {
                if (!packageName.isEmpty() && !next.contains(packageName))
                    next.insert(packageName, {title, summary, category, iconUrl});
            }
        }
        m_metadataByPackage = std::move(next);
        qInfo() << "APPSTREAM RPM METADATA LOADED:" << m_metadataByPackage.size();
        notifyMetadataUpdated();
    });
    pool->loadAsync();
#endif
}
