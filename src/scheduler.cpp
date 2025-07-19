#include "scheduler/scheduler.h"
#include "detail/task_queue_impl.h"
#include "detail/system_clock_impl.h"
#include "detail/thread_pool_impl.h"
#include "detail/statistics_calculator_impl.h"

using namespace scheduler;
using namespace detail;

static constexpr int BASE_MAX = 100;
static constexpr int BOOST    = 1000;
static constexpr std::chrono::milliseconds imminate_time{10};

Scheduler::Scheduler(size_t numThreads)
{
    if (numThreads <= 0)
        numThreads = 1;

    // Note: it’s generally better to inject abstractions (IThreadPool, IClock, ...)
    // to decouple Scheduler from specific implementations. Here, however, we explicitly
    // construct default concrete instances so that SDK users only need to depend on
    // the Scheduler interface and aren’t exposed to the internals.
    thread_pool = std::make_shared<ThreadPool>(numThreads);
    clock = std::make_shared<SystemClock>();
    data = std::make_shared<TaskQueue>();
    statistics = std::make_shared<StatisticsCalculator>();

    thread_pool->start();
}

// for testing purposes
Scheduler::Scheduler(std::shared_ptr<detail::IClock> clock_,
                     std::shared_ptr<detail::ITaskQueue> data_,
                     std::shared_ptr<detail::IThreadPool> pool_,
                     std::shared_ptr<detail::IStatisticsCalculator> stats)
    : clock{clock_}, data{data_}, thread_pool{pool_}
{
    thread_pool->start();
}

Scheduler::~Scheduler()
{
    thread_pool->stop();
}

void Scheduler::schedule(std::function<void()> task, int priority,
                         std::optional<std::chrono::steady_clock::time_point> deadline)
{
    Task t{
        std::move(task),
        calculatePriority(priority, deadline),
        sequence_number.fetch_add(1),
        std::chrono::milliseconds{0},
        clock->now(),
        deadline};

    data->push(std::move(t));

    if (!dispatching.exchange(true, std::memory_order_acq_rel))
    {
        thread_pool->submit([this]{ dispatchLoop(); });
    }
}

// Allow tasks that run repeatedly on an interval
void Scheduler::scheduleRecurring(std::function<void()> job,
                                int priority,
                                milliseconds interval)
{
    // 1) keep the real job separate
    auto jobHolder = std::make_shared<std::function<void()>>(std::move(job));

    // 2) this shared_ptr will end up holding our "recurring" lambda
    auto wrapper = std::make_shared<std::function<void()>>();

    // avoid a strong cycle: capture a weak_ptr to wrapper
    std::weak_ptr<std::function<void()>> weakWrapper = wrapper;

    // 3) build the recurring lambda
    auto recurring = [this, jobHolder, weakWrapper, priority, interval]() {
        // a) run the original job
        (*jobHolder)();

        // b) reschedule *wrapper* if still alive
        if (auto w = weakWrapper.lock()) {
            Task t{
                [w]() { (*w)(); },
                calculatePriority(priority, std::nullopt),
                sequence_number.fetch_add(1, std::memory_order_relaxed),
                interval,
                clock->now(),
                std::nullopt
            };
            data->push(std::move(t));
            if (!dispatching.exchange(true, std::memory_order_acq_rel))
                thread_pool->submit([this]{ dispatchLoop(); });
        }
    };

    // 4) now store that recurring behavior in wrapper
    *wrapper = std::move(recurring);

    // 5) enqueue the very first run
    Task t0{
        [wrapper]() { (*wrapper)(); },
        calculatePriority(priority, std::nullopt),
        sequence_number.fetch_add(1, std::memory_order_relaxed),
        interval,
        clock->now(),
        std::nullopt
    };
    data->push(std::move(t0));
    if (!dispatching.exchange(true, std::memory_order_acq_rel))
        thread_pool->submit([this]{ dispatchLoop(); });
}

std::tuple<double, double, double> Scheduler::getLatencyStatistics() const
{
    return statistics->getLatencyStatistics();
}

uint64_t Scheduler::getMissedTasks() {
    return missed_tasks.load();
}

// executed by pool threads
void Scheduler::dispatchLoop()
{
    while (true)
    {
        auto optional_task = data->pop();
        if (!optional_task)
            break;

        Task task = std::move(*optional_task);
        auto now = clock->now();
        auto latency = now - task.enqueue_time;

        statistics->updateLatencyStatistics(latency.count());

        if (task.deadline && now > *task.deadline) {
            missed_tasks.fetch_add(1, std::memory_order_relaxed);
        }

        try {
            thread_pool->submit([job = std::move(task.job)]{ job(); });
        } catch (...) {
            // nothing to do
        }
    }

    dispatching.store(false, std::memory_order_release);

    if (!data->empty() && !dispatching.exchange(true, std::memory_order_acq_rel))
    {
        thread_pool->submit([this]{ dispatchLoop(); });
    }
}

uint64_t Scheduler::calculatePriority(int priority, std::optional<std::chrono::steady_clock::time_point> deadline) {
    priority = std::clamp(priority, 0, BASE_MAX);

    if (deadline && (*deadline - clock->now() <= imminate_time)) return BASE_MAX + BOOST;

    return priority;
}