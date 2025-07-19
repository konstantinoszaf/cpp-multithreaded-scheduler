// test/statistics_calculator_test.cpp

#include "detail/statistics_calculator_impl.h"
#include <gtest/gtest.h>
#include <limits>

using namespace scheduler::detail;

TEST(StatisticsCalculator, InitialState) {
    StatisticsCalculator stats;

    auto [avg, mn, mx] = stats.getLatencyStatistics();
    // No samples, avg should be 0, min at max-int64, max at 0
    EXPECT_DOUBLE_EQ(avg, 0.0);
    EXPECT_DOUBLE_EQ(mn, static_cast<double>(std::numeric_limits<int64_t>::max()));
    EXPECT_DOUBLE_EQ(mx, 0.0);
}

TEST(StatisticsCalculator, SingleSample) {
    StatisticsCalculator stats;
    stats.updateLatencyStatistics(123);

    auto [avg, mn, mx] = stats.getLatencyStatistics();
    EXPECT_DOUBLE_EQ(avg, 123.0);
    EXPECT_DOUBLE_EQ(mn, 123.0);
    EXPECT_DOUBLE_EQ(mx, 123.0);
}

TEST(StatisticsCalculator, MultipleSamples) {
    StatisticsCalculator stats;
    stats.updateLatencyStatistics(100);
    stats.updateLatencyStatistics(200);
    stats.updateLatencyStatistics(50);

    auto [avg, mn, mx] = stats.getLatencyStatistics();
    // (100 + 200 + 50) / 3 = ~116.666 -> stored as double(sum)/count = 116.666...
    EXPECT_NEAR(avg, (100.0 + 200.0 + 50.0) / 3.0, 1e-6);
    EXPECT_DOUBLE_EQ(mn, 50.0);
    EXPECT_DOUBLE_EQ(mx, 200.0);
}
