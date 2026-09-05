#pragma once
#include "servicebase.h"
#include "core/requests.h"
namespace takeout {
class CatalogService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<void> createMerchantWithShop(const MerchantRegistration&) {
        if (!m_store.isInitialized() || m_store.snapshot().isEmpty())
            return Result<void>::failure({ErrorCode::Forbidden, QStringLiteral("请先初始化管理员"), {}});
        return Result<void>::failure(notImplemented(QStringLiteral("商家与店铺注册（W04）")));
    }
    Result<void> updateProfile(const ProfileChanges&) { return pending("updateProfile", {Role::Customer, Role::Merchant, Role::Rider}); }
    Result<void> updateShop(const ShopChanges&) { return pending("updateShop", {Role::Merchant}); }
    Result<Id> createDish(const DishDraft&) {
        auto r = pending("createDish", {Role::Merchant}); return Result<Id>::failure(r.error());
    }
    Result<void> updateDish(const Id&, const DishChanges&) { return pending("updateDish", {Role::Merchant}); }
    Result<void> deleteDish(const Id&) { return pending("deleteDish", {Role::Merchant}); }
};
} // namespace takeout
