#include "scheduler/scheduler.h"
#include "detail/task_queue_impl.h"
#include "detail/system_clock_impl.h"
#include "detail/thread_pool_impl.h"
#include "detail/statistics_calculator_impl.h"
#include "detail/task.h"
#include <iostream>

using namespace scheduler;
using namespace detail;

static constexpr int BASE_MAX = 100;
static constexpr int BOOST    = 1000;
static constexpr std::chrono::milliseconds imminate_time{10};

Scheduler::Scheduler(size_t numThreads) : running{false}
{
    if (numThreads <= 0)
        numThreads = 1;

    // Note: it’s generally better to inject abstractions (IThreadPool, IClock, ...)
    // to decouple Scheduler from specific implementations. Here, however, we explicitly
    // construct default concrete instances so that SDK users only need to depend on
    // the Scheduler interface and aren’t exposed to the internals.
    clock = std::make_shared<SystemClock>();

    // Priority comparator lambda
    auto priorityComparator = [](const Task& a, const Task& b) noexcept -> bool {
        if (a.priority != b.priority)
            return a.priority < b.priority; // higher priority first
        return a.sequence_number > b.sequence_number; // FIFO for same priority
    };
    ready_tasks = std::make_shared<TaskQueue>(priorityComparator);

    // Time comparator lambda
    auto timeComparator = [](const Task& a, const Task& b) noexcept -> bool {
        return a.scheduled_at > b.scheduled_at; // earliest scheduled_tasks first
    };
    scheduled_tasks = std::make_shared<TaskQueue>(timeComparator);

    thread_pool = std::make_shared<ThreadPool>(numThreads);
    statistics = std::make_shared<StatisticsCalculator>();

    thread_pool->start();
    this->start();
}

// for testing purposes
Scheduler::Scheduler(std::shared_ptr<detail::IClock> clock_,
                     std::shared_ptr<detail::ITaskQueue> ready_,
                     std::shared_ptr<detail::ITaskQueue> scheduled_,
                     std::shared_ptr<detail::IThreadPool> pool_,
                     std::shared_ptr<detail::IStatisticsCalculator> stats)
    : clock{clock_}, ready_tasks{ready_}, scheduled_tasks{scheduled_}, thread_pool{pool_}, running{false}
{}

Scheduler::~Scheduler()
{
    thread_pool->stop();
    this->stop();
}

void Scheduler::start() {
    {
        std::lock_guard<std::mutex> lock{dispatcher_mtx};
        if (running) return;
        running.store(true, std::memory_order_relaxed);
    }

    dispatcher_thread = std::thread{std::bind(&Scheduler::dispatchLoop, this)};
    promoter_thread = std::thread{std::bind(&Scheduler::promoteTasks, this)};
}

void Scheduler::stop() {
    {
        std::unique_lock<std::mutex> lock(promoter_mtx);
        if (!running) return;
        running.store(false, std::memory_order_relaxed);
        promoter_cv.notify_all();
    }

    {
        std::lock_guard<std::mutex> lock(dispatcher_mtx);
        dispatcher_cv.notify_all();
    }

    if (promoter_thread.joinable())   promoter_thread.join();
    if (dispatcher_thread.joinable()) dispatcher_thread.join();
}

void Scheduler::schedule(std::function<void()> job, int priority,
                         std::optional<std::chrono::steady_clock::time_point> deadline)
{
    if (!job) return;
    std::lock_guard<std::mutex> lock{dispatcher_mtx};

    auto seq = sequence_number.fetch_add(1, std::memory_order_relaxed);
    Task task{
        std::move(job), //job
        calculatePriority(priority, deadline), //priority
        seq, //sequence_number
        clock->now(), // scheduled_at
        std::chrono::milliseconds{0}, // interval
        clock->now(), //enqueue_time
        deadline, //deadline
    };

    ready_tasks->push(std::move(task));
    dispatcher_cv.notify_one();
}

// Allow tasks that run repeatedly on an interval
void Scheduler::scheduleRecurring(std::function<void()> job,
                                    int priority,
                                    milliseconds interval)
{
    if (!job) return;
    std::lock_guard<std::mutex> lock{dispatcher_mtx};

    auto seq = sequence_number.fetch_add(1, std::memory_order_relaxed);
    Task task{
        std::move(job), //job
        calculatePriority(priority, std::nullopt), //priority
        seq, //sequence_number
        clock->now(), // scheduled_at
        interval, // interval
        clock->now(), //enqueue_time
        std::nullopt, //deadline
    };

    ready_tasks->push(std::move(task));
    dispatcher_cv.notify_one();
}

