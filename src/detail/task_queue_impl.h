#pragma once
#include "task_queue.h"
#include "task.h"
#include <vector>
#include <mutex>
#include <optional>
#include <functional>
#include <chrono>

namespace scheduler::detail {
class TaskQueue : public ITaskQueue {
public:
    explicit TaskQueue(std::function<bool(const Task&, const Task&)> comparator_)
        : comparator{std::move(comparator_)} {}
    ~TaskQueue() = default;
    void push(Task&& task) override;
    /**
     * @warning pop deletes the task before returning it
    */
    std::optional<Task> pop() override;
    /**
     * takes a look at the top tasks scheduled at attribute
     * @warning peek_time does not guarantee that the returned head will be the same
     * if pop() is called right after. Using the reference after pop() can lead
     * to undefined behavior
    */
    std::optional<std::reference_wrapper<const std::chrono::steady_clock::time_point>> 
                                                            peek_time() const override;

    bool empty() const override;
    size_t size() const override;
private:
    std::vector<Task> heap_;
    mutable std::mutex mtx_;
    std::function<bool(const Task&, const Task&)> comparator;
};
}