#pragma once
#include <functional>
#include <optional>
#include <chrono>
#include "detail/statistics_calculator.h"

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

    bool operator<(Task const& other) const {
        if (scheduled_at != other.scheduled_at)
            return scheduled_at > other.scheduled_at;
        if (priority != other.priority)
            return priority < other.priority;
        return sequence_number > other.sequence_number;
    }

    void operator()() const {
        try {
            job();
        } catch(...)
        { /* log */ }
    }

    bool operator==(Task const& other) const {
        return
            priority         == other.priority &&
            scheduled_at     == other.scheduled_at &&
            interval         == other.interval &&
            enqueue_time     == other.enqueue_time &&
            deadline         == other.deadline &&
            sequence_number  == other.sequence_number;
    }

};
} //namespace scheduler::detail