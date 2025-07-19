#include <algorithm>
#include <atomic>
#include <future>
#include <gmock/gmock.h>
#include "detail/thread_pool_impl.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using namespace scheduler::detail;

class ThreadPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        pool = std::make_unique<ThreadPool>(4);
        pool->start();
    }
    void TearDown() override
    {
        pool->stop();
    }

    std::unique_ptr<ThreadPool> pool;
};

class CountDownLatch
{
public:
    // Construct with an initial count
    explicit CountDownLatch(int count)
        : count_{count}
    {
    }

    // Decrement the count. When it reaches zero, wake all waiters.
    void count_down()
    {
        std::lock_guard<std::mutex> lk{m_};
        if (count_ > 0 && --count_ == 0)
        {
            cv_.notify_all();
        }
    }

    // Block until count_ reaches zero.
    void wait()
    {
        std::unique_lock<std::mutex> lk{m_};
        cv_.wait(lk, [&]
                 { return count_ == 0; });
    }

private:
    std::mutex m_;
    std::condition_variable cv_;
    int count_;
};

struct MockTask
{
    MOCK_METHOD(void, run, ());
};

TEST_F(ThreadPoolTest, MockCallbackSetsPromise)
{
    std::promise<void> p;
    auto f = p.get_future();

    MockTask m;
    EXPECT_CALL(m, run())
        // when run() is invoked, fulfill the promise
        .WillOnce(InvokeWithoutArgs([&]
                                    { p.set_value(); }));

    // submit a lambda that calls our mock
    ASSERT_TRUE(pool->submit([&]
                             { m.run(); }));

    // wait (up to 1s) for the callback to fire
    ASSERT_EQ(std::future_status::ready,
              f.wait_for(std::chrono::seconds(1)));
}

struct CounterMock
{
    MOCK_METHOD(void, inc, (std::atomic<int> &));
};

TEST_F(ThreadPoolTest, MultipleMockTasksInvokeCallback)
{
    CounterMock m;
    std::atomic<int> counter{0};
    constexpr int N = 10;

    // Each call to inc(counter) will atomically ++counter
    EXPECT_CALL(m, inc(::testing::_))
        .Times(N)
        .WillRepeatedly(Invoke(
            [&counter](std::atomic<int> &c)
            { c.fetch_add(1, std::memory_order_relaxed); }));

    // Submit N tasks that call m.inc(counter)
    for (int i = 0; i < N; ++i)
    {
        ASSERT_TRUE(pool->submit([&]
                                 { m.inc(counter); }));
    }

    // Wait until counter == N
    // Use a promise/future as a “gate”
    std::promise<void> allDone;
    std::thread waiter([&]
                       {
    while (counter.load(std::memory_order_relaxed) < N) {}
    allDone.set_value(); });

    ASSERT_EQ(std::future_status::ready,
              allDone.get_future().wait_for(std::chrono::seconds(1)));
    waiter.join();
}

struct ExceptionMock
{
    MOCK_METHOD(void, boom, ());
    MOCK_METHOD(void, safe, ());
};

TEST_F(ThreadPoolTest, ExceptionInOneTaskDoesNotBlockOthers)
{
    ExceptionMock m;

    // First task throws
    EXPECT_CALL(m, boom())
        .WillOnce(InvokeWithoutArgs([]
                                    { throw std::runtime_error("fail"); }));

    // Second task should still run and fulfill the promise
    std::promise<void> p;
    EXPECT_CALL(m, safe())
        .WillOnce(InvokeWithoutArgs([&]
                                    { p.set_value(); }));

    ASSERT_TRUE(pool->submit([&]{ m.boom(); }));
    ASSERT_TRUE(pool->submit([&]{ m.safe(); }));

    ASSERT_EQ(std::future_status::ready,
              p.get_future().wait_for(std::chrono::seconds(1)));
}

struct NeverCalledMock
{
    MOCK_METHOD(void, shouldNotRun, ());
};

TEST_F(ThreadPoolTest, NoTasksRunAfterStop)
{
    NeverCalledMock m;
    EXPECT_CALL(m, shouldNotRun()).Times(0);

    pool->stop();
    // submit returns false, so mock is never invoked
    EXPECT_FALSE(pool->submit([&]{ m.shouldNotRun(); }));
}

struct DrainMock
{
    MOCK_METHOD(void, run, ());
};

TEST_F(ThreadPoolTest, PendingTasksDrainedOnShutdown)
{
    DrainMock m;
    constexpr int N = 5;

    // Expect run() N times, since we drain everything
    EXPECT_CALL(m, run()).Times(N);

    // Block tasks on a latch
    CountDownLatch start{1}, done{N};
    for (int i = 0; i < N; ++i)
    {
        ASSERT_TRUE(pool->submit([&] {
            start.wait();
            m.run();
            done.count_down(); }));
    }

    // Let them go and then stop (which will wait for them)
    start.count_down();
    pool->stop();

    // Wait for all of them to finish
    done.wait();
}
