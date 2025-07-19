#pragma once
#include <chrono>

namespace scheduler::detail{

class IClock {
public:
    virtual ~IClock() = default;
    virtual std::chrono::steady_clock::time_point now() const = 0;
};
} // namespace scheduler::detail