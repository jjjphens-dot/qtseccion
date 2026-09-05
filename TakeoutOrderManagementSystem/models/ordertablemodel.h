#pragma once
#include "core/entities.h"
#include <QAbstractTableModel>
namespace takeout {
class OrderQueryService;
class OrderTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { OrderId, ShopName, Status, Total, CreatedAt, ColumnCount };
    enum DataRole { IdRole = Qt::UserRole + 1, SortRole, StatusRole };
    explicit OrderTableModel(QObject* parent = nullptr);
    // Input must already be role-authorized DTOs, never a full StoreSnapshot.
    void replaceProjection(QVector<OrderRow> rows);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
private:
    QVector<OrderRow> m_rows;
};
} // namespace takeout
