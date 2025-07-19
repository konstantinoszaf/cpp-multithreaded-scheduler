#pragma once
#include <functional>

namespace scheduler::detail{
class IThreadPool {
public:
    virtual ~IThreadPool() = default;
    virtual bool submit(std::function<void()> job) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};
} //namespace scheduler::detail