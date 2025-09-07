#include "detail/thread_pool.h"
#include "scheduler/ordered_queue.h"
#include "scheduler/unordered_queue.h"
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

namespace scheduler::detail {

struct Task;

class ThreadPool : public IThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();
    bool submit(Task&& job) override;
    void start() override;
    void stop() override;
    size_t threadCount() const noexcept;
private:
    void workerLoop(size_t index);
    void promote_tasks();

    std::vector<std::thread> threads;
    scheduler::queue::OrderedQueue<Task, std::less<Task>> scheduled;
    std::vector<std::unique_ptr<scheduler::queue::UnorderedQueue<Task>>> ready_queues;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> running;
    size_t thread_num;
    std::thread recurring;
};
} // namespace scheduler::detail