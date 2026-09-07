#pragma once
#include "core/requests.h"
#include "servicebase.h"
namespace takeout {
class OrderService final : public ServiceBase {
public:
  using ServiceBase::ServiceBase;
  Result<void> updateCart(const CartChanges &changes);
  Result<Id> createOrder(const CheckoutRequest &) {
    auto r = pending("createOrder（W06）", {Role::Customer});
    return Result<Id>::failure(r.error());
  }
  Result<void> execute(const Id &, OrderAction action, const QString & = {}) {
    switch (action) {
    case OrderAction::Pay:
    case OrderAction::Cancel:
    case OrderAction::ConfirmReceipt:
      return pending("订单命令（W06）", {Role::Customer});
    case OrderAction::Accept:
    case OrderAction::Reject:
    case OrderAction::MarkReady:
      return pending("订单命令（W06）", {Role::Merchant});
    case OrderAction::Claim:
    case OrderAction::MarkDelivered:
      return pending("订单命令（W06）", {Role::Rider});
    case OrderAction::CreateOrder:
      return Result<void>::failure({ErrorCode::InvalidTransition,
                                    QStringLiteral("请使用 createOrder"),
                                    {}});
    }
    return Result<void>::failure(
        {ErrorCode::InvalidTransition, QStringLiteral("未知动作"), {}});
  }
};
} // namespace takeout
