#ifndef QBUF_MMAP_SPSC_HPP
#define QBUF_MMAP_SPSC_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

#if defined(__linux__)
#include <linux/memfd.h>
#include <sys/syscall.h>
#if __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 27)
static inline int memfd_create(const char* name, unsigned int flags) {
    return syscall(__NR_memfd_create, name, flags);
}
#endif
#endif

namespace qbuf {

/**
 * @brief Memory-mapped Single-Producer Single-Consumer lock-free queue
 *
 * A thread-safe, lock-free queue implementation using memory-mapped I/O.
 * Uses the double-mapping trick where two adjacent virtual memory regions
 * point to the same physical memory, eliminating wrap-around logic.
 *
 * @tparam T The type of elements stored in the queue
 * @tparam Capacity Maximum number of elements the queue can hold
 */
template <typename T, std::size_t Capacity>
class MmapSPSC {
public:
    static_assert(Capacity > 0, "Queue capacity must be greater than 0");
    static_assert((Capacity & (Capacity - 1)) == 0, "Queue capacity must be a power of 2");

private:
    MmapSPSC() : head_(0), tail_(0), buffer_(nullptr), fd_(-1) { initialize_mmap(); }

public:
    // non-copyable
    MmapSPSC(const MmapSPSC&) = delete;
    MmapSPSC& operator=(const MmapSPSC&) = delete;
    // non-movable
    MmapSPSC(MmapSPSC&&) = delete;
    MmapSPSC& operator=(MmapSPSC&&) = delete;

    ~MmapSPSC() { cleanup_mmap(); }

    /**
     * @brief Producer-side handle for MmapSPSC queue
     *
     * Provides a restricted interface exposing only enqueue operations and utility methods.
     * This handle is intended for use by the single producer thread.
     */
    class Sink {
        friend class MmapSPSC;
        explicit Sink(std::shared_ptr<MmapSPSC<T, Capacity>> queue) : queue_(queue) { }

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

        /**
         * @brief Check if the queue is empty
         *
         * @return true if empty, false otherwise
         */
        bool empty() const { return queue_->empty(); }

        /**
         * @brief Get approximate size of the queue
         *
         * @return Approximate number of elements in the queue
         */
        std::size_t size() const { return queue_->size(); }

    private:
        std::shared_ptr<MmapSPSC<T, Capacity>> queue_;
    };

    /**
     * @brief Consumer-side handle for MmapSPSC queue
     *
     * Provides a restricted interface exposing only dequeue operations and utility methods.
     * This handle is intended for use by the single consumer thread.
     */
    class Source {
        friend class MmapSPSC;
        explicit Source(std::shared_ptr<MmapSPSC<T, Capacity>> queue) : queue_(queue) { }

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

        /**
         * @brief Check if the queue is empty
         *
         * @return true if empty, false otherwise
         */
        bool empty() const { return queue_->empty(); }

        /**
         * @brief Get approximate size of the queue
         *
         * @return Approximate number of elements in the queue
         */
        std::size_t size() const { return queue_->size(); }

    private:
        std::shared_ptr<MmapSPSC<T, Capacity>> queue_;
    };

    /**
     * @brief Factory method to create a memory-mapped SPSC queue with sink and source handles
     *
     * @return std::pair containing (Sink, Source)
     */
    static std::pair<Sink, Source> create() {
        auto queue = std::shared_ptr<MmapSPSC<T, Capacity>>(new MmapSPSC<T, Capacity>());
        return { Sink(queue), Source(queue) };
    }

private:
    void initialize_mmap() {
        const std::size_t buffer_size = Capacity * sizeof(T);
        const std::size_t page_size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));

        // Round up to page size
        std::size_t mmap_size = buffer_size;
        if (mmap_size % page_size != 0) {
            mmap_size = ((mmap_size / page_size) + 1) * page_size;
        }

#if defined(__linux__)
        // Create anonymous memory-backed file
        fd_ = memfd_create("mmap_spsc_queue", 0);
        if (fd_ == -1) {
            throw std::runtime_error("Failed to create memfd");
        }

        // Set size
        if (ftruncate(fd_, mmap_size) != 0) {
            close(fd_);
            throw std::runtime_error("Failed to set memfd size");
        }

        // Reserve virtual address space for double mapping
        void* addr = mmap(nullptr, 2 * mmap_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (addr == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("Failed to reserve virtual memory");
        }

        // Map first region
        if (mmap(addr, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd_, 0)
            == MAP_FAILED) {
            munmap(addr, 2 * mmap_size);
            close(fd_);
            throw std::runtime_error("Failed to map first region");
        }

        // Map second region at adjacent address
        if (mmap(
                static_cast<uint8_t*>(addr) + mmap_size, mmap_size, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_FIXED, fd_, 0
            )
            == MAP_FAILED) {
            munmap(addr, 2 * mmap_size);
            close(fd_);
            throw std::runtime_error("Failed to map second region");
        }

        buffer_ = static_cast<T*>(addr);
        mmap_size_ = mmap_size;
#else
        // Fallback for non-Linux: use regular allocation
        buffer_ = static_cast<T*>(::operator new(buffer_size));
        mmap_size_ = buffer_size;
        fd_ = -1;
#endif
    }

