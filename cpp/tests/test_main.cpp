#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <ringbuf/spsc_queue.hpp>

using namespace ringbuf;

void test_basic_operations() {
    std::cout << "Testing basic operations..." << std::endl;
    SPSCQueue<int, 8> queue;

    // Test empty queue
    assert(queue.empty());
    assert(queue.size() == 0);

    // Test push and pop single element
    assert(queue.try_enqueue(42));
    assert(!queue.empty());
    assert(queue.size() == 1);

    auto value = queue.try_dequeue();
    assert(value.has_value());
    assert(value.value() == 42);
    assert(queue.empty());

    // Test pop from empty queue
    value = queue.try_dequeue();
    assert(!value.has_value());

    // Test push multiple elements
    assert(queue.try_enqueue(1));
    assert(queue.try_enqueue(2));
    assert(queue.try_enqueue(3));
    assert(queue.size() == 3);

    assert(queue.try_dequeue().value() == 1);
    assert(queue.try_dequeue().value() == 2);
    assert(queue.try_dequeue().value() == 3);
    assert(queue.empty());

    std::cout << "  PASSED: basic operations" << std::endl;
}

void test_queue_full() {
    std::cout << "Testing queue full condition..." << std::endl;
    SPSCQueue<int, 8> queue;

    // Capacity is 8, but we can only store 7 elements (one slot reserved)
    for (int i = 0; i < 7; ++i) {
        assert(queue.try_enqueue(i));
    }

    // Next push should fail
    assert(!queue.try_enqueue(999));

    std::cout << "  PASSED: queue full" << std::endl;
}

void test_fifo_ordering() {
    std::cout << "Testing FIFO ordering..." << std::endl;
    SPSCQueue<int, 8> queue;

    std::vector<int> input = {10, 20, 30, 40, 50};

    for (int val : input) {
        assert(queue.try_enqueue(val));
    }

    for (int expected : input) {
        auto value = queue.try_dequeue();
        assert(value.has_value());
        assert(value.value() == expected);
    }

    std::cout << "  PASSED: FIFO ordering" << std::endl;
}

void test_move_semantics() {
    std::cout << "Testing move semantics..." << std::endl;
    SPSCQueue<std::string, 8> queue;

    std::string str = "Hello, World!";
    assert(queue.try_enqueue(std::move(str)));

    auto value = queue.try_dequeue();
    assert(value.has_value());
    assert(value.value() == "Hello, World!");

    // Multiple string operations
    assert(queue.try_enqueue("First"));
    assert(queue.try_enqueue("Second"));
    assert(queue.try_enqueue("Third"));

    assert(queue.try_dequeue().value() == "First");
    assert(queue.try_dequeue().value() == "Second");
    assert(queue.try_dequeue().value() == "Third");

    std::cout << "  PASSED: move semantics" << std::endl;
}

void test_concurrent() {
    std::cout << "Testing concurrent producer-consumer..." << std::endl;
    SPSCQueue<int, 256> queue;
    constexpr int num_elements = 1000;
    std::atomic<bool> producer_done{false};

    // Producer thread
    std::thread producer([&queue, &producer_done]() {
        for (int i = 0; i < num_elements; ++i) {
            while (!queue.try_enqueue(i)) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::vector<int> consumed;
    std::thread consumer([&queue, &consumed, &producer_done]() {
        int count = 0;
        while (count < num_elements) {
            auto value = queue.try_dequeue();
            if (value.has_value()) {
                consumed.push_back(value.value());
                ++count;
            } else if (producer_done.load(std::memory_order_acquire)) {
                // Producer is done, try one more time
                continue;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify all elements were consumed in order
    assert(consumed.size() == num_elements);
    for (int i = 0; i < num_elements; ++i) {
        assert(consumed[i] == i);
    }
    assert(queue.empty());

    std::cout << "  PASSED: concurrent producer-consumer" << std::endl;
}

int main() {
    std::cout << "\n=== Running SPSCQueue Tests ===" << std::endl;
    try {
        test_basic_operations();
        test_queue_full();
        test_fifo_ordering();
        test_move_semantics();
        test_concurrent();

        std::cout << "\n=== All tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nTest failed with unknown exception" << std::endl;
        return 1;
    }
}
