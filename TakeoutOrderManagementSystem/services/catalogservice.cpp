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

Account *accountById(StoreSnapshot &snapshot, const Id &id) {
  for (auto &account : snapshot.accounts)
    if (account.id == id)
      return &account;
  return nullptr;
}

Shop *shopForMerchant(StoreSnapshot &snapshot, const Id &merchantId) {
  for (auto &shop : snapshot.shops)
    if (shop.merchantId == merchantId)
      return &shop;
  return nullptr;
}

Dish *dishById(StoreSnapshot &snapshot, const Id &id) {
  for (auto &dish : snapshot.dishes)
    if (dish.id == id)
      return &dish;
  return nullptr;
}

bool activeDishNameTaken(const QVector<Dish> &dishes, const Id &shopId,
                         const QString &name, const Id &except = {}) {
  const auto normalizedName = Validation::normalizeNamedEntity(name);
  for (const auto &dish : dishes)
    if (!dish.isDeleted && dish.shopId == shopId && dish.id != except &&
        Validation::normalizeNamedEntity(dish.name) == normalizedName)
      return true;
  return false;
}

void removeDishFromCarts(StoreSnapshot &snapshot, const Id &dishId) {
  for (qsizetype i = snapshot.carts.size() - 1; i >= 0; --i) {
    auto &cart = snapshot.carts[i];
    for (qsizetype j = cart.items.size() - 1; j >= 0; --j)
      if (cart.items.at(j).dishId == dishId)
        cart.items.removeAt(j);
    if (cart.items.isEmpty())
      snapshot.carts.removeAt(i);
  }
}
} // namespace

Result<void> CatalogService::createMerchantWithShop(
    const MerchantRegistration &registration) {
  const auto prepared = beginMerchantRegistration(registration);
  if (!prepared.ok())
    return Result<void>::failure(prepared.error());
  const auto derived = Credentials::derivePbkdf2(
      prepared.value().account.passwordUtf8,
      prepared.value().account.account.passwordSalt,
      prepared.value().account.account.passwordIterations);
  if (!derived.ok())
    return Result<void>::failure(derived.error());
  return completeMerchantRegistration(prepared.value(), derived.value());
}

Result<MerchantRegistrationWork>
CatalogService::beginMerchantRegistration(
    const MerchantRegistration &registration) const {
  auto candidate = m_store.snapshot();
  if (!m_store.isInitialized() || !hasAdmin(candidate))
    return Result<MerchantRegistrationWork>::failure(
        {ErrorCode::Forbidden, QStringLiteral("请先初始化管理员"), {}});
  auto checked = Validation::namedEntity(registration.shopName, "shop.name");
  if (!checked.ok())
    return Result<MerchantRegistrationWork>::failure(checked.error());
  checked = Validation::address(registration.address);
  if (!checked.ok())
    return Result<MerchantRegistrationWork>::failure(checked.error());
  checked = Validation::description(registration.description);
  if (!checked.ok())
    return Result<MerchantRegistrationWork>::failure(checked.error());

  RegisterRequest request{registration.loginName,
                          registration.password,
                          registration.displayName,
                          {},
                          Role::Merchant};
  const auto account = Credentials::prepareAccount(candidate, request);
  if (!account.ok())
    return Result<MerchantRegistrationWork>::failure(account.error());
  return Result<MerchantRegistrationWork>::success(
      {{candidate.revision, account.value(), registration.password.toUtf8()},
       normalized(registration.shopName), normalized(registration.description),
       normalized(registration.address)});
}

Result<void> CatalogService::completeMerchantRegistration(
    const MerchantRegistrationWork &work, const QByteArray &passwordHash) {
  if (!m_store.isInitialized())
    return Result<void>::failure(
        {ErrorCode::Conflict, QStringLiteral("数据层尚未初始化"), {}});
  auto candidate = m_store.snapshot();
  if (candidate.revision != work.account.baseRevision)
    return Result<void>::failure(
        {ErrorCode::Conflict, QStringLiteral("数据已变化，请重新提交"),
         QStringLiteral("revision")});
  if (!hasAdmin(candidate))
    return Result<void>::failure(
        {ErrorCode::Forbidden, QStringLiteral("请先初始化管理员"), {}});
  auto checked = Validation::namedEntity(work.shopName, "shop.name");
  if (!checked.ok())
    return checked;
  checked = Validation::address(work.address);
  if (!checked.ok())
    return checked;
  checked = Validation::description(work.description);
  if (!checked.ok())
    return checked;
  for (const auto &account : candidate.accounts)
    if (Validation::normalizeLoginName(account.loginName) ==
        Validation::normalizeLoginName(work.account.account.loginName))
      return Result<void>::failure(
          {ErrorCode::Conflict, QStringLiteral("账号已存在"), "loginName"});
  const auto account = Credentials::finalizeAccount(work.account.account,
                                                     passwordHash);
  if (!account.ok())
    return Result<void>::failure(account.error());
  Shop shop;
  shop.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  shop.merchantId = account.value().id;
  shop.name = work.shopName;
  shop.description = work.description;
  shop.address = work.address;
  shop.isOpen = false;
  shop.createdAt = account.value().createdAt;
  candidate.accounts.push_back(account.value());
  candidate.shops.push_back(std::move(shop));
  return commit(std::move(candidate));
}

