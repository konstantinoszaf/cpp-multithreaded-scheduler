#pragma once
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>

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
        std::unique_lock<std::mutex> lk(mtx);
        data_cond.wait(lk, [this] { return !heap.empty() || shutdown_; });

        if (heap.empty() && shutdown_) return;

        std::pop_heap(heap.begin(), heap.end(), cmp);
        value = std::move(heap.back());
        heap.pop_back();
    }

    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(mtx);
        data_cond.wait(lk, [this] { return !heap.empty() || shutdown_; });

        if (heap.empty() && shutdown_) return std::shared_ptr<T>();

        std::pop_heap(heap.begin(), heap.end(), cmp);
        auto res {std::make_shared<T>(std::move(heap.back()))};
        heap.pop_back();
        return res;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(mtx);

        if (heap.empty()) return false;

        std::pop_heap(heap.begin(), heap.end(), cmp);
        value = std::move(heap.back());
        heap.pop_back();

        return true;
    }

    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(mtx);

        if (heap.empty()) return std::shared_ptr<T>();

        std::pop_heap(heap.begin(), heap.end(), cmp);
        auto res {std::make_shared<T>(std::move(heap.back()))};
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
};
} // namespace scheduler::detail