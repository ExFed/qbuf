#include "test_mmap_spsc.hpp"

#include "../include/qbuf/mmap_spsc.hpp"
#include "assert.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace qbuf;

void test_mmap_basic_enqueue_dequeue() {
    std::cout << "Testing test_mmap_basic_enqueue_dequeue..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 64>::create();

    assert(sink.empty());
    assert(source.empty());

    assert(sink.try_enqueue(42));
    assert(!sink.empty());
    assert(!source.empty());

    auto value = source.try_dequeue();
    assert(value.has_value());
    assert(value.value() == 42);
    assert(sink.empty());
    assert(source.empty());

    std::cout << "  PASSED: test_mmap_basic_enqueue_dequeue" << std::endl;
}

void test_mmap_full_queue() {
    std::cout << "Testing test_mmap_full_queue..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 8>::create();

    for (int i = 0; i < 7; ++i) {
        assert(sink.try_enqueue(i));
    }

    assert(!sink.try_enqueue(999));

    for (int i = 0; i < 7; ++i) {
        auto value = source.try_dequeue();
        assert(value.has_value());
        assert(value.value() == i);
    }

    assert(!source.try_dequeue().has_value());

    std::cout << "  PASSED: test_mmap_full_queue" << std::endl;
}

void test_mmap_bulk_operations() {
    std::cout << "Testing test_mmap_bulk_operations..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 64>::create();

    std::vector<int> input = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    std::size_t enqueued = sink.try_enqueue(input.data(), input.size());
    assert(enqueued == input.size());

    std::vector<int> output(10, 0);
    std::size_t dequeued = source.try_dequeue(output.data(), output.size());
    assert(dequeued == output.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(input[i] == output[i]);
    }

    std::cout << "  PASSED: test_mmap_bulk_operations" << std::endl;
}

void test_mmap_bulk_wraparound() {
    std::cout << "Testing test_mmap_bulk_wraparound..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 16>::create();

    std::vector<int> data1 = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    assert(sink.try_enqueue(data1.data(), data1.size()) == data1.size());

    std::vector<int> partial(5, 0);
    assert(source.try_dequeue(partial.data(), 5) == 5);

    std::vector<int> data2 = { 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 };
    std::size_t enqueued = sink.try_enqueue(data2.data(), data2.size());
    assert(enqueued == 10);

    std::vector<int> output(15, 0);
    std::size_t dequeued = source.try_dequeue(output.data(), 15);
    assert(dequeued == 15);

    for (int i = 0; i < 5; ++i) {
        assert(output[i] == data1[i + 5]);
    }
    for (int i = 0; i < 10; ++i) {
        assert(output[i + 5] == data2[i]);
    }

    std::cout << "  PASSED: test_mmap_bulk_wraparound" << std::endl;
}

void test_mmap_blocking_enqueue() {
    std::cout << "Testing test_mmap_blocking_enqueue..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 8>::create();

    for (int i = 0; i < 7; ++i) {
        sink.try_enqueue(i);
    }

    std::thread consumer([source = std::move(source)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        for (int i = 0; i < 7; ++i) {
            source.try_dequeue();
        }
    });

    auto start = std::chrono::steady_clock::now();
    bool success = sink.enqueue(999, std::chrono::milliseconds(200));
    auto elapsed = std::chrono::steady_clock::now() - start;

    assert(success);
    assert(elapsed >= std::chrono::milliseconds(40));

    consumer.join();

    std::cout << "  PASSED: test_mmap_blocking_enqueue" << std::endl;
}

void test_mmap_blocking_dequeue() {
    std::cout << "Testing test_mmap_blocking_dequeue..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 8>::create();

    std::thread producer([sink = std::move(sink)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sink.try_enqueue(42);
    });

    auto start = std::chrono::steady_clock::now();
    auto value = source.dequeue(std::chrono::milliseconds(200));
    auto elapsed = std::chrono::steady_clock::now() - start;

    assert(value.has_value());
    assert(value.value() == 42);
    assert(elapsed >= std::chrono::milliseconds(40));

    producer.join();

    std::cout << "  PASSED: test_mmap_blocking_dequeue" << std::endl;
}

