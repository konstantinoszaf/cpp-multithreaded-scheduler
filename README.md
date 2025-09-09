# C++ multi-threaded Task Scheduler
Multi-Threaded Task Scheduling.
The scheduler sustains ~**2M tasks/s** with **microsecond-class latency** and **near-zero deadline misses** for 1 ms deadlines.

A multi-threaded task scheduler with support for:

* Priority-based scheduling
* Deadline handling during scheduling (enqueuing)
* Recurring tasks (fixed-interval)
* Latency statistics (min / avg / max)
* Concurrency-safe scheduling from multiple threads
* Burst and stress testing utilities

---

## Features

* **Thread Pool**: Fixed-size worker pool (default to hardware concurrency).
* **Ordered Queue**: If a task is enqueued with a deadline of 1 ms or less, it is assigned critical priority.
* **Recurring Tasks**: Schedule tasks to run at fixed intervals.
* **Latency Measurement**: Tracks enqueue-to-execute timing.
* **Missed-Deadline Counter**: Counts tasks that start after their deadline.
* **Testing Utilities**:

  * `main.cpp`: Verifies thread-safe scheduling under multi-producer load.

---

## Representative Results

| Configuration | Submit Throughput | Overall Throughput | Drain Throughput | Latency avg | p95 | p99 | Missed Deadlines |
|---|---:|---:|---:|---:|---:|---:|---:|
| **First take** (naïve, blocking; no stealing) | **1,078,643** tasks/s | **146,318** tasks/s | **161,132** tasks/s | **5.06 s** | 8.76 s | 9.19 s | **1,599,840 / 1,600,000** |
| **Final take** (work stealing, light batching) | **1,949,440** tasks/s | **1,949,170** tasks/s | **∞** | **9.27 µs** | 43.9 µs | 139 µs | **20 / 1,600,000** |

> In the optimized run, **remaining=0** at the end of submission, so **drain throughput = ∞** (workers kept pace with producers).

---

## Design Evolution

**Initial design.** The scheduler owned two internal threads, one for *ready* tasks and one for *scheduled* (time-delayed) tasks, each with its own mutex-guarded queue. When a ready task was popped, it was pushed into the thread pool’s shared priority queue (also mutex-protected). This introduced multiple hand-offs and lock contention (two internal queues + a hot-path shared priority queue), which the benchmarks showed to be inefficient.

**Optimized design.** The scheduler now acts as a pass-through: tasks go directly to **per-worker run queues**. A single ordered queue is retained for *recurring/time-delayed* tasks, which are promoted into worker queues at their due time. Workers pull in small batches, **steal work** when idle, and the shared priority queue is removed from the hot path—greatly reducing contention and queueing delay.

**Result.** In the synthetic benchmark, the optimized scheduler sustains ~**2M tasks/s** with **microsecond-class latency** and **near-zero deadline misses** for 1 ms deadlines.

---

## Sample Driver Output (Optimized)
[2025-09-09 16:39:45.976] [info] === Throughput ===
[2025-09-09 16:39:45.976] [info] Submit throughput:     1949440 tasks/s (1600000 tasks in 0.821s)
[2025-09-09 16:39:45.976] [info] Overall throughput:    1949170 tasks/s (1600000 tasks in 0.821s)
[2025-09-09 16:39:45.976] [info] Drain throughput:          inf tasks/s (0 remaining in 0.000s)
[2025-09-09 16:39:46.040] [info] === Latency (enqueue -> start) ===
[2025-09-09 16:39:46.040] [info] avg=9271.144573125 ns  min=95 ns  max=8561494 ns
[2025-09-09 16:39:46.040] [info] p95=43898 ns  p99=139389 ns  p999=395650 ns
[2025-09-09 16:39:46.040] [info] missed deadlines: 20 / 1600000

---

## Benchmark Methodology

- **Workers:** `std::thread::hardware_concurrency()`
- **Producers:** 8
- **Total tasks:** 1,600,000 (200k per producer)
- **Task body:** trivial lambda (counts down a latch)
- **Deadline:** enqueue time + **2 ms**
- **Latency metric:** **enqueue → start** (queueing delay)

### Throughput Metrics (reported by the driver)

- **Submit throughput** = total tasks ÷ time to enqueue them.  
- **Overall throughput** = total tasks ÷ (from synchronized start → all done).  
- **Drain throughput** = tasks remaining at submit-end ÷ drain time.  
  - If remaining is 0, drain throughput is **∞** (scheduler kept up with producers).

---

## Building

This project uses CMake. Requires C++20 or above, and [spdlog](https://github.com/gabime/spdlog) only
 if main.cpp is being built.

```bash
# shared library only
make
# shared library and main example
make build-main
#unit tests
make build-tests
```

The test driver `main` will be built into `build/main`.

---

## Usage

Run the test executable to see scheduling behavior, thread-safety check, and latency stats:

```bash
make run-main
#or
./build/main
# execute tests
make run-tests
#or build and run
make test
```

## Integrating the Scheduler

Include the header and link against the scheduler library:

```cpp
#include <scheduler/scheduler.h>

int main() {
  scheduler::Scheduler sched{4};
  sched.schedule([]{/*...*/}, /*priority=*/10);
  sched.scheduleRecurring([]{/*...*/}, /*priority=*/5, std::chrono::milliseconds(100));

  auto [avg, min, max] = sched.getLatencyStatistics();
}
```

---

## API Reference

* `Scheduler(size_t numThreads)` – Construct with given worker count.
* `void schedule(function<void()> job, int priority, optional<time_point> deadline)`
* `void scheduleRecurring(function<void()> job, int priority, milliseconds interval)`
* `tuple<double,double,double> getLatencyStatistics() const` - Calculates average / minimum / maximum scheduling statistics
* `uint64_t getMissedTasks() const`
* `std::tuple<double, double, double> getPvalueStatistics() const` - Calculates P95, P99, P999 statistics

---

## Testing and Profiling

* **Concurrency test**: ensures `schedule()` is thread-safe.
* **Latency stress**: recurring 1ms tasks and bursty loads.

```bash
#execute valgrind tests
make sanity
```
* Two files will be saved in the project's root directory with the results ( helgrind.out and leak_check.out)
---
