#pragma once
#include "core/requests.h"
#include "servicebase.h"

namespace takeout {
class AuthService final : public ServiceBase {
public:
  using ServiceBase::ServiceBase;
  Result<void> bootstrapAdmin(const AdminBootstrapRequest &request);
  Result<void> registerAccount(const RegisterRequest &request);
  Result<void> login(const QString &loginName, const QString &password,
                     Role expectedRole);
  Result<void> logout();

  // Shared account preparation for CatalogService's atomic merchant+shop flow.
  static Result<Account> prepareAccount(const StoreSnapshot &snapshot,
                                        const RegisterRequest &request);
};
} // namespace takeout
