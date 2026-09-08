#include "statisticsservice.h"

#include "core/validation.h"
#include <limits>

namespace takeout {
namespace {

Result<void> validateRange(const DateRange &range) {
  if (!range.from.isValid())
    return Result<void>::failure({ErrorCode::Validation,
                                  QStringLiteral("起始日期无效"),
                                  QStringLiteral("from")});
  if (!range.until.isValid())
    return Result<void>::failure({ErrorCode::Validation,
                                  QStringLiteral("结束日期无效"),
                                  QStringLiteral("until")});
  if (range.from >= range.until)
    return Result<void>::failure({
        ErrorCode::Validation, QStringLiteral("起始日期必须早于结束日期"),
        QStringLiteral("from")});
  return Result<void>::success();
}

const Shop *shopById(const StoreSnapshot &snapshot, const Id &id) {
  for (const auto &shop : snapshot.shops)
    if (shop.id == id)
      return &shop;
  return nullptr;
}

bool activeMerchant(const StoreSnapshot &snapshot, const Id &merchantId) {
  for (const auto &account : snapshot.accounts)
    if (account.id == merchantId && account.role == Role::Merchant &&
        !account.isDeleted)
      return true;
  return false;
}

} // namespace

Result<StatisticsSummary>
StatisticsService::summary(const DateRange &range) const {
  const auto permission = requireRole(
      {Role::Customer, Role::Merchant, Role::Rider, Role::Admin});
  if (!permission.ok())
    return Result<StatisticsSummary>::failure(permission.error());
  const auto checked = validateRange(range);
  if (!checked.ok())
    return Result<StatisticsSummary>::failure(checked.error());

  const auto session = m_session.current();
  const auto snapshot = m_store.snapshot();
  StatisticsSummary result;
  for (const auto &account : snapshot.accounts) {
    if (account.isDeleted)
      ++result.deletedAccountCount;
    else
      ++result.activeAccountCount;
  }
  for (const auto &shop : snapshot.shops)
    if (activeMerchant(snapshot, shop.merchantId))
      ++result.activeShopCount;

  for (const auto &order : snapshot.orders) {
    if (order.status != OrderStatus::Completed || !order.completedAt ||
        *order.completedAt < range.from || *order.completedAt >= range.until)
      continue;
    const auto *shop = shopById(snapshot, order.shopId);
    bool included = false;
    Money amount = 0;
    switch (session->role) {
    case Role::Customer:
      included = order.customerId == session->accountId;
      amount = order.totalCents;
      break;
    case Role::Merchant:
      included = shop && shop->merchantId == session->accountId;
      amount = order.subtotalCents;
      break;
    case Role::Rider:
      included = order.riderId && *order.riderId == session->accountId;
      amount = order.riderIncomeCents;
      break;
    case Role::Admin:
      included = true;
      amount = order.totalCents;
      break;
    }
    if (!included)
      continue;
    if (result.totalCents > std::numeric_limits<Money>::max() - amount)
      return Result<StatisticsSummary>::failure(
          {ErrorCode::Validation, QStringLiteral("统计金额溢出"),
           QStringLiteral("totalCents")});
    ++result.completedCount;
    result.totalCents += amount;
  }
  return Result<StatisticsSummary>::success(result);
}

} // namespace takeout
