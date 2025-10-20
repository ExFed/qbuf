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

void test_bulk_enqueue_dequeue() {
    std::cout << "Testing bulk enqueue/dequeue..." << std::endl;
    SPSCQueue<int, 16> queue;

    std::vector<int> input = {10, 20, 30, 40, 50, 60, 70, 80};
    std::vector<int> output(input.size());

    // Enqueue all elements
    std::size_t enqueued = queue.try_enqueue_bulk(input.data(), input.size());
    assert(enqueued == input.size());
    assert(queue.size() == input.size());

    // Dequeue all elements
    std::size_t dequeued = queue.try_dequeue_bulk(output.data(), output.size());
    assert(dequeued == input.size());
    assert(queue.empty());

    // Verify order
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(output[i] == input[i]);
    }

    std::cout << "  PASSED: bulk enqueue/dequeue" << std::endl;
}

void test_bulk_partial() {
    std::cout << "Testing partial bulk operations..." << std::endl;
    SPSCQueue<int, 16> queue;

    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8};

    // Enqueue first batch
    std::size_t enqueued1 = queue.try_enqueue_bulk(input.data(), 4);
    assert(enqueued1 == 4);

    // Dequeue partial
    std::vector<int> output1(2);
    std::size_t dequeued1 = queue.try_dequeue_bulk(output1.data(), 2);
    assert(dequeued1 == 2);
    assert(output1[0] == 1);
    assert(output1[1] == 2);

    // Enqueue more
    std::size_t enqueued2 = queue.try_enqueue_bulk(&input[4], 4);
    assert(enqueued2 == 4);

    // Dequeue remaining
    std::vector<int> output2(6);
    std::size_t dequeued2 = queue.try_dequeue_bulk(output2.data(), 6);
    assert(dequeued2 == 6);

    // Verify
    for (int i = 0; i < 6; ++i) {
        assert(output2[i] == (3 + i));
    }

    std::cout << "  PASSED: partial bulk operations" << std::endl;
}

void test_bulk_full_queue() {
    std::cout << "Testing bulk enqueue on full queue..." << std::endl;
    SPSCQueue<int, 8> queue;

    std::vector<int> input1 = {1, 2, 3, 4, 5, 6};
    std::vector<int> input2 = {7, 8, 9, 10};

    // Fill most of the queue (capacity is 8, so we can store 7)
    std::size_t enqueued1 = queue.try_enqueue_bulk(input1.data(), input1.size());
    assert(enqueued1 == 6);

    // Try to enqueue more than available space
    std::size_t enqueued2 = queue.try_enqueue_bulk(input2.data(), input2.size());
    assert(enqueued2 == 1); // Only 1 slot available

    // Verify size
    assert(queue.size() == 7);

    std::cout << "  PASSED: bulk enqueue on full queue" << std::endl;
}

void test_bulk_empty_dequeue() {
    std::cout << "Testing bulk dequeue from empty queue..." << std::endl;
    SPSCQueue<int, 16> queue;

    std::vector<int> output(10);
    std::size_t dequeued = queue.try_dequeue_bulk(output.data(), 10);
    assert(dequeued == 0);
    assert(queue.empty());

    std::cout << "  PASSED: bulk dequeue from empty queue" << std::endl;
}

