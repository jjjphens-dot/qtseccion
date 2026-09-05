#pragma once
#include <QString>
namespace takeout {
enum class Role { Customer, Merchant, Rider, Admin };
enum class OrderStatus {
    PendingPayment, Cancelled, PendingAcceptance, Preparing,
    ReadyForDelivery, Delivering, Completed
};
enum class PaymentStatus { Unpaid, Paid, Refunded };
enum class OrderAction {
    CreateOrder, Pay, Cancel, Accept, Reject, MarkReady,
    Claim, MarkDelivered, ConfirmReceipt
};
enum class StartupState { NeedsAdminBootstrap, Ready };

inline QString roleLabel(Role role) {
    switch (role) {
    case Role::Customer: return QStringLiteral("普通用户");
    case Role::Merchant: return QStringLiteral("商家");
    case Role::Rider: return QStringLiteral("骑手");
    case Role::Admin: return QStringLiteral("管理员");
    }
    return {};
}
inline QString statusLabel(OrderStatus status) {
    switch (status) {
    case OrderStatus::PendingPayment: return QStringLiteral("待支付");
    case OrderStatus::Cancelled: return QStringLiteral("已取消");
    case OrderStatus::PendingAcceptance: return QStringLiteral("待接单");
    case OrderStatus::Preparing: return QStringLiteral("制作中");
    case OrderStatus::ReadyForDelivery: return QStringLiteral("待配送");
    case OrderStatus::Delivering: return QStringLiteral("配送中");
    case OrderStatus::Completed: return QStringLiteral("已完成");
    }
    return {};
}
inline bool isLegalCombination(OrderStatus order, PaymentStatus payment) {
    switch (order) {
    case OrderStatus::PendingPayment: return payment == PaymentStatus::Unpaid;
    case OrderStatus::Cancelled:
        return payment == PaymentStatus::Unpaid || payment == PaymentStatus::Refunded;
    case OrderStatus::PendingAcceptance:
    case OrderStatus::Preparing:
    case OrderStatus::ReadyForDelivery:
    case OrderStatus::Delivering:
    case OrderStatus::Completed: return payment == PaymentStatus::Paid;
    }
    return false;
}
namespace Limits {
inline constexpr int SchemaVersion = 1;
inline constexpr qint64 MaxFileBytes = 100LL * 1024 * 1024;
inline constexpr qsizetype MaxOrders = 10000;
inline constexpr qint64 DefaultDeliveryFeeCents = 500;
}
} // namespace takeout
