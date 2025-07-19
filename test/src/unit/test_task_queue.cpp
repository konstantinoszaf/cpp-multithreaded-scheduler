#include <gtest/gtest.h>
#include "detail/task_queue_impl.h"
#include <memory>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <atomic>

using namespace scheduler::detail;

class TestTaskQueue : public ::testing::Test
{
protected:
    std::shared_ptr<TaskQueue> task_queue;

    void SetUp() override
    {
        task_queue = std::make_shared<TaskQueue>();
    }

    void TearDown() override
    {
        std::optional<Task> t;
        do
        {
            t = task_queue->pop();
        } while (t.has_value());
        // task_queue destroyed after all threads, no races
    }
};

TEST_F(TestTaskQueue, PopOnEmptyReturnsNullopt)
{
    auto t = task_queue->pop();
    EXPECT_FALSE(t.has_value());
}

TEST_F(TestTaskQueue, PushPopSingleTask)
{
    auto now = std::chrono::steady_clock::now();
    Task foo(
        [] {}, // task
        /*priority=*/7,
        /*seq=*/42,
        /*interval=*/std::chrono::milliseconds{0},
        /*enqueue=*/now,
        /*deadline=*/now);
    task_queue->push(std::move(foo));

    EXPECT_FALSE(task_queue->empty());
    EXPECT_EQ(task_queue->size(), 1u);

    auto t = task_queue->pop();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->priority, 7);
    EXPECT_EQ(t->sequence_number, 42);

    EXPECT_TRUE(task_queue->empty());
}

TEST_F(TestTaskQueue, PriorityOrdering)
{
    auto now = std::chrono::steady_clock::now();
    Task low(
        [] {}, 1, 1,
        std::chrono::milliseconds{0}, now, now);
    Task high(
        [] {}, 99, 2,
        std::chrono::milliseconds{0}, now, now);
    task_queue->push(std::move(low));
    task_queue->push(std::move(high));

    auto t1 = task_queue->pop();
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->priority, 99);

    auto t2 = task_queue->pop();
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->priority, 1);
}

TEST_F(TestTaskQueue, FirstNoDeadlineOrdering)
{
    auto now = std::chrono::steady_clock::now();
    Task low(
        [] {}, 3, 1,
        std::chrono::milliseconds{0}, now, std::nullopt);
    Task high(
        [] {}, 1, 2,
        std::chrono::milliseconds{0}, now, now);
    task_queue->push(std::move(low));
    task_queue->push(std::move(high));

    auto t1 = task_queue->pop();
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->priority, 1);

    auto t2 = task_queue->pop();
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->priority, 3);
}

TEST_F(TestTaskQueue, AddMultipleTasksAndCheckOrdering)
{
    auto now = std::chrono::steady_clock::now();
    Task task1([] {}, 3, 1, std::chrono::milliseconds{0}, now, std::nullopt);
    Task task2([] {}, 91, 2, std::chrono::milliseconds{0}, now, std::nullopt);
    Task task3([] {}, 89, 3, std::chrono::milliseconds{0}, now, now);
    Task task4([] {}, 90, 4, std::chrono::milliseconds{0}, now, now + std::chrono::milliseconds{1});

    task_queue->push(std::move(task1));
    task_queue->push(std::move(task2));
    task_queue->push(std::move(task3));
    task_queue->push(std::move(task4));

    auto t1 = task_queue->pop();
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->priority, 89);

    auto t2 = task_queue->pop();
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->priority, 90);

    auto t3 = task_queue->pop();
    ASSERT_TRUE(t3.has_value());
    EXPECT_EQ(t3->priority, 91);

    auto t4 = task_queue->pop();
    ASSERT_TRUE(t4.has_value());
    EXPECT_EQ(t4->priority, 3);
}

TEST_F(TestTaskQueue, FifoTieBreak)
{
    auto now = std::chrono::steady_clock::now();
    Task first(
        [] {}, 5, 100,
        std::chrono::milliseconds{0}, now, now);
    Task second(
        [] {}, 5, 101,
        std::chrono::milliseconds{0}, now, now);
    task_queue->push(std::move(first));
    task_queue->push(std::move(second));

    auto t1 = task_queue->pop();
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1->sequence_number, 100u);

    auto t2 = task_queue->pop();
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->sequence_number, 101u);
}

TEST_F(TestTaskQueue, PeekDoesNotPop)
{
    auto now = std::chrono::steady_clock::now();
    Task foo(
        [] {}, 3, 7,
        std::chrono::milliseconds{0}, now, now);
    task_queue->push(std::move(foo));

    auto p = task_queue->peek();
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->get().sequence_number, 7u);

    EXPECT_EQ(task_queue->size(), 1u);

    auto t = task_queue->pop();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->sequence_number, 7u);
}

TEST_F(TestTaskQueue, SizeReflectsPushesAndPops)
{
    auto now = std::chrono::steady_clock::now();
    EXPECT_EQ(task_queue->size(), 0u);
    for (int i = 0; i < 5; ++i)
    {
        Task tmp(
            [] {}, i, i,
            std::chrono::milliseconds{0}, now, now);
        task_queue->push(std::move(tmp));
        EXPECT_EQ(task_queue->size(), static_cast<size_t>(i + 1));
    }
    for (int i = 5; i > 0; --i)
    {
        task_queue->pop();
        EXPECT_EQ(task_queue->size(), static_cast<size_t>(i - 1));
    }
}
