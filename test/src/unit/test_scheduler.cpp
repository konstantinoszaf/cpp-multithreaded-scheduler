#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "scheduler/scheduler.h"
#include "mock_statistics_calculator.h"
#include "mock_task_queue.h"
#include "mock_thread_pool.h"

using namespace scheduler;
using namespace scheduler::detail;

class TestableScheduler : public Scheduler {
public:
    // Expose the injection constructor explicitly
    TestableScheduler(std::shared_ptr<detail::ITaskQueue> ready,
                     std::shared_ptr<detail::ITaskQueue> scheduled,
                     std::shared_ptr<detail::IThreadPool> thread_pool,
                     std::shared_ptr<detail::IStatisticsCalculator> stats)
      : Scheduler(ready, scheduled, thread_pool, stats) {}

    // Inherit default constructor
    using Scheduler::Scheduler;
    using Scheduler::dispatchLoop;
    using Scheduler::stop;
    using Scheduler::running;
    using Scheduler::calculatePriority;

    void setRunning(bool v) {
        running.store(v, std::memory_order_release);
    }
    // Expose dispatchLoop for testing
    void runDispatchLoop() {
        // Stop background thread if running
        stop();
        dispatchLoop();
    }
};

class TestScheduler : public ::testing::Test {
protected:
    std::shared_ptr<MockTaskQueue> mockScheduled;
    std::shared_ptr<MockTaskQueue> mockRecurring;
    std::shared_ptr<MockThreadPool> mockThreadPool;
    std::shared_ptr<MockStatisticsCalculator> mockStats;
    std::unique_ptr<TestableScheduler> sched;

    void SetUp() override {
        mockScheduled = std::make_shared<MockTaskQueue>();
        mockRecurring = std::make_shared<MockTaskQueue>();
        mockThreadPool = std::make_shared<MockThreadPool>();
        mockStats = std::make_shared<MockStatisticsCalculator>();

        // Default behaviors
        ON_CALL(*mockThreadPool, enqueue(testing::_)).WillByDefault(testing::Return(true));
        ON_CALL(*mockStats, getLatencyStatistics()).WillByDefault(testing::Return(std::make_tuple(0.0,0.0,0.0)));

        sched = std::make_unique<TestableScheduler>(
            mockScheduled,
            mockRecurring,
            mockThreadPool,
            mockStats
        );

        // Prevent running background dispatch thread during unit tests
        sched->stop();
    }

    void TearDown() override {
        sched.reset();
    }
};

// Tests
TEST_F(TestScheduler, EnqueueOneOffTaskPushesReadyQueue) {
    EXPECT_CALL(*mockThreadPool, stop).Times(::testing::AtLeast(1));
    EXPECT_CALL(*mockScheduled, push(testing::_));
    EXPECT_CALL(*mockRecurring, push(testing::_)).Times(0);
    sched->schedule([](){}, 5, std::nullopt);
}

TEST_F(TestScheduler, RecurringTaskPushesReadyOnce) {
    EXPECT_CALL(*mockThreadPool, stop).Times(::testing::AtLeast(1));
    EXPECT_CALL(*mockScheduled, push(testing::_)).Times(1);
    EXPECT_CALL(*mockRecurring, push(testing::_)).Times(0);
    sched->scheduleRecurring([](){}, 1, std::chrono::milliseconds{100});
}

TEST_F(TestScheduler, DispatchLoopSchedulesRecurringTask) {
    // Enable dispatch loop
    sched->setRunning(true);

    EXPECT_CALL(*mockThreadPool, stop).Times(::testing::AtLeast(1));

    // Prepare a single recurring Task in the scheduled queue
    Task recurringTask{
        []() {},                    // job
        5,                           // priority
        1,                           // sequence_number
        std::chrono::steady_clock::now(), // scheduled_at (now)
        std::chrono::milliseconds{100},   // interval
        std::chrono::steady_clock::now(), // enqueue_time
        std::nullopt                // no deadline
    };

    // Mock ready.empty() and pop() behavior
    EXPECT_CALL(*mockScheduled, empty())
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(false))
        .WillRepeatedly(testing::Return(true));

    EXPECT_CALL(*mockScheduled, pop())
        .WillOnce(testing::Return(std::optional<Task>(recurringTask)));
    // We expect the recurring_queue's push method to be called once
    EXPECT_CALL(*mockRecurring, push(testing::_)).Times(1);
    // We expect dispatch to call thread_pool.enqueue once
    EXPECT_CALL(*mockThreadPool, enqueue(testing::_)).Times(1);

    sched->runDispatchLoop();
}

TEST_F(TestScheduler, BoostsWhenDeadlineIsImminent) {
    using namespace std::chrono;

    // Arrange
    // priority to test and a deadline exactly at the imminent_time threshold (10ms)
    const int priority = 50;
    const auto deadline = std::chrono::steady_clock::now() + milliseconds{10};

    // Act
    uint64_t result = sched->calculatePriority(priority, deadline);

    // Assert
    // BASE_MAX = 100, BOOST = 1000, so we expect 100 + 1000 + 50 = 1150
    EXPECT_EQ(result, 1150u);
}

TEST_F(TestScheduler, DoesNotBoostsWhenDeadlineIsImminent) {
    using namespace std::chrono;

    // Arrange
    // priority to test and a deadline exactly at the imminent_time threshold (10ms)
    const int priority = 50;
    const auto deadline = std::chrono::steady_clock::now() + milliseconds{11};

    // Act
    uint64_t result = sched->calculatePriority(priority, deadline);

    // Assert
    // BASE_MAX = 100, BOOST = 1000, so we expect 100 + 1000 + 50 = 1150
    EXPECT_EQ(result, 50u);
}