#include "adminservice.h"

#include "core/validation.h"
#include "data/jsonrepository.h"
#include <QFileInfo>
#include <QSet>

namespace takeout {
namespace {

Account *accountById(StoreSnapshot &snapshot, const Id &id) {
  for (auto &account : snapshot.accounts)
    if (account.id == id)
      return &account;
  return nullptr;
}

bool isNonTerminal(OrderStatus status) {
  return status == OrderStatus::PendingPayment ||
         status == OrderStatus::PendingAcceptance ||
         status == OrderStatus::Preparing ||
         status == OrderStatus::ReadyForDelivery ||
         status == OrderStatus::Delivering;
}

} // namespace

AdminService::AdminService(DataStore &store, SessionContext &session,
                           JsonRepository *repository)
    : ServiceBase(store, session), m_repository(repository) {}

Result<QVector<AccountRow>> AdminService::listAccounts() const {
  const auto permission = requireRole({Role::Admin});
  if (!permission.ok())
    return Result<QVector<AccountRow>>::failure(permission.error());

  QVector<AccountRow> rows;
  const auto snapshot = m_store.snapshot();
  rows.reserve(snapshot.accounts.size());
  for (const auto &account : snapshot.accounts) {
    if (account.role == Role::Admin)
      continue;
    rows.push_back({account.id, account.loginName, account.displayName,
                    account.role, account.isDeleted, account.createdAt});
  }
  return Result<QVector<AccountRow>>::success(std::move(rows));
}

Result<void> AdminService::deleteAccount(const Id &accountId) {
  const auto permission = requireRole({Role::Admin});
  if (!permission.ok())
    return permission;
  if (!Validation::uuid(accountId, QStringLiteral("accountId")).ok())
    return Result<void>::failure({ErrorCode::Validation,
                                  QStringLiteral("账号ID无效"),
                                  QStringLiteral("accountId")});

  auto candidate = m_store.snapshot();
  auto *target = accountById(candidate, accountId);
  if (!target)
    return Result<void>::failure({ErrorCode::NotFound,
                                  QStringLiteral("账号不存在"),
                                  QStringLiteral("accountId")});
  if (target->role == Role::Admin)
    return Result<void>::failure({ErrorCode::Forbidden,
                                  QStringLiteral("不能删除管理员账号"),
                                  QStringLiteral("accountId")});
  if (target->isDeleted)
    return Result<void>::failure({ErrorCode::AlreadyProcessed,
                                  QStringLiteral("账号已经删除"),
                                  QStringLiteral("accountId")});

  for (const auto &order : candidate.orders) {
    bool blocks = false;
    if (target->role == Role::Customer)
      blocks = order.customerId == target->id && isNonTerminal(order.status);
    else if (target->role == Role::Rider)
      blocks = order.riderId && *order.riderId == target->id &&
               order.status == OrderStatus::Delivering;
    else if (target->role == Role::Merchant) {
      for (const auto &shop : candidate.shops)
        if (shop.merchantId == target->id && shop.id == order.shopId &&
            isNonTerminal(order.status)) {
          blocks = true;
          break;
        }
    }
    if (blocks)
      return Result<void>::failure({
          ErrorCode::Conflict, QStringLiteral("账号仍有关联的未完成订单"),
          QStringLiteral("accountId")});
  }

  target->isDeleted = true;
  if (target->role == Role::Merchant) {
    QSet<Id> unpublishedDishes;
    for (auto &shop : candidate.shops) {
      if (shop.merchantId != target->id)
        continue;
      shop.isOpen = false;
      for (auto &dish : candidate.dishes)
        if (dish.shopId == shop.id) {
          dish.isAvailable = false;
          unpublishedDishes.insert(dish.id);
          dish.updatedAt = QDateTime::currentDateTimeUtc();
        }
    }
    for (qsizetype i = candidate.carts.size() - 1; i >= 0; --i) {
      auto &cart = candidate.carts[i];
      for (qsizetype j = cart.items.size() - 1; j >= 0; --j)
        if (unpublishedDishes.contains(cart.items.at(j).dishId))
          cart.items.removeAt(j);
      if (cart.items.isEmpty())
        candidate.carts.removeAt(i);
    }
  }
  for (qsizetype i = candidate.carts.size() - 1; i >= 0; --i)
    if (candidate.carts.at(i).customerId == target->id)
      candidate.carts.removeAt(i);

  return commit(std::move(candidate));
}

Result<void> AdminService::exportData(const QString &path) const {
  const auto permission = requireRole({Role::Admin});
  if (!permission.ok())
    return permission;
  if (!m_repository)
    return Result<void>::failure(notImplemented(QStringLiteral("管理员数据导出")));
  return m_repository->exportSnapshot(path, m_store.snapshot());
}

Result<void> AdminService::importSnapshot(StoreSnapshot snapshot) {
  const auto permission = requireRole({Role::Admin});
  if (!permission.ok())
    return permission;

  const auto session = m_session.current();
  bool activeAdmin = false;
  bool currentAdmin = false;
  for (const auto &account : snapshot.accounts) {
    if (account.role != Role::Admin || account.isDeleted)
      continue;
    activeAdmin = true;
    if (session && account.id == session->accountId)
      currentAdmin = true;
  }
  if (!activeAdmin)
    return Result<void>::failure(
        {ErrorCode::CorruptData, QStringLiteral("导入数据必须包含有效管理员"),
         "accounts"});
  if (!currentAdmin)
    return Result<void>::failure(
        {ErrorCode::Forbidden, QStringLiteral("导入数据不能移除当前管理员账号"),
         "accounts"});

  snapshot.revision = m_store.snapshot().revision;
  const auto committed = commit(std::move(snapshot));
  if (committed.ok())
    m_session.clear();
  return committed;
}

Result<void> AdminService::importData(const QString &path) {
  const auto permission = requireRole({Role::Admin});
  if (!permission.ok())
    return permission;
  if (!m_repository)
    return Result<void>::failure(notImplemented(QStringLiteral("管理员数据导入")));
  const auto imported = m_repository->loadExternal(path);
  if (!imported.ok())
    return Result<void>::failure(imported.error());
  return importSnapshot(imported.value());
}

Result<void> AdminService::restoreBackup() {
  const auto permission = requireRole({Role::Admin});
  if (!permission.ok())
    return permission;
  if (!m_repository)
    return Result<void>::failure(notImplemented(QStringLiteral("管理员备份恢复")));
  const auto backup = m_repository->loadBackup();
  if (!backup.ok())
    return Result<void>::failure(backup.error());
  return importSnapshot(backup.value());
}

} // namespace takeout
