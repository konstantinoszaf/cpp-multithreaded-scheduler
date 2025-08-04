#pragma once
#include <functional>

namespace scheduler::detail{

struct Task;

class IThreadPool {
public:
    virtual ~IThreadPool() = default;
    virtual bool submit(Task&& job) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};
} //namespace scheduler::detail