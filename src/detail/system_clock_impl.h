#include "detail/clock.h"
#include <chrono>

namespace scheduler::detail{

class SystemClock : public IClock {
public:
    std::chrono::steady_clock::time_point now() const noexcept override {
        return std::chrono::steady_clock::now();
    }
};
} // namespace scheduler::detail