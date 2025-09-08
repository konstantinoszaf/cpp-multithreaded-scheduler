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
    std::queue<T> queue;
    std::condition_variable data_cond;
    bool shutdown_;

public:
    UnorderedQueue() : shutdown_{false} {};

    UnorderedQueue(UnorderedQueue const& other) {
        std::lock_guard<std::mutex> lk(other.mtx);
        queue = other.queue;
        shutdown_ = other.shutdown_;
    }

    void push(T&& t) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (shutdown_) return;
            queue.emplace(std::move(t));
        }
        data_cond.notify_one();
    }

    bool wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lk(mtx);
        data_cond.wait(lk, [this] { return !queue.empty() || shutdown_; });

        if (queue.empty() && shutdown_) return false;

        value = std::move(queue.front());
        queue.pop();
        return true;
    }

    std::optional<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(mtx);
        data_cond.wait(lk, [this] { return !queue.empty() || shutdown_; });

        if (queue.empty() && shutdown_) return std::nullopt;

        std::optional<T> res{std::in_place, std::move(queue.front())};
        queue.pop();

        return res;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(mtx);

        if (queue.empty()) return false;

        value = std::move(queue.front());
        queue.pop();

        return true;
    }

    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lk(mtx);

        if (queue.empty()) return std::nullopt;

        std::optional<T> res{std::in_place, std::move(queue.front())};
        queue.pop();

        return res;
    }

    bool try_pop_batch(size_t n, std::vector<T>& out) {
        std::lock_guard<std::mutex> lk(mtx);

        if (queue.empty()) return false;
        n = std::min(n, queue.size());
        for (size_t i = 0; i < n; ++i) {
            out.emplace_back(std::move(queue.front()));
            queue.pop();
        }
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