    void cleanup_mmap() {
        if (buffer_ == nullptr) return;

#if defined(__linux__)
        if (fd_ != -1) {
            munmap(buffer_, 2 * mmap_size_);
            close(fd_);
        } else {
            ::operator delete(buffer_);
        }
#else
        ::operator delete(buffer_);
#endif
        buffer_ = nullptr;
        fd_ = -1;
    }

    /**
     * @brief Try to enqueue a single element
     *
     * @param value The value to enqueue
     * @return true if successful, false if queue is full
     */
    bool try_enqueue(const T& value) {
        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto current_head = head_.load(std::memory_order_acquire);

        const auto next_tail = (current_tail + 1) & (Capacity - 1);

        if (next_tail == current_head) {
            return false;
        }

        new (&buffer_[current_tail]) T(value);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Try to enqueue a single element (move semantics)
     *
     * @param value The value to enqueue
     * @return true if successful, false if queue is full
     */
    bool try_enqueue(T&& value) {
        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto current_head = head_.load(std::memory_order_acquire);

        const auto next_tail = (current_tail + 1) & (Capacity - 1);

        if (next_tail == current_head) {
            return false;
        }

        new (&buffer_[current_tail]) T(std::move(value));
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Try to enqueue multiple elements
     *
     * @param data Pointer to array of elements to enqueue
     * @param count Number of elements to enqueue
     * @return Number of elements successfully enqueued
     */
    std::size_t try_enqueue(const T* data, std::size_t count) {
        if (count == 0) return 0;

        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto current_head = head_.load(std::memory_order_acquire);

        const auto available = (current_head > current_tail)
            ? (current_head - current_tail - 1)
            : (Capacity - current_tail + current_head - 1);

        const auto to_write = (count < available) ? count : available;
        if (to_write == 0) return 0;

        const auto first_chunk
            = (current_tail + to_write <= Capacity) ? to_write : (Capacity - current_tail);

        for (std::size_t i = 0; i < first_chunk; ++i) {
            new (&buffer_[current_tail + i]) T(data[i]);
        }

        const auto second_chunk = to_write - first_chunk;
        for (std::size_t i = 0; i < second_chunk; ++i) {
            new (&buffer_[i]) T(data[first_chunk + i]);
        }

        tail_.store((current_tail + to_write) & (Capacity - 1), std::memory_order_release);
        return to_write;
    }

    /**
     * @brief Try to dequeue a single element
     *
     * @return std::optional containing the element if successful, std::nullopt if queue is empty
     */
    std::optional<T> try_dequeue() {
        const auto current_head = head_.load(std::memory_order_relaxed);
        const auto current_tail = tail_.load(std::memory_order_acquire);

        if (current_head == current_tail) {
            return std::nullopt;
        }

        T value = std::move(buffer_[current_head]);
        buffer_[current_head].~T();

        head_.store((current_head + 1) & (Capacity - 1), std::memory_order_release);
        return value;
    }

    /**
     * @brief Try to dequeue multiple elements
     *
     * @param data Pointer to output array
     * @param count Maximum number of elements to dequeue
     * @return Number of elements successfully dequeued
     */
    std::size_t try_dequeue(T* data, std::size_t count) {
        if (count == 0) return 0;

        const auto current_head = head_.load(std::memory_order_relaxed);
        const auto current_tail = tail_.load(std::memory_order_acquire);

        const auto available = (current_tail >= current_head)
            ? (current_tail - current_head)
            : (Capacity - current_head + current_tail);

        const auto to_read = (count < available) ? count : available;
        if (to_read == 0) return 0;

        const auto first_chunk
            = (current_head + to_read <= Capacity) ? to_read : (Capacity - current_head);

        for (std::size_t i = 0; i < first_chunk; ++i) {
            data[i] = std::move(buffer_[current_head + i]);
            buffer_[current_head + i].~T();
        }

        const auto second_chunk = to_read - first_chunk;
        for (std::size_t i = 0; i < second_chunk; ++i) {
            data[first_chunk + i] = std::move(buffer_[i]);
            buffer_[i].~T();
        }

        head_.store((current_head + to_read) & (Capacity - 1), std::memory_order_release);
        return to_read;
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
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            if (try_enqueue(value)) {
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::yield();
        }
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
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            if (try_enqueue(std::move(value))) {
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::yield();
        }
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

    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
    T* buffer_;
    int fd_;
    std::size_t mmap_size_;
};

template <typename T, std::size_t Capacity>
using MmapSpscSource = typename MmapSPSC<T, Capacity>::Source;

template <typename T, std::size_t Capacity>
using MmapSpscSink = typename MmapSPSC<T, Capacity>::Sink;

} // namespace qbuf
#endif // QBUF_MMAP_SPSC_HPP