void test_mmap_blocking_bulk_enqueue() {
    std::cout << "Testing test_mmap_blocking_bulk_enqueue..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 16>::create();

    for (int i = 0; i < 10; ++i) {
        sink.try_enqueue(i);
    }

    std::thread consumer([source = std::move(source)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::vector<int> output(10, 0);
        source.try_dequeue(output.data(), 10);
    });

    std::vector<int> input = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
    auto start = std::chrono::steady_clock::now();
    bool success = sink.enqueue(input.data(), input.size(), std::chrono::milliseconds(200));
    auto elapsed = std::chrono::steady_clock::now() - start;

    assert(success);
    assert(elapsed >= std::chrono::milliseconds(40));

    consumer.join();

    std::cout << "  PASSED: test_mmap_blocking_bulk_enqueue" << std::endl;
}

void test_mmap_timeout() {
    std::cout << "Testing test_mmap_timeout..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 8>::create();

    for (int i = 0; i < 7; ++i) {
        sink.try_enqueue(i);
    }

    auto start = std::chrono::steady_clock::now();
    bool success = sink.enqueue(999, std::chrono::milliseconds(50));
    auto elapsed = std::chrono::steady_clock::now() - start;

    assert(!success);
    assert(elapsed >= std::chrono::milliseconds(40));
    assert(elapsed < std::chrono::milliseconds(100));

    std::cout << "  PASSED: test_mmap_timeout" << std::endl;
}

void test_mmap_move_semantics() {
    std::cout << "Testing test_mmap_move_semantics..." << std::endl;

    auto [sink, source] = MmapSPSC<std::unique_ptr<int>, 64>::create();

    auto ptr = std::make_unique<int>(42);
    assert(sink.try_enqueue(std::move(ptr)));
    assert(ptr == nullptr);

    auto value = source.try_dequeue();
    assert(value.has_value());
    assert(*value.value() == 42);

    std::cout << "  PASSED: test_mmap_move_semantics" << std::endl;
}

namespace {
// A move-only container type that stores some data.
template <typename T>
class MoveOnlyContainer {
public:
    explicit MoveOnlyContainer(T data) : data_(data) { }

    MoveOnlyContainer(const MoveOnlyContainer&) = delete;
    MoveOnlyContainer& operator=(const MoveOnlyContainer&) = delete;

    MoveOnlyContainer(MoveOnlyContainer&& other) noexcept
            : data_(std::move(other.data_))
            , num_moved_(other.num_moved_ + 1) {
        ++other.num_vacated_;
    }

    MoveOnlyContainer& operator=(MoveOnlyContainer&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            num_moved_ = other.num_moved_ + 1;
            ++other.num_vacated_;
        }
        return *this;
    }

    T data() const { return data_; }

    int moved() const { return num_moved_; }

    int vacated() const { return num_vacated_; }

    std::basic_ostream<char>& repr(std::basic_ostream<char>& os) const {
        return os << "MoveOnlyContainer" //
                  << "(data=" << data_ //
                  << ", moved=" << num_moved_ //
                  << ", vacated=" << num_vacated_ //
                  << ")";
    }

private:
    T data_;
    int num_moved_ { 0 };
    int num_vacated_ { 0 };
};

template <typename T>
inline std::basic_ostream<char>& operator<<(
    std::basic_ostream<char>& ss, const MoveOnlyContainer<T>& obj
) {
    return obj.repr(ss);
}
} // namespace

