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

    std::thread consumer([&source]() {
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

    std::thread producer([&sink]() {
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

    std::thread consumer([&source]() {
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

void test_mmap_blocking_rvalue_enqueue_with_movable_type() {
    std::cout << "Testing test_mmap_blocking_rvalue_enqueue_with_movable_type..." << std::endl;

    auto [sink, source] = MmapSPSC<std::unique_ptr<int>, 8>::create();

    // Fill queue to capacity (7 elements, since 1 slot is reserved)
    for (int i = 0; i < 7; ++i) {
        affirm(sink.try_enqueue(std::make_unique<int>(i)));
    }
    affirm(!sink.empty());

    // Consumer thread: sleep briefly, then drain the queue
    std::thread consumer([&source]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        for (int i = 0; i < 7; ++i) {
            auto val = source.try_dequeue();
            affirm(val.has_value());
            affirm(*val.value() == i);
        }
    });

    // Producer thread: attempt blocking rvalue enqueue with unique_ptr
    // This should eventually succeed once the consumer drains space
    auto ptr = std::make_unique<int>(99);
    auto start = std::chrono::steady_clock::now();
    bool success = sink.enqueue(std::move(ptr), std::chrono::milliseconds(500));
    auto elapsed = std::chrono::steady_clock::now() - start;

    affirm(success);
    affirm(elapsed >= std::chrono::milliseconds(40));
    // After successful enqueue, ptr should be moved-from
    affirm(ptr == nullptr);

    // Verify the consumer received the moved value correctly
    auto received = source.try_dequeue();
    affirm(received.has_value());
    // This assertion will fail if ptr was moved-from multiple times (the bug)
    affirm(*received.value() == 99);

    consumer.join();

    std::cout << "  PASSED: test_mmap_blocking_rvalue_enqueue_with_movable_type" << std::endl;
}

void test_mmap_producer_consumer_stress() {
    constexpr std::size_t total_items = 100000;
    std::cout << "Testing test_mmap_producer_consumer_stress..." << std::endl;

    auto [sink, source] = MmapSPSC<int, 4096>::create();

    std::thread producer([&sink]() {
        for (std::size_t i = 0; i < total_items; ++i) {
            while (!sink.try_enqueue(static_cast<int>(i))) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&source]() {
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
