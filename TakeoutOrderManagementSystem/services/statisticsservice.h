#pragma once
#include "servicebase.h"
#include "core/requests.h"
namespace takeout {
class StatisticsService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<StatisticsSummary> summary(const DateRange&) const {
        auto r = pending("统计（W07）", {Role::Customer, Role::Merchant, Role::Rider, Role::Admin});
        return Result<StatisticsSummary>::failure(r.error());
    }
};
} // namespace takeout
