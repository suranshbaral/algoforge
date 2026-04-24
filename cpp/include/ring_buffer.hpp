#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include "candle.hpp"

namespace algoforge {

template<std::size_t Capacity>
class RingBuffer {
public:
    RingBuffer() : head_(0), tail_(0), size_(0) {}

    // Push a candle — overwrites oldest if full
    void push(const Candle& candle) {
        buffer_[head_] = candle;
        head_ = (head_ + 1) % Capacity;

        if (size_ == Capacity) {
            // Buffer full — overwrite, advance tail
            tail_ = (tail_ + 1) % Capacity;
        } else {
            size_++;
        }
    }

    // Pop oldest candle
    Candle pop() {
        if (empty()) {
            throw std::runtime_error("RingBuffer is empty");
        }
        Candle c = buffer_[tail_];
        tail_ = (tail_ + 1) % Capacity;
        size_--;
        return c;
    }

    // Peek without consuming
    const Candle& peek() const {
        if (empty()) {
            throw std::runtime_error("RingBuffer is empty");
        }
        return buffer_[tail_];
    }

    bool empty() const { return size_ == 0; }
    bool full()  const { return size_ == Capacity; }
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return Capacity; }

private:
    std::array<Candle, Capacity> buffer_;
    std::size_t head_;   // next write position
    std::size_t tail_;   // next read position
    std::size_t size_;   // current number of elements
};

} // namespace algoforge