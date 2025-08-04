// #include <gtest/gtest.h>
// #include "detail/task_queue_impl.h"
// #include <memory>
// #include <chrono>
// #include <thread>
// #include <unordered_set>
// #include <atomic>
// #include <optional>

// using namespace scheduler::detail;

// class TestTaskQueue : public ::testing::Test {
// protected:
//     std::shared_ptr<TaskQueue> task_queue;

//     void SetUp() override
//     {
//         auto priorityComparator = [](const Task& a, const Task& b) noexcept -> bool {
//             if (a.priority != b.priority)
//                 return a.priority < b.priority; // higher priority first
//             return a.sequence_number > b.sequence_number; // FIFO for same priority
//         };
//         task_queue = std::make_shared<TaskQueue>(priorityComparator);
//     }

//     void TearDown() override
//     {
//         std::optional<Task> t;
//         do
//         {
//             t = task_queue->pop();
//         } while (t.has_value());
//         // task_queue destroyed after all threads, no races
//     }
// };

// TEST_F(TestTaskQueue, PopOnEmptyReturnsNullopt) {
//     auto t = task_queue->pop();
//     EXPECT_FALSE(t.has_value());
// }

// TEST_F(TestTaskQueue, PushPopSingleTask) {
//     auto now = std::chrono::steady_clock::now();
//     Task foo([]{}, 7, 42, now, std::chrono::milliseconds{0}, now, now);

//     task_queue->push(std::move(foo));

//     EXPECT_FALSE(task_queue->empty());
//     EXPECT_EQ(task_queue->size(), 1u);

//     auto t = task_queue->pop();
//     ASSERT_TRUE(t.has_value());
//     EXPECT_EQ(t->priority, 7);
//     EXPECT_EQ(t->sequence_number, 42);

//     EXPECT_TRUE(task_queue->empty());
// }

// TEST_F(TestTaskQueue, PriorityOrdering) {
//     auto now = std::chrono::steady_clock::now();
//     Task low([]{}, 1, 1, now, std::chrono::milliseconds{0}, now, now);
//     Task high([]{}, 99, 2, now, std::chrono::milliseconds{0},now, now);

//     task_queue->push(std::move(low));
//     task_queue->push(std::move(high));

//     auto t1 = task_queue->pop();
//     ASSERT_TRUE(t1.has_value());
//     EXPECT_EQ(t1->priority, 99);

//     auto t2 = task_queue->pop();
//     ASSERT_TRUE(t2.has_value());
//     EXPECT_EQ(t2->priority, 1);
// }

// TEST_F(TestTaskQueue, FirstNoDeadlineOrdering) {
//     auto now = std::chrono::steady_clock::now();
//     Task low([]{}, 3, 1, now, std::chrono::milliseconds{0}, now, std::nullopt);
//     Task high([]{}, 1101, 2, now, std::chrono::milliseconds{0},now, now);

//     task_queue->push(std::move(low));
//     task_queue->push(std::move(high));

//     auto t1 = task_queue->pop();
//     ASSERT_TRUE(t1.has_value());
//     EXPECT_EQ(t1->priority, 1101);

//     auto t2 = task_queue->pop();
//     ASSERT_TRUE(t2.has_value());
//     EXPECT_EQ(t2->priority, 3);
// }

// TEST_F(TestTaskQueue, AddMultipleTasksAndCheckOrdering) {
//     auto now = std::chrono::steady_clock::now();
//     Task task1([]{}, 3, 1, now, std::chrono::milliseconds{0},now, std::nullopt);
//     Task task2([]{}, 91, 2, now, std::chrono::milliseconds{0},now, std::nullopt);
//     Task task3([]{}, 1189, 3, now, std::chrono::milliseconds{0},now, now);
//     Task task4([]{}, 1190, 4, now, std::chrono::milliseconds{0},now, now + std::chrono::milliseconds{1});

//     task_queue->push(std::move(task1));
//     task_queue->push(std::move(task2));
//     task_queue->push(std::move(task3));
//     task_queue->push(std::move(task4));

//     auto t1 = task_queue->pop();
//     ASSERT_TRUE(t1.has_value());
//     EXPECT_EQ(t1->priority, 1190);

//     auto t2 = task_queue->pop();
//     ASSERT_TRUE(t2.has_value());
//     EXPECT_EQ(t2->priority, 1189);

//     auto t3 = task_queue->pop();
//     ASSERT_TRUE(t3.has_value());
//     EXPECT_EQ(t3->priority, 91);

//     auto t4 = task_queue->pop();
//     ASSERT_TRUE(t4.has_value());
//     EXPECT_EQ(t4->priority, 3);
// }

// TEST_F(TestTaskQueue, FifoTieBreak) {
//     auto now = std::chrono::steady_clock::now();
//     Task first(
//         []{ /*…*/ },   // job
//         5,              // priority
//         100,              // sequence number
//         now,            // scheduled_at
//         std::chrono::milliseconds{0},// interval
//         now,            // enqueue_time
//         std::nullopt    // no deadline
//     );

//     Task second(
//         []{ /*…*/ },   // job
//         5,              // priority
//         101,              // sequence number
//         now,            // scheduled_at
//         std::chrono::milliseconds{0},// interval
//         now,            // enqueue_time
//         std::nullopt    // no deadline
//     );

//     task_queue->push(std::move(first));
//     task_queue->push(std::move(second));

//     auto t1 = task_queue->pop();
//     ASSERT_TRUE(t1.has_value());
//     EXPECT_EQ(t1->sequence_number, 100u);

//     auto t2 = task_queue->pop();
//     ASSERT_TRUE(t2.has_value());
//     EXPECT_EQ(t2->sequence_number, 101u);
// }

// TEST_F(TestTaskQueue, PeekDoesNotPop) {
//     auto now = std::chrono::steady_clock::now();
//     Task foo(
//         []{ /*…*/ },   // job
//         3,              // priority
//         7,              // sequence number
//         now,            // scheduled_at
//         std::chrono::milliseconds{0},// interval
//         now,            // enqueue_time
//         std::nullopt    // no deadline
//     );
//     task_queue->push(std::move(foo));

//     auto p = task_queue->peek_time();
//     ASSERT_TRUE(p.has_value());
//     EXPECT_EQ(p->get(), now);

//     EXPECT_EQ(task_queue->size(), 1u);

//     auto t = task_queue->pop();
//     ASSERT_TRUE(t.has_value());
//     EXPECT_EQ(t->sequence_number, 7u);
//     EXPECT_EQ(task_queue->size(), 0u);
// }

// TEST_F(TestTaskQueue, SizeReflectsPushesAndPops) {
//     auto now = std::chrono::steady_clock::now();
//     EXPECT_EQ(task_queue->size(), 0u);
//     for (int i = 0; i < 5; ++i)
//     {
//         Task tmp([]{}, i, i, now, std::chrono::milliseconds{0},now, now);

//         task_queue->push(std::move(tmp));
//         EXPECT_EQ(task_queue->size(), static_cast<size_t>(i + 1));
//     }
//     for (int i = 5; i > 0; --i)
//     {
//         task_queue->pop();
//         EXPECT_EQ(task_queue->size(), static_cast<size_t>(i - 1));
//     }
// }
