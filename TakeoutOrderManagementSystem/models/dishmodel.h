#pragma once
#include "core/entities.h"
#include <QAbstractTableModel>

namespace takeout {
struct DishRow {
  Id id;
  Id shopId;
  QString name;
  Money priceCents = 0;
  bool isAvailable = false;
  bool isDeleted = false;
};

class DishModel final : public QAbstractTableModel {
  Q_OBJECT
public:
  enum Column { Name, Price, Available, Deleted, ColumnCount };
  enum DataRole { IdRole = Qt::UserRole + 1, ShopIdRole, SortRole };
  explicit DishModel(QObject *parent = nullptr);
  void replaceProjection(QVector<DishRow> rows);
  int rowCount(const QModelIndex &parent = {}) const override;
  int columnCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  const DishRow *rowAt(int row) const;

private:
  QVector<DishRow> m_rows;
};
} // namespace takeout
