#pragma once
#include "core/requests.h"
#include "servicebase.h"

namespace takeout {
class CatalogService final : public ServiceBase {
public:
  using ServiceBase::ServiceBase;
  Result<void> createMerchantWithShop(const MerchantRegistration &registration);
  Result<void> updateProfile(const ProfileChanges &) {
    return pending("updateProfile",
                   {Role::Customer, Role::Merchant, Role::Rider});
  }
  Result<void> updateShop(const ShopChanges &) {
    return pending("updateShop", {Role::Merchant});
  }
  Result<Id> createDish(const DishDraft &) {
    auto result = pending("createDish", {Role::Merchant});
    return Result<Id>::failure(result.error());
  }
  Result<void> updateDish(const Id &, const DishChanges &) {
    return pending("updateDish", {Role::Merchant});
  }
  Result<void> deleteDish(const Id &) {
    return pending("deleteDish", {Role::Merchant});
  }
};
} // namespace takeout
