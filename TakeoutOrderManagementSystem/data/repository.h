#pragma once
#include "core/entities.h"
#include "core/result.h"
namespace takeout {
// Injected persistence boundary. Implementations must reject invalid snapshots
// and preserve the prior file when save fails; no authentication belongs here.
class Repository {
public:
    virtual ~Repository() = default;
    virtual Result<StoreSnapshot> load() const = 0;
    virtual Result<void> save(const StoreSnapshot& snapshot) = 0;
};
} // namespace takeout
