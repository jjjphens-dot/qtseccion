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
struct OrderFilter {
    QString keyword;
    std::optional<OrderStatus> status;
    std::optional<QDateTime> from, until; // UTC half-open [from, until).
};
struct DateRange { QDateTime from, until; };
struct StatisticsSummary { qint64 completedCount = 0; Money totalCents = 0; };
} // namespace takeout
