#pragma once
#include "task_queue.h"
#include "task.h"
#include <vector>
#include <mutex>

namespace scheduler::detail {
class TaskQueue : public ITaskQueue {
public:
    TaskQueue() = default;
    ~TaskQueue() = default;
    void push(Task&& task) override;
    /**
     * @warning pop deletes the task before returning it
    */
    std::optional<Task> pop() override;
    /**
     * takes a look at the current closest task
     * @warning peek does not guarantee that the returned head will be the same
     * if pop() is called right after. Using the reference after pop() can lead
     * to undefined behavior
    */
    std::optional<std::reference_wrapper<const Task>> peek() const override;
    bool empty() const override;
    size_t size() const override;
private:
    std::vector<Task> heap_;
    mutable std::mutex mtx_;
};
}