#pragma once

#include <gmock/gmock.h>
#include "detail/statistics_calculator_impl.h"

namespace scheduler {
namespace detail {

class MockStatisticsCalculator : public IStatisticsCalculator {
public:
    MOCK_METHOD(void, updateLatencyStatistics, ((int64_t)), (override));
    MOCK_METHOD((std::tuple<double, double, double>), getLatencyStatistics, (), (const, override));
    MOCK_METHOD((std::tuple<double, double, double>), getPvalues, (), (override));
    MOCK_METHOD(void, flushThreadLocal, (), (override));
};

}  // namespace detail
}  // namespace scheduler
