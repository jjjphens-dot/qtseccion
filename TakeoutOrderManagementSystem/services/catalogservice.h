#pragma once
#include "core/requests.h"
#include "servicebase.h"

namespace takeout {
class CatalogService final : public ServiceBase {
public:
  using ServiceBase::ServiceBase;
  Result<void> createMerchantWithShop(const MerchantRegistration &registration);
  Result<void> updateProfile(const ProfileChanges &changes);
  Result<void> updateShop(const ShopChanges &changes);
  Result<Id> createDish(const DishDraft &draft);
  Result<void> updateDish(const Id &dishId, const DishChanges &changes);
  Result<void> deleteDish(const Id &dishId);
};
} // namespace takeout
