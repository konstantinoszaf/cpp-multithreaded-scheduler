#include "scheduler/scheduler.h"
#include <atomic>
#include <barrier>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

using steady_clock = std::chrono::steady_clock;

int main() {
    const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    spdlog::info("Starting benchmark with {} worker threads", threads);
    scheduler::Scheduler scheduler{threads};

    // scheduler.scheduleRecurring([](){}, 10, std::chrono::milliseconds(100));
    // Benchmark parameters
    const int producers = 8;
    const int tasks_per_producer = 200'000;
    const uint64_t total_tasks = uint64_t(producers) * tasks_per_producer;

    // Completion tracking
    std::latch done(total_tasks);
    std::atomic<uint64_t> executed{0};

    // Synchronized start for fair timing
    std::barrier start(producers + 1);

    // Spawn producers
    std::vector<std::thread> ps;
    ps.reserve(producers);
    for (int p = 0; p < producers; ++p) {
        ps.emplace_back([&] {
            start.arrive_and_wait(); // wait for the common start
            for (int i = 0; i < tasks_per_producer; ++i) {
                scheduler.schedule([&] {
                    executed.fetch_add(1, std::memory_order_relaxed);
                    done.count_down();
                }, /*priority=*/5, steady_clock::now() + std::chrono::milliseconds(1));
            }
        });
    }

    // Timings
    const auto t0 = steady_clock::now(); // overall start (just before releasing producers)
    start.arrive_and_wait();

    const auto t_submit_start = steady_clock::now();
    for (auto& th : ps) th.join();
    const auto t_submit_end = steady_clock::now();

    // Drain phase
    const uint64_t executed_at_submit_end = executed.load(std::memory_order_relaxed);
    const uint64_t remaining = total_tasks - executed_at_submit_end;

    const auto t_drain_start = t_submit_end;
    done.wait();
    const auto t_drain_end = steady_clock::now();

    // Metrics
    const double submit_secs = std::chrono::duration<double>(t_submit_end - t_submit_start).count();
    const double overall_secs = std::chrono::duration<double>(t_drain_end - t0).count();
    const double drain_secs = std::chrono::duration<double>(t_drain_end - t_drain_start).count();

    const double submit_tps  = total_tasks / submit_secs;
    const double overall_tps = total_tasks / overall_secs;
    const double drain_tps   = remaining ? (double)remaining / drain_secs : std::numeric_limits<double>::infinity();

    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== Throughput ===");
    spdlog::info("Submit throughput:  {:>10.0f} tasks/s ({} tasks in {:.3f}s)", submit_tps, total_tasks, submit_secs);
    spdlog::info("Overall throughput: {:>10.0f} tasks/s ({} tasks in {:.3f}s)", overall_tps, total_tasks, overall_secs);
    spdlog::info("Drain throughput:   {:>10.0f} tasks/s ({} remaining in {:.3f}s)", drain_tps, remaining, drain_secs);

    auto [avg, minv, maxv] = scheduler.getLatencyStatistics();
    auto [p95, p99, p999]  = scheduler.getPvalueStatistics();
    uint64_t missed        = scheduler.getMissedTasks();

    spdlog::info("=== Latency (enqueue -> start) ===");
    spdlog::info("avg={} ns  min={} ns  max={} ns", avg, minv, maxv);
    spdlog::info("p95={} ns  p99={} ns  p999={} ns", p95, p99, p999);
    spdlog::info("missed deadlines: {} / {}", missed, total_tasks);

    return 0;
}
