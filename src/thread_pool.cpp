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

    if (thread_num == 0) thread_num = 1;

    ready_queues.clear();
    ready_queues.reserve(thread_num);
    for (size_t i = 0; i < thread_num; ++i) {
        ready_queues.emplace_back(
            std::make_unique<scheduler::queue::UnorderedQueue<Task>>()
        );
    }

    threads.clear();
    threads.reserve(thread_num);
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
        ready->shutdown();

    recurring.join();

    for (std::thread& active_thread : threads) {
        active_thread.join();
    }

    threads.clear();
}

void ThreadPool::promote_tasks() {
    while (running.load(std::memory_order_acquire) || !scheduled.empty()) {
        auto job = scheduled.wait_and_pop();
        if (!job.has_value()) [[unlikely]] continue;
        const size_t i = job->sequence_number % thread_num;
        ready_queues[i]->push(std::move(job.value()));
    }
}

bool ThreadPool::submit(Task&& task) {
    if (!running.load(std::memory_order_acquire)) return false;

    if (task.recurring) {
        scheduled.push(std::move(task));
        return true;
    }

    const size_t i = task.sequence_number % thread_num;
    ready_queues[i]->push(std::move(task));
    return true;
}

void ThreadPool::workerLoop(size_t index) {
    Task t;
    auto& ready = *ready_queues[index];
    thread_local std::vector<Task> batch;
    batch.reserve(256);

    while (running.load(std::memory_order_acquire) || !ready.empty()) {
        if (auto j = ready.wait_and_pop()) {
            batch.clear();
            batch.emplace_back(std::move(*j));

            ready.try_pop_batch(191, batch);
            std::sort(batch.begin(), batch.end(), std::greater<Task>{});

            for (auto& bt : batch) bt();
        }
    }
}

size_t ThreadPool::threadCount() const noexcept {
    return thread_num;
}