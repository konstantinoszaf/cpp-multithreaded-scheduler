#include "detail/thread_pool.h"
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace scheduler::detail {
class ThreadPool : public IThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();
    bool submit(std::function<void()> job) override;
    void start() override;
    void stop() override;
    size_t threadCount() const noexcept;
private:
    void workerLoop();

    std::vector<std::thread> threads;
    std::deque<std::function<void()>> jobs;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool running;
    size_t thread_num;
};
} // namespace scheduler::detail