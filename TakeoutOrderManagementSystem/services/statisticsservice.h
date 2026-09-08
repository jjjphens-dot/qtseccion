#pragma once
#include "servicebase.h"
#include "core/requests.h"
namespace takeout {
class StatisticsService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<StatisticsSummary> summary(const DateRange &) const;
};
} // namespace takeout
