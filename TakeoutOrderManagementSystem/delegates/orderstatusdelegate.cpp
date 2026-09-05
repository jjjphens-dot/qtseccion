#include "orderstatusdelegate.h"
#include "models/ordertablemodel.h"
#include <QStyleOptionViewItem>
namespace takeout {
void OrderStatusDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const {
    QStyledItemDelegate::initStyleOption(option, index);
    const auto state = OrderStatus(index.data(OrderTableModel::StatusRole).toInt());
    option->text = statusLabel(state);
    if (state == OrderStatus::Completed) option->palette.setColor(QPalette::Text, QColor("#177245"));
    else if (state == OrderStatus::Cancelled) option->palette.setColor(QPalette::Text, QColor("#9b3434"));
}
} // namespace takeout
