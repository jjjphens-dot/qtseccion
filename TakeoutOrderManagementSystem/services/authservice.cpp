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

Error revisionChanged() {
  return {ErrorCode::Conflict, QStringLiteral("数据已变化，请重新提交"),
          QStringLiteral("revision")};
}

QByteArray dummySalt() { return QByteArray(Credentials::SaltBytes, '\0'); }

QByteArray dummyHash() { return QByteArray(Credentials::HashBytes, '\0'); }
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

Result<LoginPasswordWork>
AuthService::beginLogin(const QString &loginName, Role expectedRole) const {
  if (!m_store.isInitialized() || m_session.current())
    return Result<LoginPasswordWork>::failure(
        {ErrorCode::Conflict,
         m_session.current() ? QStringLiteral("请先注销当前账号")
                             : QStringLiteral("数据层尚未初始化"),
         {}});

  const auto snapshot = m_store.snapshot();
  LoginPasswordWork work;
  work.baseRevision = snapshot.revision;
  work.expectedRole = expectedRole;
  work.passwordSalt = dummySalt();
  work.expectedHash = dummyHash();
  const auto normalized = Validation::normalizeLoginName(loginName);
  for (const auto &account : snapshot.accounts) {
    if (account.loginName != normalized)
      continue;
    if (!account.isDeleted && account.role == expectedRole &&
        Credentials::validateStored(account).ok()) {
      work.accountId = account.id;
      work.passwordSalt = account.passwordSalt;
      work.expectedHash = account.passwordHash;
      work.passwordIterations = account.passwordIterations;
      work.accountMatch = true;
    }
    break;
  }
  return Result<LoginPasswordWork>::success(std::move(work));
}

Result<void> AuthService::completeLogin(const LoginPasswordWork &work,
                                        const QByteArray &passwordHash) {
  if (!m_store.isInitialized() || m_session.current())
    return Result<void>::failure(
        {ErrorCode::Conflict,
         m_session.current() ? QStringLiteral("请先注销当前账号")
                             : QStringLiteral("数据层尚未初始化"),
         {}});
  const auto snapshot = m_store.snapshot();
  if (snapshot.revision != work.baseRevision)
    return Result<void>::failure(revisionChanged());

  const Account *account = nullptr;
  for (const auto &candidate : snapshot.accounts)
    if (candidate.id == work.accountId) {
      account = &candidate;
      break;
    }
  if (!work.accountMatch || !account || account->isDeleted ||
      account->role != work.expectedRole ||
      !Credentials::validateStored(*account).ok() ||
      !Credentials::constantTimeEqual(passwordHash, work.expectedHash))
    return Result<void>::failure(deniedLogin());

  m_session.establish({account->id, account->role, account->displayName});
  return Result<void>::success();
}

Result<PasswordAccountDraft>
AuthService::beginAccountRegistration(const RegisterRequest &request) const {
  if (request.role != Role::Customer && request.role != Role::Rider)
    return Result<PasswordAccountDraft>::failure(
        {ErrorCode::Forbidden, QStringLiteral("此入口只接受用户或骑手注册"),
         "role"});
  if (!m_store.isInitialized())
    return Result<PasswordAccountDraft>::failure(
        {ErrorCode::Conflict, QStringLiteral("数据层尚未初始化"), {}});
  const auto snapshot = m_store.snapshot();
  if (!hasAdmin(snapshot))
    return Result<PasswordAccountDraft>::failure(
        {ErrorCode::Forbidden, QStringLiteral("请先初始化管理员"), {}});
  const auto prepared = Credentials::prepareAccount(snapshot, request);
  if (!prepared.ok())
    return Result<PasswordAccountDraft>::failure(prepared.error());
  return Result<PasswordAccountDraft>::success(
      {snapshot.revision, prepared.value(), request.password.toUtf8()});
}

Result<void> AuthService::completeAccountRegistration(
    const PasswordAccountDraft &draft, const QByteArray &passwordHash) {
  if (!m_store.isInitialized())
    return Result<void>::failure(
        {ErrorCode::Conflict, QStringLiteral("数据层尚未初始化"), {}});
  auto candidate = m_store.snapshot();
  if (candidate.revision != draft.baseRevision)
    return Result<void>::failure(revisionChanged());
  if (!hasAdmin(candidate))
    return Result<void>::failure(
        {ErrorCode::Forbidden, QStringLiteral("请先初始化管理员"), {}});
  for (const auto &account : candidate.accounts)
    if (Validation::normalizeLoginName(account.loginName) ==
        Validation::normalizeLoginName(draft.account.loginName))
      return Result<void>::failure(
          {ErrorCode::Conflict, QStringLiteral("账号已存在"), "loginName"});
  const auto finalized = Credentials::finalizeAccount(draft.account, passwordHash);
  if (!finalized.ok())
    return Result<void>::failure(finalized.error());
  candidate.accounts.push_back(finalized.value());
  return commit(std::move(candidate));
}

Result<PasswordAccountDraft>
AuthService::beginBootstrap(const AdminBootstrapRequest &request) const {
  if (!m_store.isInitialized())
    return Result<PasswordAccountDraft>::failure(
        {ErrorCode::Conflict, QStringLiteral("数据层尚未初始化"), {}});
  const auto snapshot = m_store.snapshot();
  if (!snapshot.isEmpty())
    return Result<PasswordAccountDraft>::failure(
        {ErrorCode::Forbidden, QStringLiteral("不允许初始化管理员"), {}});
  RegisterRequest registration{request.loginName, request.password,
                               request.displayName, {}, Role::Admin};
  const auto prepared = Credentials::prepareAccount(snapshot, registration);
  if (!prepared.ok())
    return Result<PasswordAccountDraft>::failure(prepared.error());
  return Result<PasswordAccountDraft>::success(
      {snapshot.revision, prepared.value(), request.password.toUtf8()});
}

Result<void> AuthService::completeBootstrap(const PasswordAccountDraft &draft,
                                            const QByteArray &passwordHash) {
  if (!m_store.isInitialized())
    return Result<void>::failure(
        {ErrorCode::Conflict, QStringLiteral("数据层尚未初始化"), {}});
  auto candidate = m_store.snapshot();
  if (candidate.revision != draft.baseRevision)
    return Result<void>::failure(revisionChanged());
  if (!candidate.isEmpty())
    return Result<void>::failure(
        {ErrorCode::Forbidden, QStringLiteral("不允许初始化管理员"), {}});
  const auto finalized = Credentials::finalizeAccount(draft.account, passwordHash);
  if (!finalized.ok())
    return Result<void>::failure(finalized.error());
  candidate.accounts.push_back(finalized.value());
  return commit(std::move(candidate));
}
} // namespace takeout
