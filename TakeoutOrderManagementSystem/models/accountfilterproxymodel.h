#pragma once

#include "core/enums.h"
#include <QSortFilterProxyModel>
#include <optional>

namespace takeout {
class AccountFilterProxyModel final : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit AccountFilterProxyModel(QObject *parent = nullptr);

  void setKeyword(QString keyword);
  void setRole(std::optional<Role> role);

protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex &sourceParent) const override;

private:
  QString m_keyword;
  std::optional<Role> m_role;
};
} // namespace takeout
