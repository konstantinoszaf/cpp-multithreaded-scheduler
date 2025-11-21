#include "detail/thread_pool_impl.h"
#include "detail/task.h"
#include <random>

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
    
    return ready_queues[i]->push(std::move(task));
}

bool ThreadPool::steal_some(size_t index, std::vector<Task>& out) {
    if (thread_num <= 1) return false;

    thread_local std::uniform_int_distribution<size_t> dist;
    thread_local bool dist_inited = false;
    if (!dist_inited) {
        dist.param(decltype(dist)::param_type{0, thread_num - 2});
        dist_inited = true;
    }

    thread_local std::mt19937_64 rng = []{
        std::random_device rd;
        std::seed_seq ss{ rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd() };
        return std::mt19937_64{ss};
    }();

    auto pick = [&] {
        size_t r = dist(rng);
        return (r >= index) ? r + 1 : r;
    };

    const int probes = std::min<size_t>(2 * std::log2(thread_num) + 1, 8);
    for (int i = 0; i < probes; ++i) {
        size_t victim = pick();
        if (ready_queues[victim]->try_steal_batch(8, out)) return true;
    }

    return false;
}

void ThreadPool::workerLoop(size_t index) {
    auto& ready = *ready_queues[index];
    thread_local std::vector<Task> batch;
    batch.reserve(256);

    while (running.load(std::memory_order_acquire) || !ready.empty()) {
        batch.clear();

        if (ready.try_pop_batch(64, batch)) {
            std::sort(batch.begin(), batch.end(), std::greater<Task>{});

            for (auto& bt : batch) bt();
            continue;
        }

        if (steal_some(index, batch)) {
            for (auto& bt : batch) bt();
            continue;
        }

        if (auto j = ready.wait_and_pop_for(std::chrono::microseconds{2})) if (j.has_value()) j->operator()();
    }
}

size_t ThreadPool::threadCount() const noexcept {
    return thread_num;
}