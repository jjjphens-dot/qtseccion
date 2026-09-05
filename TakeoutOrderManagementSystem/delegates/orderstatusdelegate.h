#pragma once
#include <QStyledItemDelegate>
namespace takeout {
class OrderStatusDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;
};
} // namespace takeout
