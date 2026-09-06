#include "validation.h"
#include <QRegularExpression>
#include <QUuid>

namespace takeout::Validation {
namespace {
Result<void> fail(const QString& message, const QString& field) {
    return Result<void>::failure({ErrorCode::Validation, message, field});
}
qsizetype codePoints(const QString& value) { return value.toUcs4().size(); }
Result<void> trimmedText(const QString& value, qsizetype maximum, const QString& field) {
    const auto normalized = value.normalized(QString::NormalizationForm_C).trimmed();
    if (normalized.isEmpty()) return fail(QStringLiteral("不能为空"), field);
    if (codePoints(normalized) > maximum) return fail(QStringLiteral("长度不能超过 %1 个字符").arg(maximum), field);
    return Result<void>::success();
}
}
QString normalizeLoginName(const QString& value) { return value.trimmed().toLower(); }
QString normalizeNamedEntity(const QString& value) {
    return value.normalized(QString::NormalizationForm_C).trimmed().toCaseFolded();
}
Result<void> loginName(const QString& value) {
    static const QRegularExpression expression(QStringLiteral("^[A-Za-z0-9_]{3,32}$"),
                                                QRegularExpression::UseUnicodePropertiesOption);
    if (!expression.match(value.trimmed()).hasMatch())
        return fail(QStringLiteral("账号需为3至32位字母、数字或下划线"), QStringLiteral("loginName"));
    return Result<void>::success();
}
Result<void> password(const QString& value) {
    const auto length = value.size(); // ASCII-only below, so UTF-16 size is the character count.
    if (length < PasswordMin || length > PasswordMax)
        return fail(QStringLiteral("密码长度需为8至128个字符"), QStringLiteral("password"));
    for (const QChar character : value) {
        const auto code = character.unicode();
        if (code < 0x21 || code > 0x7e)
            return fail(QStringLiteral("密码只能包含无空格的ASCII可打印字符"), QStringLiteral("password"));
    }
    return Result<void>::success();
}
Result<void> displayName(const QString& value) { return trimmedText(value, DisplayNameMax, QStringLiteral("displayName")); }
Result<void> address(const QString& value) { return trimmedText(value, AddressMax, QStringLiteral("address")); }
Result<void> namedEntity(const QString& value, const QString& field) { return trimmedText(value, NamedEntityMax, field); }
Result<void> description(const QString& value) {
    if (codePoints(value.normalized(QString::NormalizationForm_C).trimmed()) > DescriptionMax)
        return fail(QStringLiteral("简介长度不能超过500个字符"), QStringLiteral("description"));
    return Result<void>::success();
}
Result<void> reason(const QString& value, bool required) {
    const auto text = value.normalized(QString::NormalizationForm_C).trimmed();
    if (required && text.isEmpty()) return fail(QStringLiteral("拒单原因不能为空"), QStringLiteral("reason"));
    if (codePoints(text) > ReasonMax) return fail(QStringLiteral("原因长度不能超过200个字符"), QStringLiteral("reason"));
    return Result<void>::success();
}
Result<void> dishPrice(Money cents) {
    if (cents < 0 || cents > MaxDishPriceCents)
        return fail(QStringLiteral("菜品价格必须在0至10000元之间"), QStringLiteral("priceCents"));
    return Result<void>::success();
}
Result<void> quantity(int value) {
    if (value < 1 || value > MaxItemQuantity)
        return fail(QStringLiteral("数量必须为1至99的整数"), QStringLiteral("quantity"));
    return Result<void>::success();
}
Result<void> uuid(const Id& value, const QString& field) {
    const QUuid parsed(value);
    if (parsed.isNull() || value != parsed.toString(QUuid::WithoutBraces))
        return fail(QStringLiteral("必须是规范的小写完整UUID"), field);
    return Result<void>::success();
}
} // namespace takeout::Validation
