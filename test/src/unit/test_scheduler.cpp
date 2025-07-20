#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "scheduler/scheduler.h"
#include "mock_clock.h"
#include "mock_statistics_calculator.h"
#include "mock_task_queue.h"
#include "mock_thread_pool.h"

using namespace scheduler;
using namespace scheduler::detail;

class TestableScheduler : public Scheduler {
public:
    // Expose the injection constructor explicitly
    TestableScheduler(std::shared_ptr<detail::IClock> clock,
                     std::shared_ptr<detail::ITaskQueue> ready,
                     std::shared_ptr<detail::ITaskQueue> scheduled,
                     std::shared_ptr<detail::IThreadPool> thread_pool,
                     std::shared_ptr<detail::IStatisticsCalculator> stats)
      : Scheduler(clock, ready, scheduled, thread_pool, stats) {}

    // Inherit default constructor
    using Scheduler::Scheduler;
    using Scheduler::dispatchLoop;
    using Scheduler::stop;

    // Expose dispatchLoop for synchronous testing
    void runDispatchLoop() {
        // Stop background thread if running
        stop();
        // Run a single dispatch loop iteration
        dispatchLoop();
    }
};

class TestScheduler : public ::testing::Test {
protected:
    std::shared_ptr<MockClock> mockClock;
    std::shared_ptr<MockTaskQueue> mockReady;
    std::shared_ptr<MockTaskQueue> mockScheduled;
    std::shared_ptr<MockThreadPool> mockThreadPool;
    std::shared_ptr<MockStatisticsCalculator> mockStats;
    std::unique_ptr<TestableScheduler> sched;

    void SetUp() override {
        mockClock = std::make_shared<MockClock>();
        mockReady = std::make_shared<MockTaskQueue>();
        mockScheduled = std::make_shared<MockTaskQueue>();
        mockThreadPool = std::make_shared<MockThreadPool>();
        mockStats = std::make_shared<MockStatisticsCalculator>();

        // Default behaviors
        ON_CALL(*mockThreadPool, enqueue(testing::_)).WillByDefault(testing::Return(true));
        ON_CALL(*mockStats, getLatencyStatistics()).WillByDefault(testing::Return(std::make_tuple(0.0,0.0,0.0)));

        sched = std::make_unique<TestableScheduler>(
            mockClock,
            mockReady,
            mockScheduled,
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
    EXPECT_CALL(*mockReady, push(testing::_));
    EXPECT_CALL(*mockScheduled, push(testing::_)).Times(0);
    sched->schedule([](){}, 5, std::nullopt);
}

TEST_F(TestScheduler, RecurringTaskPushesReadyOnce) {
    EXPECT_CALL(*mockReady, push(testing::_)).Times(1);
    EXPECT_CALL(*mockScheduled, push(testing::_)).Times(0);
    sched->scheduleRecurring([](){}, 1, std::chrono::milliseconds{100});
}
