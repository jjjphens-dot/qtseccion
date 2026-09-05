#include "datastore.h"
#include <QThread>
namespace takeout {
DataStore::DataStore(Repository& repository, QObject* parent)
    : QObject(parent), m_repository(repository) {}
Result<StartupState> DataStore::initialize() {
    Q_ASSERT(thread() == QThread::currentThread());
    if (m_initialized)
        return Result<StartupState>::failure({ErrorCode::Conflict, QStringLiteral("数据层已初始化"), {}});
    const auto loaded = m_repository.load();
    if (!loaded.ok()) return Result<StartupState>::failure(loaded.error());
    const auto& candidate = loaded.value();
    bool hasAdmin = false;
    for (const auto& account : candidate.accounts)
        hasAdmin |= account.role == Role::Admin && !account.isDeleted;
    if (!candidate.isEmpty() && !hasAdmin)
        return Result<StartupState>::failure({ErrorCode::CorruptData, QStringLiteral("非空库缺少有效管理员"), "accounts"});
    m_snapshot = candidate;
    m_initialized = true;
    return Result<StartupState>::success(hasAdmin ? StartupState::Ready : StartupState::NeedsAdminBootstrap);
}
Result<void> DataStore::commitCandidate(StoreSnapshot candidate) {
    Q_ASSERT(thread() == QThread::currentThread());
    if (!m_initialized)
        return Result<void>::failure({ErrorCode::Conflict, QStringLiteral("数据尚未初始化"), {}});
    if (candidate.revision != m_snapshot.revision)
        return Result<void>::failure({ErrorCode::Conflict, QStringLiteral("数据版本已变更"), "revision"});
    candidate.revision = m_snapshot.revision + 1;
    candidate.savedAt = QDateTime::currentDateTimeUtc();
    const auto saved = m_repository.save(candidate);
    if (!saved.ok()) return saved;
    m_snapshot = std::move(candidate);
    emit committed(m_snapshot.revision);
    return Result<void>::success();
}
} // namespace takeout
