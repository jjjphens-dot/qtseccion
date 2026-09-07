#pragma once
#include "repository.h"
namespace takeout {
class JsonRepository final : public Repository {
public:
  explicit JsonRepository(QString path) : m_path(std::move(path)) {}
  Result<StoreSnapshot> load() const override;
  Result<StoreSnapshot> loadBackup() const;
  Result<void> save(const StoreSnapshot &snapshot) override;

private:
  Result<StoreSnapshot> loadFile(const QString &path) const;
  Result<void> writeFile(const QString &path,
                         const StoreSnapshot &snapshot) const;
  QString m_path;
};
} // namespace takeout
