#ifndef QBUF_MUTEX_QUEUE_HPP
#define QBUF_MUTEX_QUEUE_HPP

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace qbuf {

/**
 * @brief Mutex-based circular buffer queue
 *
 * A thread-safe queue implementation using mutex and condition variables.
 * Provides an API surface compatible with SPSC (Sink/Source handles,
 * try_/blocking operations). Reserves one slot to distinguish full from empty.
 *
 * @tparam T The type of elements stored in the queue
 * @tparam Capacity Maximum number of elements the queue can hold
 */
template <typename T, std::size_t Capacity>
class MutexQueue {
public:
    static_assert(Capacity > 1, "Queue capacity must be greater than 1");
    static_assert(
        std::is_trivially_copyable_v<T>,
        "T must be trivially copyable for bulk operations using memcpy"
    );

private:
    MutexQueue() : head_(0), tail_(0) { }

public:
    MutexQueue(const MutexQueue&) = delete;
    MutexQueue& operator=(const MutexQueue&) = delete;
    MutexQueue(MutexQueue&&) = delete;
    MutexQueue& operator=(MutexQueue&&) = delete;

    /**
     * @brief Check if the queue is empty
     *
     * @return true if empty, false otherwise
     */
    bool empty() const {
        std::scoped_lock lk(mtx_);
        return head_ == tail_;
    }

    /**
     * @brief Get size of the queue
     *
     * @return Number of elements in the queue
     */
    std::size_t size() const {
        std::scoped_lock lk(mtx_);
        return size_unlocked();
    }

    /**
     * @brief Producer-side handle for MutexQueue
     *
     * Provides a restricted interface exposing only enqueue operations and utility methods.
     */
    class Sink {
    private:
        friend class MutexQueue;
        explicit Sink(std::shared_ptr<MutexQueue<T, Capacity>> queue) : queue_(queue) { }

    public:
        // Non-copyable
        Sink(const Sink&) = delete;
        Sink& operator=(const Sink&) = delete;

        // Movable
        Sink(Sink&&) = default;
        Sink& operator=(Sink&&) = default;

        /**
         * @brief Try to enqueue a single element
         *
         * @param value The value to enqueue
         * @return true if successful, false if queue is full
         */
        bool try_enqueue(const T& value) { return queue_->try_enqueue(value); }

        /**
         * @brief Try to enqueue a single element (move semantics)
         *
         * @param value The value to enqueue
         * @return true if successful, false if queue is full
         */
        bool try_enqueue(T&& value) { return queue_->try_enqueue(std::move(value)); }

        /**
         * @brief Try to enqueue multiple elements
         *
         * @param data Pointer to array of elements to enqueue
         * @param count Number of elements to enqueue
         * @return Number of elements successfully enqueued
         */
        std::size_t try_enqueue(const T* data, std::size_t count) {
            return queue_->try_enqueue(data, count);
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
            return queue_->enqueue(value, timeout);
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
            return queue_->enqueue(std::move(value), timeout);
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
            return queue_->enqueue(data, count, timeout);
        }

        bool empty() const { return queue_->empty(); }

        std::size_t size() const {
            return queue_->size();
        }

    private:
        std::shared_ptr<MutexQueue<T, Capacity>> queue_;
    };

    /**
     * @brief Consumer-side handle for MutexQueue
     *
     * Provides a restricted interface exposing only dequeue operations and utility methods.
     */
    class Source {
    private:
        friend class MutexQueue;
        explicit Source(std::shared_ptr<MutexQueue<T, Capacity>> queue) : queue_(queue) { }

    public:
        // Non-copyable
        Source(const Source&) = delete;
        Source& operator=(const Source&) = delete;

        // Movable
        Source(Source&&) = default;
        Source& operator=(Source&&) = default;

        /**
         * @brief Try to dequeue a single element
         *
         * @return std::optional containing the element if successful, std::nullopt if queue is
         * empty
         */
        std::optional<T> try_dequeue() { return queue_->try_dequeue(); }

        /**
         * @brief Try to dequeue multiple elements
         *
         * @param data Pointer to output array
         * @param count Maximum number of elements to dequeue
         * @return Number of elements successfully dequeued
         */
        std::size_t try_dequeue(T* data, std::size_t count) {
            return queue_->try_dequeue(data, count);
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
            return queue_->dequeue(timeout);
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
            T* data, std::size_t count, std::chrono::duration<Rep, Period> timeout
        ) {
            return queue_->dequeue(data, count, timeout);
        }

        bool empty() const { return queue_->empty(); }
        std::size_t size() const { return queue_->size(); }

    private:
        std::shared_ptr<MutexQueue<T, Capacity>> queue_;
    };

    /**
     * @brief Factory method to create a queue with sink and source handles
     *
     * This is the way to create a MutexQueue. It returns a pair of handles
     * (Sink, Source) that can be used by producer and consumer threads
     * respectively.
     *
     * @return std::pair<Sink, Source> A pair of producer and consumer handles
     */
    static std::pair<Sink, Source> make_queue() {
        std::shared_ptr<MutexQueue> queue(new MutexQueue<T, Capacity>());
        return { Sink(queue), Source(queue) };
    }

private:
    friend class Sink;
    friend class Source;

    bool try_enqueue(const T& value) {
        {
            std::lock_guard lk(mtx_);
            if (full_unlocked()) return false;
            buffer_[tail_] = value;
            tail_ = next(tail_);
        }
        cv_not_empty_.notify_one();
        return true;
    }

    bool try_enqueue(T&& value) {
        {
            std::lock_guard lk(mtx_);
            if (full_unlocked()) return false;
            buffer_[tail_] = std::move(value);
            tail_ = next(tail_);
        }
        cv_not_empty_.notify_one();
        return true;
    }

    std::size_t try_enqueue(const T* data, std::size_t count) {
        if (count == 0) return 0;
        std::size_t n;
        {
            std::lock_guard lk(mtx_);
            const std::size_t free = free_unlocked();
            n = (free < count) ? free : count;
            if (n == 0) return 0;

            std::size_t first_segment = (n < (Capacity - tail_)) ? n : (Capacity - tail_);
            std::memcpy(&buffer_[tail_], data, first_segment * sizeof(T));
            std::size_t second_segment = n - first_segment;
            if (second_segment > 0) {
                std::memcpy(&buffer_[0], data + first_segment, second_segment * sizeof(T));
            }
            tail_ = (tail_ + n) % Capacity;
        }
        cv_not_empty_.notify_one();
        return n;
    }

    template <typename Rep, typename Period>
    bool enqueue(const T& value, std::chrono::duration<Rep, Period> timeout) {
        {
            std::unique_lock lk(mtx_);
            auto deadline = std::chrono::steady_clock::now() + timeout;
            while (full_unlocked()) {
                if (cv_not_full_.wait_until(lk, deadline) == std::cv_status::timeout) {
                    return false;
                }
            }
            buffer_[tail_] = value;
            tail_ = next(tail_);
        }
        cv_not_empty_.notify_one();
        return true;
    }

    template <typename Rep, typename Period>
    bool enqueue(T&& value, std::chrono::duration<Rep, Period> timeout) {
        {
            std::unique_lock lk(mtx_);
            auto deadline = std::chrono::steady_clock::now() + timeout;
            while (full_unlocked()) {
                if (cv_not_full_.wait_until(lk, deadline) == std::cv_status::timeout) {
                    return false;
                }
            }
            buffer_[tail_] = std::move(value);
            tail_ = next(tail_);
        }
        cv_not_empty_.notify_one();
        return true;
    }

    template <typename Rep, typename Period>
    bool enqueue(const T* data, std::size_t count, std::chrono::duration<Rep, Period> timeout) {
        if (count == 0) return true;

        auto deadline = std::chrono::steady_clock::now() + timeout;
        std::size_t total = 0;

        while (total < count) {
            {
                std::unique_lock lk(mtx_);
                while (free_unlocked() == 0) {
                    if (cv_not_full_.wait_until(lk, deadline) == std::cv_status::timeout) {
                        return false;
                    }
                }

                std::size_t remaining = count - total;
                std::size_t can = (free_unlocked() < remaining) ? free_unlocked() : remaining;
                std::size_t first_segment = (can < (Capacity - tail_)) ? can : (Capacity - tail_);

                std::memcpy(&buffer_[tail_], data + total, first_segment * sizeof(T));
                std::size_t second_segment = can - first_segment;
                if (second_segment > 0) {
                    std::memcpy(
                        &buffer_[0], data + total + first_segment, second_segment * sizeof(T)
                    );
                }
                tail_ = (tail_ + can) % Capacity;
                total += can;
            }
            cv_not_empty_.notify_one();
        }
        return true;
    }

    std::optional<T> try_dequeue() {
        T value;
        {
            std::lock_guard lk(mtx_);
            if (head_ == tail_) return std::nullopt;
            value = std::move(buffer_[head_]);
            head_ = next(head_);
        }
        cv_not_full_.notify_one();
        return value;
    }

    std::size_t try_dequeue(T* data, std::size_t count) {
        if (count == 0) return 0;
        std::size_t n;
        {
            std::lock_guard lk(mtx_);
            std::size_t avail = size_unlocked();
            n = (avail < count) ? avail : count;
            if (n == 0) return 0;

            std::size_t first_segment = (n < (Capacity - head_)) ? n : (Capacity - head_);
            std::memcpy(data, &buffer_[head_], first_segment * sizeof(T));
            std::size_t second_segment = n - first_segment;
            if (second_segment > 0) {
                std::memcpy(data + first_segment, &buffer_[0], second_segment * sizeof(T));
            }
            head_ = (head_ + n) % Capacity;
        }
        cv_not_full_.notify_one();
        return n;
    }

    template <typename Rep, typename Period>
    std::optional<T> dequeue(std::chrono::duration<Rep, Period> timeout) {
        T value;
        {
            std::unique_lock lk(mtx_);
            auto deadline = std::chrono::steady_clock::now() + timeout;
            while (head_ == tail_) {
                if (cv_not_empty_.wait_until(lk, deadline) == std::cv_status::timeout) {
                    return std::nullopt;
                }
            }
            value = std::move(buffer_[head_]);
            head_ = next(head_);
        }
        cv_not_full_.notify_one();
        return value;
    }

    template <typename Rep, typename Period>
    std::size_t dequeue(T* data, std::size_t count, std::chrono::duration<Rep, Period> timeout) {
        if (count == 0) return 0;

        auto deadline = std::chrono::steady_clock::now() + timeout;
        std::size_t total = 0;

        while (total < count) {
            {
                std::unique_lock lk(mtx_);
                while (size_unlocked() == 0) {
                    if (cv_not_empty_.wait_until(lk, deadline) == std::cv_status::timeout) {
                        return total;
                    }
                }

                std::size_t remaining = count - total;
                std::size_t can = (size_unlocked() < remaining) ? size_unlocked() : remaining;
                std::size_t first_segment = (can < (Capacity - head_)) ? can : (Capacity - head_);

                std::memcpy(data + total, &buffer_[head_], first_segment * sizeof(T));
                std::size_t second_segment = can - first_segment;
                if (second_segment > 0) {
                    std::memcpy(
                        data + total + first_segment, &buffer_[0], second_segment * sizeof(T)
                    );
                }
                head_ = (head_ + can) % Capacity;
                total += can;
            }
            cv_not_full_.notify_one();
        }
        return total;
    }

private:
    static constexpr std::size_t next(std::size_t i) { return (i + 1) % Capacity; }

    std::size_t size_unlocked() const {
        return (tail_ >= head_) ? (tail_ - head_) : (Capacity - head_ + tail_);
    }

    std::size_t free_unlocked() const { return (Capacity - 1) - size_unlocked(); }

    bool full_unlocked() const { return next(tail_) == head_; }

    mutable std::mutex mtx_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;

    std::size_t head_;
    std::size_t tail_;
    std::array<T, Capacity> buffer_;
};

template <typename T, std::size_t Capacity>
using MutexSource = typename MutexQueue<T, Capacity>::Source;

template <typename T, std::size_t Capacity>
using MutexSink = typename MutexQueue<T, Capacity>::Sink;

} // namespace qbuf

#endif // QBUF_MUTEX_QUEUE_HPP
