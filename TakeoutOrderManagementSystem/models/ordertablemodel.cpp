#include "ordertablemodel.h"
#include <QThread>
namespace takeout {
OrderTableModel::OrderTableModel(QObject* parent) : QAbstractTableModel(parent) {}
void OrderTableModel::replaceProjection(QVector<OrderRow> rows) {
    Q_ASSERT(thread() == QThread::currentThread());
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}
int OrderTableModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : int(m_rows.size()); }
int OrderTableModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : ColumnCount; }
QVariant OrderTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.model() != this || index.row() < 0 || index.row() >= m_rows.size()
        || index.column() < 0 || index.column() >= ColumnCount) return {};
    const auto& row = m_rows.at(index.row());
    if (role == IdRole) return row.id;
    if (role == StatusRole) return int(row.status);
    if (role != Qt::DisplayRole && role != SortRole) return {};
    switch (index.column()) {
    case OrderId: return row.id;
    case ShopName: return row.shopName;
    case Status: return role == SortRole ? QVariant(int(row.status)) : QVariant(statusLabel(row.status));
    case Total: return QVariant::fromValue(row.totalCents); // Delegate formats; sorting stays numeric.
    case CreatedAt:
        return role == SortRole ? QVariant(row.createdAt) : QVariant(row.createdAt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss"));
    }
    return {};
}
QVariant OrderTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || section < 0) return {};
    if (orientation == Qt::Vertical) return section + 1;
    const QStringList headers = {QStringLiteral("订单号"), QStringLiteral("店铺"), QStringLiteral("订单状态"),
        QStringLiteral("金额"), QStringLiteral("创建时间")};
    return section < headers.size() ? QVariant(headers.at(section)) : QVariant{};
}
Qt::ItemFlags OrderTableModel::flags(const QModelIndex& index) const {
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}
} // namespace takeout
