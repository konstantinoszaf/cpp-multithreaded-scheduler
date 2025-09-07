# C++ multi-threaded Task Scheduler
Multi-Threaded Task Scheduling.
The scheduler achieves an average task‐dispatch latency of approximately **8 microseconds** under heavy concurrent load.

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

  * `runBasicSchedulingTests()`: Exercises recurring, one-shot, and burst submissions.
  * `concurrencyTest()`: Verifies thread-safe scheduling under multi-producer load.

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

Sample output of main executable:

```
[2025-09-07 17:39:37.289] [info] === Throughput ===
[2025-09-07 17:39:37.289] [info] Submitted: 1600000 tasks in 0.669s, 2390126 tasks/s
[2025-09-07 17:39:37.347] [info] === Latency ===
[2025-09-07 17:39:37.347] [info] avg=247330.287605 ns min=183 ns max=8747658 ns
[2025-09-07 17:39:37.347] [info] p95=731302.3499999873 ns p99=7093292.039999989 ns p999=8364804.538000029 ns
[2025-09-07 17:39:37.347] [info] missed deadlines: 59409 / 1600000
```

---

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
* `tuple<double,double,double> getLatencyStatistics() const`
* `uint64_t getMissedTasks() const`

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
