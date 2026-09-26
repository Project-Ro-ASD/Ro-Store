#include "PackageTransaction.h"

#include <QtGlobal>

PackageTransaction::PackageTransaction(
    const QString &packageName,
    Operation operation,
    QObject *parent
)
    : QObject(parent),
      m_packageName(packageName),
      m_operation(operation)
{
}

QString PackageTransaction::packageName() const
{
    return m_packageName;
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
