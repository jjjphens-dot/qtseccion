#pragma once
#include "servicebase.h"
#include "core/requests.h"
namespace takeout {
class OrderQueryService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<QVector<OrderRow>> visibleOrders(const OrderFilter& = {}) const;
    Result<OrderDetail> orderDetail(const Id &orderId) const;
};
} // namespace takeout
