#include "atomicfilewriter.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

namespace takeout {
namespace {
Result<void> persistence(const QString &path, const QString &detail) {
  return Result<void>::failure(
      {ErrorCode::Persistence,
       QStringLiteral("无法访问数据文件：%1").arg(detail), path});
}
} // namespace

Result<void> QSaveFileWriter::write(const QString &path,
                                    const QByteArray &bytes) const {
  const QFileInfo info(path);
  if (!QDir().mkpath(info.absolutePath()))
    return persistence(path, QStringLiteral("无法创建数据目录"));

  QSaveFile file(path);
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly))
    return persistence(path, file.errorString());
  if (file.write(bytes) != bytes.size()) {
    const auto message = file.errorString();
    file.cancelWriting();
    return persistence(path, message);
  }
  if (!file.commit())
    return persistence(path, file.errorString());
  return Result<void>::success();
}
} // namespace takeout
