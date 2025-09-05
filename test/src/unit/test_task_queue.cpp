#include "scheduler/ordered_queue.h"
#include "detail/task.h"
#include <gtest/gtest.h>

using namespace scheduler::queue;
using namespace scheduler::detail;
using namespace std::chrono;

TEST(OrderedQueue, TestEmptyQueue) {
    OrderedQueue<Task, std::less<Task>> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0);
}

TEST(OrderedQueue, InsertTaskToQueue) {
    OrderedQueue<Task, std::less<Task>> q;
    Task t;
    q.push(t);
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 1);
}

TEST(OrderedQueue, InsertArecurringTask) {
    OrderedQueue<Task, std::less<Task>> q;
    auto now = steady_clock::now();
    Task t {
        [](){}, //job
        100, //priority
        3, //sequence_number
        now + milliseconds{10}, // scheduled_at
        milliseconds{10}, // interval
        now, //enqueue_time
        std::nullopt, //deadline
        false //recurring
    };

    q.push(t);

    std::optional<Task> rsp = q.try_pop();

    EXPECT_EQ(rsp, std::nullopt);

    rsp = q.wait_and_pop();

    EXPECT_EQ(t, *rsp);
}

TEST(OrderedQueue, TestQueueShorting) {
    OrderedQueue<Task, std::less<Task>> q;
    auto now = steady_clock::now();
    Task t1{
        [](){}, //job
        10, //priority
        1, //sequence_number
        now, // scheduled_at
        milliseconds{0}, // interval
        now, //enqueue_time
        std::nullopt, //deadline
        false //recurring
    };

    Task t2 {
        [](){}, //job
        90, //priority
        2, //sequence_number
        now, // scheduled_at
        milliseconds{0}, // interval
        now, //enqueue_time
        now + milliseconds{9}, //deadline
        false //recurring
    };

    Task t3 {
        [](){}, //job
        100, //priority
        3, //sequence_number
        now + milliseconds{10}, // scheduled_at
        milliseconds{10}, // interval
        now, //enqueue_time
        std::nullopt, //deadline
        false //recurring
    };

    Task t4 {
        [](){}, //job
        9, //priority
        4, //sequence_number
        now + milliseconds{11}, // scheduled_at
        milliseconds{11}, // interval
        now, //enqueue_time
        std::nullopt, //deadline
        false //recurring
    };

    EXPECT_EQ(q.size(), 0);
    q.push(t1);
    EXPECT_EQ(q.size(), 1);
    q.push(t2);
    EXPECT_EQ(q.size(), 2);
    q.push(t3);
    EXPECT_EQ(q.size(), 3);
    q.push(t4);
    EXPECT_EQ(q.size(), 4);

    Task res1;
    EXPECT_TRUE(q.try_pop(res1));
    EXPECT_EQ(t2, res1);
    EXPECT_EQ(q.size(), 3);

    Task res2;
    EXPECT_TRUE(q.try_pop(res2));
    EXPECT_EQ(t1, res2);
    EXPECT_EQ(q.size(), 2);

    Task res3;
    EXPECT_FALSE(q.try_pop(res3));

    auto r3 = q.wait_and_pop();
    auto r4 = q.wait_and_pop();

    EXPECT_EQ(t3, r3);
    EXPECT_EQ(t4, r4);
}