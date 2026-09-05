#include "orderfilterproxymodel.h"
#include "ordertablemodel.h"
namespace takeout {
OrderFilterProxyModel::OrderFilterProxyModel(QObject* parent) : QSortFilterProxyModel(parent) {
    setSortRole(OrderTableModel::SortRole);
    setDynamicSortFilter(true);
}
void OrderFilterProxyModel::setFilter(OrderFilter filter) {
    beginFilterChange();
    m_filter = std::move(filter);
    endFilterChange(Direction::Rows);
}
bool OrderFilterProxyModel::filterAcceptsRow(int row, const QModelIndex& parent) const {
    if (!sourceModel()) return false;
    const auto idIndex = sourceModel()->index(row, OrderTableModel::OrderId, parent);
    if (!idIndex.data(OrderTableModel::IdRole).toString().contains(m_filter.keyword, Qt::CaseInsensitive)) return false;
    if (m_filter.status && idIndex.data(OrderTableModel::StatusRole).toInt() != int(*m_filter.status)) return false;
    const auto time = sourceModel()->index(row, OrderTableModel::CreatedAt, parent).data(OrderTableModel::SortRole).toDateTime();
    if (m_filter.from && time < *m_filter.from) return false;
    if (m_filter.until && time >= *m_filter.until) return false;
    return true;
}
} // namespace takeout
