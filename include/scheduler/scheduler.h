#pragma once
#include <functional>
#include <tuple>
#include <optional>
#include <chrono>
#include <thread>
#include <memory>

namespace scheduler {

namespace detail {
    class IClock;
    class ITaskQueue;
}

class Scheduler {
public:
    // Constructor/Destructor
    explicit Scheduler(size_t numThreads =
                       std::thread::hardware_concurrency());
    ~Scheduler(); // joins and cleans up threads

    // Scheduling tasks
    // Schedules a task with a specific priority
    // and an optional deadline
    // (e.g., a time_point from std::chrono).
    void schedule(std::function<void()> task, int priority,
      std::optional<std::chrono::steady_clock::time_point> deadline =
                                                       std::nullopt);

    // Allow tasks that run repeatedly on an interval
    void scheduleRecurring(std::function<void()> task, int priority,
                           std::chrono::milliseconds interval);

    // Performance metrics
    // Returns average, min, max latency so far
    std::tuple<double, double, double> getLatencyStatistics() const;

private:
    // Implementation details
    // Scheduler(size_t numThreads,
    //     std::shared_ptr<detail::IClock> clock,
    //     std::shared_ptr<detail::ITaskQueue> queue);
    // std::shared_ptr<detail::IClock> clock;
    std::shared_ptr<detail::ITaskQueue> pq;
};

}; // namespace Scheduler