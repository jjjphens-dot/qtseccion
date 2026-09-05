#include "appcontext.h"
#include <QDir>
namespace takeout {
AppContext::AppContext(AppPaths paths)
    : m_paths(std::move(paths)), m_lock(m_paths.lockFile()), m_repository(m_paths.dataFile()),
      m_store(m_repository), m_auth(m_store, m_session), m_catalog(m_store, m_session),
      m_orders(m_store, m_session), m_orderQuery(m_store, m_session),
      m_admin(m_store, m_session), m_statistics(m_store, m_session) {}
Result<StartupState> AppContext::initialize() {
    if (!QDir().mkpath(m_paths.directory))
        return Result<StartupState>::failure({ErrorCode::Persistence, QStringLiteral("无法创建数据目录"), m_paths.directory});
    if (!m_lock.tryLock(0))
        return Result<StartupState>::failure({ErrorCode::Conflict,
            m_lock.error() == QLockFile::LockFailedError ? QStringLiteral("已有实例使用此数据目录")
                                                       : QStringLiteral("数据目录锁不可用"), m_paths.directory});
    return m_store.initialize();
}
} // namespace takeout
