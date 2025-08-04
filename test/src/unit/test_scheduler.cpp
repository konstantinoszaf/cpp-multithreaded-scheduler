#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "scheduler/scheduler.h"
#include "mock_statistics_calculator.h"
#include "mock_task_queue.h"
#include "mock_thread_pool.h"
#include "detail/task.h"

using namespace scheduler;
using namespace scheduler::detail;

class TestableScheduler : public Scheduler {
public:
    // Expose the injection constructor explicitly
    TestableScheduler(std::unique_ptr<detail::IThreadPool> thread_pool,
                     std::unique_ptr<detail::IStatisticsCalculator> stats)
      : Scheduler(std::move(thread_pool), std::move(stats)) {}

    // Inherit default constructor
    using Scheduler::Scheduler;
    using Scheduler::calculatePriority;

};

class TestScheduler : public ::testing::Test {
protected:
    std::unique_ptr<MockThreadPool> mockThreadPool;
    std::unique_ptr<MockStatisticsCalculator> mockStats;
    std::unique_ptr<TestableScheduler> sched;
    MockThreadPool* rawThreadPool;
    MockStatisticsCalculator* rawStats;

    void SetUp() override {
        mockThreadPool = std::make_unique<MockThreadPool>();
        mockStats = std::make_unique<MockStatisticsCalculator>();

        rawThreadPool = mockThreadPool.get();
        rawStats = mockStats.get();

        // Default behaviors
        ON_CALL(*mockThreadPool, submit(testing::_)).WillByDefault(testing::Return(true));
        ON_CALL(*mockStats, getLatencyStatistics()).WillByDefault(testing::Return(std::make_tuple(0.0,0.0,0.0)));

        sched = std::make_unique<TestableScheduler>(
            std::move(mockThreadPool),
            std::move(mockStats)
        );
    }

    void TearDown() override {
        sched.reset();
    }
};

// Tests
TEST_F(TestScheduler, EnqueueOneOffTaskPushesReadyQueue) {
    EXPECT_CALL(*rawThreadPool, stop).Times(::testing::AtLeast(1));
    EXPECT_CALL(*rawThreadPool, submit(testing::_)).Times(1);

    sched->schedule([](){}, 5, std::nullopt);
}

// TEST_F(TestScheduler, RecurringTaskPushesReadyOnce) {
//     EXPECT_CALL(*rawThreadPool, stop).Times(::testing::AtLeast(1));
//     sched->scheduleRecurring([](){}, 1, std::chrono::milliseconds{100});
// }

// TEST_F(TestScheduler, DispatchLoopSchedulesRecurringTask) {

//     EXPECT_CALL(*rawThreadPool, stop).Times(::testing::AtLeast(1));
//     // We expect dispatch to call thread_pool.enqueue once

// }

TEST_F(TestScheduler, BoostsWhenDeadlineIsImminent) {
    using namespace std::chrono;

    // Arrange
    // priority to test and a deadline exactly at the imminent_time threshold (10ms)
    const int priority = 50;
    const auto deadline = std::chrono::steady_clock::now() + microseconds{10};

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
    const auto deadline = std::chrono::steady_clock::now() + microseconds{1000};

    // Act
    uint64_t result = sched->calculatePriority(priority, deadline);

    // Assert
    // BASE_MAX = 100, BOOST = 1000, so we expect 100 + 1000 + 50 = 1150
    EXPECT_EQ(result, 50u);
}