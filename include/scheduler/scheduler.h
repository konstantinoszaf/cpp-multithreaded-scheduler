#pragma once
#include <functional>
#include <tuple>
#include <optional>
#include <chrono>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include "scheduler_interface.h"

namespace scheduler {

namespace detail {
    class ITaskQueue;
    class IThreadPool;
    class IStatisticsCalculator;
    struct Task;
}

class Scheduler : public IScheduler {
public:
    explicit Scheduler(size_t numThreads =
                       std::thread::hardware_concurrency());
    ~Scheduler(); // joins and cleans up threads

    // Scheduling tasks
    // Schedules a task with a specific priority
    // and an optional deadline
    void schedule(std::function<void()> task, int priority,
      std::optional<std::chrono::steady_clock::time_point> deadline =
                                                       std::nullopt) override;

    // Allow tasks that run repeatedly on an interval
    void scheduleRecurring(std::function<void()> task, int priority,
                           std::chrono::milliseconds interval) override;

    // Performance metrics
    // Returns average, min, max latency so far
    std::tuple<double, double, double> getLatencyStatistics() const override;
    std::tuple<double, double, double> getPvalueStatistics() const override;
    uint64_t getMissedTasks() override;

    // Implementation details
protected:
    Scheduler(std::unique_ptr<detail::IThreadPool> thread_pool,
        std::unique_ptr<detail::IStatisticsCalculator> stats);
    uint64_t calculatePriority(int priority,
        std::optional<std::chrono::steady_clock::time_point> deadline, std::chrono::steady_clock::time_point now);

private:
    std::unique_ptr<detail::IThreadPool> thread_pool;
    std::unique_ptr<detail::IStatisticsCalculator> statistics;
    std::atomic<uint64_t> sequence_number{0};
    std::atomic<uint64_t> missed_tasks{0};
    std::chrono::milliseconds deadline_imminence{10};
};

}; // namespace Scheduler