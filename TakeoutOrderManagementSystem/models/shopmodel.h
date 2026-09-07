#pragma once
#include "core/entities.h"
#include <QAbstractTableModel>

namespace takeout {
struct ShopRow {
  Id id;
  QString name;
  QString description;
  QString address;
  bool isOpen = false;
};

class ShopModel final : public QAbstractTableModel {
  Q_OBJECT
public:
  enum Column { Name, Address, Description, Open, ColumnCount };
  enum DataRole { IdRole = Qt::UserRole + 1, SortRole };
  explicit ShopModel(QObject *parent = nullptr);
  void replaceProjection(QVector<ShopRow> rows);
  int rowCount(const QModelIndex &parent = {}) const override;
  int columnCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  const ShopRow *rowAt(int row) const;

private:
  QVector<ShopRow> m_rows;
};
} // namespace takeout
