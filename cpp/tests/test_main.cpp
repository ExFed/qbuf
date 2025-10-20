#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <qbuf/spsc.hpp>

using namespace qbuf;

void test_basic_operations() {
    std::cout << "Testing basic operations..." << std::endl;
    SPSC<int, 8> queue;

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
    SPSC<int, 8> queue;

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
    SPSC<int, 8> queue;

    std::vector<int> input = {10, 20, 30, 40, 50};
    std::vector<int> output;

    for (int val : input) {
        assert(queue.try_enqueue(val));
    }

    for (int expected : input) {
        auto value = queue.try_dequeue();
        assert(value.has_value());
        assert(value.value() == expected);
        output.push_back(value.value());
    }

    // Verify inputs produced match outputs consumed in same order
    assert(input.size() == output.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(input[i] == output[i]);
    }

    std::cout << "  PASSED: FIFO ordering" << std::endl;
}

void test_move_semantics() {
    std::cout << "Testing move semantics..." << std::endl;
    SPSC<std::string, 8> queue;

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
    SPSC<int, 256> queue;
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
    SPSC<int, 16> queue;

    std::vector<int> input = {10, 20, 30, 40, 50, 60, 70, 80};
    std::vector<int> output(input.size());

    // Enqueue all elements
    std::size_t enqueued = queue.try_enqueue(input.data(), input.size());
    assert(enqueued == input.size());
    assert(queue.size() == input.size());

    // Dequeue all elements
    std::size_t dequeued = queue.try_dequeue(output.data(), output.size());
    assert(dequeued == input.size());
    assert(queue.empty());

    // Verify inputs produced match outputs consumed in same order
    assert(input.size() == output.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(output[i] == input[i]);
    }
    // Verify each element individually
    for (std::size_t i = 0; i < output.size(); ++i) {
        assert(output.at(i) == input.at(i));
    }

    std::cout << "  PASSED: bulk enqueue/dequeue" << std::endl;
}

void test_bulk_partial() {
    std::cout << "Testing partial bulk operations..." << std::endl;
    SPSC<int, 16> queue;

    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<int> consumed_output;

    // Enqueue first batch
    std::size_t enqueued1 = queue.try_enqueue(input.data(), 4);
    assert(enqueued1 == 4);

    // Dequeue partial
    std::vector<int> output1(2);
    std::size_t dequeued1 = queue.try_dequeue(output1.data(), 2);
    assert(dequeued1 == 2);
    assert(output1[0] == 1);
    assert(output1[1] == 2);
    assert(output1[0] == input[0]);
    assert(output1[1] == input[1]);
    consumed_output.insert(consumed_output.end(), output1.begin(), output1.end());

    // Enqueue more
    std::size_t enqueued2 = queue.try_enqueue(&input[4], 4);
    assert(enqueued2 == 4);

    // Dequeue remaining
    std::vector<int> output2(6);
    std::size_t dequeued2 = queue.try_dequeue(output2.data(), 6);
    assert(dequeued2 == 6);
    consumed_output.insert(consumed_output.end(), output2.begin(), output2.begin() + 6);

    // Verify all consumed elements match original input order
    for (std::size_t i = 0; i < consumed_output.size(); ++i) {
        assert(consumed_output[i] == input[i]);
    }
    // Verify
    for (int i = 0; i < 6; ++i) {
        assert(output2[i] == (3 + i));
    }

    std::cout << "  PASSED: partial bulk operations" << std::endl;
}

void test_bulk_full_queue() {
    std::cout << "Testing bulk enqueue on full queue..." << std::endl;
    SPSC<int, 8> queue;

    std::vector<int> input1 = {1, 2, 3, 4, 5, 6};
    std::vector<int> input2 = {7, 8, 9, 10};

    // Fill most of the queue (capacity is 8, so we can store 7)
    std::size_t enqueued1 = queue.try_enqueue(input1.data(), input1.size());
    assert(enqueued1 == 6);

    // Try to enqueue more than available space
    std::size_t enqueued2 = queue.try_enqueue(input2.data(), input2.size());
    assert(enqueued2 == 1); // Only 1 slot available

    // Verify size
    assert(queue.size() == 7);

    std::cout << "  PASSED: bulk enqueue on full queue" << std::endl;
}

