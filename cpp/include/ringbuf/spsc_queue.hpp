#pragma once

#include <atomic>
#include <array>
#include <optional>
#include <cstddef>

namespace ringbuf {

/**
 * @brief Single-Producer Single-Consumer lock-free queue
 *
 * A thread-safe, lock-free queue implementation for single producer and
 * single consumer scenarios using C++17 features.
 *
 * @tparam T The type of elements stored in the queue
 * @tparam Capacity Maximum number of elements the queue can hold
 */
template <typename T, std::size_t Capacity>
class SPSCQueue {
public:
    static_assert(Capacity > 0, "Queue capacity must be greater than 0");
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Queue capacity must be a power of 2");

    SPSCQueue() : head_(0), tail_(0) {}

    /**
     * @brief Try to enqueue an element
     *
     * @param value The value to enqueue
     * @return true if successful, false if queue is full
     */
    bool try_enqueue(const T& value) {
        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto next_tail = increment(current_tail);

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }

        buffer_[current_tail] = value;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Try to enqueue an element (move semantics)
     *
     * @param value The value to enqueue
     * @return true if successful, false if queue is full
     */
    bool try_enqueue(T&& value) {
        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto next_tail = increment(current_tail);

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }

        buffer_[current_tail] = std::move(value);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Try to dequeue an element
     *
     * @return std::optional containing the value if successful, std::nullopt if queue is empty
     */
    std::optional<T> try_dequeue() {
        const auto current_head = head_.load(std::memory_order_relaxed);

        if (current_head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt; // Queue is empty
        }

        T value = std::move(buffer_[current_head]);
        head_.store(increment(current_head), std::memory_order_release);
        return value;
    }

    /**
     * @brief Check if the queue is empty
     *
     * @return true if empty, false otherwise
     */
    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get approximate size of the queue
     *
     * @return Approximate number of elements in the queue
     */
    std::size_t size() const {
        const auto head = head_.load(std::memory_order_acquire);
        const auto tail = tail_.load(std::memory_order_acquire);
        return (tail >= head) ? (tail - head) : (Capacity - head + tail);
    }

private:
    static constexpr std::size_t increment(std::size_t idx) {
        return (idx + 1) & (Capacity - 1);
    }

    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
    std::array<T, Capacity> buffer_;
};

} // namespace ringbuf
