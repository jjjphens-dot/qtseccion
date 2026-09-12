#include "accountfilterproxymodel.h"

#include "accountmodel.h"
#include <utility>

namespace takeout {

AccountFilterProxyModel::AccountFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

void AccountFilterProxyModel::setKeyword(QString keyword) {
  beginFilterChange();
  m_keyword = std::move(keyword);
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void AccountFilterProxyModel::setRole(std::optional<Role> role) {
  beginFilterChange();
  m_role = role;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool AccountFilterProxyModel::filterAcceptsRow(
    int sourceRow, const QModelIndex &sourceParent) const {
  const auto *model = sourceModel();
  if (!model)
    return false;
  const auto login = model->index(sourceRow, AccountModel::LoginName,
                                  sourceParent)
                         .data()
                         .toString();
  const auto display = model->index(sourceRow, AccountModel::DisplayName,
                                    sourceParent)
                           .data()
                           .toString();
  const auto keyword = m_keyword.trimmed();
  if (!keyword.isEmpty() &&
      !login.contains(keyword, Qt::CaseInsensitive) &&
      !display.contains(keyword, Qt::CaseInsensitive))
    return false;
  if (m_role && model->index(sourceRow, 0, sourceParent)
                          .data(AccountModel::RoleRole)
                          .toInt() != int(*m_role))
    return false;
  return true;
}

} // namespace takeout
