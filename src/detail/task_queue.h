#pragma once
#include <optional>
#include <chrono>

namespace scheduler::detail {

struct Task;

class ITaskQueue {
public:
    virtual ~ITaskQueue() = default;
    virtual void push(Task&& task) = 0;
    virtual std::optional<Task> pop() = 0;
    virtual std::optional<std::reference_wrapper<const std::chrono::steady_clock::time_point>> 
                                                                        peek_time() const = 0;
    virtual bool empty() const = 0;
    virtual size_t size() const = 0;
};
} // namespace scheduler