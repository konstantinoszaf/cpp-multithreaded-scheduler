#include "scheduler/scheduler.h"
#include "detail/task_queue_impl.h"
#include "detail/system_clock_impl.h"
#include "detail/thread_pool_impl.h"
#include "detail/statistics_calculator_impl.h"
#include "detail/task.h"

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

    // Time comparator lambda
    auto timeComparator = [](const Task& a, const Task& b) noexcept -> bool {
        return a.scheduled_at > b.scheduled_at; // earliest scheduled first
    };

    ready = std::make_shared<TaskQueue>(priorityComparator);
    scheduled = std::make_shared<TaskQueue>(timeComparator);
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
    : clock{clock_}, ready{ready_}, scheduled{scheduled_}, thread_pool{pool_}, running{false}
{}

Scheduler::~Scheduler()
{
    thread_pool->stop();
    this->stop();
}

void Scheduler::start() {
    {
        std::lock_guard<std::mutex> lock{mtx};
        if (running) return;
        running = true;
    }

    thread = std::thread{std::bind(&Scheduler::dispatchLoop, this)};
}

void Scheduler::stop() {
    {
        std::unique_lock<std::mutex> lock{mtx};
        if (!running) return;
        running = false;
    }

    cv.notify_all();
    if (thread.joinable()) thread.join();
}

void Scheduler::schedule(std::function<void()> job, int priority,
                         std::optional<std::chrono::steady_clock::time_point> deadline)
{
    if (!job) return;
    std::lock_guard<std::mutex> lock{mtx};

    sequence_number.fetch_add(1, std::memory_order_relaxed);
    Task task{
        std::move(job), //job
        calculatePriority(priority, deadline), //priority
        sequence_number.load(), //sequence_number
        clock->now(), // scheduled_at
        std::chrono::milliseconds{0}, // interval
        clock->now(), //enqueue_time
        deadline, //deadline
    };

    ready->push(std::move(task));
    cv.notify_one();
}

// Allow tasks that run repeatedly on an interval
void Scheduler::scheduleRecurring(std::function<void()> job,
                                    int priority,
                                    milliseconds interval)
{
    if (!job) return;
    std::lock_guard<std::mutex> lock{mtx};

    sequence_number.fetch_add(1, std::memory_order_relaxed);
    Task task{
        std::move(job), //job
        calculatePriority(priority, std::nullopt), //priority
        sequence_number.load(), //sequence_number
        clock->now(), // scheduled_at
        interval, // interval
        clock->now(), //enqueue_time
        std::nullopt, //deadline
    };

    ready->push(std::move(task));
    cv.notify_one();
}

void Scheduler::dispatchLoop() {
    while (true) {
        std::unique_lock<std::mutex> lk(mtx);
        if (!running && ready->empty()) break; // drain all jobs before exit

        auto now = clock->now();

        // 1) Promote any due tasks from scheduled to ready
        while (!scheduled->empty()) {
            auto peek = scheduled->peek_time();
            if (!peek || peek->get() > now) break;
            ready->push(std::move(*scheduled->pop()));
        }

        // 2) If no ready tasks, sleep until either:
        //    a) a new task arrives (cv.notify_one)
        //    b) the next scheduled task’s time
        if (ready->empty()) {
            if (scheduled->empty()) {
                cv.wait(lk);  // nothing at all
            } else {
                auto next_time = scheduled->peek_time();
                cv.wait_until(lk, next_time->get());
            }
            // when we wake, lk is re-acquired -> loop back to (1)
            continue;
        }

        // 3) Pop the highest-priority ready task
        Task task = std::move(*ready->pop());
        bool is_recurring = task.interval.count() > 0;

        Task recurring_task;
        if (is_recurring) {
            auto now = clock->now();
            recurring_task = Task{
                task.job, // copy the callable
                task.priority,
                sequence_number.fetch_add(1, std::memory_order_relaxed),
                now + task.interval,        // scheduled_at
                task.interval,
                now,        // enqueue_time
                std::nullopt                // no deadline for recurring
            };
        }

        lk.unlock();
        dispatchOne(std::move(task));

        if (is_recurring) {
            std::lock_guard<std::mutex> lock{mtx};
            scheduled->push(std::move(recurring_task));
            cv.notify_one();
        }

    }
}

void Scheduler::dispatchOne(Task task) {
    thread_pool->submit([this, task = std::move(task)]{
        auto start = clock->now();
        statistics->updateLatencyStatistics(std::chrono::duration_cast<std::chrono::milliseconds>(start - task.enqueue_time).count());
        if (task.deadline && start > *task.deadline)
            missed_tasks.fetch_add(1, std::memory_order_relaxed);

        try {
            if (!task.job) {
                return;
            }
            task.job(); }
        catch(...) { /* log */ }
    });
}

std::tuple<double, double, double> Scheduler::getLatencyStatistics() const
{
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
