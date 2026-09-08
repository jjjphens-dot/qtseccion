#pragma once
#include "servicebase.h"
namespace takeout {
class AdminService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<QVector<AccountRow>> listAccounts() const;
    Result<void> deleteAccount(const Id &accountId);
};
} // namespace takeout
