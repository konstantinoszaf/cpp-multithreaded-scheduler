#include "detail/task_queue_impl.h"
#include <algorithm>

using namespace scheduler::detail;

void TaskQueue::push(Task&& task) {
    std::lock_guard<std::mutex> guard{mtx_};
    heap_.push_back(std::move(task));
    std::push_heap(heap_.begin(), heap_.end());
}

std::optional<Task> TaskQueue::pop() {
    std::lock_guard<std::mutex> guard{mtx_};
    if (heap_.empty()) {
        return std::nullopt;
    }

    // move the largest to the back
    std::pop_heap(heap_.begin(), heap_.end());
    Task task = std::move((heap_.back()));
    heap_.pop_back();
    return task;
}

std::optional<std::reference_wrapper<const Task>> TaskQueue::peek() const {
    std::lock_guard<std::mutex> guard{mtx_};
    if (heap_.empty()) {
        return std::nullopt;
    }

    return std::cref(heap_.front());
}

bool TaskQueue::empty() const {
    std::lock_guard<std::mutex> guard{mtx_};
    return heap_.empty();
}

size_t TaskQueue::size() const {
    std::lock_guard<std::mutex> guard{mtx_};
    return heap_.size();
}