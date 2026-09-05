#pragma once
#include "sessioncontext.h"
#include "data/datastore.h"
#include <initializer_list>
namespace takeout {
class ServiceBase {
public:
    ServiceBase(DataStore& store, SessionContext& session) : m_store(store), m_session(session) {}
    virtual ~ServiceBase() = default;
protected:
    Result<void> requireRole(std::initializer_list<Role> roles) const {
        const auto session = m_session.current();
        if (!m_store.isInitialized() || !session)
            return Result<void>::failure({ErrorCode::Forbidden, QStringLiteral("请先登录"), {}});
        const auto snapshot = m_store.snapshot();
        for (const auto& account : snapshot.accounts) {
            if (account.id != session->accountId || account.isDeleted || account.role != session->role) continue;
            for (Role role : roles) if (role == account.role) return Result<void>::success();
        }
        return Result<void>::failure({ErrorCode::Forbidden, QStringLiteral("无操作权限"), {}});
    }
    Result<void> pending(const QString& operation, std::initializer_list<Role> roles) const {
        auto permission = requireRole(roles);
        if (!permission.ok()) return permission;
        return Result<void>::failure(notImplemented(operation));
    }
    Result<void> commit(StoreSnapshot candidate) { return m_store.commitCandidate(std::move(candidate)); }
    DataStore& m_store;
    SessionContext& m_session;
};
} // namespace takeout
