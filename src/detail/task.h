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

    // If my deadline time is later than theirs, i am less than them
    // If our deadlines times are the same, and my priority is less than them,
    // i am less than them
    // if all are equal, and my sequence number is larger than them, i am less
    // than them (FIFO)
    bool operator<(Task const& other) const noexcept {
        auto i_have = deadline.has_value();
        auto they_have = other.deadline.has_value();

        if (i_have != they_have)
            return i_have < they_have;

        if (i_have && they_have) {
            if (deadline.value() != other.deadline.value())
                // a < b if my deadline is *later* than theirs
                return deadline.value() > other.deadline.value();
        }

        if (priority != other.priority)
            return priority < other.priority;
        return sequence_number > other.sequence_number;
    }
};
} //namespace scheduler::detail