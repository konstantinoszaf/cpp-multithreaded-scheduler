#include "scheduler/scheduler.h"
#include "detail/task_queue_impl.h"
// #include <src/detail/clock.h>

using namespace scheduler;

Scheduler::Scheduler(size_t numThreads) {

};

Scheduler::~Scheduler() {

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