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
}

void ThreadPool::stop() {
    if (!running.load(std::memory_order_acquire)) return;
    running.store(false, std::memory_order_release);

    jobs.shutdown();

    for (std::thread& active_thread : threads) {
        active_thread.join();
    }
    threads.clear();
}

bool ThreadPool::submit(Task&& task) {
    if (!running.load(std::memory_order_acquire)) return false;

    jobs.push(std::move(task));
    return true;
}

void ThreadPool::workerLoop() {
    while (running.load(std::memory_order_acquire) || !jobs.empty()) {
        auto job = jobs.try_pop();

        if (!job) job = jobs.wait_and_pop();

        try {
            if (job) (*job)();
        } catch (...) {
            // ignore
        }
    }
}

size_t ThreadPool::threadCount() const noexcept {
    return thread_num;
}