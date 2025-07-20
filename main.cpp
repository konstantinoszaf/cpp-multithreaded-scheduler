#include <scheduler/scheduler.h>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <spdlog/spdlog.h>

int main() {
    using namespace std::chrono_literals;

    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [tid=%t] %v");
    scheduler::Scheduler scheduler{4};
    // spdlog::info("Scheduling multiple tasks");
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> priority_dist(1, 10);

    auto executions = std::make_shared<std::atomic<uint64_t>>(0);

    auto job = [executions](){
        auto count = executions->fetch_add(1);
        // spdlog::info("#{} executed.", count);
    };

    // Schedule recurring task
    scheduler.scheduleRecurring(job, 5, 1ms);

    // Schedule tasks with varying priorities and explicit deadlines
    for (int i = 0; i < 10; ++i) {
        auto p = priority_dist(gen);
        scheduler.schedule(
            [i, p](){
                // spdlog::info("#{} one off with prio {}", i, p);
            },
            p,
            std::chrono::steady_clock::now() + std::chrono::milliseconds(10 * i)
        );
    }

    // Simulate bursty workload
    for (int i = 0; i < 10000; ++i) {
        scheduler.schedule(job, priority_dist(gen),
            std::chrono::steady_clock::now() + 20ms);
    }

    // Allow tasks to execute
    std::this_thread::sleep_for(2s);

    // Retrieve latency statistics and print them
    auto [average, minimum, maximum] = scheduler.getLatencyStatistics();
    uint64_t missed = scheduler.getMissedTasks();

    spdlog::info("=== Scheduler Latency Results ===");
    spdlog::info("Average Latency: {} ms", average);
    spdlog::info("Minimum Latency: {} ms", minimum);
    spdlog::info("Maximum Latency: {} ms", maximum);
    spdlog::info("Missed tasks: {}", missed);

    return 0;
}
