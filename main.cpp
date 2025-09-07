#include "scheduler/scheduler.h"
#include <atomic>
#include <chrono>
#include <latch>
#include <random>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

using steady_clock = std::chrono::steady_clock;

int main() {
    const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    scheduler::Scheduler scheduler{threads};

    // Benchmark parameters
    const int producers = 8;
    const int tasks_per_producer = 200'000;
    const uint64_t total_tasks = uint64_t(producers) * tasks_per_producer;

    // Count completions with a latch
    std::latch done(total_tasks);

    // ---- Submit phase (measure enqueue throughput) ----
    auto t_submit_start = steady_clock::now();

    std::vector<std::thread> ps;
    ps.reserve(producers);
    for (int p = 0; p < producers; ++p) {
        ps.emplace_back([&]() {
            for (int i = 0; i < tasks_per_producer; ++i) {
                scheduler.schedule([&done]() noexcept { done.count_down(); },
                                   5, steady_clock::now() + std::chrono::milliseconds(2));
            }
        });
    }
    for (auto& th : ps) th.join();

    auto t_submit_end = steady_clock::now();

    done.wait(); // block until every task has executed

    // ---- Report ----
    const double submit_secs =
        std::chrono::duration<double>(t_submit_end - t_submit_start).count();

    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== Throughput ===");
    spdlog::info("Submitted: {} tasks in {:.3f}s, {:.0f} tasks/s",
                 total_tasks, submit_secs, total_tasks / submit_secs);

    auto [avg, minv, maxv] = scheduler.getLatencyStatistics();
    auto [p95, p99, p999]  = scheduler.getPvalueStatistics();
    uint64_t missed        = scheduler.getMissedTasks();

    spdlog::info("=== Latency ===");
    spdlog::info("avg={} ns min={} ns max={} ns", avg, minv, maxv);
    spdlog::info("p95={} ns p99={} ns p999={} ns", p95, p99, p999);
    spdlog::info("missed deadlines: {} / {}", missed, total_tasks);

    return 0;
}
