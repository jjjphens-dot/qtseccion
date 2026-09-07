#include "jsoncodec.h"
#include "core/orderpolicy.h"
#include <QJsonArray>
#include <QJsonValue>
#include <cmath>

namespace takeout::JsonCodec {
namespace {
constexpr double MaxExactInteger = 9007199254740991.0;
Error bad(const QString &path) {
  return {ErrorCode::CorruptData, QStringLiteral("数据字段无效：%1").arg(path),
          path};
}
bool exactKeys(const QJsonObject &o, std::initializer_list<const char *> keys) {
  if (o.size() != qsizetype(keys.size()))
    return false;
  for (const auto key : keys)
    if (!o.contains(QLatin1String(key)))
      return false;
  return true;
}
bool string(const QJsonObject &o, const char *key, QString &out) {
  const auto v = o.value(QLatin1String(key));
  if (!v.isString())
    return false;
  out = v.toString();
  return true;
}
bool boolean(const QJsonObject &o, const char *key, bool &out) {
  const auto v = o.value(QLatin1String(key));
  if (!v.isBool())
    return false;
  out = v.toBool();
  return true;
}
bool integer64(const QJsonObject &o, const char *key, qint64 &out) {
  const auto v = o.value(QLatin1String(key));
  if (!v.isDouble())
    return false;
  const double n = v.toDouble();
  if (!std::isfinite(n) || std::floor(n) != n || std::abs(n) > MaxExactInteger)
    return false;
  out = qint64(n);
  return true;
}
bool integer(const QJsonObject &o, const char *key, int &out) {
  qint64 n;
  if (!integer64(o, key, n) || n < INT_MIN || n > INT_MAX)
    return false;
  out = int(n);
  return true;
}
bool dateValue(const QJsonValue &v, QDateTime &out) {
  if (!v.isString())
    return false;
  const QString source = v.toString();
  const auto parsed = QDateTime::fromString(source, Qt::ISODateWithMs);
  if (!parsed.isValid() || parsed.offsetFromUtc() != 0 ||
      parsed.toUTC().toString(Qt::ISODateWithMs) != source)
    return false;
  out = parsed.toUTC();
  return true;
}
bool date(const QJsonObject &o, const char *key, QDateTime &out) {
  return dateValue(o.value(QLatin1String(key)), out);
}
bool optionalDate(const QJsonObject &o, const char *key,
                  std::optional<QDateTime> &out) {
  const auto v = o.value(QLatin1String(key));
  if (v.isNull()) {
    out.reset();
    return true;
  }
  QDateTime d;
  if (!dateValue(v, d))
    return false;
  out = d;
  return true;
}
bool optionalString(const QJsonObject &o, const char *key,
                    std::optional<QString> &out) {
  const auto v = o.value(QLatin1String(key));
  if (v.isNull()) {
    out.reset();
    return true;
  }
  if (!v.isString())
    return false;
  out = v.toString();
  return true;
}
bool bytes(const QJsonObject &o, const char *key, QByteArray &out) {
  QString s;
  if (!string(o, key, s))
    return false;
  const auto raw = s.toLatin1();
  out = QByteArray::fromBase64(raw);
  return out.toBase64() == raw;
}

QString text(Role v) {
  switch (v) {
  case Role::Customer:
    return "customer";
  case Role::Merchant:
    return "merchant";
  case Role::Rider:
    return "rider";
  case Role::Admin:
    return "admin";
  }
  return {};
}
QString text(OrderStatus v) {
  switch (v) {
  case OrderStatus::PendingPayment:
    return "pendingPayment";
  case OrderStatus::Cancelled:
    return "cancelled";
  case OrderStatus::PendingAcceptance:
    return "pendingAcceptance";
  case OrderStatus::Preparing:
    return "preparing";
  case OrderStatus::ReadyForDelivery:
    return "readyForDelivery";
  case OrderStatus::Delivering:
    return "delivering";
  case OrderStatus::Completed:
    return "completed";
  }
  return {};
}
QString text(PaymentStatus v) {
  switch (v) {
  case PaymentStatus::Unpaid:
    return "unpaid";
  case PaymentStatus::Paid:
    return "paid";
  case PaymentStatus::Refunded:
    return "refunded";
  }
  return {};
}
QString text(OrderAction v) {
  switch (v) {
  case OrderAction::CreateOrder:
    return "createOrder";
  case OrderAction::Pay:
    return "pay";
  case OrderAction::Cancel:
    return "cancel";
  case OrderAction::Accept:
    return "accept";
  case OrderAction::Reject:
    return "reject";
  case OrderAction::MarkReady:
    return "markReady";
  case OrderAction::Claim:
    return "claim";
  case OrderAction::MarkDelivered:
    return "markDelivered";
  case OrderAction::ConfirmReceipt:
    return "confirmReceipt";
  }
  return {};
}
bool parse(const QString &s, Role &v) {
  if (s == "customer")
    v = Role::Customer;
  else if (s == "merchant")
    v = Role::Merchant;
  else if (s == "rider")
    v = Role::Rider;
  else if (s == "admin")
    v = Role::Admin;
  else
    return false;
  return true;
}
bool parse(const QString &s, OrderStatus &v) {
  if (s == "pendingPayment")
    v = OrderStatus::PendingPayment;
  else if (s == "cancelled")
    v = OrderStatus::Cancelled;
  else if (s == "pendingAcceptance")
    v = OrderStatus::PendingAcceptance;
  else if (s == "preparing")
    v = OrderStatus::Preparing;
  else if (s == "readyForDelivery")
    v = OrderStatus::ReadyForDelivery;
  else if (s == "delivering")
    v = OrderStatus::Delivering;
  else if (s == "completed")
    v = OrderStatus::Completed;
  else
    return false;
  return true;
}
bool parse(const QString &s, PaymentStatus &v) {
  if (s == "unpaid")
    v = PaymentStatus::Unpaid;
  else if (s == "paid")
    v = PaymentStatus::Paid;
  else if (s == "refunded")
    v = PaymentStatus::Refunded;
  else
    return false;
  return true;
}
bool parse(const QString &s, OrderAction &v) {
  if (s == "createOrder")
    v = OrderAction::CreateOrder;
  else if (s == "pay")
    v = OrderAction::Pay;
  else if (s == "cancel")
    v = OrderAction::Cancel;
  else if (s == "accept")
    v = OrderAction::Accept;
  else if (s == "reject")
    v = OrderAction::Reject;
  else if (s == "markReady")
    v = OrderAction::MarkReady;
  else if (s == "claim")
    v = OrderAction::Claim;
  else if (s == "markDelivered")
    v = OrderAction::MarkDelivered;
  else if (s == "confirmReceipt")
    v = OrderAction::ConfirmReceipt;
  else
    return false;
  return true;
}
template <class E>
bool enumeration(const QJsonObject &o, const char *key, E &out) {
  QString s;
  return string(o, key, s) && parse(s, out);
}
QString iso(const QDateTime &d) {
  return d.toUTC().toString(Qt::ISODateWithMs);
}
QJsonValue optional(const std::optional<QDateTime> &d) {
  return d ? QJsonValue(iso(*d)) : QJsonValue(QJsonValue::Null);
}
QJsonValue optional(const std::optional<QString> &s) {
  return s ? QJsonValue(*s) : QJsonValue(QJsonValue::Null);
}

QJsonObject encodeAccount(const Account &v) {
  return {{"id", v.id},
          {"loginName", v.loginName},
          {"displayName", v.displayName},
          {"role", text(v.role)},
          {"passwordSalt", QString::fromLatin1(v.passwordSalt.toBase64())},
          {"passwordHash", QString::fromLatin1(v.passwordHash.toBase64())},
          {"passwordIterations", v.passwordIterations},
          {"passwordAlgorithm", v.passwordAlgorithm},
          {"defaultAddress", v.defaultAddress},
          {"isDeleted", v.isDeleted},
          {"createdAt", iso(v.createdAt)}};
}
QJsonObject encodeShop(const Shop &v) {
  return {{"id", v.id},
          {"merchantId", v.merchantId},
          {"name", v.name},
          {"description", v.description},
          {"address", v.address},
          {"isOpen", v.isOpen},
          {"createdAt", iso(v.createdAt)}};
}
QJsonObject encodeDish(const Dish &v) {
  return {{"id", v.id},
          {"shopId", v.shopId},
          {"name", v.name},
          {"priceCents", v.priceCents},
          {"isAvailable", v.isAvailable},
          {"isDeleted", v.isDeleted},
          {"createdAt", iso(v.createdAt)},
          {"updatedAt", iso(v.updatedAt)}};
}
QJsonObject encodeCartItem(const CartItem &v) {
  return {{"dishId", v.dishId}, {"quantity", v.quantity}};
}
QJsonObject encodeCart(const Cart &v) {
  QJsonArray a;
  for (const auto &i : v.items)
    a.append(encodeCartItem(i));
  return {{"customerId", v.customerId}, {"shopId", v.shopId}, {"items", a}};
}
QJsonObject encodeOrderItem(const OrderItem &v) {
  return {{"dishId", v.dishId},
          {"dishNameSnapshot", v.dishNameSnapshot},
          {"unitPriceCents", v.unitPriceCents},
          {"quantity", v.quantity},
          {"lineTotalCents", v.lineTotalCents}};
}
QJsonObject encodeHistory(const OrderHistoryEntry &v) {
  return {{"action", text(v.action)},
          {"fromStatus", v.fromStatus ? QJsonValue(text(*v.fromStatus))
                                      : QJsonValue(QJsonValue::Null)},
          {"toStatus", text(v.toStatus)},
          {"actorId", v.actorId},
          {"at", iso(v.at)},
          {"reason", v.reason}};
}
QJsonObject encodeOrder(const Order &v) {
  QJsonArray items, history;
  for (const auto &i : v.items)
    items.append(encodeOrderItem(i));
  for (const auto &i : v.history)
    history.append(encodeHistory(i));
  return {{"id", v.id},
          {"customerId", v.customerId},
          {"shopId", v.shopId},
          {"riderId", optional(v.riderId)},
          {"items", items},
          {"status", text(v.status)},
          {"paymentStatus", text(v.paymentStatus)},
          {"subtotalCents", v.subtotalCents},
          {"deliveryFeeCents", v.deliveryFeeCents},
          {"totalCents", v.totalCents},
          {"riderIncomeCents", v.riderIncomeCents},
          {"customerNameSnapshot", v.customerNameSnapshot},
          {"addressSnapshot", v.addressSnapshot},
          {"shopNameSnapshot", v.shopNameSnapshot},
          {"shopAddressSnapshot", v.shopAddressSnapshot},
          {"riderNameSnapshot", optional(v.riderNameSnapshot)},
          {"createdAt", iso(v.createdAt)},
          {"updatedAt", iso(v.updatedAt)},
          {"paidAt", optional(v.paidAt)},
          {"acceptedAt", optional(v.acceptedAt)},
          {"readyAt", optional(v.readyAt)},
          {"claimedAt", optional(v.claimedAt)},
          {"deliveredAt", optional(v.deliveredAt)},
          {"completedAt", optional(v.completedAt)},
          {"cancelledAt", optional(v.cancelledAt)},
          {"cancelReason", v.cancelReason},
          {"history", history}};
}

template <class T, class F>
Result<QVector<T>> array(const QJsonObject &o, const char *key,
                         const QString &path, F decode) {
  const auto v = o.value(QLatin1String(key));
  if (!v.isArray())
    return Result<QVector<T>>::failure(bad(path));
  QVector<T> out;
  const auto a = v.toArray();
  out.reserve(a.size());
  for (qsizetype i = 0; i < a.size(); ++i) {
    if (!a[i].isObject())
      return Result<QVector<T>>::failure(bad(path + QString("[%1]").arg(i)));
    auto r = decode(a[i].toObject(), path + QString("[%1]").arg(i));
    if (!r.ok())
      return Result<QVector<T>>::failure(r.error());
    out.push_back(r.value());
  }
  return Result<QVector<T>>::success(out);
}
Result<Account> decodeAccount(const QJsonObject &o, const QString &p) {
  Account v;
  if (!exactKeys(o, {"id", "loginName", "displayName", "role", "passwordSalt",
                     "passwordHash", "passwordIterations", "passwordAlgorithm",
                     "defaultAddress", "isDeleted", "createdAt"}) ||
      !string(o, "id", v.id) || !string(o, "loginName", v.loginName) ||
      !string(o, "displayName", v.displayName) ||
      !enumeration(o, "role", v.role) ||
      !bytes(o, "passwordSalt", v.passwordSalt) ||
      !bytes(o, "passwordHash", v.passwordHash) ||
      !integer(o, "passwordIterations", v.passwordIterations) ||
      !string(o, "passwordAlgorithm", v.passwordAlgorithm) ||
      !string(o, "defaultAddress", v.defaultAddress) ||
      !boolean(o, "isDeleted", v.isDeleted) ||
      !date(o, "createdAt", v.createdAt))
    return Result<Account>::failure(bad(p));
  return Result<Account>::success(v);
}
Result<Shop> decodeShop(const QJsonObject &o, const QString &p) {
  Shop v;
  if (!exactKeys(o, {"id", "merchantId", "name", "description", "address",
                     "isOpen", "createdAt"}) ||
      !string(o, "id", v.id) || !string(o, "merchantId", v.merchantId) ||
      !string(o, "name", v.name) || !string(o, "description", v.description) ||
      !string(o, "address", v.address) || !boolean(o, "isOpen", v.isOpen) ||
      !date(o, "createdAt", v.createdAt))
    return Result<Shop>::failure(bad(p));
  return Result<Shop>::success(v);
}
Result<Dish> decodeDish(const QJsonObject &o, const QString &p) {
  Dish v;
  if (!exactKeys(o, {"id", "shopId", "name", "priceCents", "isAvailable",
                     "isDeleted", "createdAt", "updatedAt"}) ||
      !string(o, "id", v.id) || !string(o, "shopId", v.shopId) ||
      !string(o, "name", v.name) || !integer64(o, "priceCents", v.priceCents) ||
      !boolean(o, "isAvailable", v.isAvailable) ||
      !boolean(o, "isDeleted", v.isDeleted) ||
      !date(o, "createdAt", v.createdAt) || !date(o, "updatedAt", v.updatedAt))
    return Result<Dish>::failure(bad(p));
  return Result<Dish>::success(v);
}
Result<CartItem> decodeCartItem(const QJsonObject &o, const QString &p) {
  CartItem v;
  if (!exactKeys(o, {"dishId", "quantity"}) || !string(o, "dishId", v.dishId) ||
      !integer(o, "quantity", v.quantity))
    return Result<CartItem>::failure(bad(p));
  return Result<CartItem>::success(v);
}
Result<Cart> decodeCart(const QJsonObject &o, const QString &p) {
  Cart v;
  if (!exactKeys(o, {"customerId", "shopId", "items"}) ||
      !string(o, "customerId", v.customerId) || !string(o, "shopId", v.shopId))
    return Result<Cart>::failure(bad(p));
  auto a = array<CartItem>(o, "items", p + ".items", decodeCartItem);
  if (!a.ok())
    return Result<Cart>::failure(a.error());
  v.items = a.value();
  return Result<Cart>::success(v);
}
Result<OrderItem> decodeOrderItem(const QJsonObject &o, const QString &p) {
  OrderItem v;
  if (!exactKeys(o, {"dishId", "dishNameSnapshot", "unitPriceCents", "quantity",
                     "lineTotalCents"}) ||
      !string(o, "dishId", v.dishId) ||
      !string(o, "dishNameSnapshot", v.dishNameSnapshot) ||
      !integer64(o, "unitPriceCents", v.unitPriceCents) ||
      !integer(o, "quantity", v.quantity) ||
      !integer64(o, "lineTotalCents", v.lineTotalCents))
    return Result<OrderItem>::failure(bad(p));
  return Result<OrderItem>::success(v);
}
Result<OrderHistoryEntry> decodeHistory(const QJsonObject &o,
                                        const QString &p) {
  OrderHistoryEntry v;
  if (!exactKeys(
          o, {"action", "fromStatus", "toStatus", "actorId", "at", "reason"}) ||
      !enumeration(o, "action", v.action) ||
      !enumeration(o, "toStatus", v.toStatus) ||
      !string(o, "actorId", v.actorId) || !date(o, "at", v.at) ||
      !string(o, "reason", v.reason))
    return Result<OrderHistoryEntry>::failure(bad(p));
  const auto f = o.value("fromStatus");
  if (f.isNull())
    v.fromStatus.reset();
  else if (f.isString()) {
    OrderStatus s;
    if (!parse(f.toString(), s))
      return Result<OrderHistoryEntry>::failure(bad(p + ".fromStatus"));
    v.fromStatus = s;
  } else
    return Result<OrderHistoryEntry>::failure(bad(p + ".fromStatus"));
  return Result<OrderHistoryEntry>::success(v);
}
Result<Order> decodeOrder(const QJsonObject &o, const QString &p) {
  Order v;
  if (!exactKeys(o, {"id",
                     "customerId",
                     "shopId",
                     "riderId",
                     "items",
                     "status",
                     "paymentStatus",
                     "subtotalCents",
                     "deliveryFeeCents",
                     "totalCents",
                     "riderIncomeCents",
                     "customerNameSnapshot",
                     "addressSnapshot",
                     "shopNameSnapshot",
                     "shopAddressSnapshot",
                     "riderNameSnapshot",
                     "createdAt",
                     "updatedAt",
                     "paidAt",
                     "acceptedAt",
                     "readyAt",
                     "claimedAt",
                     "deliveredAt",
                     "completedAt",
                     "cancelledAt",
                     "cancelReason",
                     "history"}) ||
      !string(o, "id", v.id) || !string(o, "customerId", v.customerId) ||
      !string(o, "shopId", v.shopId) ||
      !optionalString(o, "riderId", v.riderId) ||
      !enumeration(o, "status", v.status) ||
      !enumeration(o, "paymentStatus", v.paymentStatus) ||
      !integer64(o, "subtotalCents", v.subtotalCents) ||
      !integer64(o, "deliveryFeeCents", v.deliveryFeeCents) ||
      !integer64(o, "totalCents", v.totalCents) ||
      !integer64(o, "riderIncomeCents", v.riderIncomeCents) ||
      !string(o, "customerNameSnapshot", v.customerNameSnapshot) ||
      !string(o, "addressSnapshot", v.addressSnapshot) ||
      !string(o, "shopNameSnapshot", v.shopNameSnapshot) ||
      !string(o, "shopAddressSnapshot", v.shopAddressSnapshot) ||
      !optionalString(o, "riderNameSnapshot", v.riderNameSnapshot) ||
      !date(o, "createdAt", v.createdAt) ||
      !date(o, "updatedAt", v.updatedAt) ||
      !optionalDate(o, "paidAt", v.paidAt) ||
      !optionalDate(o, "acceptedAt", v.acceptedAt) ||
      !optionalDate(o, "readyAt", v.readyAt) ||
      !optionalDate(o, "claimedAt", v.claimedAt) ||
      !optionalDate(o, "deliveredAt", v.deliveredAt) ||
      !optionalDate(o, "completedAt", v.completedAt) ||
      !optionalDate(o, "cancelledAt", v.cancelledAt) ||
      !string(o, "cancelReason", v.cancelReason))
    return Result<Order>::failure(bad(p));
  auto items = array<OrderItem>(o, "items", p + ".items", decodeOrderItem);
  if (!items.ok())
    return Result<Order>::failure(items.error());
  auto history =
      array<OrderHistoryEntry>(o, "history", p + ".history", decodeHistory);
  if (!history.ok())
    return Result<Order>::failure(history.error());
  v.items = items.value();
  v.history = history.value();
  return Result<Order>::success(v);
}
} // namespace

QJsonObject encode(const StoreSnapshot &s) {
  QJsonArray accounts, shops, dishes, carts, orders;
  for (const auto &v : s.accounts)
    accounts.append(encodeAccount(v));
  for (const auto &v : s.shops)
    shops.append(encodeShop(v));
  for (const auto &v : s.dishes)
    dishes.append(encodeDish(v));
  for (const auto &v : s.carts)
    carts.append(encodeCart(v));
  for (const auto &v : s.orders)
    orders.append(encodeOrder(v));
  return {{"schemaVersion", s.schemaVersion},
          {"revision", s.revision},
          {"savedAt", iso(s.savedAt)},
          {"accounts", accounts},
          {"shops", shops},
          {"dishes", dishes},
          {"carts", carts},
          {"orders", orders}};
}

Result<StoreSnapshot> decode(const QJsonObject &o) {
  StoreSnapshot s;
  if (!exactKeys(o, {"schemaVersion", "revision", "savedAt", "accounts",
                     "shops", "dishes", "carts", "orders"}) ||
      !integer(o, "schemaVersion", s.schemaVersion))
    return Result<StoreSnapshot>::failure(bad("root"));
  if (s.schemaVersion != Limits::SchemaVersion)
    return Result<StoreSnapshot>::failure(
        {ErrorCode::UnsupportedVersion,
         QStringLiteral("不支持的数据版本：%1").arg(s.schemaVersion),
         "schemaVersion"});
  if (!integer64(o, "revision", s.revision) || s.revision < 0 ||
      !date(o, "savedAt", s.savedAt))
    return Result<StoreSnapshot>::failure(bad("metadata"));
  auto accounts = array<Account>(o, "accounts", "accounts", decodeAccount);
  if (!accounts.ok())
    return Result<StoreSnapshot>::failure(accounts.error());
  auto shops = array<Shop>(o, "shops", "shops", decodeShop);
  if (!shops.ok())
    return Result<StoreSnapshot>::failure(shops.error());
  auto dishes = array<Dish>(o, "dishes", "dishes", decodeDish);
  if (!dishes.ok())
    return Result<StoreSnapshot>::failure(dishes.error());
  auto carts = array<Cart>(o, "carts", "carts", decodeCart);
  if (!carts.ok())
    return Result<StoreSnapshot>::failure(carts.error());
  auto orders = array<Order>(o, "orders", "orders", decodeOrder);
  if (!orders.ok())
    return Result<StoreSnapshot>::failure(orders.error());
  s.accounts = accounts.value();
  s.shops = shops.value();
  s.dishes = dishes.value();
  s.carts = carts.value();
  s.orders = orders.value();
  const auto valid = OrderPolicy::validateAll(s);
  if (!valid.ok())
    return Result<StoreSnapshot>::failure(
        {ErrorCode::CorruptData,
         QStringLiteral("业务数据校验失败：%1").arg(valid.error().message),
         valid.error().field});
  return Result<StoreSnapshot>::success(s);
}
} // namespace takeout::JsonCodec
