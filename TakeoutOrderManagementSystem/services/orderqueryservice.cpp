#include "orderqueryservice.h"
namespace takeout {
namespace {
const Shop *shopById(const StoreSnapshot &snapshot, const Id &id) {
  for (const auto &shop : snapshot.shops)
    if (shop.id == id)
      return &shop;
  return nullptr;
}

const Order *orderById(const StoreSnapshot &snapshot, const Id &id) {
  for (const auto &order : snapshot.orders)
    if (order.id == id)
      return &order;
  return nullptr;
}

bool visibleTo(const Order &order, const StoreSnapshot &snapshot,
               const Session &session) {
  if (session.role == Role::Admin)
    return true;
  if (session.role == Role::Customer)
    return order.customerId == session.accountId;
  const auto *shop = shopById(snapshot, order.shopId);
  if (session.role == Role::Merchant)
    return shop && shop->merchantId == session.accountId;
  if (session.role == Role::Rider)
    return order.status == OrderStatus::ReadyForDelivery ||
           (order.riderId && *order.riderId == session.accountId);
  return false;
}

bool matches(const Order &order, const OrderFilter &filter) {
  if (!filter.keyword.trimmed().isEmpty() &&
      !order.id.contains(filter.keyword.trimmed(), Qt::CaseInsensitive) &&
      !order.shopNameSnapshot.contains(filter.keyword.trimmed(),
                                       Qt::CaseInsensitive))
    return false;
  if (filter.status && order.status != *filter.status)
    return false;
  if (filter.from && order.createdAt < *filter.from)
    return false;
  if (filter.until && order.createdAt >= *filter.until)
    return false;
  return true;
}

OrderDetail detailFor(const Order &order, const Session &session) {
  OrderDetail detail;
  detail.id = order.id;
  detail.customerId = order.customerId;
  detail.shopId = order.shopId;
  detail.items = order.items;
  detail.status = order.status;
  detail.paymentStatus = order.paymentStatus;
  detail.subtotalCents = order.subtotalCents;
  detail.deliveryFeeCents = order.deliveryFeeCents;
  detail.totalCents = order.totalCents;
  detail.riderIncomeCents = order.riderIncomeCents;
  detail.shopName = order.shopNameSnapshot;
  detail.shopAddress = order.shopAddressSnapshot;
  detail.riderName = order.riderNameSnapshot;
  detail.createdAt = order.createdAt;
  detail.updatedAt = order.updatedAt;
  detail.paidAt = order.paidAt;
  detail.acceptedAt = order.acceptedAt;
  detail.readyAt = order.readyAt;
  detail.claimedAt = order.claimedAt;
  detail.deliveredAt = order.deliveredAt;
  detail.completedAt = order.completedAt;
  detail.cancelledAt = order.cancelledAt;
  detail.cancelReason = order.cancelReason;
  detail.history = order.history;
  const bool riderOwns = session.role == Role::Rider && order.riderId &&
                         *order.riderId == session.accountId;
  if (session.role != Role::Rider || riderOwns) {
    detail.customerName = order.customerNameSnapshot;
    detail.address = order.addressSnapshot;
  } else {
    detail.customerId.clear();
    detail.history.clear();
  }
  return detail;
}
} // namespace

Result<QVector<OrderRow>> OrderQueryService::visibleOrders(
    const OrderFilter &filter) const {
  const auto permission = requireRole(
      {Role::Customer, Role::Merchant, Role::Rider, Role::Admin});
  if (!permission.ok())
    return Result<QVector<OrderRow>>::failure(permission.error());
  const auto session = m_session.current();
  const auto snapshot = m_store.snapshot();
  QVector<OrderRow> rows;
  for (const auto &order : snapshot.orders) {
    if (!visibleTo(order, snapshot, *session) || !matches(order, filter))
      continue;
    rows.push_back({order.id, order.shopNameSnapshot, order.status,
                    order.totalCents, order.createdAt});
  }
  return Result<QVector<OrderRow>>::success(std::move(rows));
}

Result<OrderDetail> OrderQueryService::orderDetail(const Id &orderId) const {
  const auto permission = requireRole(
      {Role::Customer, Role::Merchant, Role::Rider, Role::Admin});
  if (!permission.ok())
    return Result<OrderDetail>::failure(permission.error());
  const auto session = m_session.current();
  const auto snapshot = m_store.snapshot();
  const auto *order = orderById(snapshot, orderId);
  if (!order || !visibleTo(*order, snapshot, *session))
    return Result<OrderDetail>::failure(
        {ErrorCode::NotFound, QStringLiteral("订单不存在或无权查看"), "orderId"});
  return Result<OrderDetail>::success(detailFor(*order, *session));
}
} // namespace takeout
