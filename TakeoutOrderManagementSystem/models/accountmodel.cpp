#include "accountmodel.h"

#include <QThread>

namespace takeout {

AccountModel::AccountModel(QObject *parent) : QAbstractTableModel(parent) {}

void AccountModel::replaceProjection(QVector<AccountRow> rows) {
  Q_ASSERT(thread() == QThread::currentThread());
  beginResetModel();
  m_rows = std::move(rows);
  endResetModel();
}

int AccountModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_rows.size());
}

int AccountModel::columnCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : ColumnCount;
}

QVariant AccountModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.model() != this || index.row() < 0 ||
      index.row() >= m_rows.size() || index.column() < 0 ||
      index.column() >= ColumnCount)
    return {};
  const auto &row = m_rows.at(index.row());
  if (role == IdRole)
    return row.id;
  if (role == RoleRole)
    return int(row.role);
  if (role != Qt::DisplayRole && role != SortRole)
    return {};
  switch (index.column()) {
  case LoginName:
    return row.loginName;
  case DisplayName:
    return row.displayName;
  case RoleColumn:
    return role == SortRole ? QVariant(int(row.role))
                            : QVariant(roleLabel(row.role));
  case Status:
    return role == SortRole ? QVariant(row.isDeleted)
                            : QVariant(row.isDeleted ? QStringLiteral("已删除")
                                                      : QStringLiteral("正常"));
  case CreatedAt:
    return role == SortRole ? QVariant(row.createdAt)
                            : QVariant(row.createdAt.toLocalTime().toString(
                                  QStringLiteral("yyyy-MM-dd HH:mm:ss")));
  default:
    return {};
  }
}

QVariant AccountModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const {
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section < 0)
    return {};
  const QStringList headers{QStringLiteral("登录名"), QStringLiteral("显示名"),
                            QStringLiteral("角色"), QStringLiteral("状态"),
                            QStringLiteral("创建时间")};
  return section < headers.size() ? QVariant(headers.at(section)) : QVariant{};
}

Qt::ItemFlags AccountModel::flags(const QModelIndex &index) const {
  return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                         : Qt::NoItemFlags;
}

} // namespace takeout