Result<void> CatalogService::updateProfile(const ProfileChanges &changes) {
  const auto permission =
      requireRole({Role::Customer, Role::Merchant, Role::Rider});
  if (!permission.ok())
    return permission;
  const auto current = m_session.current();
  auto candidate = m_store.snapshot();
  auto *account = accountById(candidate, current->accountId);
  if (!account)
    return Result<void>::failure(
        {ErrorCode::NotFound, QStringLiteral("账号不存在"), "accountId"});
  auto checked = Validation::displayName(changes.displayName);
  if (!checked.ok())
    return checked;
  if (current->role == Role::Customer) {
    checked = Validation::address(changes.address);
    if (!checked.ok())
      return checked;
  } else if (!changes.address.trimmed().isEmpty()) {
    return Result<void>::failure({ErrorCode::Validation,
                                  QStringLiteral("此角色不保存配送地址"),
                                  "address"});
  }
  const auto displayName = normalized(changes.displayName);
  account->displayName = displayName;
  if (current->role == Role::Customer)
    account->defaultAddress = normalized(changes.address);
  const auto saved = commit(std::move(candidate));
  if (saved.ok())
    m_session.updateDisplayName(displayName);
  return saved;
}

Result<void> CatalogService::updateShop(const ShopChanges &changes) {
  const auto permission = requireRole({Role::Merchant});
  if (!permission.ok())
    return permission;
  auto candidate = m_store.snapshot();
  const auto current = m_session.current();
  auto *shop = shopForMerchant(candidate, current->accountId);
  if (!shop)
    return Result<void>::failure(
        {ErrorCode::NotFound, QStringLiteral("店铺不存在"), "shopId"});
  auto checked = Validation::namedEntity(changes.name, "shop.name");
  if (!checked.ok())
    return checked;
  checked = Validation::address(changes.address);
  if (!checked.ok())
    return checked;
  checked = Validation::description(changes.description);
  if (!checked.ok())
    return checked;
  shop->name = normalized(changes.name);
  shop->description = normalized(changes.description);
  shop->address = normalized(changes.address);
  shop->isOpen = changes.isOpen;
  return commit(std::move(candidate));
}

Result<Id> CatalogService::createDish(const DishDraft &draft) {
  const auto permission = requireRole({Role::Merchant});
  if (!permission.ok())
    return Result<Id>::failure(permission.error());
  auto candidate = m_store.snapshot();
  const auto current = m_session.current();
  const auto *shop = shopForMerchant(candidate, current->accountId);
  if (!shop)
    return Result<Id>::failure(
        {ErrorCode::NotFound, QStringLiteral("店铺不存在"), "shopId"});
  auto checked = Validation::namedEntity(draft.name, "dish.name");
  if (!checked.ok())
    return Result<Id>::failure(checked.error());
  checked = Validation::dishPrice(draft.priceCents);
  if (!checked.ok())
    return Result<Id>::failure(checked.error());
  if (activeDishNameTaken(candidate.dishes, shop->id, draft.name))
    return Result<Id>::failure(
        {ErrorCode::Conflict, QStringLiteral("同店已有同名菜品"), "dish.name"});
  Dish dish;
  dish.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  dish.shopId = shop->id;
  dish.name = normalized(draft.name);
  dish.priceCents = draft.priceCents;
  dish.isAvailable = draft.isAvailable;
  dish.isDeleted = false;
  dish.createdAt = QDateTime::currentDateTimeUtc();
  dish.updatedAt = dish.createdAt;
  candidate.dishes.push_back(dish);
  const auto saved = commit(std::move(candidate));
  if (!saved.ok())
    return Result<Id>::failure(saved.error());
  return Result<Id>::success(dish.id);
}

Result<void> CatalogService::updateDish(const Id &dishId,
                                        const DishChanges &changes) {
  const auto permission = requireRole({Role::Merchant});
  if (!permission.ok())
    return permission;
  auto candidate = m_store.snapshot();
  const auto current = m_session.current();
  auto *shop = shopForMerchant(candidate, current->accountId);
  auto *dish = dishById(candidate, dishId);
  if (!shop || !dish || dish->shopId != shop->id)
    return Result<void>::failure({ErrorCode::NotFound,
                                  QStringLiteral("菜品不存在或不属于当前店铺"),
                                  "dishId"});
  if (dish->isDeleted)
    return Result<void>::failure({ErrorCode::Conflict,
                                  QStringLiteral("已删除菜品不能直接修改"),
                                  "dishId"});
  auto checked = Validation::namedEntity(changes.name, "dish.name");
  if (!checked.ok())
    return checked;
  checked = Validation::dishPrice(changes.priceCents);
  if (!checked.ok())
    return checked;
  if (activeDishNameTaken(candidate.dishes, shop->id, changes.name, dishId))
    return Result<void>::failure(
        {ErrorCode::Conflict, QStringLiteral("同店已有同名菜品"), "dish.name"});
  dish->name = normalized(changes.name);
  dish->priceCents = changes.priceCents;
  dish->isAvailable = changes.isAvailable;
  if (!dish->isAvailable)
    removeDishFromCarts(candidate, dish->id);
  dish->updatedAt = QDateTime::currentDateTimeUtc();
  return commit(std::move(candidate));
}

Result<void> CatalogService::deleteDish(const Id &dishId) {
  const auto permission = requireRole({Role::Merchant});
  if (!permission.ok())
    return permission;
  auto candidate = m_store.snapshot();
  const auto current = m_session.current();
  auto *shop = shopForMerchant(candidate, current->accountId);
  auto *dish = dishById(candidate, dishId);
  if (!shop || !dish || dish->shopId != shop->id)
    return Result<void>::failure({ErrorCode::NotFound,
                                  QStringLiteral("菜品不存在或不属于当前店铺"),
                                  "dishId"});
  if (dish->isDeleted)
    return Result<void>::failure({ErrorCode::AlreadyProcessed,
                                  QStringLiteral("菜品已经删除"), "dishId"});
  dish->isDeleted = true;
  dish->isAvailable = false;
  removeDishFromCarts(candidate, dish->id);
  dish->updatedAt = QDateTime::currentDateTimeUtc();
  return commit(std::move(candidate));
}
} // namespace takeout
