#include <scheduler/scheduler.h>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <spdlog/spdlog.h>

void runBasicSchedulingTests(scheduler::Scheduler& scheduler,
                              std::shared_ptr<std::atomic<uint64_t>> executions) {
    using namespace std::chrono_literals;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> priority_dist(1, 100);

    uint64_t startCount = executions->load(std::memory_order_relaxed);
    // Define periodic job
    auto job_rec = [executions]() {
        auto count = executions->fetch_add(1, std::memory_order_relaxed);
        // spdlog::info("Periodic Task #{} executed.", count); //disabled for better speed
    };

    auto job_one_off = [executions]() {
        auto count = executions->fetch_add(1, std::memory_order_relaxed);
        // spdlog::info("One-off Task #{} executed.", count); //disabled for better speed
    };
    // Schedule a high-frequency recurring task

    // Schedule one-off tasks with varying priorities and deadlines
    for (int i = 0; i < 10; ++i) {
        int p = priority_dist(gen);
        scheduler.schedule(
            [i, p]() {
                // spdlog::info("One-off task #{} with priority {} executed.", i, p); //disabled for better speed
            },
            p,
            std::chrono::steady_clock::now() + std::chrono::microseconds(20 + i)
        );
    }

    // Simulate bursty workload of 10k tasks
    for (int i = 0; i < 1000; ++i) {
        scheduler.schedule(job_one_off, priority_dist(gen),
            std::chrono::steady_clock::now() + std::chrono::microseconds(40));
    }
}

// Helper to test concurrency safety: multiple threads scheduling tasks
void concurrencyTest(scheduler::Scheduler& scheduler,
                     std::shared_ptr<std::atomic<uint64_t>> executions,
                     int numProducers,
                     int tasksPerProducer) {
    // Capture starting execution count to measure only this phase
    uint64_t startCount = executions->load(std::memory_order_relaxed);

    std::vector<std::thread> producers;
    producers.reserve(numProducers);
    for (int t = 0; t < numProducers; ++t) {
        producers.emplace_back([&, t]() {
            for (int i = 0; i < tasksPerProducer; ++i) {
                scheduler.schedule(
                    [executions]() {
                        executions->fetch_add(1, std::memory_order_relaxed);
                    },
                    /*priority=*/5,
                    std::chrono::steady_clock::now() + std::chrono::microseconds{40}
                );
            }
        });
    }
    for (auto &th : producers) {
        th.join();
    }

    // Allow scheduler to process all submitted tasks
    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint64_t expected = uint64_t(numProducers) * tasksPerProducer;
    uint64_t actual = executions->load(std::memory_order_relaxed) - startCount;
    spdlog::info("Concurrency test: expected {} tasks, actually ran {}", expected, actual);
}


int main() {
    using namespace std::chrono_literals;

    scheduler::Scheduler scheduler{std::thread::hardware_concurrency()};

    // Atomic counter for executed tasks
    auto executions = std::make_shared<std::atomic<uint64_t>>(0);

    runBasicSchedulingTests(scheduler, executions);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    concurrencyTest(scheduler, executions, /*numProducers=*/8, /*tasksPerProducer=*/200000);

    auto [average, minimum, maximum] = scheduler.getLatencyStatistics();
    uint64_t missed = scheduler.getMissedTasks();

    spdlog::info("=== Scheduler Latency Results ===");
    spdlog::info("Average Latency: {} ns", average);
    spdlog::info("Minimum Latency: {} ns", minimum);
    spdlog::info("Maximum Latency: {} ns", maximum);
    spdlog::info("Missed tasks: {} out of {}", missed, executions->load());

    auto [p95, p99, p999] = scheduler.getPvalueStatistics();

    spdlog::info("=== Scheduler Percentile Results ===");
    spdlog::info("P95: {} ns", p95);
    spdlog::info("P99: {} ns", p99);
    spdlog::info("P999: {} ns", p999);

    scheduler.scheduleRecurring([](){spdlog::info("Periodic Task executed.");}, /*priority=*/5, 100ms);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return 0;
}
