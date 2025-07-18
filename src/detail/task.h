#pragma once
#include <functional>
#include <optional>
#include <chrono>

namespace scheduler::detail {

using milliseconds = std::chrono::milliseconds;
using time_point   = std::chrono::steady_clock::time_point;

struct Task {
    std::function<void()> task;
    int priority;
    milliseconds interval; // zero means one off task
    time_point enqueue_time;
    std::optional<time_point> deadline;
    uint64_t sequence_number;

    Task(std::function<void()> f,
         int prio,
         uint64_t seq,
         milliseconds intrvl,
         time_point enqueue,
         std::optional<time_point> dl)
      : task(std::move(f))
      , priority(prio)
      , interval(intrvl)
      , deadline(dl)
      , sequence_number(seq)
    {}

    Task(std::function<void()> f,
         int prio,
         uint64_t seq)
       : Task(std::move(f), prio, seq,
         milliseconds{0},
         std::chrono::steady_clock::now(),
         std::optional<time_point>{std::nullopt})
    {}

    // If my scheduled time is later than theirs, i am less than them
    // If our scheduled times are the same, and my priority is less than them,
    // i am less than them
    // if all are equal, and my sequence number is larger than them, i am less
    // than them (FIFO)
    bool operator<(Task const& other) const noexcept {
        if (deadline != other.deadline) {
            if (!deadline) return false;
            if (!other.deadline) return true;
            return *deadline > *other.deadline;
        }
        if (priority != other.priority)
            return priority < other.priority;
        return sequence_number > other.sequence_number;
    }
};
} //namespace scheduler::detail