void test_bulk_empty_dequeue() {
    std::cout << "Testing bulk dequeue from empty queue..." << std::endl;
    SPSC<int, 16> queue;

    std::vector<int> output(10);
    std::size_t dequeued = queue.try_dequeue(output.data(), 10);
    assert(dequeued == 0);
    assert(queue.empty());

    std::cout << "  PASSED: bulk dequeue from empty queue" << std::endl;
}

void test_bulk_wrap_around() {
    std::cout << "Testing bulk operations with wrap-around..." << std::endl;
    SPSC<int, 8> queue;

    // Fill, partially consume, then fill again to create wrap-around
    std::vector<int> batch1 = {1, 2, 3, 4};
    std::vector<int> batch2 = {5, 6};
    std::vector<int> batch3 = {7, 8, 9, 10};
    std::vector<int> output(10);
    std::vector<int> expected_order = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    // Enqueue first batch
    std::size_t e1 = queue.try_enqueue(batch1.data(), batch1.size());
    assert(e1 == 4);

    // Consume 2 elements
    std::size_t d1 = queue.try_dequeue(output.data(), 2);
    assert(d1 == 2);
    assert(output[0] == 1 && output[1] == 2);
    assert(output[0] == batch1[0] && output[1] == batch1[1]);

    // Enqueue more
    std::size_t e2 = queue.try_enqueue(batch2.data(), batch2.size());
    assert(e2 == 2);

    // Now queue should have: [3, 4, 5, 6] with tail wrapped
    std::size_t size = queue.size();
    assert(size == 4);

    // Enqueue another batch (this should handle wrap-around)
    std::size_t e3 = queue.try_enqueue(batch3.data(), batch3.size());
    assert(e3 == 3); // Can only fit 3 more (capacity is 8, 1 reserved)

    // Dequeue all
    std::vector<int> final_output(7);
    std::size_t d2 = queue.try_dequeue(final_output.data(), 7);
    assert(d2 == 7);

    // Verify order: 3, 4, 5, 6, 7, 8, 9
    int expected[] = {3, 4, 5, 6, 7, 8, 9};
    for (int i = 0; i < 7; ++i) {
        assert(final_output[i] == expected[i]);
    }

    // Verify full consumption matches expected order
    assert(expected_order[0] == output[0]);
    assert(expected_order[1] == output[1]);
    for (int i = 0; i < 7; ++i) {
        assert(expected_order[2 + i] == final_output[i]);
    }

    std::cout << "  PASSED: bulk operations with wrap-around" << std::endl;
}

void test_bulk_with_strings() {
    std::cout << "Testing bulk operations with strings..." << std::endl;
    SPSC<std::string, 16> queue;

    std::vector<std::string> input = {"hello", "world", "test", "bulk"};
    std::vector<std::string> output(input.size());

    std::size_t enqueued = queue.try_enqueue(input.data(), input.size());
    assert(enqueued == input.size());

    std::size_t dequeued = queue.try_dequeue(output.data(), output.size());
    assert(dequeued == input.size());

    // Verify inputs produced match outputs consumed in same order
    assert(input.size() == output.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(output[i] == input[i]);
        assert(output.at(i) == input.at(i));
    }

    std::cout << "  PASSED: bulk operations with strings" << std::endl;
}

