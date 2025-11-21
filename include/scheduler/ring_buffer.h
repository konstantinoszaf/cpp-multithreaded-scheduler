#pragma once
#include <vector>
#include <stdexcept>

namespace scheduler::queue {
template<typename T>
class RingBuffer {
    std::vector<T> data;
    std::size_t front_idx;
    std::size_t back_idx;
    std::size_t capacity;

public:
    RingBuffer() = delete;
    explicit RingBuffer(std::size_t capacity_) : capacity{capacity_ + 1} { 
        if (capacity == 0)
            throw std::runtime_error("Maximum capacity reached");

        front_idx = 0;
        back_idx = 0;
        data.resize(capacity);
    }

    RingBuffer(const RingBuffer& other)
        : data(other.data)
        , front_idx(other.front_idx)
        , back_idx(other.back_idx)
        , capacity(other.capacity)
    {}

    RingBuffer(RingBuffer&& other) noexcept
        : data(std::move(other.data))
        , front_idx(other.front_idx)
        , back_idx(other.back_idx)
        , capacity(other.capacity)
    {
        other.front_idx = 0;
        other.back_idx = 0;
        other.capacity = 0;
    }

    RingBuffer& operator=(const RingBuffer& other) {
        if (this != &other) {
            data = other.data;
            front_idx = other.front_idx;
            back_idx = other.back_idx;
            capacity = other.capacity;
        }
        return *this;
    }

    RingBuffer& operator=(RingBuffer&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            front_idx = other.front_idx;
            back_idx = other.back_idx;
            capacity = other.capacity;

            other.front_idx = 0;
            other.back_idx = 0;
            other.capacity = 0;
        }
        return *this;
    }

    std::size_t size() const noexcept {
        if (back_idx >= front_idx) {
            return back_idx - front_idx;
        }

        return capacity - (front_idx - back_idx);
    }

    bool empty() const noexcept {
        return front_idx == back_idx;
    }

    bool full() const noexcept {
        return (back_idx + 1) % capacity == front_idx;
    }

    bool push_back(T val) {
        if (full()) return false;
        
        data[back_idx] = std::move(val);
        back_idx = (back_idx + 1) % capacity;
        return true;
    }

    // precondition: !empty(), undefined behavior otherwise
    T& back() noexcept {
        std::size_t idx = (back_idx + capacity - 1) % capacity;
        return data[idx];
    }

    // precondition: !empty(), undefined behavior otherwise
    const T& back() const noexcept {
        std::size_t idx = (back_idx + capacity - 1) % capacity;
        return data[idx];
    }

    bool pop_back() noexcept {
        if (empty()) return false;
        back_idx = (back_idx + capacity - 1) % capacity;
        return true;
    }

    // precondition: !empty(), undefined behavior otherwise
    T& front() noexcept {
        return data[front_idx];
    }

    // precondition: !empty(), undefined behavior otherwise
    const T& front() const noexcept {
        return data[front_idx];
    }

    bool pop_front() noexcept {
        if (empty()) return false;
        front_idx = (front_idx + 1) % capacity;
        return true;
    }
};
} // namespace scheduler::queue