void test_bulk_wrap_around() {
    std::cout << "Testing bulk operations with wrap-around..." << std::endl;
    SPSCQueue<int, 8> queue;

    // Fill, partially consume, then fill again to create wrap-around
    std::vector<int> batch1 = {1, 2, 3, 4};
    std::vector<int> batch2 = {5, 6};
    std::vector<int> batch3 = {7, 8, 9, 10};
    std::vector<int> output(10);

    // Enqueue first batch
    std::size_t e1 = queue.try_enqueue_bulk(batch1.data(), batch1.size());
    assert(e1 == 4);

    // Consume 2 elements
    std::size_t d1 = queue.try_dequeue_bulk(output.data(), 2);
    assert(d1 == 2);
    assert(output[0] == 1 && output[1] == 2);

    // Enqueue more
    std::size_t e2 = queue.try_enqueue_bulk(batch2.data(), batch2.size());
    assert(e2 == 2);

    // Now queue should have: [3, 4, 5, 6] with tail wrapped
    std::size_t size = queue.size();
    assert(size == 4);

    // Enqueue another batch (this should handle wrap-around)
    std::size_t e3 = queue.try_enqueue_bulk(batch3.data(), batch3.size());
    assert(e3 == 3); // Can only fit 3 more (capacity is 8, 1 reserved)

    // Dequeue all
    std::vector<int> final_output(7);
    std::size_t d2 = queue.try_dequeue_bulk(final_output.data(), 7);
    assert(d2 == 7);

    // Verify order: 3, 4, 5, 6, 7, 8, 9
    int expected[] = {3, 4, 5, 6, 7, 8, 9};
    for (int i = 0; i < 7; ++i) {
        assert(final_output[i] == expected[i]);
    }

    std::cout << "  PASSED: bulk operations with wrap-around" << std::endl;
}

void test_bulk_with_strings() {
    std::cout << "Testing bulk operations with strings..." << std::endl;
    SPSCQueue<std::string, 16> queue;

    std::vector<std::string> input = {"hello", "world", "test", "bulk"};
    std::vector<std::string> output(input.size());

    std::size_t enqueued = queue.try_enqueue_bulk(input.data(), input.size());
    assert(enqueued == input.size());

    std::size_t dequeued = queue.try_dequeue_bulk(output.data(), output.size());
    assert(dequeued == input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(output[i] == input[i]);
    }

    std::cout << "  PASSED: bulk operations with strings" << std::endl;
}

void test_bulk_concurrent() {
    std::cout << "Testing concurrent bulk operations..." << std::endl;
    SPSCQueue<int, 512> queue;
    constexpr int num_batches = 50;
    constexpr int batch_size = 20;
    constexpr int total_elements = num_batches * batch_size;
    std::atomic<bool> producer_done{false};

    // Producer: enqueue in batches
    std::thread producer([&queue, &producer_done]() {
        for (int b = 0; b < num_batches; ++b) {
            std::vector<int> batch(batch_size);
            for (int i = 0; i < batch_size; ++i) {
                batch[i] = b * batch_size + i;
            }
            std::size_t enqueued = 0;
            while (enqueued < batch_size) {
                enqueued += queue.try_enqueue_bulk(batch.data() + enqueued,
                                                     batch_size - enqueued);
                if (enqueued < batch_size) {
                    std::this_thread::yield();
                }
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer: dequeue in batches
    std::vector<int> consumed;
    std::thread consumer([&queue, &consumed, &producer_done]() {
        std::vector<int> buffer(batch_size);
        int total = 0;
        while (total < total_elements) {
            std::size_t dequeued = queue.try_dequeue_bulk(buffer.data(), batch_size);
            for (std::size_t i = 0; i < dequeued; ++i) {
                consumed.push_back(buffer[i]);
            }
            total += dequeued;

            if (dequeued == 0 && !producer_done.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify
    assert(consumed.size() == total_elements);
    for (int i = 0; i < total_elements; ++i) {
        assert(consumed[i] == i);
    }
    assert(queue.empty());

    std::cout << "  PASSED: concurrent bulk operations" << std::endl;
}

int main() {
    std::cout << "\n=== Running SPSCQueue Tests ===" << std::endl;
    try {
        test_basic_operations();
        test_queue_full();
        test_fifo_ordering();
        test_move_semantics();
        test_concurrent();
        test_bulk_enqueue_dequeue();
        test_bulk_partial();
        test_bulk_full_queue();
        test_bulk_empty_dequeue();
        test_bulk_wrap_around();
        test_bulk_with_strings();
        test_bulk_concurrent();

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
