#include "orderservice.h"
#include "core/orderpolicy.h"
#include "core/validation.h"
#include <QDateTime>
#include <QSet>
#include <QUuid>
#include <limits>

namespace takeout {
namespace {
Shop *shopById(StoreSnapshot &snapshot, const Id &id) {
  for (auto &shop : snapshot.shops)
    if (shop.id == id)
      return &shop;
  return nullptr;
}
const Shop *shopById(const StoreSnapshot &snapshot, const Id &id) {
  for (const auto &shop : snapshot.shops)
    if (shop.id == id)
      return &shop;
  return nullptr;
}
Account *accountById(StoreSnapshot &snapshot, const Id &id) {
  for (auto &account : snapshot.accounts)
    if (account.id == id)
      return &account;
  return nullptr;
}
const Account *accountById(const StoreSnapshot &snapshot, const Id &id) {
  for (const auto &account : snapshot.accounts)
    if (account.id == id)
      return &account;
  return nullptr;
}
const Dish *dishById(const StoreSnapshot &snapshot, const Id &id) {
  for (const auto &dish : snapshot.dishes)
    if (dish.id == id)
      return &dish;
  return nullptr;
}
Order *orderById(StoreSnapshot &snapshot, const Id &id) {
  for (auto &order : snapshot.orders)
    if (order.id == id)
      return &order;
  return nullptr;
}
bool lineTotal(Money price, int quantity, Money &result) {
  if (price < 0 || quantity <= 0 ||
      price > std::numeric_limits<Money>::max() / quantity)
    return false;
  result = price * quantity;
  return true;
}
Error invalidTransition(const QString &message) {
  return {ErrorCode::InvalidTransition, message, "orderId"};
}
Error wrongOwner() {
  return {ErrorCode::Forbidden, QStringLiteral("无权操作此订单"), "orderId"};
}
bool merchantOwns(const StoreSnapshot &snapshot, const Order &order,
                  const Id &merchantId) {
  const auto *shop = shopById(snapshot, order.shopId);
  return shop && shop->merchantId == merchantId;
}
const Account *activeMerchant(const StoreSnapshot &snapshot, const Shop &shop) {
  const auto *merchant = accountById(snapshot, shop.merchantId);
  return merchant && merchant->role == Role::Merchant && !merchant->isDeleted
             ? merchant
             : nullptr;
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

Result<Id> OrderService::createOrder(const CheckoutRequest &request) {
  const auto permission = requireRole({Role::Customer});
  if (!permission.ok())
    return Result<Id>::failure(permission.error());
  const auto session = m_session.current();
  auto candidate = m_store.snapshot();
  const auto *customer = accountById(candidate, session->accountId);
  if (!customer || customer->role != Role::Customer || customer->isDeleted)
    return Result<Id>::failure(
        {ErrorCode::Forbidden, QStringLiteral("当前账号不可下单"), "customerId"});
  qsizetype cartIndex = -1;
  for (qsizetype i = 0; i < candidate.carts.size(); ++i)
    if (candidate.carts.at(i).customerId == customer->id) {
      cartIndex = i;
      break;
    }
  if (cartIndex < 0 || candidate.carts.at(cartIndex).items.isEmpty())
    return Result<Id>::failure(
        {ErrorCode::Conflict, QStringLiteral("购物车为空"), "cart"});
  if (candidate.orders.size() >= Limits::MaxOrders)
    return Result<Id>::failure(
        {ErrorCode::Validation, QStringLiteral("订单数量已达到上限"), "orders"});
  const auto cart = candidate.carts.at(cartIndex);
  auto *shop = shopById(candidate, cart.shopId);
  if (!shop || !shop->isOpen)
    return Result<Id>::failure(
        {ErrorCode::Conflict, QStringLiteral("店铺当前未营业"), "shopId"});
  if (!activeMerchant(candidate, *shop))
    return Result<Id>::failure(
        {ErrorCode::Conflict, QStringLiteral("店铺商家不可用"), "shopId"});
  const auto name = (request.customerName.trimmed().isEmpty()
                         ? customer->displayName
                         : request.customerName)
                        .normalized(QString::NormalizationForm_C)
                        .trimmed();
  const auto address = (request.address.trimmed().isEmpty()
                            ? customer->defaultAddress
                            : request.address)
                           .normalized(QString::NormalizationForm_C)
                           .trimmed();
  auto checked = Validation::displayName(name);
  if (!checked.ok())
    return Result<Id>::failure(checked.error());
  checked = Validation::address(address);
  if (!checked.ok())
    return Result<Id>::failure(checked.error());
  Order order;
  order.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  order.customerId = customer->id;
  order.shopId = shop->id;
  order.customerNameSnapshot = name;
  order.addressSnapshot = address;
  order.shopNameSnapshot = shop->name;
  order.shopAddressSnapshot = shop->address;
  order.deliveryFeeCents = Limits::DefaultDeliveryFeeCents;
  order.createdAt = QDateTime::currentDateTimeUtc();
  order.updatedAt = order.createdAt;
  order.history.push_back({OrderAction::CreateOrder,
                           {},
                           OrderStatus::PendingPayment,
                           customer->id,
                           order.createdAt,
                           {}});
  QSet<Id> dishIds;
  for (const auto &item : cart.items) {
    if (dishIds.contains(item.dishId) || !Validation::quantity(item.quantity).ok())
      return Result<Id>::failure(
          {ErrorCode::Validation, QStringLiteral("购物车商品无效"), "items"});
    const auto *dish = dishById(candidate, item.dishId);
    if (!dish || dish->shopId != shop->id || dish->isDeleted ||
        !dish->isAvailable)
      return Result<Id>::failure(
          {ErrorCode::Conflict, QStringLiteral("购物车包含不可购买的菜品"), "items"});
    Money line = 0;
    if (!lineTotal(dish->priceCents, item.quantity, line) ||
        order.subtotalCents > Validation::MaxOrderTotalCents - line)
      return Result<Id>::failure(
          {ErrorCode::Validation, QStringLiteral("订单金额超出上限"), "totalCents"});
    order.items.push_back(
        {dish->id, dish->name, dish->priceCents, item.quantity, line});
    order.subtotalCents += line;
    dishIds.insert(item.dishId);
  }
  if (order.subtotalCents > Validation::MaxOrderTotalCents - order.deliveryFeeCents)
    return Result<Id>::failure(
        {ErrorCode::Validation, QStringLiteral("订单金额超出上限"), "totalCents"});
  order.totalCents = order.subtotalCents + order.deliveryFeeCents;
  candidate.carts.removeAt(cartIndex);
  candidate.orders.push_back(order);
  const auto saved = commit(std::move(candidate));
  if (!saved.ok())
    return Result<Id>::failure(saved.error());
  return Result<Id>::success(order.id);
}

Result<void> OrderService::execute(const Id &orderId, OrderAction action,
                                   const QString &reason) {
  if (action == OrderAction::CreateOrder)
    return Result<void>::failure(
        {ErrorCode::InvalidTransition, QStringLiteral("请使用 createOrder"), {}});
  const auto permission = requireRole(
      {Role::Customer, Role::Merchant, Role::Rider});
  if (!permission.ok())
    return permission;
  const auto session = m_session.current();
  auto candidate = m_store.snapshot();
  auto *order = orderById(candidate, orderId);
  if (!order)
    return Result<void>::failure(
        {ErrorCode::NotFound, QStringLiteral("订单不存在"), "orderId"});
  const auto now = QDateTime::currentDateTimeUtc();
  auto append = [&](OrderAction currentAction, OrderStatus next,
                    const QString &why = QString()) -> Result<void> {
    if (!OrderPolicy::canTransition(order->status, currentAction))
      return Result<void>::failure(invalidTransition(QStringLiteral("当前状态不允许此操作")));
    const auto previous = order->status;
    order->status = next;
    order->updatedAt = now;
    order->history.push_back({currentAction, previous, next, session->accountId,
                              now, why});
    return Result<void>::success();
  };
  switch (action) {
  case OrderAction::Pay: {
    if (session->role != Role::Customer || order->customerId != session->accountId)
      return Result<void>::failure(wrongOwner());
    if (order->paymentStatus != PaymentStatus::Unpaid)
      return Result<void>::failure(invalidTransition(QStringLiteral("订单已经支付或取消")));
    const auto *shop = shopById(candidate, order->shopId);
    if (!shop || !shop->isOpen || !activeMerchant(candidate, *shop))
      return Result<void>::failure({ErrorCode::Conflict, QStringLiteral("店铺已停止接单"), "shopId"});
    for (const auto &item : order->items) {
      const auto *dish = dishById(candidate, item.dishId);
      if (!dish || dish->isDeleted || !dish->isAvailable ||
          dish->priceCents != item.unitPriceCents)
        return Result<void>::failure({ErrorCode::Conflict,
                                      QStringLiteral("菜品已下架或价格发生变化"),
                                      "items"});
    }
    const auto moved = append(OrderAction::Pay, OrderStatus::PendingAcceptance);
    if (!moved.ok())
      return moved;
    order->paymentStatus = PaymentStatus::Paid;
    order->paidAt = now;
    break;
  }
  case OrderAction::Cancel: {
    if (session->role != Role::Customer || order->customerId != session->accountId)
      return Result<void>::failure(wrongOwner());
    if (order->paymentStatus != PaymentStatus::Unpaid)
      return Result<void>::failure(invalidTransition(QStringLiteral("已支付订单不能由顾客取消")));
    if (!Validation::reason(reason, false).ok())
      return Result<void>::failure(
          {ErrorCode::Validation, QStringLiteral("取消原因无效"), "reason"});
    const auto normalizedReason =
        reason.normalized(QString::NormalizationForm_C).trimmed();
    {
      const auto moved = append(OrderAction::Cancel, OrderStatus::Cancelled,
                                normalizedReason);
      if (!moved.ok())
        return moved;
    }
    order->cancelledAt = now;
    order->cancelReason = normalizedReason;
    break;
  }
  case OrderAction::Accept:
    if (session->role != Role::Merchant || !merchantOwns(candidate, *order, session->accountId))
      return Result<void>::failure(wrongOwner());
    {
      const auto moved = append(OrderAction::Accept, OrderStatus::Preparing);
      if (!moved.ok())
        return moved;
    }
    order->acceptedAt = now;
    break;
  case OrderAction::Reject:
    if (session->role != Role::Merchant || !merchantOwns(candidate, *order, session->accountId))
      return Result<void>::failure(wrongOwner());
    if (!Validation::reason(reason, true).ok())
      return Result<void>::failure({ErrorCode::Validation, QStringLiteral("拒单原因不能为空"), "reason"});
    {
      const auto moved = append(OrderAction::Reject, OrderStatus::Cancelled, reason.trimmed());
      if (!moved.ok())
        return moved;
    }
    order->paymentStatus = PaymentStatus::Refunded;
    order->cancelledAt = now;
    order->cancelReason = reason.trimmed();
    break;
  case OrderAction::MarkReady:
    if (session->role != Role::Merchant || !merchantOwns(candidate, *order, session->accountId))
      return Result<void>::failure(wrongOwner());
    {
      const auto moved = append(OrderAction::MarkReady, OrderStatus::ReadyForDelivery);
      if (!moved.ok())
        return moved;
    }
    order->readyAt = now;
    break;
  case OrderAction::Claim: {
    if (session->role != Role::Rider)
      return Result<void>::failure(wrongOwner());
    const auto *rider = accountById(candidate, session->accountId);
    if (!rider || rider->role != Role::Rider || rider->isDeleted)
      return Result<void>::failure({ErrorCode::Forbidden, QStringLiteral("当前骑手不可接单"), "riderId"});
    const auto moved = append(OrderAction::Claim, OrderStatus::Delivering);
    if (!moved.ok())
      return moved;
    order->riderId = rider->id;
    order->riderNameSnapshot = rider->displayName;
    order->claimedAt = now;
    break;
  }
  case OrderAction::MarkDelivered:
    if (session->role != Role::Rider || !order->riderId ||
        *order->riderId != session->accountId)
      return Result<void>::failure(wrongOwner());
    {
      const auto moved = append(OrderAction::MarkDelivered, OrderStatus::Delivering);
      if (!moved.ok())
        return moved;
    }
    order->deliveredAt = now;
    break;
  case OrderAction::ConfirmReceipt:
    if (session->role != Role::Customer || order->customerId != session->accountId)
      return Result<void>::failure(wrongOwner());
    {
      const auto moved = append(OrderAction::ConfirmReceipt, OrderStatus::Completed);
      if (!moved.ok())
        return moved;
    }
    order->completedAt = now;
    order->riderIncomeCents = order->deliveryFeeCents;
    break;
  case OrderAction::CreateOrder:
    return Result<void>::failure(
        {ErrorCode::InvalidTransition, QStringLiteral("请使用 createOrder"),
         "action"});
  default:
    return Result<void>::failure(
        {ErrorCode::InvalidTransition, QStringLiteral("未知订单动作"),
         "action"});
  }
  return commit(std::move(candidate));
}
} // namespace takeout
