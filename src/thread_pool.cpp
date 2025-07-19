#include "detail/thread_pool_impl.h"

using namespace scheduler::detail;

ThreadPool::ThreadPool(size_t numThreads)
: running{false}, thread_num{numThreads} {}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    {
        std::lock_guard<std::mutex> lock{queue_mutex};
        if (running) return;
        running = true;
    }

    if (thread_num <= 0) thread_num = 1;
    threads.reserve(thread_num);

    for (size_t i = 0; i < thread_num; ++i) {
        threads.emplace_back([this]{ this->workerLoop(); });
    }
}

void ThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lock{queue_mutex};
        if (!running) return;
        running = false;
    }

    cv.notify_all();
    for (std::thread& active_thread : threads) {
        active_thread.join();
    }
    threads.clear();
}

bool ThreadPool::submit(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock{queue_mutex};
        if (!running) return false;
        jobs.emplace_back(std::move(job));
    }
    cv.notify_one();
    return true;
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock{queue_mutex};
            cv.wait(lock, [this]{ return !running || !jobs.empty(); });

            if (!running && jobs.empty()) break; // drain all jobs before exit

            job = std::move(jobs.front());
            jobs.pop_front();
        }

        try {
            job();
        } catch (...) {
            // handle or log accordingly
        }

    }
}

size_t ThreadPool::threadCount() const noexcept {
    return thread_num;
}