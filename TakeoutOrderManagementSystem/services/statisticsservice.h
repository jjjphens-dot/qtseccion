#pragma once
#include "servicebase.h"
#include "core/requests.h"
namespace takeout {
class StatisticsService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<RoleStatistics> roleSummary(const DateRange &) const;
    Result<AdminStatistics> adminSummary(const DateRange &) const;
};
} // namespace takeout
