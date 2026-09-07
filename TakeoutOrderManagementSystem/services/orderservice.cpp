#include "orderservice.h"
#include "core/validation.h"

namespace takeout {
namespace {
Shop *shopById(StoreSnapshot &snapshot, const Id &id) {
  for (auto &shop : snapshot.shops)
    if (shop.id == id)
      return &shop;
  return nullptr;
}
const Dish *dishById(const StoreSnapshot &snapshot, const Id &id) {
  for (const auto &dish : snapshot.dishes)
    if (dish.id == id)
      return &dish;
  return nullptr;
}
} // namespace

Result<void> OrderService::updateCart(const CartChanges &changes) {
  const auto permission = requireRole({Role::Customer});
  if (!permission.ok())
    return permission;
  const auto current = m_session.current();
  auto candidate = m_store.snapshot();
  if (changes.items.size() > Validation::MaxDistinctItems)
    return Result<void>::failure({ErrorCode::Validation,
                                  QStringLiteral("购物车商品种类不能超过50种"),
                                  "items"});
  if (!shopById(candidate, changes.shopId))
    return Result<void>::failure(
        {ErrorCode::NotFound, QStringLiteral("店铺不存在"), "shopId"});
  QSet<Id> dishIds;
  for (const auto &item : changes.items) {
    if (!Validation::quantity(item.quantity).ok() ||
        dishIds.contains(item.dishId))
      return Result<void>::failure(
          {ErrorCode::Validation, QStringLiteral("购物车商品数量或重复项无效"),
           "items"});
    const auto *dish = dishById(candidate, item.dishId);
    if (!dish || dish->shopId != changes.shopId || dish->isDeleted ||
        !dish->isAvailable)
      return Result<void>::failure({ErrorCode::Conflict,
                                    QStringLiteral("购物车包含不可购买的菜品"),
                                    "items"});
    dishIds.insert(item.dishId);
  }
  for (qsizetype i = candidate.carts.size() - 1; i >= 0; --i)
    if (candidate.carts.at(i).customerId == current->accountId)
      candidate.carts.removeAt(i);
  if (!changes.items.isEmpty())
    candidate.carts.push_back(
        {current->accountId, changes.shopId, changes.items});
  return commit(std::move(candidate));
}
} // namespace takeout
