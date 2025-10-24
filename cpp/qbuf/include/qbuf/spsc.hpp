#ifndef QBUF_SPSC_HPP
#define QBUF_SPSC_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>
#include <thread>

namespace qbuf {

/**
 * @brief Single-Producer Single-Consumer lock-free queue
 *
 * A thread-safe, lock-free queue implementation for single producer and
 * single consumer scenarios.
 *
 * @tparam T The type of elements stored in the queue
 * @tparam Capacity Maximum number of elements the queue can hold
 */
template <typename T, std::size_t Capacity>
class SPSC {
public:
    static_assert(Capacity > 0, "Queue capacity must be greater than 0");
    static_assert((Capacity & (Capacity - 1)) == 0, "Queue capacity must be a power of 2");

    SPSC()
        : head_(0)
        , tail_(0) { }

    // non-copyable
    SPSC(const SPSC&) = delete;
    SPSC& operator=(const SPSC&) = delete;
    // non-movable
    SPSC(SPSC&&) = delete;
    SPSC& operator=(SPSC&&) = delete;

    /**
     * @brief Check if the queue is empty
     *
     * @return true if empty, false otherwise
     */
    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
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

    /**
     * @brief Producer-side handle for SPSC queue
     *
     * Provides a restricted interface exposing only enqueue operations and utility methods.
     * This handle is intended for use by the single producer thread.
     */
    class Sink {
    public:
        Sink(SPSC<T, Capacity>& queue)
            : queue_(queue) { }

        // Non-copyable. Move constructible (reference is re-bound to the same queue)
        Sink(const Sink&) = delete;
        Sink& operator=(const Sink&) = delete;
        Sink(Sink&&) = default;
        // Not move-assignable due to reference member
        Sink& operator=(Sink&&) = delete;

        /**
         * @brief Try to enqueue a single element
         *
         * @param value The value to enqueue
         * @return true if successful, false if queue is full
         */
        bool try_enqueue(const T& value) { return queue_.try_enqueue(value); }

        /**
         * @brief Try to enqueue a single element (move semantics)
         *
         * @param value The value to enqueue
         * @return true if successful, false if queue is full
         */
        bool try_enqueue(T&& value) { return queue_.try_enqueue(std::move(value)); }

        /**
         * @brief Try to enqueue multiple elements
         *
         * @param data Pointer to array of elements to enqueue
         * @param count Number of elements to enqueue
         * @return Number of elements successfully enqueued
         */
        std::size_t try_enqueue(const T* data, std::size_t count) {
            return queue_.try_enqueue(data, count);
        }

        /**
         * @brief Block until an element can be enqueued with timeout
         *
         * @tparam Rep The arithmetic type representing the timeout duration count
         * @tparam Period The `std::ratio` type representing the timeout duration period
         * @param value The value to enqueue
         * @param timeout Maximum time to wait for enqueue
         * @return true if successful, false if timeout expired
         */
        template <typename Rep, typename Period>
        bool enqueue(const T& value, std::chrono::duration<Rep, Period> timeout) {
            return queue_.enqueue(value, timeout);
        }

        /**
         * @brief Block until an element can be enqueued with timeout (move semantics)
         *
         * @tparam Rep The arithmetic type representing the timeout duration count
         * @tparam Period The `std::ratio` type representing the timeout duration period
         * @param value The value to enqueue (will be moved)
         * @param timeout Maximum time to wait for enqueue
         * @return true if successful, false if timeout expired
         */
        template <typename Rep, typename Period>
        bool enqueue(T&& value, std::chrono::duration<Rep, Period> timeout) {
            return queue_.enqueue(std::move(value), timeout);
        }

        /**
         * @brief Block until all elements are enqueued with timeout
         *
         * @tparam Rep The arithmetic type representing the timeout duration count
         * @tparam Period The `std::ratio` type representing the timeout duration period
         * @param data Pointer to array of elements to enqueue
         * @param count Number of elements to enqueue
         * @param timeout Maximum time to wait for all elements to be enqueued
         * @return true if all elements were enqueued, false if timeout expired
         */
        template <typename Rep, typename Period>
        bool enqueue(const T* data, std::size_t count, std::chrono::duration<Rep, Period> timeout) {
            return queue_.enqueue(data, count, timeout);
        }

