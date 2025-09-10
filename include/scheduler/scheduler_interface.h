#pragma once
#include <functional>
#include <tuple>
#include <optional>
#include <chrono>

namespace scheduler {
class IScheduler {
public:
    virtual ~IScheduler() = default;

    // Scheduling tasks
    // Schedules a task with a specific priority
    // and an optional deadline
    virtual void schedule(std::function<void()> task, int priority,
      std::optional<std::chrono::steady_clock::time_point> deadline =
                                                       std::nullopt) = 0;

    // Allow tasks that run repeatedly on an interval
    virtual void scheduleRecurring(std::function<void()> task, int priority,
                           std::chrono::milliseconds interval) = 0;

    // Performance metrics
    // Returns average, min, max latency so far
    virtual std::tuple<double, double, double> getLatencyStatistics() const = 0;
    virtual std::tuple<double, double, double> getPvalueStatistics() const = 0;
    virtual uint64_t getMissedTasks() = 0;
};
}; // namespace Scheduler