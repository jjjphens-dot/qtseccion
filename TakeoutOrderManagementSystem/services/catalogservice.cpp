#include "catalogservice.h"
#include "authservice.h"
#include "core/validation.h"
#include <QUuid>

namespace takeout {
namespace {
bool hasAdmin(const StoreSnapshot &snapshot) {
  for (const auto &account : snapshot.accounts)
    if (account.role == Role::Admin && !account.isDeleted)
      return true;
  return false;
}

QString normalized(const QString &value) {
  return value.normalized(QString::NormalizationForm_C).trimmed();
}
} // namespace

Result<void> CatalogService::createMerchantWithShop(
    const MerchantRegistration &registration) {
  auto candidate = m_store.snapshot();
  if (!m_store.isInitialized() || !hasAdmin(candidate))
    return Result<void>::failure(
        {ErrorCode::Forbidden, QStringLiteral("请先初始化管理员"), {}});
  auto checked = Validation::namedEntity(registration.shopName, "shop.name");
  if (!checked.ok())
    return checked;
  checked = Validation::address(registration.address);
  if (!checked.ok())
    return checked;
  checked = Validation::description(registration.description);
  if (!checked.ok())
    return checked;

  RegisterRequest request{registration.loginName,
                          registration.password,
                          registration.displayName,
                          {},
                          Role::Merchant};
  const auto account = AuthService::prepareAccount(candidate, request);
  if (!account.ok())
    return Result<void>::failure(account.error());

  Shop shop;
  shop.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  shop.merchantId = account.value().id;
  shop.name = normalized(registration.shopName);
  shop.description = normalized(registration.description);
  shop.address = normalized(registration.address);
  shop.isOpen = false;
  shop.createdAt = account.value().createdAt;
  candidate.accounts.push_back(account.value());
  candidate.shops.push_back(std::move(shop));
  return commit(std::move(candidate));
}
} // namespace takeout
