#pragma once
#include "enums.h"
#include <QByteArray>
#include <QDateTime>
#include <QVector>
#include <optional>

namespace takeout {
using Id = QString; // Full UUID, never a row number or abbreviated display ID.
using Money = qint64;
struct Account {
    Id id;
    QString loginName, displayName;
    Role role = Role::Customer;
    QByteArray passwordSalt, passwordHash;
    int passwordIterations = 0;
    QString passwordAlgorithm, defaultAddress;
    bool isDeleted = false;
    QDateTime createdAt;
};
struct Shop {
    Id id, merchantId;
    QString name, description, address;
    bool isOpen = false;
    QDateTime createdAt;
};
struct Dish {
    Id id, shopId;
    QString name;
    Money priceCents = 0;
    bool isAvailable = false, isDeleted = false;
    QDateTime createdAt, updatedAt;
};
struct CartItem { Id dishId; int quantity = 1; };
struct Cart { Id customerId, shopId; QVector<CartItem> items; };
struct OrderItem {
    Id dishId;
    QString dishNameSnapshot;
    Money unitPriceCents = 0;
    int quantity = 1;
    Money lineTotalCents = 0;
};
struct OrderHistoryEntry {
    OrderAction action = OrderAction::CreateOrder;
    std::optional<OrderStatus> fromStatus;
    OrderStatus toStatus = OrderStatus::PendingPayment;
    Id actorId;
    QDateTime at;
    QString reason;
};
struct Order {
    Id id, customerId, shopId;
    std::optional<Id> riderId;
    QVector<OrderItem> items;
    OrderStatus status = OrderStatus::PendingPayment;
    PaymentStatus paymentStatus = PaymentStatus::Unpaid;
    Money subtotalCents = 0, deliveryFeeCents = 0, totalCents = 0, riderIncomeCents = 0;
    QString customerNameSnapshot, addressSnapshot, shopNameSnapshot, shopAddressSnapshot;
    std::optional<QString> riderNameSnapshot;
    QDateTime createdAt, updatedAt;
    std::optional<QDateTime> paidAt, acceptedAt, readyAt, claimedAt,
        deliveredAt, completedAt, cancelledAt;
    QString cancelReason;
    QVector<OrderHistoryEntry> history;
};
struct StoreSnapshot {
    int schemaVersion = Limits::SchemaVersion;
    qint64 revision = 0;
    QDateTime savedAt;
    QVector<Account> accounts;
    QVector<Shop> shops;
    QVector<Dish> dishes;
    QVector<Cart> carts;
    QVector<Order> orders;
    bool isEmpty() const {
        return accounts.isEmpty() && shops.isEmpty() && dishes.isEmpty()
            && carts.isEmpty() && orders.isEmpty();
    }
};

// Only this authorized, non-sensitive projection may reach an order table.
struct OrderRow {
    Id id;
    QString shopName;
    OrderStatus status = OrderStatus::PendingPayment;
    Money totalCents = 0;
    QDateTime createdAt;
};
} // namespace takeout
