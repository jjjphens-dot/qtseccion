#pragma once
#include "repository.h"
namespace takeout {
class JsonRepository final : public Repository {
public:
    explicit JsonRepository(QString path) : m_path(std::move(path)) {}
    Result<StoreSnapshot> load() const override;
    Result<void> save(const StoreSnapshot& snapshot) override;
private:
    QString m_path;
};
} // namespace takeout
