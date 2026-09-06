#pragma once
#include "entities.h"
#include "result.h"

namespace takeout::Validation {
inline constexpr qsizetype LoginMin = 3;
inline constexpr qsizetype LoginMax = 32;
inline constexpr qsizetype PasswordMin = 8;
inline constexpr qsizetype PasswordMax = 128;
inline constexpr qsizetype DisplayNameMax = 40;
inline constexpr qsizetype AddressMax = 200;
inline constexpr qsizetype NamedEntityMax = 60;
inline constexpr qsizetype DescriptionMax = 500;
inline constexpr qsizetype ReasonMax = 200;
inline constexpr Money MaxDishPriceCents = 1'000'000;
inline constexpr Money MaxOrderTotalCents = 100'000'000;
inline constexpr int MaxItemQuantity = 99;
inline constexpr qsizetype MaxDistinctItems = 50;

QString normalizeLoginName(const QString& value);
QString normalizeNamedEntity(const QString& value);
Result<void> loginName(const QString& value);
Result<void> password(const QString& value);
Result<void> displayName(const QString& value);
Result<void> address(const QString& value);
Result<void> namedEntity(const QString& value, const QString& field);
Result<void> description(const QString& value);
Result<void> reason(const QString& value, bool required);
Result<void> dishPrice(Money cents);
Result<void> quantity(int value);
Result<void> uuid(const Id& value, const QString& field);
} // namespace takeout::Validation
