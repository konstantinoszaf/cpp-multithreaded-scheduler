#pragma once
#include <functional>
#include <optional>
#include <chrono>
namespace scheduler::detail {

using milliseconds = std::chrono::milliseconds;
using time_point   = std::chrono::steady_clock::time_point;

struct Task {
    std::function<void()> job;
    uint64_t priority;
    time_point scheduled_at;
    milliseconds interval;
    time_point enqueue_time;
    std::optional<time_point> deadline;
    uint64_t sequence_number;

    Task() = default;
    Task(std::function<void()> f,
         uint64_t prio,
         uint64_t seq,
         time_point scheduled,
         milliseconds intrvl,
         time_point enqueue,
         std::optional<time_point> dl)
      : job(std::move(f))
      , priority(prio)
      , scheduled_at(scheduled)
      , interval(intrvl)
      , enqueue_time(enqueue)
      , deadline(dl)
      , sequence_number(seq)
    {}

    // // If my priority is less than them, i am less than them
    // // if they are equal, and my sequence number is greater than them, i am less
    // // than them (FIFO)
    // bool operator<(Task const& other) const noexcept {
    //     if (priority != other.priority)
    //         return priority < other.priority;
    //     return sequence_number > other.sequence_number;
    // }
};
} //namespace scheduler::detail