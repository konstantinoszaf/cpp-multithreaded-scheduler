#include "detail/statistics_calculator_impl.h"
#include <cmath>
#include <algorithm>

using namespace scheduler::detail;
static constexpr size_t FLUSH_THRESHOLD = 4096;

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

    tls_samples().push_back(latency);
    if (tls_samples().size() >= FLUSH_THRESHOLD) flushThreadLocal();
}

std::tuple<double, double, double> StatisticsCalculator::getPvalues() {
    flushThreadLocal();
    std::vector<int64_t> all;
    {
        std::lock_guard<std::mutex> lk(merge_mtx);
        all = merged;
    }

    if (all.empty()) return {};

    std::sort(all.begin(), all.end());
    return {
        percentile(all, 0.95),
        percentile(all, 0.99),
        percentile(all, 0.999)
    };
}

void StatisticsCalculator::flushThreadLocal() {
    auto& buf = tls_samples();
    if (buf.empty()) return;

    std::lock_guard<std::mutex> lk(merge_mtx);
    merged.insert(merged.end(), std::make_move_iterator(buf.begin()), std::make_move_iterator(buf.end()));

    buf.clear();
}

double StatisticsCalculator::percentile(const std::vector<int64_t>& v, double p) {
    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();

    size_t n = v.size();
    double pos = p * (n - 1);
    size_t lo = static_cast<size_t>(std::floor(pos));
    size_t hi = static_cast<size_t>(std::ceil(pos));
    double a = static_cast<double>(v[lo]);
    double b = static_cast<double>(v[hi]);
    double frac = pos - lo;
    return (lo == hi) ? a : (a + (b - a) * frac);
}