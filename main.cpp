#include <scheduler/scheduler.h>
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>

int main() {
    using namespace std::chrono_literals;

    scheduler::Scheduler scheduler{4};

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> priority_dist(1, 10);

    auto executions = std::make_shared<std::atomic<uint64_t>>(0);

    auto job = [executions](){
        auto count = executions->fetch_add(1);
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::cout << ms << "#" << count << " executed by thread "
                  << std::this_thread::get_id() << "\n";
    };

    // Schedule recurring task
    scheduler.scheduleRecurring(job, 5, 50ms);

    // Schedule tasks with varying priorities and explicit deadlines
    for (int i = 0; i < 10; ++i) {
        auto p = priority_dist(gen);
        scheduler.schedule(
            [i, p](){
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                std::cout << ms << " " << i << " one-off with prio " << p << '\n';
            },
            p,
            std::chrono::steady_clock::now() + std::chrono::milliseconds(10 * i)
        );
    }

    // Simulate bursty workload
    for (int i = 0; i < 1000; ++i) {
        scheduler.schedule(job, priority_dist(gen),
            std::chrono::steady_clock::now() + 50ms);
    }

    // Allow tasks to execute
    std::this_thread::sleep_for(2s);

    // Retrieve latency statistics and print them
    auto [average, minimum, maximum] = scheduler.getLatencyStatistics();
    uint64_t missed = scheduler.getMissedTasks();

    std::cout << "\n=== Scheduler Latency Results ===\n"
              << "Average Latency: " << average << " ms\n"
              << "Minimum Latency: " << minimum << " ms\n"
              << "Maximum Latency: " << maximum << " ms\n"
              << "Missed tasks: " << missed << '\n';

    return 0;
}
