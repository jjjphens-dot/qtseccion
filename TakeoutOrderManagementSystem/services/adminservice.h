#pragma once
#include "servicebase.h"
namespace takeout {
class AdminService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<void> deleteAccount(const Id&) { return pending("deleteAccount（W07）", {Role::Admin}); }
};
} // namespace takeout
