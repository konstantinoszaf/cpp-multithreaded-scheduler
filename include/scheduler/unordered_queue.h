#pragma once
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace scheduler::queue {

template<typename T>
class UnorderedQueue {
private:
    mutable std::mutex mtx;
    std::deque<T> queue;
    std::condition_variable data_cond;
    bool shutdown_;
    std::atomic<int> approx_size;
public:
    UnorderedQueue() : shutdown_{false}, approx_size{0} {};

    UnorderedQueue(UnorderedQueue const& other) {
        std::lock_guard<std::mutex> lk(other.mtx);
        queue = other.queue;
        shutdown_ = other.shutdown_;
    }

    void push(T&& t) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (shutdown_) return;
            queue.emplace_back(std::move(t));
        }
        approx_size.fetch_add(1, std::memory_order_relaxed);
        data_cond.notify_one();
    }

    bool wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lk(mtx);
        data_cond.wait(lk, [this] { return !queue.empty() || shutdown_; });

        if (queue.empty() && shutdown_) return false;

        value = std::move(queue.front());
        queue.pop_front();
        approx_size.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    std::optional<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(mtx);
        data_cond.wait(lk, [this] { return !queue.empty() || shutdown_; });

        if (queue.empty() && shutdown_) return std::nullopt;

        std::optional<T> res{std::in_place, std::move(queue.front())};
        queue.pop_front();
        approx_size.fetch_sub(1, std::memory_order_relaxed);

        return res;
    }

    std::optional<T> wait_and_pop_for(std::chrono::microseconds ttl) {
        std::unique_lock<std::mutex> lk(mtx);
        if (!data_cond.wait_for(lk, ttl, [this] { return !queue.empty() || shutdown_; }))
            return std::nullopt;

        if (queue.empty() && shutdown_) return std::nullopt;

        std::optional<T> res{std::in_place, std::move(queue.front())};
        queue.pop_front();
        approx_size.fetch_sub(1, std::memory_order_relaxed);

        return res;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(mtx);

        if (queue.empty()) return false;

        value = std::move(queue.front());
        queue.pop_front();
        approx_size.fetch_sub(1, std::memory_order_relaxed);

        return true;
    }

    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lk(mtx);

        if (queue.empty()) return std::nullopt;

        std::optional<T> res{std::in_place, std::move(queue.front())};
        queue.pop_front();
        approx_size.fetch_sub(1, std::memory_order_relaxed);

        return res;
    }

    bool try_pop_batch(size_t n, std::vector<T>& out) {
        std::lock_guard<std::mutex> lk(mtx);

        if (queue.empty()) return false;
        n = std::min(n, queue.size());
        for (size_t i = 0; i < n; ++i) {
            out.emplace_back(std::move(queue.front()));
            queue.pop_front();
        }

        int prev = approx_size.fetch_sub(n, std::memory_order_relaxed);
        if (prev < (int)n) approx_size.store(0, std::memory_order_relaxed);
        return true;
    }

    bool try_steal_batch(size_t n, std::vector<T>& out) {
        if (approx_size.load(std::memory_order_relaxed) <= 1) return false;

        if (!mtx.try_lock()) return false;
        std::lock_guard<std::mutex> lk(mtx, std::adopt_lock);

        if (queue.empty()) return false;
        n = std::min(n, queue.size() / 2);
        for (size_t i = 0; i < n; ++i) {
            out.emplace_back(std::move(queue.back()));
            queue.pop_back();
        }

        int prev = approx_size.fetch_sub((int)n, std::memory_order_relaxed);
        if (prev < (int)n) approx_size.store(0, std::memory_order_relaxed);
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        return queue.empty();
    }

    void shutdown() {
        {
            std::lock_guard lk(mtx);
            shutdown_ = true;
        }

        data_cond.notify_all();
    }
};
} // namespace scheduler::detail