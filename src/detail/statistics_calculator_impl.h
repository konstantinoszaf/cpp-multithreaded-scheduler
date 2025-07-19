#pragma once
#include "detail/statistics_calculator.h"
#include <tuple>
#include <chrono>
#include <atomic>

namespace scheduler::detail {
class StatisticsCalculator : public IStatisticsCalculator {
public:
    ~StatisticsCalculator() = default;
    StatisticsCalculator() : count{0}, sum{0},
                            min{std::numeric_limits<int64_t>::max()},
                            max{0} {}
    std::tuple<double, double, double> getLatencyStatistics() const override;
    void updateLatencyStatistics(int64_t latency) override;
private:
    std::atomic<uint64_t> count;
    std::atomic<int64_t> min;
    std::atomic<int64_t> max;
    std::atomic<int64_t> sum;
};
} // namespace scheduler::detail
