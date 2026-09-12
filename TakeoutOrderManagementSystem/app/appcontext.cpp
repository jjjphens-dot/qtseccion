#include "appcontext.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
namespace takeout {
namespace {
bool hasActiveAdmin(const StoreSnapshot &snapshot) {
    for (const auto &account : snapshot.accounts)
        if (account.role == Role::Admin && !account.isDeleted)
            return true;
    return false;
}

QString corruptCopyPath(const QString &primary) {
    const QFileInfo info(primary);
    const auto stamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    const auto base = QDir(info.absolutePath()).filePath(
        QStringLiteral("%1.corrupt.%2").arg(info.completeBaseName(), stamp));
    QString candidate = base + QStringLiteral(".json");
    int suffix = 1;
    while (QFileInfo::exists(candidate))
        candidate = base + QStringLiteral(".%1.json").arg(suffix++);
    return candidate;
}
} // namespace

AppContext::AppContext(AppPaths paths)
    : m_paths(std::move(paths)), m_lock(m_paths.lockFile()), m_repository(m_paths.dataFile()),
      m_store(m_repository), m_auth(m_store, m_session), m_catalog(m_store, m_session),
      m_orders(m_store, m_session), m_orderQuery(m_store, m_session),
      m_admin(m_store, m_session, &m_repository),
      m_statistics(m_store, m_session) {}
Result<StartupState> AppContext::initialize() {
    if (m_startupPhase != StartupPhase::NeverInitialized)
        return Result<StartupState>::failure(
            {ErrorCode::Conflict, QStringLiteral("启动状态已确定，不能重复初始化"), {}});
    if (!QDir().mkpath(m_paths.directory))
    {
        m_startupPhase = StartupPhase::FailedNonRecoverable;
        return Result<StartupState>::failure({ErrorCode::Persistence, QStringLiteral("无法创建数据目录"), m_paths.directory});
    }
    if (!m_lock.tryLock(0))
    {
        m_startupPhase = StartupPhase::FailedNonRecoverable;
        return Result<StartupState>::failure({ErrorCode::Conflict,
            m_lock.error() == QLockFile::LockFailedError ? QStringLiteral("已有实例使用此数据目录")
                                                       : QStringLiteral("数据目录锁不可用"), m_paths.directory});
    }
    const auto initialized = m_store.initialize();
    if (initialized.ok())
        m_startupPhase = StartupPhase::Initialized;
    else if (initialized.error().code == ErrorCode::RecoveryAvailable)
        m_startupPhase = StartupPhase::RecoveryAvailable;
    else
        m_startupPhase = StartupPhase::FailedNonRecoverable;
    return initialized;
}

Result<StartupState> AppContext::recoverFromBackup() {
    if (m_startupPhase != StartupPhase::RecoveryAvailable ||
        !m_lock.isLocked() || m_store.isInitialized())
        return Result<StartupState>::failure(
            {ErrorCode::Conflict, QStringLiteral("当前状态不允许启动恢复"), {}});
    const auto backup = m_repository.loadBackup();
    if (!backup.ok()) {
        m_startupPhase = StartupPhase::FailedNonRecoverable;
        return Result<StartupState>::failure(backup.error());
    }
    if (!backup.value().isEmpty() && !hasActiveAdmin(backup.value()))
    {
        m_startupPhase = StartupPhase::FailedNonRecoverable;
        return Result<StartupState>::failure(
            {ErrorCode::CorruptData, QStringLiteral("备份缺少有效管理员，拒绝恢复"),
             "accounts"});
    }
    const auto primary = m_paths.dataFile();
    if (QFileInfo::exists(primary)) {
        const auto damagedCopy = corruptCopyPath(primary);
        if (!QFile::copy(primary, damagedCopy))
            return Result<StartupState>::failure(
                {ErrorCode::Persistence,
                 QStringLiteral("无法保留损坏的主数据文件，已取消恢复"),
                 damagedCopy});
    }
    const auto restored = m_repository.restoreSnapshot(backup.value());
    if (!restored.ok())
        return Result<StartupState>::failure(restored.error());
    const auto initialized = m_store.initialize();
    if (initialized.ok())
        m_startupPhase = StartupPhase::Initialized;
    else
        m_startupPhase = StartupPhase::FailedNonRecoverable;
    return initialized;
}
} // namespace takeout
