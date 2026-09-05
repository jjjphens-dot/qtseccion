#pragma once
#include <QStyledItemDelegate>
namespace takeout {
class MoneyDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QString displayText(const QVariant& value, const QLocale& locale) const override;
};
} // namespace takeout
