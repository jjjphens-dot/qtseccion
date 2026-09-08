#include "adminservice.h"

#include "core/validation.h"
#include <QSet>

namespace takeout {
namespace {

Account *accountById(StoreSnapshot &snapshot, const Id &id) {
  for (auto &account : snapshot.accounts)
    if (account.id == id)
      return &account;
  return nullptr;
}

bool isNonTerminal(OrderStatus status) {
  return status == OrderStatus::PendingPayment ||
         status == OrderStatus::PendingAcceptance ||
         status == OrderStatus::Preparing ||
         status == OrderStatus::ReadyForDelivery ||
         status == OrderStatus::Delivering;
}

} // namespace

Result<QVector<AccountRow>> AdminService::listAccounts() const {
  const auto permission = requireRole({Role::Admin});
  if (!permission.ok())
    return Result<QVector<AccountRow>>::failure(permission.error());

  QVector<AccountRow> rows;
  const auto snapshot = m_store.snapshot();
  rows.reserve(snapshot.accounts.size());
  for (const auto &account : snapshot.accounts) {
    if (account.role == Role::Admin)
      continue;
    rows.push_back({account.id, account.loginName, account.displayName,
                    account.role, account.isDeleted, account.createdAt});
  }
  return Result<QVector<AccountRow>>::success(std::move(rows));
}

Result<void> AdminService::deleteAccount(const Id &accountId) {
  const auto permission = requireRole({Role::Admin});
  if (!permission.ok())
    return permission;
  if (!Validation::uuid(accountId, QStringLiteral("accountId")).ok())
    return Result<void>::failure({ErrorCode::Validation,
                                  QStringLiteral("账号ID无效"),
                                  QStringLiteral("accountId")});

  auto candidate = m_store.snapshot();
  auto *target = accountById(candidate, accountId);
  if (!target)
    return Result<void>::failure({ErrorCode::NotFound,
                                  QStringLiteral("账号不存在"),
                                  QStringLiteral("accountId")});
  if (target->role == Role::Admin)
    return Result<void>::failure({ErrorCode::Forbidden,
                                  QStringLiteral("不能删除管理员账号"),
                                  QStringLiteral("accountId")});
  if (target->isDeleted)
    return Result<void>::failure({ErrorCode::AlreadyProcessed,
                                  QStringLiteral("账号已经删除"),
                                  QStringLiteral("accountId")});

  for (const auto &order : candidate.orders) {
    bool blocks = false;
    if (target->role == Role::Customer)
      blocks = order.customerId == target->id && isNonTerminal(order.status);
    else if (target->role == Role::Rider)
      blocks = order.riderId && *order.riderId == target->id &&
               order.status == OrderStatus::Delivering;
    else if (target->role == Role::Merchant) {
      for (const auto &shop : candidate.shops)
        if (shop.merchantId == target->id && shop.id == order.shopId &&
            isNonTerminal(order.status)) {
          blocks = true;
          break;
        }
    }
    if (blocks)
      return Result<void>::failure({
          ErrorCode::Conflict, QStringLiteral("账号仍有关联的未完成订单"),
          QStringLiteral("accountId")});
  }

  target->isDeleted = true;
  if (target->role == Role::Merchant) {
    QSet<Id> unpublishedDishes;
    for (auto &shop : candidate.shops) {
      if (shop.merchantId != target->id)
        continue;
      shop.isOpen = false;
      for (auto &dish : candidate.dishes)
        if (dish.shopId == shop.id) {
          dish.isAvailable = false;
          unpublishedDishes.insert(dish.id);
          dish.updatedAt = QDateTime::currentDateTimeUtc();
        }
    }
    for (qsizetype i = candidate.carts.size() - 1; i >= 0; --i) {
      auto &cart = candidate.carts[i];
      for (qsizetype j = cart.items.size() - 1; j >= 0; --j)
        if (unpublishedDishes.contains(cart.items.at(j).dishId))
          cart.items.removeAt(j);
      if (cart.items.isEmpty())
        candidate.carts.removeAt(i);
    }
  }
  for (qsizetype i = candidate.carts.size() - 1; i >= 0; --i)
    if (candidate.carts.at(i).customerId == target->id)
      candidate.carts.removeAt(i);

  return commit(std::move(candidate));
}

} // namespace takeout
