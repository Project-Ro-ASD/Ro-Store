#pragma once

#include <QAbstractListModel>
#include <QList>

class PackageTransaction;

class PackageTransactionModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        TransactionRole = Qt::UserRole + 1,
        PackageNameRole,
        OperationRole,
        StateRole,
        ProgressRole,
        DownloadedBytesRole,
        TotalBytesRole,
        ErrorMessageRole
    };
    Q_ENUM(Role)

    explicit PackageTransactionModel(QObject *parent = nullptr);

    int rowCount(
        const QModelIndex &parent = QModelIndex()
    ) const override;

    QVariant data(
        const QModelIndex &index,
        int role
    ) const override;

    QHash<int, QByteArray> roleNames() const override;

    void appendTransaction(PackageTransaction *transaction);

    PackageTransaction *transactionAt(int row) const;

    Q_INVOKABLE QObject *get(int row) const;

private:
    void connectTransaction(PackageTransaction *transaction);

    QList<PackageTransaction *> m_transactions;
};
