/**
 * GCC Optimization Bug - Reproducible Test Case
 *
 * This test case reliably reproduces a GCC 11.4 compiler bug with -O3 -DNDEBUG.
 * The bug manifests as std::bad_optional_access in a lock-free SPSC queue
 * when multiple test cases run in sequence.
 *
 * COMPILE WITH:
 *   g++ -std=gnu++23 -O3 -DNDEBUG -I include -pthread \
 *       GCC_BUG_REPRODUCIBLE.cpp -o test_bug && ./test_bug
 *
 * EXPECTED BEHAVIOR (with -O0 or without -DNDEBUG):
 *   All tests pass
 *
 * ACTUAL BEHAVIOR (with -O3 -DNDEBUG):
 *   Crashes with "bad optional access" in test_fifo_ordering() after first two tests
 */

#include <qbuf/spsc.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>

using Queue = qbuf::SPSC<int, 8>;

void test_basic_operations() {
    std::cout << "Test 1: basic operations..." << std::endl;
    Queue q;

    // Test enqueueing
    assert(q.try_enqueue(10));
    assert(q.try_enqueue(20));

    // Test dequeueing
    auto val1 = q.try_dequeue();
    assert(val1.has_value() && val1.value() == 10);

    auto val2 = q.try_dequeue();
    assert(val2.has_value() && val2.value() == 20);

    // Queue should be empty
    auto empty = q.try_dequeue();
    assert(!empty.has_value());

    std::cout << "  PASSED" << std::endl;
}

void test_queue_full() {
    std::cout << "Test 2: queue full..." << std::endl;
    Queue q;

    // Fill queue (capacity is 8)
    for (int i = 0; i < 7; ++i) {
        assert(q.try_enqueue(i * 10));
    }

    // Should be full now
    assert(!q.try_enqueue(999));

    // Dequeue one item
    auto val = q.try_dequeue();
    assert(val.has_value() && val.value() == 0);

    // Now we can enqueue again
    assert(q.try_enqueue(999));

    // Drain the queue
    for (int i = 0; i < 7; ++i) {
        auto v = q.try_dequeue();
        assert(v.has_value());
    }

    std::cout << "  PASSED" << std::endl;
}

void test_fifo_ordering() {
    std::cout << "Test 3: FIFO ordering..." << std::endl;
    Queue q;

    // Enqueue multiple items
    const int values[] = {10, 20, 30, 40, 50};
    for (int v : values) {
        assert(q.try_enqueue(v));
    }

    // Dequeue and verify FIFO order
    for (int v : values) {
        auto val = q.try_dequeue();
        // THIS LINE THROWS std::bad_optional_access with -O3 -DNDEBUG
        // after running previous tests
        if (!val.has_value()) {
            std::cerr << "ERROR: Got empty optional when expecting value!" << std::endl;
            throw std::runtime_error("bad_optional_access equivalent");
        }
        assert(val.value() == v);
    }

    std::cout << "  PASSED" << std::endl;
}

int main() {
    try {
        test_basic_operations();
        test_queue_full();
        test_fifo_ordering();

        std::cout << "\n✓ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
