#pragma once
#include "servicebase.h"
#include "core/requests.h"
namespace takeout {
class OrderQueryService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<QVector<OrderRow>> visibleOrders(const OrderFilter& = {}) const {
        auto r = pending("授权订单查询（W06）", {Role::Customer, Role::Merchant, Role::Rider});
        return Result<QVector<OrderRow>>::failure(r.error());
    }
    // Detailed role-scoped DTO deliberately deferred to W06; do not expose Order.
};
} // namespace takeout
