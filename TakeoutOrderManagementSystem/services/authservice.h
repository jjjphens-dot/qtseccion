#pragma once
#include "core/credentials.h"
#include "core/requests.h"
#include "servicebase.h"

namespace takeout {
struct LoginPasswordWork {
  qint64 baseRevision = 0;
  Id accountId;
  Role expectedRole = Role::Customer;
  QByteArray passwordSalt;
  QByteArray expectedHash;
  int passwordIterations = Credentials::Iterations;
  bool accountMatch = false;
};

struct PasswordAccountDraft {
  qint64 baseRevision = 0;
  Account account;
  QByteArray passwordUtf8;
};

class AuthService final : public ServiceBase {
public:
  using ServiceBase::ServiceBase;
  Result<void> bootstrapAdmin(const AdminBootstrapRequest &request);
  Result<void> registerAccount(const RegisterRequest &request);
  Result<void> login(const QString &loginName, const QString &password,
                     Role expectedRole);
  Result<void> logout();

  Result<LoginPasswordWork> beginLogin(const QString &loginName,
                                       const QString &password,
                                       Role expectedRole) const;
  Result<void> completeLogin(const LoginPasswordWork &work,
                             const QByteArray &passwordHash);
  Result<PasswordAccountDraft>
  beginAccountRegistration(const RegisterRequest &request) const;
  Result<void> completeAccountRegistration(const PasswordAccountDraft &draft,
                                           const QByteArray &passwordHash);
  Result<PasswordAccountDraft>
  beginBootstrap(const AdminBootstrapRequest &request) const;
  Result<void> completeBootstrap(const PasswordAccountDraft &draft,
                                 const QByteArray &passwordHash);

  // Shared account preparation for CatalogService's atomic merchant+shop flow.
  static Result<Account> prepareAccount(const StoreSnapshot &snapshot,
                                        const RegisterRequest &request);
};
} // namespace takeout
