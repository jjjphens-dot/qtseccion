#pragma once
#include "servicebase.h"
namespace takeout {
class JsonRepository;
class AdminService final : public ServiceBase {
public:
    AdminService(DataStore &store, SessionContext &session,
                 JsonRepository *repository = nullptr);
    Result<QVector<AccountRow>> listAccounts() const;
    Result<void> deleteAccount(const Id &accountId);
    Result<void> exportData(const QString &path) const;
    Result<void> importData(const QString &path);
    Result<void> restoreBackup();

private:
    Result<void> importSnapshot(StoreSnapshot snapshot);
    JsonRepository *m_repository = nullptr;
};
} // namespace takeout
