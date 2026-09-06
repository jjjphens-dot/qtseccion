#pragma once
#include "entities.h"
#include "result.h"

namespace takeout::OrderPolicy {
Result<void> validate(const Order& order, const StoreSnapshot& snapshot);
Result<void> validateAll(const StoreSnapshot& snapshot);
bool canTransition(OrderStatus from, OrderAction action);
} // namespace takeout::OrderPolicy
