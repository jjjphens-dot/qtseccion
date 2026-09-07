#include "authservice.h"
#include "core/credentials.h"
#include "core/validation.h"

namespace takeout {
namespace {
bool hasAdmin(const StoreSnapshot &snapshot) {
  for (const auto &account : snapshot.accounts)
    if (account.role == Role::Admin && !account.isDeleted)
      return true;
  return false;
}

Error deniedLogin() {
  return {ErrorCode::Forbidden, QStringLiteral("账号、密码或角色不匹配"),
          QStringLiteral("credentials")};
}
} // namespace

Result<Account> AuthService::prepareAccount(const StoreSnapshot &snapshot,
                                            const RegisterRequest &request) {
  return Credentials::createAccount(snapshot, request);
}

Result<void> AuthService::bootstrapAdmin(const AdminBootstrapRequest &request) {
  if (!m_store.isInitialized() || !m_store.snapshot().isEmpty())
    return Result<void>::failure(
        {ErrorCode::Forbidden, QStringLiteral("不允许初始化管理员"), {}});
  RegisterRequest registration{request.loginName,
                               request.password,
                               request.displayName,
                               {},
                               Role::Admin};
  auto candidate = m_store.snapshot();
  const auto account = prepareAccount(candidate, registration);
  if (!account.ok())
    return Result<void>::failure(account.error());
  candidate.accounts.push_back(account.value());
  return commit(std::move(candidate));
}

Result<void> AuthService::registerAccount(const RegisterRequest &request) {
  if (request.role != Role::Customer && request.role != Role::Rider)
    return Result<void>::failure({ErrorCode::Forbidden,
                                  QStringLiteral("此入口只接受用户或骑手注册"),
                                  "role"});
  auto candidate = m_store.snapshot();
  if (!m_store.isInitialized() || !hasAdmin(candidate))
    return Result<void>::failure(
        {ErrorCode::Forbidden, QStringLiteral("请先初始化管理员"), {}});
  const auto account = prepareAccount(candidate, request);
  if (!account.ok())
    return Result<void>::failure(account.error());
  candidate.accounts.push_back(account.value());
  return commit(std::move(candidate));
}

Result<void> AuthService::login(const QString &loginName,
                                const QString &password, Role expectedRole) {
  if (!m_store.isInitialized() || m_session.current())
    return Result<void>::failure({ErrorCode::Conflict,
                                  m_session.current()
                                      ? QStringLiteral("请先注销当前账号")
                                      : QStringLiteral("数据层尚未初始化"),
                                  {}});
  const auto normalized = Validation::normalizeLoginName(loginName);
  for (const auto &account : m_store.snapshot().accounts) {
    if (account.loginName != normalized)
      continue;
    if (account.isDeleted || account.role != expectedRole ||
        !Credentials::verifyPassword(account, password))
      return Result<void>::failure(deniedLogin());
    m_session.establish({account.id, account.role, account.displayName});
    return Result<void>::success();
  }
  return Result<void>::failure(deniedLogin());
}

Result<void> AuthService::logout() {
  m_session.clear();
  return Result<void>::success();
}
} // namespace takeout
