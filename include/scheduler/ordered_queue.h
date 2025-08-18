#pragma once
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <chrono>

namespace scheduler::queue {

template<typename T, typename Compare = std::less<T>>
class OrderedQueue {
private:
    mutable std::mutex mtx;
    std::vector<T> heap;
    std::condition_variable data_cond;
    Compare cmp;
    bool shutdown_;

public:
    OrderedQueue() : shutdown_{false} {};

    OrderedQueue(OrderedQueue const& other) {
        std::lock_guard<std::mutex> lk(other.mtx);
        heap = other.heap;
        shutdown_ = other.shutdown_;
    }

    template<class U>
    void push(U&& new_value) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (shutdown_) return;
            heap.push_back(std::forward<U>(new_value));
            std::push_heap(heap.begin(), heap.end(), cmp);
        }
        data_cond.notify_one();
    }

    void wait_and_pop(T& value) {
        while (true) {
            std::unique_lock<std::mutex> lk(mtx);
            if (heap.empty() && shutdown_) return;
    
            if (heap.empty()) {
                data_cond.wait(lk, [this] { return !heap.empty() || shutdown_; });
                continue;
            }

            auto &next = heap.front();
            auto now = std::chrono::steady_clock::now();

            if (next.scheduled_at > now) {
                data_cond.wait_until(lk, next.scheduled_at, [&] {
                    return heap.front().scheduled_at < next.scheduled_at || shutdown_; }
                );
                continue;
            }

            std::pop_heap(heap.begin(), heap.end(), cmp);
            value = std::move(heap.back());
            heap.pop_back();
        }

        return;
    }

    T wait_and_pop() {
        while (true) {
            std::unique_lock<std::mutex> lk(mtx);
            if (heap.empty() && shutdown_) return {};
    
            if (heap.empty()) {
                data_cond.wait(lk, [this] { return !heap.empty() || shutdown_; });
                continue;
            }

            auto &next = heap.front();
            auto now = std::chrono::steady_clock::now();

            if (next.scheduled_at > now) {
                data_cond.wait_until(lk, next.scheduled_at, [&] {
                    return heap.front().scheduled_at < next.scheduled_at || shutdown_; }
                );
                continue;
            }

            std::pop_heap(heap.begin(), heap.end(), cmp);
            auto res {std::move(heap.back())};
            heap.pop_back();
            return res;
        }

        return {};
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(mtx);

        if (heap.empty()) return false;

        auto &next = heap.front();
        auto now = std::chrono::steady_clock::now();
        if (next.scheduled_at > now) return false;

        std::pop_heap(heap.begin(), heap.end(), cmp);
        value = std::move(heap.back());
        heap.pop_back();

        return true;
    }

    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lk(mtx);

        if (heap.empty()) return std::nullopt;

        auto &next = heap.front();
        auto now = std::chrono::steady_clock::now();
        if (next.scheduled_at > now) return std::nullopt;

        std::pop_heap(heap.begin(), heap.end(), cmp);
        auto res {std::move(heap.back())};
        heap.pop_back();

        return res;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx);
        return heap.empty();
    }

    void shutdown() {
        {
            std::lock_guard lk(mtx);
            shutdown_ = true;
        }

        data_cond.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(mtx);
        return heap.size();
    }
};
} // namespace scheduler::detail