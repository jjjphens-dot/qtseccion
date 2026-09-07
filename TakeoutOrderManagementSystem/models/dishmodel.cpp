#include "dishmodel.h"
#include <QThread>

namespace takeout {
DishModel::DishModel(QObject *parent) : QAbstractTableModel(parent) {}
void DishModel::replaceProjection(QVector<DishRow> rows) {
  Q_ASSERT(thread() == QThread::currentThread());
  beginResetModel();
  m_rows = std::move(rows);
  endResetModel();
}
int DishModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_rows.size());
}
int DishModel::columnCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : ColumnCount;
}
QVariant DishModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.model() != this || index.row() < 0 ||
      index.row() >= m_rows.size() || index.column() < 0 ||
      index.column() >= ColumnCount)
    return {};
  const auto &row = m_rows.at(index.row());
  if (role == IdRole)
    return row.id;
  if (role == ShopIdRole)
    return row.shopId;
  if (role != Qt::DisplayRole && role != SortRole)
    return {};
  switch (index.column()) {
  case Name:
    return row.name;
  case Price:
    return QVariant::fromValue(row.priceCents);
  case Available:
    return role == SortRole
               ? QVariant(row.isAvailable)
               : QVariant(row.isAvailable ? QStringLiteral("上架")
                                          : QStringLiteral("下架"));
  case Deleted:
    return role == SortRole ? QVariant(row.isDeleted)
                            : QVariant(row.isDeleted ? QStringLiteral("已删除")
                                                     : QStringLiteral("正常"));
  default:
    return {};
  }
}
QVariant DishModel::headerData(int section, Qt::Orientation orientation,
                               int role) const {
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section < 0)
    return {};
  const QStringList headers{QStringLiteral("菜品"), QStringLiteral("价格"),
                            QStringLiteral("供应"), QStringLiteral("删除")};
  return section < headers.size() ? QVariant(headers.at(section)) : QVariant{};
}
Qt::ItemFlags DishModel::flags(const QModelIndex &index) const {
  return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                         : Qt::NoItemFlags;
}
const DishRow *DishModel::rowAt(int row) const {
  return row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr;
}
} // namespace takeout
