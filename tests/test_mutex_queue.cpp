#include "assert.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <qbuf/mutex_queue.hpp>
#include <thread>
#include <vector>

using namespace qbuf;

void test_mutex_basic_operations() {
    std::cout << "Testing basic operations..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    // Test empty queue
    assert(source.empty());
    assert(source.size() == 0);

    // Test push and pop single element
    assert(sink.try_enqueue(42));
    assert(!source.empty());
    assert(source.size() == 1);

    auto value = source.try_dequeue();
    assert(value.has_value());
    assert(value.value() == 42);
    assert(source.empty());

    // Test pop from empty queue
    value = source.try_dequeue();
    assert(!value.has_value());

    // Test push multiple elements
    assert(sink.try_enqueue(1));
    assert(sink.try_enqueue(2));
    assert(sink.try_enqueue(3));
    assert(source.size() == 3);

    auto val1 = source.try_dequeue();
    assert(val1.has_value());
    assert(val1.value() == 1);
    auto val2 = source.try_dequeue();
    assert(val2.has_value());
    assert(val2.value() == 2);
    auto val3 = source.try_dequeue();
    assert(val3.has_value());
    assert(val3.value() == 3);
    assert(source.empty());

    std::cout << "  PASSED: basic operations" << std::endl;
}

void test_mutex_queue_full() {
    std::cout << "Testing queue full condition..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    // Capacity is 8, but we can only store 7 elements (one slot reserved)
    for (int i = 0; i < 7; ++i) {
        assert(sink.try_enqueue(i));
    }

    // Next push should fail
    assert(!sink.try_enqueue(999));

    std::cout << "  PASSED: queue full" << std::endl;
}

