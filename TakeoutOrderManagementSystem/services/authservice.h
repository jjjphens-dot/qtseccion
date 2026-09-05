#pragma once
#include "servicebase.h"
#include "core/requests.h"
namespace takeout {
class AuthService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<void> bootstrapAdmin(const AdminBootstrapRequest&) {
        if (!m_store.isInitialized() || !m_store.snapshot().isEmpty())
            return Result<void>::failure({ErrorCode::Forbidden, QStringLiteral("不允许初始化管理员"), {}});
        return Result<void>::failure(notImplemented(QStringLiteral("管理员初始化（W04）")));
    }
    Result<void> registerAccount(const RegisterRequest& request) {
        if (request.role != Role::Customer && request.role != Role::Rider)
            return Result<void>::failure({ErrorCode::Forbidden, QStringLiteral("此入口只接受用户或骑手注册"), "role"});
        if (!m_store.isInitialized() || m_store.snapshot().isEmpty())
            return Result<void>::failure({ErrorCode::Forbidden, QStringLiteral("请先初始化管理员"), {}});
        return Result<void>::failure(notImplemented(QStringLiteral("注册（W04）")));
    }
    Result<void> login(const QString&, const QString&, Role) {
        return Result<void>::failure(notImplemented(QStringLiteral("认证（W04）")));
    }
    Result<void> logout() { m_session.clear(); return Result<void>::success(); }
};
} // namespace takeout