        /**
         * @brief Check if the queue is empty
         *
         * @return true if empty, false otherwise
         */
        bool empty() const { return queue_.empty(); }

        /**
         * @brief Get approximate size of the queue
         *
         * @return Approximate number of elements in the queue
         */
        std::size_t size() const { return queue_.size(); }

    private:
        SPSC<T, Capacity>& queue_;
    };

    /**
     * @brief Consumer-side handle for SPSC queue
     *
     * Provides a restricted interface exposing only dequeue operations and utility methods.
     * This handle is intended for use by the single consumer thread.
     */
    class Source {
    public:
        Source(SPSC<T, Capacity>& queue)
            : queue_(queue) { }

        // Non-copyable. Move constructible (reference is re-bound to the same queue)
        Source(const Source&) = delete;
        Source& operator=(const Source&) = delete;
        Source(Source&&) = default;
        // Not move-assignable due to reference member
        Source& operator=(Source&&) = delete;

        /**
         * @brief Try to dequeue a single element
         *
         * @return std::optional containing the value if successful, std::nullopt if queue is empty
         */
        std::optional<T> try_dequeue() { return queue_.try_dequeue(); }

        /**
         * @brief Try to dequeue multiple elements
         *
         * @param data Pointer to output array
         * @param count Maximum number of elements to dequeue
         * @return Number of elements successfully dequeued
         */
        std::size_t try_dequeue(T* data, std::size_t count) {
            return queue_.try_dequeue(data, count);
        }

        /**
         * @brief Block until an element can be dequeued with timeout
         *
         * @tparam Rep The arithmetic type representing the timeout duration count
         * @tparam Period The `std::ratio` type representing the timeout duration period
         * @param timeout Maximum time to wait for dequeue
         * @return std::optional containing the element if successful, std::nullopt if timeout
         * expired
         */
        template <typename Rep, typename Period>
        std::optional<T> dequeue(std::chrono::duration<Rep, Period> timeout) {
            return queue_.dequeue(timeout);
        }

        /**
         * @brief Block until all elements are dequeued with timeout
         *
         * @tparam Rep The arithmetic type representing the timeout duration count
         * @tparam Period The `std::ratio` type representing the timeout duration period
         * @param data Pointer to output array
         * @param count Number of elements to dequeue
         * @param timeout Maximum time to wait for all elements to be dequeued
         * @return Number of elements successfully dequeued (may be less than count if timeout)
         */
        template <typename Rep, typename Period>
        std::size_t dequeue(
            T* data, std::size_t count, std::chrono::duration<Rep, Period> timeout) {
            return queue_.dequeue(data, count, timeout);
        }

        /**
         * @brief Check if the queue is empty
         *
         * @return true if empty, false otherwise
         */
        bool empty() const { return queue_.empty(); }

        /**
         * @brief Get approximate size of the queue
         *
         * @return Approximate number of elements in the queue
         */
        std::size_t size() const { return queue_.size(); }

    private:
        SPSC<T, Capacity>& queue_;
    };

private:
    friend class Sink;
    friend class Source;

    static constexpr std::size_t increment(std::size_t idx) { return (idx + 1) & (Capacity - 1); }

    /**
     * @brief Try to enqueue a single element
     *
     * @param value The value to enqueue
     * @return true if successful, false if queue is full
     */
    bool try_enqueue(const T& value) { return try_enqueue(T(value)); }