void test_bulk_concurrent() {
    std::cout << "Testing concurrent bulk operations..." << std::endl;
    SPSC<int, 512> queue;
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
                enqueued += queue.try_enqueue(batch.data() + enqueued,
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
            std::size_t dequeued = queue.try_dequeue(buffer.data(), batch_size);
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

void test_blocking_enqueue() {
    std::cout << "Testing blocking enqueue..." << std::endl;
    SPSC<int, 8> queue;

    // Fill the queue to near capacity (7 elements max)
    for (int i = 0; i < 7; ++i) {
        assert(queue.enqueue(i, std::chrono::seconds(5)));
    }
    assert(queue.size() == 7);

    // Producer thread tries to enqueue (will block until consumer makes space)
    std::atomic<bool> producer_done{false};
    std::thread producer([&queue, &producer_done]() {
        assert(queue.enqueue(99, std::chrono::seconds(5))); // Will block until space available
        producer_done.store(true, std::memory_order_release);
    });

    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Consumer dequeues one element, unblocking the producer
    auto value = queue.try_dequeue();
    assert(value.has_value());
    assert(value.value() == 0);

    // Wait for producer to complete
    producer.join();
    assert(producer_done.load());
    assert(queue.size() == 7); // Should have 1-6 and 99

    std::cout << "  PASSED: blocking enqueue" << std::endl;
}

void test_blocking_dequeue() {
    std::cout << "Testing blocking dequeue..." << std::endl;
    SPSC<int, 16> queue;

    std::atomic<int> dequeued_value{-1};
    std::atomic<bool> consumer_done{false};

    // Consumer thread tries to dequeue from empty queue (will block)
    std::thread consumer([&queue, &dequeued_value, &consumer_done]() {
        auto value = queue.dequeue(std::chrono::seconds(5));
        if (value.has_value()) {
            dequeued_value.store(value.value(), std::memory_order_release);
        }
        consumer_done.store(true, std::memory_order_release);
    });

    // Give consumer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(!consumer_done.load()); // Consumer should still be blocked

    // Producer enqueues an element, unblocking consumer
    assert(queue.enqueue(42, std::chrono::seconds(5)));

    // Wait for consumer to complete
    consumer.join();
    assert(consumer_done.load());
    assert(dequeued_value.load() == 42);
    assert(queue.empty());

    std::cout << "  PASSED: blocking dequeue" << std::endl;
}

void test_blocking_concurrent() {
    std::cout << "Testing blocking concurrent operations..." << std::endl;
    SPSC<int, 256> queue;
    constexpr int num_elements = 1000;

    // Producer thread
    std::thread producer([&queue]() {
        for (int i = 0; i < num_elements; ++i) {
            assert(queue.enqueue(i, std::chrono::seconds(5)));
        }
    });

    // Consumer thread
    std::vector<int> consumed;
    std::thread consumer([&queue, &consumed]() {
        for (int i = 0; i < num_elements; ++i) {
            auto value = queue.dequeue(std::chrono::seconds(5));
            assert(value.has_value());
            consumed.push_back(value.value());
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

    std::cout << "  PASSED: blocking concurrent operations" << std::endl;
}

void test_blocking_with_strings() {
    std::cout << "Testing blocking operations with strings..." << std::endl;
    SPSC<std::string, 16> queue;

    std::string result;
    std::atomic<bool> done{false};

    // Consumer thread dequeues (will block initially)
    std::thread consumer([&queue, &result, &done]() {
        auto value = queue.dequeue(std::chrono::seconds(5));
        if (value.has_value()) {
            result = value.value();
        }
        done.store(true, std::memory_order_release);
    });

    // Give consumer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Producer enqueues
    assert(queue.enqueue(std::string("Hello, World!"), std::chrono::seconds(5)));

    consumer.join();
    assert(done.load());
    assert(result == "Hello, World!");
    assert(queue.empty());

    std::cout << "  PASSED: blocking operations with strings" << std::endl;
}

void test_blocking_stress() {
    std::cout << "Testing blocking operations under stress..." << std::endl;
    SPSC<int, 64> queue;
    constexpr int total_ops = 10000;
    std::atomic<int> consumed_count{0};

    // Producer: continuously enqueue
    std::thread producer([&queue]() {
        for (int i = 0; i < total_ops; ++i) {
            assert(queue.enqueue(i, std::chrono::seconds(5)));
        }
    });

    // Consumer: continuously dequeue
    std::thread consumer([&queue, &consumed_count]() {
        for (int i = 0; i < total_ops; ++i) {
            auto value = queue.dequeue(std::chrono::seconds(5));
            assert(value.has_value());
            assert(value.value() == i);
            consumed_count.fetch_add(1, std::memory_order_release);
        }
    });

    producer.join();
    consumer.join();

    assert(consumed_count.load() == total_ops);
    assert(queue.empty());

    std::cout << "  PASSED: blocking stress test" << std::endl;
}

void test_blocking_bulk_enqueue() {
    std::cout << "Testing blocking bulk enqueue..." << std::endl;
    SPSC<int, 16> queue;

    // Fill most of the queue
    std::vector<int> initial_batch = {1, 2, 3, 4, 5, 6, 7};
    assert(queue.enqueue(initial_batch.data(), initial_batch.size(), std::chrono::seconds(5)));
    assert(queue.size() == 7);

    // Try to enqueue a large batch (will block until consumer drains)
    std::vector<int> large_batch(20);
    for (int i = 0; i < 20; ++i) {
        large_batch[i] = 100 + i;
    }

    std::atomic<bool> producer_done{false};
    std::thread producer([&queue, &large_batch, &producer_done]() {
        assert(queue.enqueue(large_batch.data(), large_batch.size(), std::chrono::seconds(5)));
        producer_done.store(true, std::memory_order_release);
    });

    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(!producer_done.load()); // Should still be blocked

    // Consumer drains the queue gradually
    std::vector<int> consumed;
    int total_to_drain = 27;  // 7 initial + 20 large_batch
    while (consumed.size() < total_to_drain) {
        auto value = queue.try_dequeue();
        if (value.has_value()) {
            consumed.push_back(value.value());
        } else {
            std::this_thread::yield();
        }
    }

    producer.join();
    assert(producer_done.load());

    // Verify all elements
    assert(consumed.size() == 27);
    for (int i = 0; i < 7; ++i) {
        assert(consumed[i] == i + 1);
    }
    for (int i = 0; i < 20; ++i) {
        assert(consumed[7 + i] == 100 + i);
    }

    std::cout << "  PASSED: blocking bulk enqueue" << std::endl;
}

void test_blocking_bulk_dequeue() {
    std::cout << "Testing blocking bulk dequeue..." << std::endl;
    SPSC<int, 128> queue;

    std::vector<int> output(50);
    std::atomic<bool> consumer_done{false};

    // Consumer thread tries to dequeue 50 elements (will block)
    std::thread consumer([&queue, &output, &consumer_done]() {
        std::size_t dequeued = queue.dequeue(output.data(), 50, std::chrono::seconds(5));
        assert(dequeued == 50);
        consumer_done.store(true, std::memory_order_release);
    });

    // Give consumer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(!consumer_done.load()); // Should be blocked

    // Producer enqueues all 50 elements in batches
    std::vector<int> batch1(25);
    std::vector<int> batch2(25);
    for (int i = 0; i < 25; ++i) {
        batch1[i] = i;
        batch2[i] = 25 + i;
    }

    assert(queue.enqueue(batch1.data(), 25, std::chrono::seconds(5)));
    assert(queue.enqueue(batch2.data(), 25, std::chrono::seconds(5)));

    // Wait for consumer to complete
    consumer.join();
    assert(consumer_done.load());

    // Verify all elements
    for (int i = 0; i < 50; ++i) {
        assert(output[i] == i);
    }
    assert(queue.empty());

    std::cout << "  PASSED: blocking bulk dequeue" << std::endl;
}

void test_blocking_bulk_concurrent() {
    std::cout << "Testing blocking bulk concurrent operations..." << std::endl;
    SPSC<int, 256> queue;
    constexpr int num_batches = 100;
    constexpr int batch_size = 50;
    constexpr int total_elements = num_batches * batch_size;

    // Producer: enqueue in batches using blocking API
    std::thread producer([&queue]() {
        for (int b = 0; b < num_batches; ++b) {
            std::vector<int> batch(batch_size);
            for (int i = 0; i < batch_size; ++i) {
                batch[i] = b * batch_size + i;
            }
            assert(queue.enqueue(batch.data(), batch_size, std::chrono::seconds(5)));
        }
    });

    // Consumer: dequeue in batches using blocking API
    std::vector<int> consumed;
    std::thread consumer([&queue, &consumed, total_elements]() {
        std::vector<int> batch(batch_size);
        int total_dequeued = 0;
        while (total_dequeued < total_elements) {
            int remaining = total_elements - total_dequeued;
            int to_dequeue = (remaining < batch_size) ? remaining : batch_size;
            std::size_t dequeued = queue.dequeue(batch.data(), to_dequeue, std::chrono::seconds(5));
            assert(dequeued == to_dequeue);
            for (int i = 0; i < to_dequeue; ++i) {
                consumed.push_back(batch[i]);
            }
            total_dequeued += to_dequeue;
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

    std::cout << "  PASSED: blocking bulk concurrent operations" << std::endl;
}

void test_blocking_bulk_mixed() {
    std::cout << "Testing mixed blocking and non-blocking bulk operations..." << std::endl;
    SPSC<int, 64> queue;

    std::vector<int> input1(10);
    for (int i = 0; i < 10; ++i) input1[i] = i;

    std::vector<int> input2(8);
    for (int i = 0; i < 8; ++i) input2[i] = 100 + i;

    // Enqueue with blocking bulk
    assert(queue.enqueue(input1.data(), input1.size(), std::chrono::seconds(5)));
    assert(queue.size() == 10);

    // Dequeue with non-blocking try_dequeue
    std::vector<int> output1(5);
    std::size_t dequeued = queue.try_dequeue(output1.data(), 5);
    assert(dequeued == 5);
    assert(queue.size() == 5);

    // Verify first dequeue matches first half of input1
    for (int i = 0; i < 5; ++i) {
        assert(output1[i] == input1[i]);
    }

    // Enqueue more with try_enqueue
    std::size_t enqueued = queue.try_enqueue(input2.data(), 8);
    assert(enqueued == 8);
    assert(queue.size() == 13);

    // Dequeue all remaining with blocking bulk
    std::vector<int> output2(13);
    std::size_t dequeued2 = queue.dequeue(output2.data(), 13, std::chrono::seconds(5));
    assert(dequeued2 == 13);
    assert(queue.empty());

    // Verify sequence - outputs match inputs in order
    int idx = 0;
    for (int i = 5; i < 10; ++i) {
        assert(output2[idx] == input1[i]);
        assert(output2[idx++] == i);
    }
    for (int i = 0; i < 8; ++i) {
        assert(output2[idx] == input2[i]);
        assert(output2[idx++] == 100 + i);
    }

    std::cout << "  PASSED: mixed blocking and non-blocking bulk operations" << std::endl;
}

void test_blocking_bulk_with_strings() {
    std::cout << "Testing blocking bulk with strings..." << std::endl;
    SPSC<std::string, 32> queue;

    std::vector<std::string> input = {
        "hello", "world", "blocking", "bulk", "operations",
        "are", "now", "fully", "implemented", "and", "tested"
    };
    std::vector<std::string> output(input.size());

    // Producer: enqueue all with blocking bulk
    std::thread producer([&queue, &input]() {
        assert(queue.enqueue(input.data(), input.size(), std::chrono::seconds(5)));
    });

    // Consumer: dequeue all with blocking bulk
    std::thread consumer([&queue, &output]() {
        std::size_t dequeued = queue.dequeue(output.data(), output.size(), std::chrono::seconds(5));
        assert(dequeued == output.size());
    });

    producer.join();
    consumer.join();

    // Verify inputs produced match outputs consumed in same order
    assert(output.size() == input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(output[i] == input[i]);
        assert(output.at(i) == input.at(i));
    }
    assert(queue.empty());

    std::cout << "  PASSED: blocking bulk with strings" << std::endl;
}

void test_enqueue_timeout_on_full() {
    std::cout << "Testing enqueue timeout when queue is full..." << std::endl;
    SPSC<int, 8> queue;

    // Fill the queue to capacity (7 elements max)
    for (int i = 0; i < 7; ++i) {
        assert(queue.try_enqueue(i));
    }
    assert(queue.size() == 7);

    // Try to enqueue with short timeout (should fail)
    bool success = queue.enqueue(999, std::chrono::milliseconds(50));
    assert(!success); // Should timeout
    assert(queue.size() == 7); // Queue unchanged

    std::cout << "  PASSED: enqueue timeout on full" << std::endl;
}

void test_enqueue_timeout_with_space() {
    std::cout << "Testing enqueue timeout when space becomes available..." << std::endl;
    SPSC<int, 8> queue;

    // Fill the queue
    for (int i = 0; i < 7; ++i) {
        assert(queue.enqueue(i, std::chrono::seconds(5)));
    }

    // Producer thread tries to enqueue with timeout
    std::atomic<bool> producer_done{false};
    std::atomic<bool> producer_success{false};
    std::thread producer([&queue, &producer_done, &producer_success]() {
        producer_success.store(queue.enqueue(999, std::chrono::seconds(2)),
                               std::memory_order_release);
        producer_done.store(true, std::memory_order_release);
    });

    // Give producer time to attempt the enqueue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Consumer makes space
    auto value = queue.try_dequeue();
    assert(value.has_value());
    assert(value.value() == 0);

    // Wait for producer to complete
    producer.join();
    assert(producer_done.load());
    assert(producer_success.load()); // Should succeed eventually

    std::cout << "  PASSED: enqueue timeout with space" << std::endl;
}

void test_dequeue_timeout_on_empty() {
    std::cout << "Testing dequeue timeout when queue is empty..." << std::endl;
    SPSC<int, 8> queue;

    assert(queue.empty());

    // Try to dequeue with short timeout (should return nullopt)
    auto value = queue.dequeue(std::chrono::milliseconds(50));
    assert(!value.has_value()); // Should timeout
    assert(queue.empty());

    std::cout << "  PASSED: dequeue timeout on empty" << std::endl;
}

void test_dequeue_timeout_with_data() {
    std::cout << "Testing dequeue timeout when data becomes available..." << std::endl;
    SPSC<int, 8> queue;

    // Consumer thread tries to dequeue with timeout
    std::atomic<bool> consumer_done{false};
    std::atomic<int> dequeued_value{-1};
    std::thread consumer([&queue, &consumer_done, &dequeued_value]() {
        auto value = queue.dequeue(std::chrono::seconds(2));
        if (value.has_value()) {
            dequeued_value.store(value.value(), std::memory_order_release);
        }
        consumer_done.store(true, std::memory_order_release);
    });

    // Give consumer time to attempt the dequeue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Producer enqueues an element
    assert(queue.enqueue(42, std::chrono::seconds(5)));

    // Wait for consumer to complete
    consumer.join();
    assert(consumer_done.load());
    assert(dequeued_value.load() == 42); // Should succeed eventually

    std::cout << "  PASSED: dequeue timeout with data" << std::endl;
}

void test_bulk_enqueue_timeout_on_full() {
    std::cout << "Testing bulk enqueue timeout when queue is full..." << std::endl;
    SPSC<int, 8> queue;

    // Fill the queue to capacity (7 elements max for capacity 8)
    std::vector<int> initial(7);
    for (int i = 0; i < 7; ++i) initial[i] = i;
    assert(queue.enqueue(initial.data(), initial.size(), std::chrono::seconds(5)));

    // Try to enqueue bulk with short timeout (should fail)
    std::vector<int> batch = {100, 101, 102, 103};
    bool success = queue.enqueue(batch.data(), batch.size(),
                                 std::chrono::milliseconds(50));
    assert(!success); // Should timeout
    assert(queue.size() == 7); // Queue unchanged

    std::cout << "  PASSED: bulk enqueue timeout on full" << std::endl;
}

void test_bulk_enqueue_timeout_with_space() {
    std::cout << "Testing bulk enqueue timeout when space becomes available..." << std::endl;
    SPSC<int, 16> queue;

    // Fill most of the queue
    std::vector<int> initial(7);
    for (int i = 0; i < 7; ++i) initial[i] = i;
    assert(queue.enqueue(initial.data(), initial.size(), std::chrono::seconds(5)));

    // Producer thread tries to enqueue bulk with timeout
    std::vector<int> large_batch(10);
    for (int i = 0; i < 10; ++i) large_batch[i] = 100 + i;

    std::atomic<bool> producer_done{false};
    std::atomic<bool> producer_success{false};
    std::thread producer([&queue, &large_batch, &producer_done, &producer_success]() {
        producer_success.store(queue.enqueue(large_batch.data(), large_batch.size(),
                                             std::chrono::seconds(2)),
                               std::memory_order_release);
        producer_done.store(true, std::memory_order_release);
    });

    // Give producer time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Consumer gradually drains the queue
    for (int i = 0; i < 3; ++i) {
        auto val = queue.try_dequeue();
        assert(val.has_value());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    producer.join();
    assert(producer_done.load());
    assert(producer_success.load()); // Should succeed eventually

    std::cout << "  PASSED: bulk enqueue timeout with space" << std::endl;
}

void test_bulk_dequeue_timeout_on_empty() {
    std::cout << "Testing bulk dequeue timeout when queue is empty..." << std::endl;
    SPSC<int, 16> queue;

    assert(queue.empty());

    // Try to dequeue bulk with short timeout
    std::vector<int> output(10);
    std::size_t dequeued = queue.dequeue(output.data(), output.size(),
                                         std::chrono::milliseconds(50));
    assert(dequeued == 0); // Should timeout with no data
    assert(queue.empty());

    std::cout << "  PASSED: bulk dequeue timeout on empty" << std::endl;
}

void test_bulk_dequeue_timeout_with_partial_data() {
    std::cout << "Testing bulk dequeue timeout with partial data..." << std::endl;
    SPSC<int, 16> queue;

    // Enqueue fewer elements than requested
    std::vector<int> initial = {1, 2, 3};
    assert(queue.enqueue(initial.data(), initial.size(), std::chrono::seconds(5)));

    // Consumer thread tries to dequeue more than available
    std::vector<int> output(10);
    std::atomic<bool> consumer_done{false};
    std::atomic<std::size_t> dequeued_count{0};

    std::thread consumer([&queue, &output, &consumer_done, &dequeued_count]() {
        std::size_t dequeued = queue.dequeue(output.data(), output.size(),
                                             std::chrono::milliseconds(100));
        dequeued_count.store(dequeued, std::memory_order_release);
        consumer_done.store(true, std::memory_order_release);
    });

    consumer.join();
    assert(consumer_done.load());
    assert(dequeued_count.load() == 3); // Should return what was available

    std::cout << "  PASSED: bulk dequeue timeout with partial data" << std::endl;
}

void test_graceful_shutdown_with_enqueue_timeout() {
    std::cout << "Testing graceful producer shutdown with timeout..." << std::endl;
    SPSC<int, 64> queue;
    std::atomic<bool> shutdown{false};
    constexpr int target_elements = 100;
    std::atomic<int> enqueued_count{0};

    // Producer that respects shutdown signal using timeouts
    std::thread producer([&queue, &shutdown, &enqueued_count]() {
        for (int i = 0; i < target_elements; ++i) {
            // Try to enqueue with a timeout that allows periodic shutdown checks
            bool success = queue.enqueue(i, std::chrono::milliseconds(50));
            if (success) {
                enqueued_count.fetch_add(1, std::memory_order_release);
            } else if (shutdown.load(std::memory_order_acquire)) {
                // Timeout occurred and shutdown requested
                break;
            }
            // If timeout occurred but shutdown not requested, retry
        }
    });

    // Let producer run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Signal shutdown
    shutdown.store(true, std::memory_order_release);

    // Wait for producer to exit gracefully
    producer.join();

    // Producer should have enqueued some elements before shutdown
    int enqueued = enqueued_count.load(std::memory_order_acquire);
    assert(enqueued > 0);
    assert(enqueued <= target_elements);

    std::cout << "  PASSED: graceful producer shutdown with timeout" << std::endl;
}

void test_graceful_shutdown_with_dequeue_timeout() {
    std::cout << "Testing graceful consumer shutdown with timeout..." << std::endl;
    SPSC<int, 64> queue;
    std::atomic<bool> shutdown{false};
    std::atomic<int> dequeued_count{0};

    // Producer thread
    std::thread producer([&queue, &shutdown]() {
        for (int i = 0; i < 50; ++i) {
            assert(queue.enqueue(i, std::chrono::seconds(5)));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Consumer that respects shutdown signal using timeouts
    std::thread consumer([&queue, &shutdown, &dequeued_count]() {
        while (!shutdown.load(std::memory_order_acquire)) {
            // Try to dequeue with a timeout that allows shutdown checks
            auto value = queue.dequeue(std::chrono::milliseconds(50));
            if (value.has_value()) {
                dequeued_count.fetch_add(1, std::memory_order_release);
            }
            // Timeout or no value means we can check shutdown flag again
        }
    });

    // Let threads run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal shutdown
    shutdown.store(true, std::memory_order_release);

    // Wait for threads to exit gracefully
    producer.join();
    consumer.join();

    // Consumer should have dequeued some elements
    int dequeued = dequeued_count.load(std::memory_order_acquire);
    assert(dequeued > 0);

    std::cout << "  PASSED: graceful consumer shutdown with timeout" << std::endl;
}

void test_graceful_shutdown_with_bulk_operations() {
    std::cout << "Testing graceful shutdown with bulk operations..." << std::endl;
    SPSC<int, 128> queue;
    std::atomic<bool> producer_shutdown{false};
    std::atomic<bool> consumer_shutdown{false};
    std::atomic<int> total_produced{0};
    std::atomic<int> total_consumed{0};

    // Producer with bulk operations and timeout-based shutdown checks
    std::thread producer([&queue, &producer_shutdown, &total_produced]() {
        for (int batch = 0; batch < 20 && !producer_shutdown.load(std::memory_order_acquire); ++batch) {
            std::vector<int> batch_data(10);
            for (int i = 0; i < 10; ++i) {
                batch_data[i] = batch * 10 + i;
            }

            bool success = queue.enqueue(batch_data.data(), batch_data.size(),
                                        std::chrono::milliseconds(100));
            if (success) {
                total_produced.fetch_add(10, std::memory_order_release);
            } else if (!producer_shutdown.load(std::memory_order_acquire)) {
                // Timeout but not shutdown, retry
                --batch;
            }
        }
    });

    // Consumer with bulk operations and timeout-based shutdown checks
    std::thread consumer([&queue, &consumer_shutdown, &total_consumed]() {
        std::vector<int> buffer(20);
        while (!consumer_shutdown.load(std::memory_order_acquire)) {
            std::size_t dequeued = queue.dequeue(buffer.data(), buffer.size(),
                                                 std::chrono::milliseconds(100));
            if (dequeued > 0) {
                total_consumed.fetch_add(dequeued, std::memory_order_release);
            }
        }

        // Final drain before exit
        std::vector<int> final_buffer(50);
        while (true) {
            std::size_t dequeued = queue.dequeue(final_buffer.data(), final_buffer.size(),
                                                 std::chrono::milliseconds(10));
            if (dequeued == 0) break;
            total_consumed.fetch_add(dequeued, std::memory_order_release);
        }
    });

    // Let them run
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Signal shutdown
    producer_shutdown.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    consumer_shutdown.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    // Verify data consistency
    int produced = total_produced.load(std::memory_order_acquire);
    int consumed = total_consumed.load(std::memory_order_acquire);
    assert(produced > 0);
    assert(produced == consumed);

    std::cout << "  PASSED: graceful shutdown with bulk operations" << std::endl;
}

void test_move_semantics_with_timeout() {
    std::cout << "Testing move semantics with enqueue timeout..." << std::endl;
    SPSC<std::string, 8> queue;

    // Fill the queue
    for (int i = 0; i < 7; ++i) {
        assert(queue.enqueue(std::string("element") + std::to_string(i),
                             std::chrono::seconds(5)));
    }

    // Try to enqueue with move and timeout (should fail)
    std::string to_enqueue = "timeout_test";
    bool success = queue.enqueue(std::move(to_enqueue), std::chrono::milliseconds(50));
    assert(!success); // Should timeout

    // Note: after failed enqueue with timeout, the moved string may or may not be valid
    // depending on implementation. We don't rely on it here.

    std::cout << "  PASSED: move semantics with timeout" << std::endl;
}

int main() {
    std::cout << "\n=== Running SPSC Tests ===" << std::endl;
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
        test_blocking_enqueue();
        test_blocking_dequeue();
        test_blocking_concurrent();
        test_blocking_with_strings();
        test_blocking_stress();
        test_blocking_bulk_enqueue();
        test_blocking_bulk_dequeue();
        test_blocking_bulk_concurrent();
        test_blocking_bulk_mixed();
        test_blocking_bulk_with_strings();
        test_enqueue_timeout_on_full();
        test_enqueue_timeout_with_space();
        test_dequeue_timeout_on_empty();
        test_dequeue_timeout_with_data();
        test_bulk_enqueue_timeout_on_full();
        test_bulk_enqueue_timeout_with_space();
        test_bulk_dequeue_timeout_on_empty();
        test_bulk_dequeue_timeout_with_partial_data();
        test_graceful_shutdown_with_enqueue_timeout();
        test_graceful_shutdown_with_dequeue_timeout();
        test_graceful_shutdown_with_bulk_operations();
        test_move_semantics_with_timeout();

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
