#include "scheduler/scheduler.h"
#include "detail/thread_pool_impl.h"
#include "detail/statistics_calculator_impl.h"
#include "detail/task.h"

using namespace scheduler;
using namespace detail;
using steady_clock = std::chrono::steady_clock;

static constexpr int BASE_MAX = 100;
static constexpr int BOOST    = 1000;
static constexpr std::chrono::microseconds imminent_time{500};

Scheduler::Scheduler(size_t numThreads)
{
    if (numThreads <= 0)
        numThreads = std::thread::hardware_concurrency();

    // Note: it’s generally better to inject abstractions (IThreadPool, ...)
    // to decouple Scheduler from specific implementations. Here, however, we explicitly
    // construct default concrete instances so that SDK users only need to depend on
    // the Scheduler interface and aren’t exposed to the internals.

    thread_pool = std::make_unique<ThreadPool>(numThreads);
    statistics = std::make_unique<StatisticsCalculator>();

    thread_pool->start();
}

// for testing purposes
Scheduler::Scheduler(std::unique_ptr<detail::IThreadPool> pool_,
                     std::unique_ptr<detail::IStatisticsCalculator> stats)
    : thread_pool{std::move(pool_)}, statistics{std::move(stats)} {}

Scheduler::~Scheduler()
{
    thread_pool->stop();
    statistics->flushThreadLocal();
}

void Scheduler::schedule(std::function<void()> job, int priority,
                         std::optional<std::chrono::steady_clock::time_point> deadline)
{
    if (!job) return;

    auto now = steady_clock::now();
    auto t = [this, job = std::move(job), enqueue_time = now, deadline]() mutable {
            auto start = steady_clock::now();
            statistics->updateLatencyStatistics(std::chrono::duration_cast<std::chrono::nanoseconds>(start - enqueue_time).count());
            if (deadline && start > *deadline)
                missed_tasks.fetch_add(1, std::memory_order_relaxed);

            try {
                job();
            } catch(...)
            { /* log */ }
    };

    auto seq = sequence_number.fetch_add(1, std::memory_order_acquire);

    Task task{
        std::move(t), //job
        calculatePriority(priority, deadline, now), //priority
        seq, //sequence_number
        now, // scheduled_at
        std::chrono::milliseconds{0}, // interval
        now, //enqueue_time
        deadline, //deadline
    };

    thread_pool->submit(std::move(task));
}

// Allow tasks that run repeatedly on an interval
void Scheduler::scheduleRecurring(std::function<void()> job, int priority,
                                                            milliseconds interval)
{
    if (!job) return;

    auto now = steady_clock::now();
    auto t = [this, job = std::move(job), enqueue_time = now, priority, interval]() mutable {
            auto start = steady_clock::now();
            statistics->updateLatencyStatistics(std::chrono::duration_cast<std::chrono::nanoseconds>(start - enqueue_time).count());

            try {
                job();
                this->scheduleRecurring(job, priority, interval);
            } catch(...)
            { /* log */ }
    };

    auto seq = sequence_number.fetch_add(1, std::memory_order_acquire);

    Task task{
        std::move(t), //job
        calculatePriority(priority, std::nullopt, now), //priority
        seq, //sequence_number
        now + interval, // scheduled_at
        interval, // interval
        now, //enqueue_time
        std::nullopt, //deadline
    };

    thread_pool->submit(std::move(task));
}

std::tuple<double, double, double> Scheduler::getLatencyStatistics() const {
    return statistics->getLatencyStatistics();
}

std::tuple<double, double, double> Scheduler::getPvalueStatistics() const {
    return statistics->getPvalues();
}

uint64_t Scheduler::getMissedTasks() {
    return missed_tasks.load();
}

// this is a static way of handling tasks that are in danger of reaching their deadline.
uint64_t Scheduler::calculatePriority(int priority,
    std::optional<std::chrono::steady_clock::time_point> deadline, steady_clock::time_point now)
{
    priority = std::clamp(priority, 0, BASE_MAX);

    if (deadline && (*deadline - now <= imminent_time)) return BASE_MAX + BOOST + priority;

    return priority;
}
