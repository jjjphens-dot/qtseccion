#include "appcontext.h"
#include <QDir>
namespace takeout {
namespace {
bool hasActiveAdmin(const StoreSnapshot &snapshot) {
    for (const auto &account : snapshot.accounts)
        if (account.role == Role::Admin && !account.isDeleted)
            return true;
    return false;
}
} // namespace

AppContext::AppContext(AppPaths paths)
    : m_paths(std::move(paths)), m_lock(m_paths.lockFile()), m_repository(m_paths.dataFile()),
      m_store(m_repository), m_auth(m_store, m_session), m_catalog(m_store, m_session),
      m_orders(m_store, m_session), m_orderQuery(m_store, m_session),
      m_admin(m_store, m_session, &m_repository),
      m_statistics(m_store, m_session) {}
Result<StartupState> AppContext::initialize() {
    if (!QDir().mkpath(m_paths.directory))
        return Result<StartupState>::failure({ErrorCode::Persistence, QStringLiteral("无法创建数据目录"), m_paths.directory});
    if (!m_lock.tryLock(0))
        return Result<StartupState>::failure({ErrorCode::Conflict,
            m_lock.error() == QLockFile::LockFailedError ? QStringLiteral("已有实例使用此数据目录")
                                                       : QStringLiteral("数据目录锁不可用"), m_paths.directory});
    return m_store.initialize();
}

Result<StartupState> AppContext::recoverFromBackup() {
    if (m_store.isInitialized())
        return Result<StartupState>::failure(
            {ErrorCode::Conflict, QStringLiteral("数据层已经初始化，无需启动恢复"), {}});
    const auto backup = m_repository.loadBackup();
    if (!backup.ok())
        return Result<StartupState>::failure(backup.error());
    if (!backup.value().isEmpty() && !hasActiveAdmin(backup.value()))
        return Result<StartupState>::failure(
            {ErrorCode::CorruptData, QStringLiteral("备份缺少有效管理员，拒绝恢复"),
             "accounts"});
    const auto restored = m_repository.restoreSnapshot(backup.value());
    if (!restored.ok())
        return Result<StartupState>::failure(restored.error());
    return m_store.initialize();
}
} // namespace takeout