void test_mmap_blocking_rvalue_enqueue_with_movable_type() {
    std::cout << "Testing test_mmap_blocking_rvalue_enqueue_with_movable_type..." << std::endl;

    using namespace std::chrono;
    using namespace std::chrono_literals;

    // Concurrency parameters
    constexpr auto CAPACITY = 8;
    constexpr auto ITERS = 20;
    constexpr auto TIMEOUT = 500ms;
    constexpr auto BP_DELAY = 50ms;
    constexpr auto BP_THRESH = 40ms;

    // The queue under test
    auto [sink, source] = MmapSPSC<MoveOnlyContainer<int>, 8>::create();

    // Fill queue to capacity (7 elements, since 1 slot is reserved)
    for (int i = 0; i < 7; ++i) {
        assert(sink.try_enqueue(MoveOnlyContainer<int>(i)));
    }
    assert(7 == sink.size());

    // Producer thread: attempt blocking enqueue while full
    std::thread producer([&, sink = std::move(sink)]() mutable {
        auto obj = MoveOnlyContainer<int>(99);

        // Time enqueue call to ensure it blocks
        auto start = steady_clock::now();
        bool success = sink.enqueue(std::move(obj), TIMEOUT);
        auto elapsed = steady_clock::now() - start;

        assert(success); // Confirm enqueue succeeded
        assert(elapsed >= BP_THRESH); // Confirm blocked due to backpressure
        assert(1 == obj.vacated()); // Confirm object was moved-from *once*
    });

    // Apply some backpressure to force the producer to block
    std::this_thread::sleep_for(BP_DELAY);

    // Confirm queue is full
    assert(7 == source.size());

    // Consume all leading items except the blocked one
    for (int i = 0; i < 7; ++i) {
        auto val = source.try_dequeue();
        assert(val.has_value());
        assert(val.value().data() == i);
    }

    // Wait for the producer to be done
    producer.join();

    // We know the producer has enqueued its last item
    auto received = source.try_dequeue();
    assert(received.has_value()); // Confirm dequeue was successful
    assert(1 <= received.value().moved()); // Confirm moved *at least once*
    assert(0 == received.value().vacated()); // Confirm not moved-from
    assert(99 == received.value().data()); // Confirm correct data received

    std::cout << "  PASSED: test_mmap_blocking_rvalue_enqueue_with_movable_type" << std::endl;
}

void test_mmap_producer_consumer_stress() {
    constexpr std::size_t total_items = 100000;
    std::cout << "Testing test_mmap_producer_consumer_stress..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 4096>::create();

    // capture by reference due to assertion below
    std::thread producer([&sink = sink]() mutable {
        for (std::size_t i = 0; i < total_items; ++i) {
            while (!sink.try_enqueue(static_cast<int>(i))) {
                std::this_thread::yield();
            }
        }
    });

    // capture by reference due to assertion below
    std::thread consumer([&source = source]() mutable {
        for (std::size_t i = 0; i < total_items; ++i) {
            std::optional<int> value;
            while (!(value = source.try_dequeue()).has_value()) {
                std::this_thread::yield();
            }
            assert(value.value() == static_cast<int>(i));
        }
    });

    producer.join();
    consumer.join();

    assert(sink.empty());
    assert(source.empty());

    std::cout << "  PASSED: test_mmap_producer_consumer_stress" << std::endl;
}

void run_all_mmap_spsc_tests() {
    std::cout << "\n=== Running MmapSPSC Tests ===" << std::endl;

    test_mmap_basic_enqueue_dequeue();
    test_mmap_full_queue();
    test_mmap_bulk_operations();
    test_mmap_bulk_wraparound();
    test_mmap_blocking_enqueue();
    test_mmap_blocking_dequeue();
    test_mmap_blocking_bulk_enqueue();
    test_mmap_timeout();
    test_mmap_move_semantics();
    test_mmap_blocking_rvalue_enqueue_with_movable_type();
    test_mmap_producer_consumer_stress();

    std::cout << "\n=== All MmapSPSC tests passed ===" << std::endl;
}
