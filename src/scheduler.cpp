#include "scheduler/scheduler.h"
#include "detail/task_queue_impl.h"
#include "detail/system_clock_impl.h"
#include "detail/thread_pool_impl.h"

using namespace scheduler;
using namespace detail;

Scheduler::Scheduler(size_t numThreads) {
    if (numThreads <= 0) numThreads = 1;
    thread_pool = std::make_shared<ThreadPool>(numThreads);
    clock = std::make_shared<SystemClock>();
    data = std::make_shared<TaskQueue>();
};

Scheduler::Scheduler(std::shared_ptr<detail::IClock> clock_,
                    std::shared_ptr<detail::ITaskQueue> data_,
                    std::shared_ptr<detail::IThreadPool> pool_)
  : clock{clock_}, data{data_}, thread_pool{pool_} {};

Scheduler::~Scheduler() {
    thread_pool->stop();
}

void Scheduler::schedule(std::function<void()> task, int priority,
    std::optional<std::chrono::steady_clock::time_point> deadline) {

}

// Allow tasks that run repeatedly on an interval
void Scheduler::scheduleRecurring(std::function<void()> task, int priority,
                                            std::chrono::milliseconds interval) {

}

std::tuple<double, double, double> Scheduler::getLatencyStatistics() const {
    return {};
}