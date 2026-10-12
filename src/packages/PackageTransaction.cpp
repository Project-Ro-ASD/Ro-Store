#include "PackageTransaction.h"

#include <QtGlobal>
#include <QDateTime>
#include <QClipboard>
#include <QGuiApplication>

PackageTransaction::PackageTransaction(
    const QString &packageName,
    Operation operation,
    const QString &repositoryId,
    QObject *parent
)
    : QObject(parent),
      m_packageName(packageName),
      m_repositoryId(repositoryId),
      m_operation(operation)
{
}

QString PackageTransaction::packageName() const
{
    return m_packageName;
}

void PackageTransaction::copyTechnicalLog(const QString &text) const
{
    if (QGuiApplication::clipboard())
        QGuiApplication::clipboard()->setText(text);
}

QString PackageTransaction::repositoryId() const
{
    return m_repositoryId;
}

QString PackageTransaction::technicalLog() const
{
    return m_technicalLog;
}

void PackageTransaction::appendTechnicalLog(const QString &message)
{
    // Keep the diagnostic history bound to the transaction so it survives
    // manager session recycling and can be copied from QML after failure.
    const QString line = QStringLiteral("[%1] %2\n")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs), message);
    m_technicalLog += line;
    constexpr qsizetype maxSize = 96 * 1024;
    if (m_technicalLog.size() > maxSize) {
        m_technicalLog = QStringLiteral("[Earlier entries truncated]\n")
                         + m_technicalLog.right(maxSize - 200);
    }
    emit technicalLogChanged();
}

PackageTransaction::Operation PackageTransaction::operation() const
{
    return m_operation;
}

PackageTransaction::State PackageTransaction::state() const
{
    return m_state;
}

int PackageTransaction::progress() const
{
    return m_progress;
}

qulonglong PackageTransaction::downloadedBytes() const
{
    return m_downloadedBytes;
}

qulonglong PackageTransaction::totalBytes() const
{
    return m_totalBytes;
}

qulonglong PackageTransaction::downloadSpeedBytesPerSecond() const
{
    return m_downloadSpeedBytesPerSecond;
}

QString PackageTransaction::errorMessage() const
{
    return m_errorMessage;
}

QVariantList PackageTransaction::resolvedItems() const
{
    return m_resolvedItems;
}

int PackageTransaction::resolvedItemCount() const
{
    return m_resolvedItems.size();
}

bool PackageTransaction::isFinished() const
{
    return m_state == State::Finished ||
           m_state == State::Failed ||
           m_state == State::Cancelled;
}

void PackageTransaction::setState(State state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    emit stateChanged();
}

void PackageTransaction::setProgress(int progress)
{
    const int boundedProgress = qBound(0, progress, 100);

    if (m_progress == boundedProgress) {
        return;
    }

    m_progress = boundedProgress;
    emit progressChanged();
}

void PackageTransaction::setDownloadedBytes(qulonglong bytes)
{
    if (m_downloadedBytes == bytes) {
        return;
    }

    m_downloadedBytes = bytes;
    emit downloadedBytesChanged();
}

void PackageTransaction::setTotalBytes(qulonglong bytes)
{
    if (m_totalBytes == bytes) {
        return;
    }

    m_totalBytes = bytes;
    emit totalBytesChanged();
}

void PackageTransaction::setDownloadSpeedBytesPerSecond(
    qulonglong bytesPerSecond
)
{
    if (m_downloadSpeedBytesPerSecond == bytesPerSecond) {
        return;
    }

    m_downloadSpeedBytesPerSecond = bytesPerSecond;
    emit downloadSpeedBytesPerSecondChanged();
}

void PackageTransaction::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }

    m_errorMessage = message;
    emit errorMessageChanged();
}

void PackageTransaction::setResolvedItems(
    const QVariantList &items
)
{
    if (m_resolvedItems == items) {
        return;
    }

    m_resolvedItems = items;
    emit resolvedItemsChanged();
}

void PackageTransaction::fail(const QString &message)
{
    setErrorMessage(message);
    setState(State::Failed);
}
