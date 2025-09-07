#include "detail/thread_pool_impl.h"
#include "detail/task.h"

using namespace scheduler::detail;

ThreadPool::ThreadPool(size_t numThreads)
: running{false}, thread_num{numThreads} {}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    if (running.load(std::memory_order_acquire)) return;
    running.store(true, std::memory_order_release);

    if (thread_num <= 0) thread_num = 1;
    threads.reserve(thread_num);
    ready_queues.resize(thread_num);

    for (size_t i = 0; i < thread_num; ++i) {
        threads.emplace_back([this, i]{ this->workerLoop(i); });
    }

    recurring = std::thread(&ThreadPool::promote_tasks, this);
}

void ThreadPool::stop() {
    if (!running.load(std::memory_order_acquire)) return;
    running.store(false, std::memory_order_release);

    scheduled.shutdown();

    for (auto& ready : ready_queues)
        ready.shutdown();

    recurring.join();

    for (std::thread& active_thread : threads) {
        active_thread.join();
    }

    threads.clear();
}

void ThreadPool::promote_tasks() {
    while (running.load(std::memory_order_acquire) || !scheduled.empty()) {
        auto job = scheduled.wait_and_pop();
        const size_t i = job.sequence_number % thread_num;
        ready_queues[i].push(std::move(job));
    }
}

bool ThreadPool::submit(Task&& task) {
    if (!running.load(std::memory_order_acquire)) return false;

    if (task.recurring) {
        scheduled.push(task);
        return true;
    }

    const size_t i = task.sequence_number % thread_num;
    ready_queues[i].push(std::move(task));
    return true;
}

void ThreadPool::workerLoop(size_t index) {
    Task t;
    auto& ready = ready_queues[index];
    while (running.load(std::memory_order_acquire) || !ready.empty()) {
        while (auto j = ready.try_pop()) { try { (*j)(); } catch (...) {} }

        if (auto j = ready.wait_and_pop()) [[likely]] {
            try { (*j)(); } catch (...) {}
        }
    }
}

size_t ThreadPool::threadCount() const noexcept {
    return thread_num;
}