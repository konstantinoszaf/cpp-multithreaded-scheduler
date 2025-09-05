#include <algorithm>
#include <atomic>
#include <future>
#include <gmock/gmock.h>
#include "detail/thread_pool_impl.h"
#include "detail/task.h"
#include <latch>

using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using namespace scheduler::detail;

// Helper to build a Task from a simple lambda
static Task makeTask(std::function<void()> f) {
    auto now = std::chrono::steady_clock::now();
    return Task{
        std::move(f),                    // job
        0u,                              // priority
        0u,                              // sequence_number
        now,                             // scheduled_at
        std::chrono::milliseconds{0},    // interval
        now,                             // enqueue_time
        std::nullopt,                    // no deadline
        false                            // not recurring
    };
}

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = std::make_unique<ThreadPool>(4);
        pool->start();
    }
    void TearDown() override {
        pool->stop();
    }

    std::unique_ptr<ThreadPool> pool;
};

struct MockTask {
    MOCK_METHOD(void, run, ());
};

TEST_F(ThreadPoolTest, MockCallbackSetsPromise) {
    std::promise<void> p;
    auto f = p.get_future();

    MockTask m;
    EXPECT_CALL(m, run())
        .WillOnce(InvokeWithoutArgs([&]{ p.set_value(); }));

    // now wrap it into a Task
    ASSERT_TRUE(pool->submit(makeTask([&]{ m.run(); })));

    ASSERT_EQ(std::future_status::ready,
              f.wait_for(std::chrono::seconds(1)));
}

struct CounterMock {
    MOCK_METHOD(void, inc, (std::atomic<int>&));
};

TEST_F(ThreadPoolTest, MultipleMockTasksInvokeCallback) {
    CounterMock m;
    std::atomic<int> counter{0};
    constexpr int N = 10;

    EXPECT_CALL(m, inc(_))
        .Times(N)
        .WillRepeatedly(Invoke([&counter](std::atomic<int>& c){
            c.fetch_add(1, std::memory_order_relaxed);
        }));

    for (int i = 0; i < N; ++i) {
        ASSERT_TRUE(pool->submit(makeTask([&]{ m.inc(counter); })));
    }

    // Wait until counter == N
    std::promise<void> allDone;
    std::thread waiter([&]{
        while (counter.load(std::memory_order_relaxed) < N) {}
        allDone.set_value();
    });

    ASSERT_EQ(std::future_status::ready,
              allDone.get_future().wait_for(std::chrono::seconds(1)));
    waiter.join();
}

struct ExceptionMock {
    MOCK_METHOD(void, boom, ());
    MOCK_METHOD(void, safe, ());
};

TEST_F(ThreadPoolTest, ExceptionInOneTaskDoesNotBlockOthers) {
    ExceptionMock m;

    EXPECT_CALL(m, boom())
        .WillOnce(InvokeWithoutArgs([]{ throw std::runtime_error("fail"); }));

    std::promise<void> p;
    EXPECT_CALL(m, safe())
        .WillOnce(InvokeWithoutArgs([&]{ p.set_value(); }));

    ASSERT_TRUE(pool->submit(makeTask([&]{ m.boom(); })));
    ASSERT_TRUE(pool->submit(makeTask([&]{ m.safe(); })));

    ASSERT_EQ(std::future_status::ready,
              p.get_future().wait_for(std::chrono::seconds(1)));
}

struct NeverCalledMock {
    MOCK_METHOD(void, shouldNotRun, ());
};

TEST_F(ThreadPoolTest, NoTasksRunAfterStop) {
    NeverCalledMock m;
    EXPECT_CALL(m, shouldNotRun()).Times(0);

    pool->stop();
    EXPECT_FALSE(pool->submit(makeTask([&]{ m.shouldNotRun(); })));
}

struct DrainMock {
    MOCK_METHOD(void, run, ());
};

TEST_F(ThreadPoolTest, PendingTasksDrainedOnShutdown) {
    DrainMock m;
    constexpr int N = 5;

    EXPECT_CALL(m, run()).Times(N);

    std::latch start{1}, done{N};
    for (int i = 0; i < N; ++i) {
        ASSERT_TRUE(pool->submit(makeTask([&]{
            start.wait();
            m.run();
            done.count_down();
        })));
    }

    start.count_down();
    pool->stop();
    done.wait();
}