    /**
     * @brief Try to enqueue a single element (move semantics)
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
     * @brief Try to enqueue multiple elements efficiently
     *
     * Enqueues up to `count` elements from the input array. Returns the number
     * of elements actually enqueued. Optimized to avoid modulo in hot loop.
     *
     * @param data Pointer to array of elements to enqueue
     * @param count Number of elements to enqueue
     * @return Number of elements successfully enqueued
     */
    std::size_t try_enqueue(const T* data, std::size_t count) {
        if (count == 0) return 0;

        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto current_head = head_.load(std::memory_order_acquire);

        // Calculate available space
        std::size_t available;
        if (current_tail >= current_head) {
            available = (Capacity - 1) - (current_tail - current_head);
        } else {
            available = current_head - current_tail - 1;
        }

        // Limit enqueue to available space
        std::size_t to_enqueue = (count < available) ? count : available;

        if (to_enqueue == 0) return 0;

        std::size_t enqueued_total = 0;
        std::size_t src_idx = 0;
        std::size_t tail_idx = current_tail;

        // First segment: write until end of buffer or until head
        std::size_t first_seg_size;
        if (current_tail >= current_head) {
            // Normal case: write from tail to end of buffer
            first_seg_size = std::min(to_enqueue, Capacity - tail_idx);
        } else {
            // Wrap case: write from tail up to (but not past) head
            first_seg_size = std::min(to_enqueue, current_head - tail_idx - 1);
        }

        for (std::size_t i = 0; i < first_seg_size; ++i) {
            buffer_[tail_idx + i] = data[src_idx + i];
        }
        enqueued_total = first_seg_size;
        tail_idx = (tail_idx + first_seg_size) & (Capacity - 1);
        to_enqueue -= first_seg_size;
        src_idx += first_seg_size;

        // Second segment: if we wrapped and still have elements to enqueue
        if (to_enqueue > 0) {
            std::size_t second_seg_size = std::min(to_enqueue, current_head - 1);
            for (std::size_t i = 0; i < second_seg_size; ++i) {
                buffer_[i] = data[src_idx + i];
            }
            enqueued_total += second_seg_size;
            tail_idx = second_seg_size;
        }

        tail_.store(tail_idx, std::memory_order_release);
        return enqueued_total;
    }

    /**
     * @brief Try to dequeue a single element
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
     * @brief Try to dequeue multiple elements efficiently
     *
     * Dequeues up to `count` elements into the output array. Returns the number
     * of elements actually dequeued. Optimized to avoid modulo in hot loop.
     *
     * @param data Pointer to output array
     * @param count Maximum number of elements to dequeue
     * @return Number of elements successfully dequeued
     */
    std::size_t try_dequeue(T* data, std::size_t count) {
        if (count == 0) return 0;

        const auto current_head = head_.load(std::memory_order_relaxed);
        const auto current_tail = tail_.load(std::memory_order_acquire);

        // Calculate available elements
        std::size_t available;
        if (current_tail >= current_head) {
            available = current_tail - current_head;
        } else {
            available = Capacity - current_head + current_tail;
        }

        // Limit dequeue to available elements
        std::size_t to_dequeue = (count < available) ? count : available;

        if (to_dequeue == 0) return 0;

        std::size_t head_idx = current_head;
        std::size_t dequeued = 0;

        // First segment: from current_head to end of buffer (or wrap point)
        if (current_tail >= current_head) {
            // Normal case: elements are contiguous from head to tail
            std::size_t first_segment = std::min(to_dequeue, current_tail - head_idx);
            for (std::size_t i = 0; i < first_segment; ++i) {
                data[i] = std::move(buffer_[head_idx + i]);
            }
            head_idx += first_segment;
            dequeued = first_segment;
            to_dequeue -= first_segment;
        } else {
            // Wrap case: elements go from head to end, then from start to tail
            std::size_t first_segment = std::min(to_dequeue, Capacity - head_idx);
            for (std::size_t i = 0; i < first_segment; ++i) {
                data[i] = std::move(buffer_[head_idx + i]);
            }
            head_idx = (head_idx + first_segment) & (Capacity - 1);
            dequeued = first_segment;
            to_dequeue -= first_segment;
        }

        // Second segment: if we wrapped and still need elements
        if (to_dequeue > 0 && head_idx == 0) {
            std::size_t second_segment = std::min(to_dequeue, current_tail);
            for (std::size_t i = 0; i < second_segment; ++i) {
                data[dequeued + i] = std::move(buffer_[i]);
            }
            head_idx = second_segment;
            dequeued += second_segment;
        }

        head_.store(head_idx, std::memory_order_release);
        return dequeued;
    }

