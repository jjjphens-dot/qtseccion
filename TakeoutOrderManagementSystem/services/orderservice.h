#pragma once
#include "core/requests.h"
#include "servicebase.h"
namespace takeout {
class OrderService final : public ServiceBase {
public:
  using ServiceBase::ServiceBase;
  Result<void> updateCart(const CartChanges &changes);
  Result<Id> createOrder(const CheckoutRequest &request);
  Result<void> execute(const Id &orderId, OrderAction action,
                       const QString &reason = {});
};
} // namespace takeout
