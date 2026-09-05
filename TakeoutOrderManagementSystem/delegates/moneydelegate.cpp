#include "moneydelegate.h"
namespace takeout {
QString MoneyDelegate::displayText(const QVariant& value, const QLocale&) const {
    const qint64 cents = value.toLongLong();
    // Unsigned magnitude avoids overflow even for the signed minimum.
    const quint64 magnitude = cents < 0 ? quint64(-(cents + 1)) + 1 : quint64(cents);
    return QStringLiteral("%1￥%2.%3").arg(cents < 0 ? "-" : "")
        .arg(magnitude / 100).arg(magnitude % 100, 2, 10, QChar('0'));
}
} // namespace takeout
