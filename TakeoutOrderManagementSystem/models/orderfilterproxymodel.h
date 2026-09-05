#pragma once
#include "core/requests.h"
#include <QSortFilterProxyModel>
namespace takeout {
class OrderFilterProxyModel final : public QSortFilterProxyModel {
public:
    explicit OrderFilterProxyModel(QObject* parent = nullptr);
    void setFilter(OrderFilter filter);
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
private:
    OrderFilter m_filter;
};
} // namespace takeout
