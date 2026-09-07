#include "jsonrepository.h"
#include "core/orderpolicy.h"
#include "jsoncodec.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace takeout {
namespace {
Error corrupt(const QString &field, const QString &detail = {}) {
  return {ErrorCode::CorruptData,
          detail.isEmpty() ? QStringLiteral("数据结构无效：%1").arg(field)
                           : detail,
          field};
}
Error persistence(const QString &path, const QString &detail) {
  return {ErrorCode::Persistence,
          QStringLiteral("无法访问数据文件：%1").arg(detail), path};
}
} // namespace

Result<StoreSnapshot> JsonRepository::loadFile(const QString &path) const {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return Result<StoreSnapshot>::failure(
        persistence(path, file.errorString()));
  if (file.size() > Limits::MaxFileBytes)
    return Result<StoreSnapshot>::failure(
        corrupt("fileSize", QStringLiteral("数据文件超过 100 MiB 上限")));
  const auto bytes = file.read(Limits::MaxFileBytes + 1);
  if (file.error() != QFileDevice::NoError)
    return Result<StoreSnapshot>::failure(
        persistence(path, file.errorString()));
  if (bytes.size() > Limits::MaxFileBytes)
    return Result<StoreSnapshot>::failure(
        corrupt("fileSize", QStringLiteral("数据文件超过 100 MiB 上限")));
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return Result<StoreSnapshot>::failure(corrupt(
        "JSON", QStringLiteral("JSON 解析失败：%1").arg(error.errorString())));
  return JsonCodec::decode(document.object());
}

Result<StoreSnapshot> JsonRepository::loadBackup() const {
  const auto path = m_path + ".bak";
  if (!QFileInfo::exists(path))
    return Result<StoreSnapshot>::failure(
        {ErrorCode::NotFound, QStringLiteral("备份文件不存在"), path});
  return loadFile(path);
}

Result<StoreSnapshot> JsonRepository::load() const {
  if (!QFileInfo::exists(m_path)) {
    if (!QFileInfo::exists(m_path + ".bak"))
      return Result<StoreSnapshot>::success(StoreSnapshot{});
    const auto backup = loadBackup();
    if (!backup.ok())
      return backup;
    return Result<StoreSnapshot>::failure(
        {ErrorCode::RecoveryAvailable,
         QStringLiteral("主数据文件缺失，已验证备份可用于恢复"),
         m_path + ".bak"});
  }
  const auto primary = loadFile(m_path);
  if (primary.ok())
    return primary;
  if (primary.error().code == ErrorCode::UnsupportedVersion)
    return primary;
  if (QFileInfo::exists(m_path + ".bak")) {
    const auto backup = loadBackup();
    if (backup.ok())
      return Result<StoreSnapshot>::failure(
          {ErrorCode::RecoveryAvailable,
           QStringLiteral("主数据文件损坏，已验证备份可用于恢复：%1")
               .arg(primary.error().message),
           m_path + ".bak"});
  }
  return primary;
}

Result<void> JsonRepository::writeFile(const QString &path,
                                       const StoreSnapshot &snapshot) const {
  const auto bytes = QJsonDocument(JsonCodec::encode(snapshot))
                         .toJson(QJsonDocument::Indented);
  if (bytes.size() > Limits::MaxFileBytes)
    return Result<void>::failure(
        corrupt("fileSize", QStringLiteral("序列化数据超过 100 MiB 上限")));
  const QFileInfo info(path);
  if (!QDir().mkpath(info.absolutePath()))
    return Result<void>::failure(
        persistence(path, QStringLiteral("无法创建数据目录")));
  QSaveFile file(path);
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly))
    return Result<void>::failure(persistence(path, file.errorString()));
  if (file.write(bytes) != bytes.size()) {
    const auto message = file.errorString();
    file.cancelWriting();
    return Result<void>::failure(persistence(path, message));
  }
  if (!file.commit())
    return Result<void>::failure(persistence(path, file.errorString()));
  return Result<void>::success();
}

Result<void> JsonRepository::save(const StoreSnapshot &snapshot) {
  if (snapshot.schemaVersion != Limits::SchemaVersion)
    return Result<void>::failure(
        {ErrorCode::UnsupportedVersion,
         QStringLiteral("不支持的数据版本：%1").arg(snapshot.schemaVersion),
         "schemaVersion"});
  if (snapshot.revision < 0 || snapshot.revision > 9007199254740991LL ||
      !snapshot.savedAt.isValid())
    return Result<void>::failure(corrupt("snapshot"));
  const auto valid = OrderPolicy::validateAll(snapshot);
  if (!valid.ok())
    return Result<void>::failure(valid.error());
  if (QFileInfo::exists(m_path)) {
    const auto previous = loadFile(m_path);
    if (!previous.ok())
      return Result<void>::failure(previous.error());
    const auto backup = writeFile(m_path + ".bak", previous.value());
    if (!backup.ok())
      return backup;
  }
  return writeFile(m_path, snapshot);
}
} // namespace takeout