void test_mutex_fifo_ordering() {
    std::cout << "Testing FIFO ordering..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    std::vector<int> input = { 10, 20, 30, 40, 50 };
    std::vector<int> output;

    for (int val : input) {
        assert(sink.try_enqueue(val));
    }

    for (int expected : input) {
        auto value = source.try_dequeue();
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

void test_mutex_concurrent() {
    std::cout << "Testing concurrent producer-consumer..." << std::endl;
    auto [sink, source] = MutexQueue<int, 256>::make_queue();
    constexpr int num_elements = 1000;
    std::atomic<bool> producer_done { false };

    // Producer thread
    std::thread producer([sink = std::move(sink), &producer_done]() mutable {
        for (int i = 0; i < num_elements; ++i) {
            while (!sink.try_enqueue(i)) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::vector<int> consumed;
    std::thread consumer([&, &source = source]() {
        int count = 0;
        while (count < num_elements) {
            auto value = source.try_dequeue();
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
    assert(source.empty());

    std::cout << "  PASSED: concurrent producer-consumer" << std::endl;
}

void test_mutex_bulk_enqueue_dequeue() {
    std::cout << "Testing bulk enqueue/dequeue..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    std::vector<int> input = { 10, 20, 30, 40, 50, 60, 70, 80 };
    std::vector<int> output(input.size());

    // Enqueue all elements
    assert(sink.try_enqueue(input.data(), input.size()) == input.size());
    assert(source.size() == input.size());

    // Dequeue all elements
    assert(source.try_dequeue(output.data(), output.size()) == input.size());
    assert(source.empty());

    // Verify inputs produced match outputs consumed in same order
    assert(input.size() == output.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(output[i] == input[i]);
    }

    std::cout << "  PASSED: bulk enqueue/dequeue" << std::endl;
}

void test_mutex_bulk_partial() {
    std::cout << "Testing partial bulk operations..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    std::vector<int> input = { 1, 2, 3, 4, 5, 6, 7, 8 };
    std::vector<int> consumed_output;

    // Enqueue first batch
    assert(sink.try_enqueue(input.data(), 4) == 4);

    // Dequeue partial
    std::vector<int> output1(2);
    assert(source.try_dequeue(output1.data(), 2) == 2);
    assert(output1[0] == 1);
    assert(output1[1] == 2);
    consumed_output.insert(consumed_output.end(), output1.begin(), output1.end());

    // Enqueue more
    assert(sink.try_enqueue(&input[4], 4) == 4);

    // Dequeue remaining
    std::vector<int> output2(6);
    assert(source.try_dequeue(output2.data(), 6) == 6);
    consumed_output.insert(consumed_output.end(), output2.begin(), output2.begin() + 6);

    // Verify all consumed elements match original input order
    for (std::size_t i = 0; i < consumed_output.size(); ++i) {
        assert(consumed_output[i] == input[i]);
    }

    std::cout << "  PASSED: partial bulk operations" << std::endl;
}

void test_mutex_bulk_full_queue() {
    std::cout << "Testing bulk enqueue on full queue..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    std::vector<int> input1 = { 1, 2, 3, 4, 5, 6 };
    std::vector<int> input2 = { 7, 8, 9, 10 };

    // Fill most of the queue (capacity is 8, so we can store 7)
    assert(sink.try_enqueue(input1.data(), input1.size()) == 6);

    // Try to enqueue more than available space
    assert(sink.try_enqueue(input2.data(), input2.size()) == 1); // Only 1 slot available

    // Verify size
    assert(source.size() == 7);

    std::cout << "  PASSED: bulk enqueue on full queue" << std::endl;
}

void test_mutex_bulk_empty_dequeue() {
    std::cout << "Testing bulk dequeue from empty queue..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    std::vector<int> output(10);
    assert(source.try_dequeue(output.data(), 10) == 0);
    assert(source.empty());

    std::cout << "  PASSED: bulk dequeue from empty queue" << std::endl;
}

void test_mutex_bulk_wrap_around() {
    std::cout << "Testing bulk operations with wrap-around..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    // Fill, partially consume, then fill again to create wrap-around
    std::vector<int> batch1 = { 1, 2, 3, 4 };
    std::vector<int> batch2 = { 5, 6 };
    std::vector<int> batch3 = { 7, 8, 9, 10 };
    std::vector<int> output(10);

    // Enqueue first batch
    assert(sink.try_enqueue(batch1.data(), batch1.size()) == 4);

    // Consume 2 elements
    assert(source.try_dequeue(output.data(), 2) == 2);
    assert(output[0] == 1 && output[1] == 2);

    // Enqueue more
    assert(sink.try_enqueue(batch2.data(), batch2.size()) == 2);

    // Now queue should have: [3, 4, 5, 6] with tail wrapped
    assert(source.size() == 4);

    // Enqueue another batch (this should handle wrap-around)
    assert(sink.try_enqueue(batch3.data(), batch3.size()) == 3); // Can only fit 3 more

    // Dequeue all
    std::vector<int> final_output(7);
    assert(source.try_dequeue(final_output.data(), 7) == 7);

    // Verify order: 3, 4, 5, 6, 7, 8, 9
    int expected[] = { 3, 4, 5, 6, 7, 8, 9 };
    for (int i = 0; i < 7; ++i) {
        assert(final_output[i] == expected[i]);
    }

    std::cout << "  PASSED: bulk operations with wrap-around" << std::endl;
}

void test_mutex_bulk_concurrent() {
    std::cout << "Testing concurrent bulk operations..." << std::endl;
    auto [sink, source] = MutexQueue<int, 512>::make_queue();
    constexpr int num_batches = 50;
    constexpr int batch_size = 20;
    constexpr int total_elements = num_batches * batch_size;
    std::atomic<bool> producer_done { false };

    // Producer: enqueue in batches
    std::thread producer([&, sink = std::move(sink)]() mutable {
        for (int b = 0; b < num_batches; ++b) {
            std::vector<int> batch(batch_size);
            for (int i = 0; i < batch_size; ++i) {
                batch[i] = b * batch_size + i;
            }
            std::size_t enqueued = 0;
            while (enqueued < batch_size) {
                enqueued += sink.try_enqueue(batch.data() + enqueued, batch_size - enqueued);
                if (enqueued < batch_size) {
                    std::this_thread::yield();
                }
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer: dequeue in batches
    std::vector<int> consumed;
    std::thread consumer([&, &source = source]() {
        std::vector<int> buffer(batch_size);
        int total = 0;
        while (total < total_elements) {
            std::size_t dequeued = source.try_dequeue(buffer.data(), batch_size);
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
    assert(source.empty());

    std::cout << "  PASSED: concurrent bulk operations" << std::endl;
}

void test_mutex_blocking_enqueue() {
    std::cout << "Testing blocking enqueue..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    // Fill the queue to near capacity (7 elements max)
    for (int i = 0; i < 7; ++i) {
        assert(sink.enqueue(i, std::chrono::seconds(5)));
    }
    assert(source.size() == 7);

    // Producer thread tries to enqueue (will block until consumer makes space)
    std::atomic<bool> producer_done { false };
    std::thread producer([&, sink = std::move(sink)]() mutable {
        assert(sink.enqueue(99, std::chrono::seconds(5)));
        producer_done.store(true, std::memory_order_release);
    });

    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Consumer dequeues one element, unblocking the producer
    auto value = source.try_dequeue();
    assert(value.has_value());
    assert(value.value() == 0);

    // Wait for producer to complete
    producer.join();
    assert(producer_done.load());
    assert(source.size() == 7); // Should have 1-6 and 99

    std::cout << "  PASSED: blocking enqueue" << std::endl;
}

void test_mutex_blocking_dequeue() {
    std::cout << "Testing blocking dequeue..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    std::atomic<int> dequeued_value { -1 };
    std::atomic<bool> consumer_done { false };

    // Consumer thread tries to dequeue from empty queue (will block)
    std::thread consumer([&, &source = source]() {
        auto value = source.dequeue(std::chrono::seconds(5));
        if (value.has_value()) {
            dequeued_value.store(value.value(), std::memory_order_release);
        }
        consumer_done.store(true, std::memory_order_release);
    });

    // Give consumer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(!consumer_done.load()); // Consumer should still be blocked

    // Producer enqueues an element, unblocking consumer
    assert(sink.enqueue(42, std::chrono::seconds(5)));

    // Wait for consumer to complete
    consumer.join();
    assert(consumer_done.load());
    assert(dequeued_value.load() == 42);
    assert(source.empty());

    std::cout << "  PASSED: blocking dequeue" << std::endl;
}

void test_mutex_blocking_concurrent() {
    std::cout << "Testing blocking concurrent operations..." << std::endl;
    auto [sink, source] = MutexQueue<int, 256>::make_queue();
    constexpr int num_elements = 1000;

    // Producer thread
    std::thread producer([&, sink = std::move(sink)]() mutable {
        for (int i = 0; i < num_elements; ++i) {
            assert(sink.enqueue(i, std::chrono::seconds(5)));
        }
    });

    // Consumer thread
    std::vector<int> consumed;
    std::thread consumer([&, &source = source]() {
        for (int i = 0; i < num_elements; ++i) {
            auto value = source.dequeue(std::chrono::seconds(5));
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
    assert(source.empty());

    std::cout << "  PASSED: blocking concurrent operations" << std::endl;
}

void test_mutex_blocking_stress() {
    std::cout << "Testing blocking operations under stress..." << std::endl;
    auto [sink, source] = MutexQueue<int, 64>::make_queue();
    constexpr int total_ops = 10000;
    std::atomic<int> consumed_count { 0 };

    // Producer: continuously enqueue
    std::thread producer([&, sink = std::move(sink)]() mutable {
        for (int i = 0; i < total_ops; ++i) {
            assert(sink.enqueue(i, std::chrono::seconds(5)));
        }
    });

    // Consumer: continuously dequeue
    std::thread consumer([&, &source = source]() {
        for (int i = 0; i < total_ops; ++i) {
            auto value = source.dequeue(std::chrono::seconds(5));
            assert(value.has_value());
            assert(value.value() == i);
            consumed_count.fetch_add(1, std::memory_order_release);
        }
    });

    producer.join();
    consumer.join();

    assert(consumed_count.load() == total_ops);
    assert(source.empty());

    std::cout << "  PASSED: blocking stress test" << std::endl;
}

void test_mutex_blocking_bulk_enqueue() {
    std::cout << "Testing blocking bulk enqueue..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    // Fill most of the queue
    std::vector<int> initial_batch = { 1, 2, 3, 4, 5, 6, 7 };
    assert(sink.enqueue(initial_batch.data(), initial_batch.size(), std::chrono::seconds(5)));
    assert(source.size() == 7);

    // Try to enqueue a large batch (will block until consumer drains)
    std::vector<int> large_batch(20);
    for (int i = 0; i < 20; ++i) {
        large_batch[i] = 100 + i;
    }

    std::atomic<bool> producer_done { false };
    std::thread producer([&, sink = std::move(sink)]() mutable {
        assert(sink.enqueue(large_batch.data(), large_batch.size(), std::chrono::seconds(5)));
        producer_done.store(true, std::memory_order_release);
    });

    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(!producer_done.load()); // Should still be blocked

    // Consumer drains the queue gradually
    std::vector<int> consumed;
    int total_to_drain = 27; // 7 initial + 20 large_batch
    while (consumed.size() < total_to_drain) {
        auto value = source.try_dequeue();
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

void test_mutex_blocking_bulk_dequeue() {
    std::cout << "Testing blocking bulk dequeue..." << std::endl;
    auto [sink, source] = MutexQueue<int, 128>::make_queue();

    std::vector<int> output(50);
    std::atomic<bool> consumer_done { false };

    // Consumer thread tries to dequeue 50 elements (will block)
    std::thread consumer([&, &source = source]() {
        assert(source.dequeue(output.data(), 50, std::chrono::seconds(5)) == 50);
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

    assert(sink.enqueue(batch1.data(), 25, std::chrono::seconds(5)));
    assert(sink.enqueue(batch2.data(), 25, std::chrono::seconds(5)));

    // Wait for consumer to complete
    consumer.join();
    assert(consumer_done.load());

    // Verify all elements
    for (int i = 0; i < 50; ++i) {
        assert(output[i] == i);
    }
    assert(source.empty());

    std::cout << "  PASSED: blocking bulk dequeue" << std::endl;
}

void test_mutex_blocking_bulk_concurrent() {
    std::cout << "Testing blocking bulk concurrent operations..." << std::endl;
    auto [sink, source] = MutexQueue<int, 256>::make_queue();
    constexpr int num_batches = 100;
    constexpr int batch_size = 50;
    constexpr int total_elements = num_batches * batch_size;

    // Producer: enqueue in batches using blocking API
    std::thread producer([&, sink = std::move(sink)]() mutable {
        for (int b = 0; b < num_batches; ++b) {
            std::vector<int> batch(batch_size);
            for (int i = 0; i < batch_size; ++i) {
                batch[i] = b * batch_size + i;
            }
            assert(sink.enqueue(batch.data(), batch_size, std::chrono::seconds(5)));
        }
    });

    // Consumer: dequeue in batches using blocking API
    std::vector<int> consumed;
    std::thread consumer([&, &source = source]() {
        std::vector<int> batch(batch_size);
        int total_dequeued = 0;
        while (total_dequeued < total_elements) {
            int remaining = total_elements - total_dequeued;
            int to_dequeue = (remaining < batch_size) ? remaining : batch_size;
            assert(source.dequeue(batch.data(), to_dequeue, std::chrono::seconds(5)) == to_dequeue);
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
    assert(source.empty());

    std::cout << "  PASSED: blocking bulk concurrent operations" << std::endl;
}

void test_mutex_blocking_bulk_mixed() {
    std::cout << "Testing mixed blocking and non-blocking bulk operations..." << std::endl;
    auto [sink, source] = MutexQueue<int, 64>::make_queue();

    std::vector<int> input1(10);
    for (int i = 0; i < 10; ++i) input1[i] = i;

    std::vector<int> input2(8);
    for (int i = 0; i < 8; ++i) input2[i] = 100 + i;

    // Enqueue with blocking bulk
    assert(sink.enqueue(input1.data(), input1.size(), std::chrono::seconds(5)));
    assert(source.size() == 10);

    // Dequeue with non-blocking try_dequeue
    std::vector<int> output1(5);
    assert(source.try_dequeue(output1.data(), 5) == 5);
    assert(source.size() == 5);

    // Verify first dequeue matches first half of input1
    for (int i = 0; i < 5; ++i) {
        assert(output1[i] == input1[i]);
    }

    // Enqueue more with try_enqueue
    assert(sink.try_enqueue(input2.data(), 8) == 8);
    assert(source.size() == 13);

    // Dequeue all remaining with blocking bulk
    std::vector<int> output2(13);
    assert(source.dequeue(output2.data(), 13, std::chrono::seconds(5)) == 13);
    assert(source.empty());

    // Verify sequence
    int idx = 0;
    for (int i = 5; i < 10; ++i) {
        assert(output2[idx++] == input1[i]);
    }
    for (int i = 0; i < 8; ++i) {
        assert(output2[idx++] == input2[i]);
    }

    std::cout << "  PASSED: mixed blocking and non-blocking bulk operations" << std::endl;
}

void test_mutex_enqueue_timeout_on_full() {
    std::cout << "Testing enqueue timeout when queue is full..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    // Fill the queue to capacity (7 elements max)
    for (int i = 0; i < 7; ++i) {
        assert(sink.try_enqueue(i));
    }
    assert(source.size() == 7);

    // Try to enqueue with short timeout (should fail)
    assert(!sink.enqueue(999, std::chrono::milliseconds(50))); // Should timeout
    assert(source.size() == 7); // Queue unchanged

    std::cout << "  PASSED: enqueue timeout on full" << std::endl;
}

void test_mutex_enqueue_timeout_with_space() {
    std::cout << "Testing enqueue timeout when space becomes available..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    // Fill the queue
    for (int i = 0; i < 7; ++i) {
        assert(sink.enqueue(i, std::chrono::seconds(5)));
    }

    // Producer thread tries to enqueue with timeout
    std::atomic<bool> producer_done { false };
    std::atomic<bool> producer_success { false };
    std::thread producer([&, sink = std::move(sink)]() mutable {
        producer_success.store(
            sink.enqueue(999, std::chrono::seconds(2)), std::memory_order_release
        );
        producer_done.store(true, std::memory_order_release);
    });

    // Give producer time to attempt the enqueue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Consumer makes space
    auto value = source.try_dequeue();
    assert(value.has_value());
    assert(value.value() == 0);

    // Wait for producer to complete
    producer.join();
    assert(producer_done.load());
    assert(producer_success.load()); // Should succeed eventually

    std::cout << "  PASSED: enqueue timeout with space" << std::endl;
}

void test_mutex_dequeue_timeout_on_empty() {
    std::cout << "Testing dequeue timeout when queue is empty..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    assert(source.empty());

    // Try to dequeue with short timeout (should return nullopt)
    assert(!source.dequeue(std::chrono::milliseconds(50)).has_value()); // Should timeout
    assert(source.empty());

    std::cout << "  PASSED: dequeue timeout on empty" << std::endl;
}

void test_mutex_dequeue_timeout_with_data() {
    std::cout << "Testing dequeue timeout when data becomes available..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    // Consumer thread tries to dequeue with timeout
    std::atomic<bool> consumer_done { false };
    std::atomic<int> dequeued_value { -1 };
    std::thread consumer([&, source = std::move(source)]() mutable {
        auto value = source.dequeue(std::chrono::seconds(2));
        if (value.has_value()) {
            dequeued_value.store(value.value(), std::memory_order_release);
        }
        consumer_done.store(true, std::memory_order_release);
    });

    // Give consumer time to attempt the dequeue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Producer enqueues an element
    assert(sink.enqueue(42, std::chrono::seconds(5)));

    // Wait for consumer to complete
    consumer.join();
    assert(consumer_done.load());
    assert(dequeued_value.load() == 42); // Should succeed eventually

    std::cout << "  PASSED: dequeue timeout with data" << std::endl;
}

void test_mutex_bulk_enqueue_timeout_on_full() {
    std::cout << "Testing bulk enqueue timeout when queue is full..." << std::endl;
    auto [sink, source] = MutexQueue<int, 8>::make_queue();

    // Fill the queue to capacity (7 elements max for capacity 8)
    std::vector<int> initial(7);
    for (int i = 0; i < 7; ++i) initial[i] = i;
    assert(sink.enqueue(initial.data(), initial.size(), std::chrono::seconds(5)));

    // Try to enqueue bulk with short timeout (should fail)
    std::vector<int> batch = { 100, 101, 102, 103 };
    assert(
        !sink.enqueue(batch.data(), batch.size(), std::chrono::milliseconds(50))
    ); // Should timeout
    assert(source.size() == 7); // Queue unchanged

    std::cout << "  PASSED: bulk enqueue timeout on full" << std::endl;
}

void test_mutex_bulk_enqueue_timeout_with_space() {
    std::cout << "Testing bulk enqueue timeout when space becomes available..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    // Fill most of the queue
    std::vector<int> initial(7);
    for (int i = 0; i < 7; ++i) initial[i] = i;
    assert(sink.enqueue(initial.data(), initial.size(), std::chrono::seconds(5)));

    // Producer thread tries to enqueue bulk with timeout
    std::vector<int> large_batch(10);
    for (int i = 0; i < 10; ++i) large_batch[i] = 100 + i;

    std::atomic<bool> producer_done { false };
    std::atomic<bool> producer_success { false };
    std::thread producer([&, sink = std::move(sink)]() mutable {
        producer_success.store(
            sink.enqueue(large_batch.data(), large_batch.size(), std::chrono::seconds(2)),
            std::memory_order_release
        );
        producer_done.store(true, std::memory_order_release);
    });

    // Give producer time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Consumer gradually drains the queue
    for (int i = 0; i < 3; ++i) {
        auto val = source.try_dequeue();
        assert(val.has_value());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    producer.join();
    assert(producer_done.load());
    assert(producer_success.load()); // Should succeed eventually

    std::cout << "  PASSED: bulk enqueue timeout with space" << std::endl;
}

void test_mutex_bulk_dequeue_timeout_on_empty() {
    std::cout << "Testing bulk dequeue timeout when queue is empty..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    assert(source.empty());

    // Try to dequeue bulk with short timeout
    std::vector<int> output(10);
    std::size_t dequeued
        = source.dequeue(output.data(), output.size(), std::chrono::milliseconds(50));
    assert(dequeued == 0); // Should timeout with no data
    assert(source.empty());

    std::cout << "  PASSED: bulk dequeue timeout on empty" << std::endl;
}

void test_mutex_bulk_dequeue_timeout_with_partial_data() {
    std::cout << "Testing bulk dequeue timeout with partial data..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    // Enqueue fewer elements than requested
    std::vector<int> initial = { 1, 2, 3 };
    assert(sink.enqueue(initial.data(), initial.size(), std::chrono::seconds(5)));

    // Consumer thread tries to dequeue more than available
    std::vector<int> output(10);
    std::atomic<bool> consumer_done { false };
    std::atomic<std::size_t> dequeued_count { 0 };

    std::thread consumer([&, source = std::move(source)]() mutable {
        std::size_t dequeued
            = source.dequeue(output.data(), output.size(), std::chrono::milliseconds(100));
        dequeued_count.store(dequeued, std::memory_order_release);
        consumer_done.store(true, std::memory_order_release);
    });

    consumer.join();
    assert(consumer_done.load());
    assert(dequeued_count.load() == 3); // Should return what was available

    std::cout << "  PASSED: bulk dequeue timeout with partial data" << std::endl;
}

void test_mutex_graceful_shutdown_with_enqueue_timeout() {
    std::cout << "Testing graceful producer shutdown with timeout..." << std::endl;
    auto [sink, source] = MutexQueue<int, 64>::make_queue();
    std::atomic<bool> shutdown { false };
    constexpr int target_elements = 100;
    std::atomic<int> enqueued_count { 0 };

    // Producer that respects shutdown signal using timeouts
    std::thread producer([&, sink = std::move(sink)]() mutable {
        for (int i = 0; i < target_elements; ++i) {
            // Try to enqueue with a timeout that allows periodic shutdown checks
            bool success = sink.enqueue(i, std::chrono::milliseconds(50));
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

void test_mutex_graceful_shutdown_with_dequeue_timeout() {
    std::cout << "Testing graceful consumer shutdown with timeout..." << std::endl;
    auto [sink, source] = MutexQueue<int, 64>::make_queue();
    std::atomic<bool> shutdown { false };
    std::atomic<int> dequeued_count { 0 };

    // Producer thread
    std::thread producer([&, sink = std::move(sink)]() mutable {
        for (int i = 0; i < 50; ++i) {
            assert(sink.enqueue(i, std::chrono::seconds(5)));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Consumer that respects shutdown signal using timeouts
    std::thread consumer([&, &source = source]() {
        while (!shutdown.load(std::memory_order_acquire)) {
            // Try to dequeue with a timeout that allows shutdown checks
            auto value = source.dequeue(std::chrono::milliseconds(50));
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

void test_mutex_graceful_shutdown_with_bulk_operations() {
    std::cout << "Testing graceful shutdown with bulk operations..." << std::endl;
    auto [sink, source] = MutexQueue<int, 128>::make_queue();
    std::atomic<bool> producer_shutdown { false };
    std::atomic<bool> consumer_shutdown { false };
    std::atomic<int> total_produced { 0 };
    std::atomic<int> total_consumed { 0 };

    // Producer with bulk operations and timeout-based shutdown checks
    std::thread producer([&, sink = std::move(sink)]() mutable {
        for (int batch = 0; batch < 20 && !producer_shutdown.load(std::memory_order_acquire);
             ++batch) {
            std::vector<int> batch_data(10);
            for (int i = 0; i < 10; ++i) {
                batch_data[i] = batch * 10 + i;
            }

            bool success = sink.enqueue(
                batch_data.data(), batch_data.size(), std::chrono::milliseconds(100)
            );
            if (success) {
                total_produced.fetch_add(10, std::memory_order_release);
            } else if (!producer_shutdown.load(std::memory_order_acquire)) {
                // Timeout but not shutdown, retry
                --batch;
            }
        }
    });

    // Consumer with bulk operations and timeout-based shutdown checks
    std::thread consumer([&, &source = source]() {
        std::vector<int> buffer(20);
        while (!consumer_shutdown.load(std::memory_order_acquire)) {
            std::size_t dequeued
                = source.dequeue(buffer.data(), buffer.size(), std::chrono::milliseconds(100));
            if (dequeued > 0) {
                total_consumed.fetch_add(dequeued, std::memory_order_release);
            }
        }

        // Final drain before exit
        std::vector<int> final_buffer(50);
        while (true) {
            std::size_t dequeued = source.dequeue(
                final_buffer.data(), final_buffer.size(), std::chrono::milliseconds(10)
            );
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

void test_mutex_sink() {
    std::cout << "Testing MutexSink..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    // Test single enqueue
    assert(sink.try_enqueue(10));
    assert(sink.size() == 1);

    // Test bulk enqueue
    std::vector<int> batch = { 20, 30, 40 };
    assert(sink.try_enqueue(batch.data(), batch.size()) == batch.size());
    assert(sink.size() == 4);

    // Test empty check
    assert(!sink.empty());

    // Test blocking enqueue
    assert(sink.enqueue(50, std::chrono::seconds(5)));
    assert(sink.size() == 5);

    std::cout << "  PASSED: MutexSink" << std::endl;
}

void test_mutex_source() {
    std::cout << "Testing MutexSource..." << std::endl;
    auto [sink, source] = MutexQueue<int, 16>::make_queue();

    // Setup - enqueue elements
    sink.try_enqueue(10);
    sink.try_enqueue(20);
    sink.try_enqueue(30);

    // Test single dequeue
    auto val = source.try_dequeue();
    assert(val.has_value());
    assert(val.value() == 10);
    assert(source.size() == 2);

    // Test bulk dequeue
    std::vector<int> output(2);
    assert(source.try_dequeue(output.data(), 2) == 2);
    assert(output[0] == 20);
    assert(output[1] == 30);

    // Test empty check
    assert(source.empty());

    std::cout << "  PASSED: MutexSource" << std::endl;
}

void test_mutex_sink_source_concurrent() {
    std::cout << "Testing MutexSink and MutexSource concurrently..." << std::endl;
    auto [sink, source] = MutexQueue<int, 256>::make_queue();

    constexpr int num_elements = 100;
    std::atomic<bool> producer_done { false };

    // Producer thread
    std::thread producer([&, sink = std::move(sink)]() mutable {
        for (int i = 0; i < num_elements; ++i) {
            assert(sink.enqueue(i, std::chrono::seconds(5)));
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::vector<int> consumed;
    std::thread consumer([&, &source = source]() {
        for (int i = 0; i < num_elements; ++i) {
            auto value = source.dequeue(std::chrono::seconds(5));
            assert(value.has_value());
            consumed.push_back(value.value());
        }
    });

    producer.join();
    consumer.join();

    // Verify
    assert(consumed.size() == num_elements);
    for (int i = 0; i < num_elements; ++i) {
        assert(consumed[i] == i);
    }
    assert(source.empty());

    std::cout << "  PASSED: MutexSink and MutexSource concurrent" << std::endl;
}

void run_all_mutex_queue_tests() {
    std::cout << "\n=== Running MutexQueue Tests ===" << std::endl;

    test_mutex_basic_operations();
    test_mutex_queue_full();
    test_mutex_fifo_ordering();
    test_mutex_concurrent();
    test_mutex_bulk_enqueue_dequeue();
    test_mutex_bulk_partial();
    test_mutex_bulk_full_queue();
    test_mutex_bulk_empty_dequeue();
    test_mutex_bulk_wrap_around();
    test_mutex_bulk_concurrent();
    test_mutex_blocking_enqueue();
    test_mutex_blocking_dequeue();
    test_mutex_blocking_concurrent();
    test_mutex_blocking_stress();
    test_mutex_blocking_bulk_enqueue();
    test_mutex_blocking_bulk_dequeue();
    test_mutex_blocking_bulk_concurrent();
    test_mutex_blocking_bulk_mixed();
    test_mutex_enqueue_timeout_on_full();
    test_mutex_enqueue_timeout_with_space();
    test_mutex_dequeue_timeout_on_empty();
    test_mutex_dequeue_timeout_with_data();
    test_mutex_bulk_enqueue_timeout_on_full();
    test_mutex_bulk_enqueue_timeout_with_space();
    test_mutex_bulk_dequeue_timeout_on_empty();
    test_mutex_bulk_dequeue_timeout_with_partial_data();
    test_mutex_graceful_shutdown_with_enqueue_timeout();
    test_mutex_graceful_shutdown_with_dequeue_timeout();
    test_mutex_graceful_shutdown_with_bulk_operations();
    test_mutex_sink();
    test_mutex_source();
    test_mutex_sink_source_concurrent();

    std::cout << "\n=== All MutexQueue tests passed ===" << std::endl;
}

int main() {
    try {
        run_all_mutex_queue_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nTest failed with unknown exception" << std::endl;
        return 1;
    }
}
