#include "PackageTransactionModel.h"
#include "PackageTransaction.h"

PackageTransactionModel::PackageTransactionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PackageTransactionModel::rowCount(
    const QModelIndex &parent
) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_transactions.size();
}

QVariant PackageTransactionModel::data(
    const QModelIndex &index,
    int role
) const
{
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= m_transactions.size()) {
        return {};
    }

    PackageTransaction *transaction =
        m_transactions.at(index.row());

    switch (role) {
    case TransactionRole:
        return QVariant::fromValue(
            static_cast<QObject *>(transaction)
        );

    case PackageNameRole:
        return transaction->packageName();

    case OperationRole:
        return QVariant::fromValue(
            transaction->operation()
        );

    case StateRole:
        return QVariant::fromValue(
            transaction->state()
        );

    case ProgressRole:
        return transaction->progress();

    case DownloadedBytesRole:
        return QVariant::fromValue(
            transaction->downloadedBytes()
        );

    case TotalBytesRole:
        return QVariant::fromValue(
            transaction->totalBytes()
        );

    case ErrorMessageRole:
        return transaction->errorMessage();

    default:
        return {};
    }
}

QHash<int, QByteArray> PackageTransactionModel::roleNames() const
{
    return {
        { TransactionRole, "transaction" },
        { PackageNameRole, "packageName" },
        { OperationRole, "operation" },
        { StateRole, "state" },
        { ProgressRole, "progress" },
        { DownloadedBytesRole, "downloadedBytes" },
        { TotalBytesRole, "totalBytes" },
        { ErrorMessageRole, "errorMessage" }
    };
}

void PackageTransactionModel::appendTransaction(
    PackageTransaction *transaction
)
{
    if (!transaction ||
        m_transactions.contains(transaction)) {
        return;
    }

    if (!transaction->parent()) {
        transaction->setParent(this);
    }

    const int row = m_transactions.size();

    beginInsertRows(
        QModelIndex(),
        row,
        row
    );

    m_transactions.append(transaction);

    endInsertRows();

    connectTransaction(transaction);
}

PackageTransaction *PackageTransactionModel::transactionAt(
    int row
) const
{
    if (row < 0 || row >= m_transactions.size()) {
        return nullptr;
    }

    return m_transactions.at(row);
}

QObject *PackageTransactionModel::get(int row) const
{
    return transactionAt(row);
}

void PackageTransactionModel::connectTransaction(
    PackageTransaction *transaction
)
{
    const auto updateRow =
        [this, transaction]() {
            const int row =
                m_transactions.indexOf(transaction);

            if (row < 0) {
                return;
            }

            const QModelIndex modelIndex =
                index(row, 0);

            emit dataChanged(
                modelIndex,
                modelIndex
            );
        };

    connect(
        transaction,
        &PackageTransaction::stateChanged,
        this,
        updateRow
    );

    connect(
        transaction,
        &PackageTransaction::progressChanged,
        this,
        updateRow
    );

    connect(
        transaction,
        &PackageTransaction::downloadedBytesChanged,
        this,
        updateRow
    );

    connect(
        transaction,
        &PackageTransaction::totalBytesChanged,
        this,
        updateRow
    );

    connect(
        transaction,
        &PackageTransaction::errorMessageChanged,
        this,
        updateRow
    );

    connect(
        transaction,
        &PackageTransaction::resolvedItemsChanged,
        this,
        updateRow
    );

    connect(
        transaction,
        &QObject::destroyed,
        this,
        [this, transaction]() {
            const int row =
                m_transactions.indexOf(transaction);

            if (row < 0) {
                return;
            }

            beginRemoveRows(
                QModelIndex(),
                row,
                row
            );

            m_transactions.removeAt(row);

            endRemoveRows();
        }
    );
}
