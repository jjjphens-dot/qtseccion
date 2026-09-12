#pragma once
#include "repository.h"
#include "atomicfilewriter.h"
#include <memory>
namespace takeout {
class JsonRepository final : public Repository {
public:
  explicit JsonRepository(
      QString path,
      std::shared_ptr<const AtomicFileWriter> writer = nullptr);
  Result<StoreSnapshot> load() const override;
  Result<StoreSnapshot> loadBackup() const;
  Result<StoreSnapshot> loadExternal(const QString &path) const;
  Result<void> exportSnapshot(const QString &path,
                              const StoreSnapshot &snapshot) const;
  Result<void> restoreSnapshot(const StoreSnapshot &snapshot) const;
  Result<void> save(const StoreSnapshot &snapshot) override;

private:
  Result<StoreSnapshot> loadFile(const QString &path) const;
  Result<void> writeFile(const QString &path,
                         const StoreSnapshot &snapshot) const;
  QString m_path;
  std::shared_ptr<const AtomicFileWriter> m_writer;
};
} // namespace takeout
