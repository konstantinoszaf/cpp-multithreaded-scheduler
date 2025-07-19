#include "detail/statistics_calculator_impl.h"

using namespace scheduler::detail;

std::tuple<double, double, double> StatisticsCalculator::getLatencyStatistics() const {
    auto cnt = count.load(std::memory_order_relaxed);
    double avg = 0.0;
    if (cnt > 0) {
        avg = double(sum.load(std::memory_order_relaxed) / double(cnt));
    }

    return {
        avg,
        double(min.load(std::memory_order_relaxed)),
        double(max.load(std::memory_order_relaxed))
    };
}

void StatisticsCalculator::updateLatencyStatistics(int64_t latency) {
    sum.fetch_add(latency, std::memory_order_relaxed);
    count.fetch_add(1, std::memory_order_relaxed);

    {
        int64_t prev_min = min.load(std::memory_order_relaxed);
        while (prev_min > latency && !min.compare_exchange_weak(prev_min, latency, std::memory_order_relaxed)) {}
    }

    {
        int64_t prev_max = max.load(std::memory_order_relaxed);
        while (prev_max < latency && !max.compare_exchange_weak(prev_max, latency, std::memory_order_relaxed)) {}
    }
}