#pragma once

#include "core/entities.h"
#include <QAbstractTableModel>

namespace takeout {

class AccountModel final : public QAbstractTableModel {
  Q_OBJECT
public:
  enum Column { LoginName, DisplayName, RoleColumn, Status, CreatedAt, ColumnCount };
  enum DataRole { IdRole = Qt::UserRole + 1, RoleRole, SortRole };

  explicit AccountModel(QObject *parent = nullptr);
  void replaceProjection(QVector<AccountRow> rows);
  int rowCount(const QModelIndex &parent = {}) const override;
  int columnCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
  QVector<AccountRow> m_rows;
};

} // namespace takeout
