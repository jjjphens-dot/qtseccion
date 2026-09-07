#pragma once
#include "core/entities.h"
#include "core/result.h"
#include <QJsonObject>

namespace takeout::JsonCodec {
QJsonObject encode(const StoreSnapshot &snapshot);
Result<StoreSnapshot> decode(const QJsonObject &root);
} // namespace takeout::JsonCodec
