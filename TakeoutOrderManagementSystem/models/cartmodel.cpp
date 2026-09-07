#include "cartmodel.h"
#include <QThread>

namespace takeout {
CartModel::CartModel(QObject *parent) : QAbstractTableModel(parent) {}
void CartModel::replaceProjection(const Id &shopId, QVector<CartRow> rows) {
  Q_ASSERT(thread() == QThread::currentThread());
  beginResetModel();
  m_shopId = shopId;
  m_rows = std::move(rows);
  endResetModel();
}
int CartModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_rows.size());
}
int CartModel::columnCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : ColumnCount;
}
QVariant CartModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.model() != this || index.row() < 0 ||
      index.row() >= m_rows.size() || index.column() < 0 ||
      index.column() >= ColumnCount)
    return {};
  const auto &row = m_rows.at(index.row());
  if (role == DishIdRole)
    return row.dishId;
  if (role == ShopIdRole)
    return row.shopId;
  if (role != Qt::DisplayRole && role != SortRole)
    return {};
  switch (index.column()) {
  case Dish:
    return row.dishName;
  case UnitPrice:
    return QVariant::fromValue(row.unitPriceCents);
  case Quantity:
    return row.quantity;
  case LineTotal:
    return QVariant::fromValue(row.lineTotalCents);
  default:
    return {};
  }
}
QVariant CartModel::headerData(int section, Qt::Orientation orientation,
                               int role) const {
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section < 0)
    return {};
  const QStringList headers{QStringLiteral("菜品"), QStringLiteral("单价"),
                            QStringLiteral("数量"), QStringLiteral("小计")};
  return section < headers.size() ? QVariant(headers.at(section)) : QVariant{};
}
Qt::ItemFlags CartModel::flags(const QModelIndex &index) const {
  return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                         : Qt::NoItemFlags;
}
QVector<CartItem> CartModel::items() const {
  QVector<CartItem> result;
  result.reserve(m_rows.size());
  for (const auto &row : m_rows)
    result.push_back({row.dishId, row.quantity});
  return result;
}
qsizetype CartModel::indexOfDish(const Id &dishId) const {
  for (qsizetype i = 0; i < m_rows.size(); ++i)
    if (m_rows.at(i).dishId == dishId)
      return i;
  return -1;
}
} // namespace takeout
