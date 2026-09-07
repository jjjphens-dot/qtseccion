#include "shopmodel.h"
#include <QThread>

namespace takeout {
ShopModel::ShopModel(QObject *parent) : QAbstractTableModel(parent) {}
void ShopModel::replaceProjection(QVector<ShopRow> rows) {
  Q_ASSERT(thread() == QThread::currentThread());
  beginResetModel();
  m_rows = std::move(rows);
  endResetModel();
}
int ShopModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_rows.size());
}
int ShopModel::columnCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : ColumnCount;
}
QVariant ShopModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.model() != this || index.row() < 0 ||
      index.row() >= m_rows.size() || index.column() < 0 ||
      index.column() >= ColumnCount)
    return {};
  const auto &row = m_rows.at(index.row());
  if (role == IdRole)
    return row.id;
  if (role != Qt::DisplayRole && role != SortRole)
    return {};
  switch (index.column()) {
  case Name:
    return row.name;
  case Address:
    return row.address;
  case Description:
    return row.description;
  case Open:
    return role == SortRole ? QVariant(row.isOpen)
                            : QVariant(row.isOpen ? QStringLiteral("营业")
                                                  : QStringLiteral("休息"));
  default:
    return {};
  }
}
QVariant ShopModel::headerData(int section, Qt::Orientation orientation,
                               int role) const {
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section < 0)
    return {};
  const QStringList headers{QStringLiteral("店铺"), QStringLiteral("地址"),
                            QStringLiteral("简介"), QStringLiteral("状态")};
  return section < headers.size() ? QVariant(headers.at(section)) : QVariant{};
}
Qt::ItemFlags ShopModel::flags(const QModelIndex &index) const {
  return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                         : Qt::NoItemFlags;
}
const ShopRow *ShopModel::rowAt(int row) const {
  return row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr;
}
} // namespace takeout