void Scheduler::promoteTasks() {
    while (true) {
        std::unique_lock<std::mutex> lock{promoter_mtx};
        if (!running && scheduled_tasks->empty()) break;

        // sleep until there is something in the queue, or until the next periodic
        // task should be triggered
        if (scheduled_tasks->empty()) {
            promoter_cv.wait(lock, [&]{ return !running || !scheduled_tasks->empty(); });
        } else {
            auto next_time = scheduled_tasks->peek_time();
            if (!next_time.has_value()) {
                scheduled_tasks->pop();
                continue;
            }

            auto trigger_time = next_time->get();
            promoter_cv.wait_until(lock, trigger_time, [&]{
                return !running || trigger_time <= clock->now();
            });
        }

        auto now = clock->now();
        // promote the tasks to ready in batches
        bool inform_dispatcher = false;
        while (!scheduled_tasks->empty()) {

            auto peek = scheduled_tasks->peek_time();
            if (!peek || peek->get() > now) break;
            ready_tasks->push(std::move(*scheduled_tasks->pop()));
            inform_dispatcher = true;
        }

        lock.unlock();
        if (inform_dispatcher)
        {
            std::lock_guard<std::mutex> lk(dispatcher_mtx);
            dispatcher_cv.notify_one();
        }
    }
}

void Scheduler::dispatchLoop() {
    constexpr size_t MAX_BATCH{8};
    std::vector<Task> batch;
    std::vector<Task> recurring;
    batch.reserve(MAX_BATCH);
    recurring.reserve(MAX_BATCH);

    while (true) {
        std::unique_lock<std::mutex> lk(dispatcher_mtx);
        if (!running && ready_tasks->empty()) break; // drain all jobs before exit

        if (ready_tasks->empty()) {
            dispatcher_cv.wait(lk, [&]{ return !running || !ready_tasks->empty(); });
        }

        while (batch.size() < MAX_BATCH && !ready_tasks->empty()) {
            auto t = ready_tasks->pop();
            if (!t.has_value()) break;
            batch.push_back(std::move(*t));
        }

        lk.unlock();

        for (auto& t : batch) {
            Task task = std::move(t);
            bool is_recurring = task.interval.count() > 0;

            if (is_recurring) {
                auto now = clock->now();
                auto seq = sequence_number.fetch_add(1, std::memory_order_relaxed);
                recurring.push_back(Task{
                    task.job, // copy the callable
                    task.priority,
                    seq,
                    now + task.interval, // scheduled_at
                    task.interval,
                    now, // enqueue_time
                    std::nullopt // no deadline for recurring
                });
            }

            thread_pool->enqueue([this, task = std::move(task)]{ dispatchOne(std::move(task)); });
        }
        batch.clear();
        if (!recurring.empty()) {
            std::lock_guard<std::mutex> lk(dispatcher_mtx);
            for (auto& rt : recurring) {
                scheduled_tasks->push(std::move(rt));
            }
            {
                std::lock_guard<std::mutex> lk(promoter_mtx);
                promoter_cv.notify_one();
            }
        }
        recurring.clear();
    }
}

void Scheduler::dispatchOne(Task task) {
    auto start = clock->now();
    statistics->updateLatencyStatistics(std::chrono::duration_cast<std::chrono::milliseconds>(start - task.enqueue_time).count());
    if (task.deadline && start > *task.deadline)
        missed_tasks.fetch_add(1, std::memory_order_relaxed);

    try {
        task.job();
    } catch(...)
    { /* log */ }
}

std::tuple<double, double, double> Scheduler::getLatencyStatistics() const {
    return statistics->getLatencyStatistics();
}

uint64_t Scheduler::getMissedTasks() {
    return missed_tasks.load();
}

uint64_t Scheduler::calculatePriority(int priority, std::optional<std::chrono::steady_clock::time_point> deadline) {
    priority = std::clamp(priority, 0, BASE_MAX);

    if (deadline && (*deadline - clock->now() <= imminate_time)) return BASE_MAX + BOOST;

    return priority;
}
