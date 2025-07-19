#pragma once

#include <gmock/gmock.h>
#include "detail/system_clock_impl.h"

namespace scheduler {
namespace detail {

class MockClock : public IClock {
public:
    MOCK_METHOD((std::chrono::steady_clock::time_point), now, (), (const, override));
};

}  // namespace detail
}  // namespace scheduler
