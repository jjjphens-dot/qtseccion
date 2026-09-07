#include "orderpolicy.h"
#include "credentials.h"
#include "validation.h"
#include <QHash>
#include <QSet>
#include <limits>

namespace takeout::OrderPolicy {
namespace {
Result<void> corrupt(const QString& message, const QString& field) {
    return Result<void>::failure({ErrorCode::CorruptData, message, field});
}
Result<void> corrupt(const char* message, const QString& field) {
    return corrupt(QString::fromUtf8(message), field);
}
template<class T> const T* findById(const QVector<T>& values, const Id& id) {
    for (const auto& value : values) if (value.id == id) return &value;
    return nullptr;
}
const Account* account(const StoreSnapshot& snapshot, const Id& id, Role role) {
    const auto* result = findById(snapshot.accounts, id);
    return result && result->role == role ? result : nullptr;
}
bool same(const std::optional<QDateTime>& value, const std::optional<QDateTime>& expected) {
    return value == expected;
}
bool safeLineTotal(Money price, int quantity, Money& result) {
    if (price < 0 || quantity < 0 || (quantity != 0 && price > std::numeric_limits<Money>::max() / quantity)) return false;
    result = price * quantity;
    return true;
}
}

bool canTransition(OrderStatus from, OrderAction action) {
    switch (action) {
    case OrderAction::CreateOrder: return false;
    case OrderAction::Pay: case OrderAction::Cancel: return from == OrderStatus::PendingPayment;
    case OrderAction::Accept: case OrderAction::Reject: return from == OrderStatus::PendingAcceptance;
    case OrderAction::MarkReady: return from == OrderStatus::Preparing;
    case OrderAction::Claim: return from == OrderStatus::ReadyForDelivery;
    case OrderAction::MarkDelivered: case OrderAction::ConfirmReceipt: return from == OrderStatus::Delivering;
    }
    return false;
}

Result<void> validate(const Order& order, const StoreSnapshot& snapshot) {
    if (!isLegalCombination(order.status, order.paymentStatus))
        return corrupt(QStringLiteral("订单状态与支付状态组合非法"), QStringLiteral("paymentStatus"));
    if (!order.createdAt.isValid() || !order.updatedAt.isValid() || order.updatedAt < order.createdAt)
        return corrupt(QStringLiteral("订单时间无效"), QStringLiteral("createdAt"));
    if (!account(snapshot, order.customerId, Role::Customer))
        return corrupt(QStringLiteral("订单用户引用无效"), QStringLiteral("customerId"));
    const auto* shop = findById(snapshot.shops, order.shopId);
    if (!shop || !account(snapshot, shop->merchantId, Role::Merchant))
        return corrupt(QStringLiteral("订单店铺或商家引用无效"), QStringLiteral("shopId"));
    if (order.items.isEmpty() || order.items.size() > Validation::MaxDistinctItems)
        return corrupt(QStringLiteral("订单商品种类数无效"), QStringLiteral("items"));
    QSet<Id> itemIds;
    Money subtotal = 0;
    for (qsizetype i = 0; i < order.items.size(); ++i) {
        const auto& item = order.items.at(i);
        const auto* dish = findById(snapshot.dishes, item.dishId);
        if (!dish || dish->shopId != order.shopId || itemIds.contains(item.dishId))
            return corrupt(QStringLiteral("订单商品引用重复或无效"), QStringLiteral("items[%1].dishId").arg(i));
        itemIds.insert(item.dishId);
        if (!Validation::quantity(item.quantity).ok() || !Validation::dishPrice(item.unitPriceCents).ok()
            || item.dishNameSnapshot.trimmed().isEmpty())
            return corrupt(QStringLiteral("订单商品快照无效"), QStringLiteral("items[%1]").arg(i));
        Money line = 0;
        if (!safeLineTotal(item.unitPriceCents, item.quantity, line) || line != item.lineTotalCents
            || subtotal > Validation::MaxOrderTotalCents - line)
            return corrupt(QStringLiteral("订单商品金额无效"), QStringLiteral("items[%1].lineTotalCents").arg(i));
        subtotal += line;
    }
    if (subtotal != order.subtotalCents || order.deliveryFeeCents < 0
        || subtotal > Validation::MaxOrderTotalCents - order.deliveryFeeCents
        || order.totalCents != subtotal + order.deliveryFeeCents
        || order.totalCents > Validation::MaxOrderTotalCents)
        return corrupt(QStringLiteral("订单总额不一致"), QStringLiteral("totalCents"));
    if (order.customerNameSnapshot.trimmed().isEmpty() || order.addressSnapshot.trimmed().isEmpty()
        || order.shopNameSnapshot.trimmed().isEmpty() || order.shopAddressSnapshot.trimmed().isEmpty())
        return corrupt(QStringLiteral("订单必要快照为空"), QStringLiteral("snapshots"));
    if (order.history.isEmpty()) return corrupt(QStringLiteral("订单历史为空"), QStringLiteral("history"));

    const auto& first = order.history.first();
    if (first.action != OrderAction::CreateOrder || first.fromStatus || first.toStatus != OrderStatus::PendingPayment
        || first.actorId != order.customerId || first.at != order.createdAt)
        return corrupt(QStringLiteral("首条历史不是合法创建记录"), QStringLiteral("history[0]"));
    OrderStatus state = OrderStatus::PendingPayment;
    PaymentStatus payment = PaymentStatus::Unpaid;
    std::optional<Id> rider;
    std::optional<QString> riderName;
    std::optional<QDateTime> paidAt, acceptedAt, readyAt, claimedAt, deliveredAt, completedAt, cancelledAt;
    QString cancelReason;
    QDateTime previous = first.at;
    for (qsizetype i = 1; i < order.history.size(); ++i) {
        const auto& entry = order.history.at(i);
        if (!entry.fromStatus || *entry.fromStatus != state || entry.at < previous || !canTransition(state, entry.action))
            return corrupt(QStringLiteral("历史状态不连续或动作非法"), QStringLiteral("history[%1]").arg(i));
        const auto customerActor = entry.actorId == order.customerId && account(snapshot, entry.actorId, Role::Customer);
        const auto merchantActor = entry.actorId == shop->merchantId && account(snapshot, entry.actorId, Role::Merchant);
        switch (entry.action) {
        case OrderAction::CreateOrder:
            return corrupt(QStringLiteral("创建动作重复"), QStringLiteral("history[%1]").arg(i));
        case OrderAction::Pay:
            if (!customerActor || entry.toStatus != OrderStatus::PendingAcceptance) return corrupt("支付历史操作者或状态无效", QStringLiteral("history[%1]").arg(i));
            state = entry.toStatus; payment = PaymentStatus::Paid; paidAt = entry.at; break;
        case OrderAction::Cancel:
            if (!customerActor || entry.toStatus != OrderStatus::Cancelled) return corrupt("取消历史操作者或状态无效", QStringLiteral("history[%1]").arg(i));
            state = entry.toStatus; cancelledAt = entry.at; cancelReason = entry.reason; break;
        case OrderAction::Accept:
            if (!merchantActor || entry.toStatus != OrderStatus::Preparing) return corrupt("接单历史操作者或状态无效", QStringLiteral("history[%1]").arg(i));
            state = entry.toStatus; acceptedAt = entry.at; break;
        case OrderAction::Reject:
            if (!merchantActor || entry.toStatus != OrderStatus::Cancelled || !Validation::reason(entry.reason, true).ok()) return corrupt("拒单历史操作者、状态或原因无效", QStringLiteral("history[%1]").arg(i));
            state = entry.toStatus; payment = PaymentStatus::Refunded; cancelledAt = entry.at; cancelReason = entry.reason; break;
        case OrderAction::MarkReady:
            if (!merchantActor || entry.toStatus != OrderStatus::ReadyForDelivery) return corrupt("出餐历史操作者或状态无效", QStringLiteral("history[%1]").arg(i));
            state = entry.toStatus; readyAt = entry.at; break;
        case OrderAction::Claim: {
            const auto* actor = account(snapshot, entry.actorId, Role::Rider);
            if (!actor || entry.toStatus != OrderStatus::Delivering || rider) return corrupt("认领历史操作者或状态无效", QStringLiteral("history[%1]").arg(i));
            if (!order.riderNameSnapshot || order.riderNameSnapshot->trimmed().isEmpty())
                return corrupt("骑手姓名快照为空", QStringLiteral("riderNameSnapshot"));
            state = entry.toStatus; rider = entry.actorId; riderName = order.riderNameSnapshot; claimedAt = entry.at; break;
        }
        case OrderAction::MarkDelivered:
            if (!rider || entry.actorId != *rider || entry.toStatus != OrderStatus::Delivering || deliveredAt)
                return corrupt("送达历史操作者或状态无效", QStringLiteral("history[%1]").arg(i));
            deliveredAt = entry.at; break;
        case OrderAction::ConfirmReceipt:
            if (!customerActor || !deliveredAt || entry.toStatus != OrderStatus::Completed)
                return corrupt("确认收货历史操作者或状态无效", QStringLiteral("history[%1]").arg(i));
            state = entry.toStatus; completedAt = entry.at; break;
        }
        previous = entry.at;
    }
    if (state != order.status || payment != order.paymentStatus || order.updatedAt != previous
        || rider != order.riderId || riderName != order.riderNameSnapshot
        || !same(order.paidAt, paidAt) || !same(order.acceptedAt, acceptedAt)
        || !same(order.readyAt, readyAt) || !same(order.claimedAt, claimedAt)
        || !same(order.deliveredAt, deliveredAt) || !same(order.completedAt, completedAt)
        || !same(order.cancelledAt, cancelledAt) || order.cancelReason != cancelReason)
        return corrupt(QStringLiteral("订单字段与历史重放结果不一致"), QStringLiteral("history"));
    const auto expectedIncome = state == OrderStatus::Completed ? order.deliveryFeeCents : 0;
    if (order.riderIncomeCents != expectedIncome)
        return corrupt(QStringLiteral("骑手收入与完成状态不一致"), QStringLiteral("riderIncomeCents"));
    return Result<void>::success();
}

Result<void> validateAll(const StoreSnapshot& snapshot) {
    if (snapshot.schemaVersion != Limits::SchemaVersion || snapshot.revision < 0)
        return corrupt(QStringLiteral("存储版本或修订号无效"), QStringLiteral("schemaVersion"));
    if (snapshot.orders.size() > Limits::MaxOrders)
        return corrupt(QStringLiteral("订单数量超过10000"), QStringLiteral("orders"));
    QSet<Id> ids;
    auto addId = [&ids](const Id& id, const QString& field) -> Result<void> {
        if (!Validation::uuid(id, field).ok() || ids.contains(id)) return corrupt(QStringLiteral("ID无效或重复"), field);
        ids.insert(id); return Result<void>::success();
    };
    QSet<QString> logins;
    for (qsizetype i=0; i<snapshot.accounts.size(); ++i) {
        const auto& value = snapshot.accounts.at(i);
        if (!addId(value.id, QStringLiteral("accounts[%1].id").arg(i)).ok()
            || !Validation::loginName(value.loginName).ok() || !Validation::displayName(value.displayName).ok()
            || value.loginName != Validation::normalizeLoginName(value.loginName)
            || !Credentials::validateStored(value).ok()
            || !value.createdAt.isValid()
            || (value.role != Role::Customer && !value.defaultAddress.isEmpty())
            || (value.role == Role::Customer && !Validation::address(value.defaultAddress).ok()))
            return corrupt(QStringLiteral("账号字段无效"), QStringLiteral("accounts[%1]").arg(i));
        const auto login = Validation::normalizeLoginName(value.loginName);
        if (logins.contains(login)) return corrupt(QStringLiteral("登录账号重复"), QStringLiteral("accounts[%1].loginName").arg(i));
        logins.insert(login);
    }
    QHash<Id, int> merchantShops;
    for (qsizetype i=0; i<snapshot.shops.size(); ++i) {
        const auto& value = snapshot.shops.at(i);
        if (!addId(value.id, QStringLiteral("shops[%1].id").arg(i)).ok()
            || !account(snapshot, value.merchantId, Role::Merchant)
            || !Validation::namedEntity(value.name, "shop.name").ok()
            || !Validation::address(value.address).ok() || !Validation::description(value.description).ok()
            || !value.createdAt.isValid())
            return corrupt(QStringLiteral("店铺字段或商家引用无效"), QStringLiteral("shops[%1]").arg(i));
        if (++merchantShops[value.merchantId] != 1) return corrupt(QStringLiteral("一个商家只能对应一个店铺"), QStringLiteral("merchantId"));
    }
    for (const auto& value : snapshot.accounts) {
        if (value.role == Role::Merchant && merchantShops.value(value.id) != 1)
            return corrupt(QStringLiteral("每个商家必须恰有一个店铺"), QStringLiteral("merchantId"));
    }
    QHash<Id, QSet<QString>> dishNames;
    for (qsizetype i=0; i<snapshot.dishes.size(); ++i) {
        const auto& value = snapshot.dishes.at(i);
        if (!addId(value.id, QStringLiteral("dishes[%1].id").arg(i)).ok() || !findById(snapshot.shops, value.shopId)
            || !Validation::namedEntity(value.name, "dish.name").ok() || !Validation::dishPrice(value.priceCents).ok()
            || !value.createdAt.isValid() || !value.updatedAt.isValid() || value.updatedAt < value.createdAt)
            return corrupt(QStringLiteral("菜品字段或店铺引用无效"), QStringLiteral("dishes[%1]").arg(i));
        const auto normalized = Validation::normalizeNamedEntity(value.name);
        if (!value.isDeleted && dishNames[value.shopId].contains(normalized)) return corrupt(QStringLiteral("同店有效菜品重名"), QStringLiteral("dishes[%1].name").arg(i));
        if (!value.isDeleted) dishNames[value.shopId].insert(normalized);
    }
    QSet<Id> carts;
    for (qsizetype i=0; i<snapshot.carts.size(); ++i) {
        const auto& cart = snapshot.carts.at(i);
        if (!account(snapshot, cart.customerId, Role::Customer) || !findById(snapshot.shops, cart.shopId)
            || carts.contains(cart.customerId) || cart.items.size() > Validation::MaxDistinctItems)
            return corrupt(QStringLiteral("购物车归属无效或重复"), QStringLiteral("carts[%1]").arg(i));
        carts.insert(cart.customerId);
        QSet<Id> items;
        for (const auto& item : cart.items) {
            const auto* dish = findById(snapshot.dishes, item.dishId);
            if (!dish || dish->shopId != cart.shopId || dish->isDeleted || !dish->isAvailable
                || items.contains(item.dishId) || !Validation::quantity(item.quantity).ok())
                return corrupt(QStringLiteral("购物车商品无效"), QStringLiteral("carts[%1].items").arg(i));
            items.insert(item.dishId);
        }
    }
    for (qsizetype i=0; i<snapshot.orders.size(); ++i) {
        if (!addId(snapshot.orders.at(i).id, QStringLiteral("orders[%1].id").arg(i)).ok())
            return corrupt(QStringLiteral("订单ID无效或重复"), QStringLiteral("orders[%1].id").arg(i));
        const auto result = validate(snapshot.orders.at(i), snapshot);
        if (!result.ok()) return result;
    }
    return Result<void>::success();
}
} // namespace takeout::OrderPolicy
