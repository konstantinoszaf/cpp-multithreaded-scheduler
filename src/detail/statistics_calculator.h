#pragma once
#include <tuple>
#include <cstdint>

namespace scheduler::detail {
class IStatisticsCalculator {
public:
    virtual ~IStatisticsCalculator() = default;
    virtual std::tuple<double, double, double> getLatencyStatistics() const = 0;
    virtual void updateLatencyStatistics(std::int64_t latency) = 0;
};
} // namespace scheduler::detail
