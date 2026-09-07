#pragma once
#include "entities.h"
namespace takeout {
struct RegisterRequest {
    QString loginName, password, displayName, address;
    Role role = Role::Customer;
};
struct AdminBootstrapRequest { QString loginName, password, displayName; };
struct MerchantRegistration {
    QString loginName, password, displayName, shopName, description, address;
};
struct ProfileChanges { QString displayName, address; };
struct ShopChanges { QString name, description, address; bool isOpen = false; };
struct DishDraft { QString name; Money priceCents = 0; bool isAvailable = false; };
using DishChanges = DishDraft;
struct CartChanges { Id shopId; QVector<CartItem> items; };
struct CheckoutRequest { QString customerName, address; };
// Role-filtered order detail. It deliberately omits the mutable Order entity
// and leaves customerName/address empty for an unclaimed rider view.
struct OrderDetail {
    Id id, customerId, shopId;
    QVector<OrderItem> items;
    OrderStatus status = OrderStatus::PendingPayment;
    PaymentStatus paymentStatus = PaymentStatus::Unpaid;
    Money subtotalCents = 0, deliveryFeeCents = 0, totalCents = 0, riderIncomeCents = 0;
    QString customerName, address, shopName, shopAddress;
    std::optional<QString> riderName;
    QDateTime createdAt, updatedAt;
    std::optional<QDateTime> paidAt, acceptedAt, readyAt, claimedAt,
        deliveredAt, completedAt, cancelledAt;
    QString cancelReason;
    QVector<OrderHistoryEntry> history;
};
struct OrderFilter {
    QString keyword;
    std::optional<OrderStatus> status;
    std::optional<QDateTime> from, until; // UTC half-open [from, until).
};
struct DateRange { QDateTime from, until; };
struct StatisticsSummary { qint64 completedCount = 0; Money totalCents = 0; };
} // namespace takeout
