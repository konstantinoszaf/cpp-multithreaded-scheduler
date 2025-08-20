#pragma once
#include "detail/statistics_calculator.h"
#include <tuple>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>

namespace scheduler::detail {
class StatisticsCalculator : public IStatisticsCalculator {
public:
    ~StatisticsCalculator() = default;
    StatisticsCalculator() : count{0}, sum{0},
                            min{std::numeric_limits<int64_t>::max()},
                            max{0} {}
    std::tuple<double, double, double> getLatencyStatistics() const override;
    std::tuple<double, double, double> getPvalues() override;
    void updateLatencyStatistics(int64_t latency) override;
    void flushThreadLocal();

private:
    double percentile(const std::vector<int64_t>& v, double p);
    static std::vector<int64_t>& tls_samples() {
        static thread_local std::vector<int64_t> tls;
        return tls;
    }

    std::atomic<uint64_t> count;
    std::atomic<int64_t> min;
    std::atomic<int64_t> max;
    std::atomic<int64_t> sum;
    mutable std::mutex merge_mtx;
    std::vector<int64_t> merged;
};
} // namespace scheduler::detail