    /**
     * @brief Block until an element can be enqueued with timeout
     *
     * Blocks the calling thread until the element is successfully enqueued or
     * the timeout expires. Uses a busy-wait with yielding to minimize latency.
     *
     * @tparam Rep The arithmetic type representing the timeout duration count
     * @tparam Period The `std::ratio` type representing the timeout duration period
     * @param value The value to enqueue
     * @param timeout Maximum time to wait for enqueue
     * @return true if successful, false if timeout expired
     */
    template <typename Rep, typename Period>
    bool enqueue(const T& value, std::chrono::duration<Rep, Period> timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!try_enqueue(value)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    /**
     * @brief Block until an element can be enqueued with timeout (move semantics)
     *
     * Blocks the calling thread until the element is successfully enqueued or
     * the timeout expires. Uses a busy-wait with yielding to minimize latency.
     *
     * @tparam Rep The arithmetic type representing the timeout duration count
     * @tparam Period The `std::ratio` type representing the timeout duration period
     * @param value The value to enqueue (will be moved)
     * @param timeout Maximum time to wait for enqueue
     * @return true if successful, false if timeout expired
     */
    template <typename Rep, typename Period>
    bool enqueue(T&& value, std::chrono::duration<Rep, Period> timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!try_enqueue(std::move(value))) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    /**
     * @brief Block until all elements are enqueued with timeout
     *
     * Blocks until all `count` elements from the input array are successfully enqueued
     * or the timeout expires. Enqueues in segments to handle wrap-around efficiently.
     *
     * @tparam Rep The arithmetic type representing the timeout duration count
     * @tparam Period The `std::ratio` type representing the timeout duration period
     * @param data Pointer to array of elements to enqueue
     * @param count Number of elements to enqueue
     * @param timeout Maximum time to wait for all elements to be enqueued
     * @return true if all elements were enqueued, false if timeout expired
     */
    template <typename Rep, typename Period>
    bool enqueue(const T* data, std::size_t count, std::chrono::duration<Rep, Period> timeout) {
        if (count == 0) return true;

        auto deadline = std::chrono::steady_clock::now() + timeout;
        std::size_t total_enqueued = 0;
        while (total_enqueued < count) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }

            std::size_t enqueued = try_enqueue(data + total_enqueued, count - total_enqueued);
            total_enqueued += enqueued;

            if (total_enqueued < count) {
                std::this_thread::yield();
            }
        }
        return true;
    }

    /**
     * @brief Block until an element can be dequeued with timeout
     *
     * Blocks the calling thread until an element is successfully dequeued or
     * the timeout expires. Uses a busy-wait with yielding to minimize latency.
     *
     * @tparam Rep The arithmetic type representing the timeout duration count
     * @tparam Period The `std::ratio` type representing the timeout duration period
     * @param timeout Maximum time to wait for dequeue
     * @return std::optional containing the element if successful, std::nullopt if timeout expired
     */
    template <typename Rep, typename Period>
    std::optional<T> dequeue(std::chrono::duration<Rep, Period> timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            auto value = try_dequeue();
            if (value.has_value()) {
                return value;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return std::nullopt;
            }
            std::this_thread::yield();
        }
    }

    /**
     * @brief Block until all elements are dequeued with timeout
     *
     * Blocks until all `count` elements are successfully dequeued into the output array
     * or the timeout expires. Reads in segments to handle wrap-around efficiently.
     *
     * @tparam Rep The arithmetic type representing the timeout duration count
     * @tparam Period The `std::ratio` type representing the timeout duration period
     * @param data Pointer to output array
     * @param count Number of elements to dequeue
     * @param timeout Maximum time to wait for all elements to be dequeued
     * @return Number of elements successfully dequeued (may be less than count if timeout)
     */
    template <typename Rep, typename Period>
    std::size_t dequeue(T* data, std::size_t count, std::chrono::duration<Rep, Period> timeout) {
        if (count == 0) return 0;

        auto deadline = std::chrono::steady_clock::now() + timeout;
        std::size_t total_dequeued = 0;
        while (total_dequeued < count) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return total_dequeued;
            }

            std::size_t dequeued = try_dequeue(data + total_dequeued, count - total_dequeued);
            total_dequeued += dequeued;

            if (total_dequeued < count) {
                std::this_thread::yield();
            }
        }
        return total_dequeued;
    }



    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
    std::array<T, Capacity> buffer_;
};

template <typename T, std::size_t Capacity>
using SpscSource = SPSC<T, Capacity>::Source;

template <typename T, std::size_t Capacity>
using SpscSink = SPSC<T, Capacity>::Sink;

} // namespace qbuf
#endif // QBUF_SPSC_HPP
