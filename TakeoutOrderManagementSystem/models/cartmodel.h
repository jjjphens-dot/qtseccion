#pragma once
#include "core/entities.h"
#include <QAbstractTableModel>

namespace takeout {
struct CartRow {
  Id dishId;
  Id shopId;
  QString dishName;
  Money unitPriceCents = 0;
  int quantity = 0;
  Money lineTotalCents = 0;
};

class CartModel final : public QAbstractTableModel {
  Q_OBJECT
public:
  enum Column { Dish, UnitPrice, Quantity, LineTotal, ColumnCount };
  enum DataRole { DishIdRole = Qt::UserRole + 1, ShopIdRole, SortRole };
  explicit CartModel(QObject *parent = nullptr);
  void replaceProjection(const Id &shopId, QVector<CartRow> rows);
  int rowCount(const QModelIndex &parent = {}) const override;
  int columnCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVector<CartItem> items() const;
  Id shopId() const { return m_shopId; }
  qsizetype indexOfDish(const Id &dishId) const;

private:
  Id m_shopId;
  QVector<CartRow> m_rows;
};
} // namespace takeout
