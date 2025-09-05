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

    for (size_t i = 0; i < thread_num; ++i) {
        threads.emplace_back([this]{ this->workerLoop(); });
    }

    recurring = std::thread(&ThreadPool::promote_tasks, this);
}

void ThreadPool::stop() {
    if (!running.load(std::memory_order_acquire)) return;
    running.store(false, std::memory_order_release);

    scheduled.shutdown();
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
        ready.push(std::move(job));
    }
}

bool ThreadPool::submit(Task&& task) {
    if (!running.load(std::memory_order_acquire)) return false;

    if (task.recurring) scheduled.push(task);
    else                ready.push(std::move(task));
    return true;
}

void ThreadPool::workerLoop() {
    Task t;
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