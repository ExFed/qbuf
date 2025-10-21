#include <iostream>
#include <vector>
#include <cassert>
#include <qbuf/spsc.hpp>

using namespace qbuf;

void test_basic_operations() {
    std::cout << "Testing basic operations..." << std::endl;
    SPSC<int, 8> queue;

    assert(queue.empty());
    assert(queue.size() == 0);

    assert(queue.try_enqueue(42));
    assert(!queue.empty());
    assert(queue.size() == 1);

    auto value = queue.try_dequeue();
    assert(value.has_value());
    assert(value.value() == 42);
    assert(queue.empty());

    value = queue.try_dequeue();
    assert(!value.has_value());

    assert(queue.try_enqueue(1));
    assert(queue.try_enqueue(2));
    assert(queue.try_enqueue(3));
    assert(queue.size() == 3);

    auto val1 = queue.try_dequeue();
    assert(val1.has_value());
    assert(val1.value() == 1);
    auto val2 = queue.try_dequeue();
    assert(val2.has_value());
    assert(val2.value() == 2);
    auto val3 = queue.try_dequeue();
    assert(val3.has_value());
    assert(val3.value() == 3);
    assert(queue.empty());

    std::cout << "  PASSED: basic operations" << std::endl;
}

void test_queue_full() {
    std::cout << "Testing queue full condition..." << std::endl;
    SPSC<int, 8> queue;

    for (int i = 0; i < 7; ++i) {
        assert(queue.try_enqueue(i));
    }

    assert(!queue.try_enqueue(999));

    std::cout << "  PASSED: queue full" << std::endl;
}

void test_fifo_ordering() {
    std::cout << "Testing FIFO ordering..." << std::endl;
    SPSC<int, 8> queue;

    std::vector<int> input = {10, 20, 30, 40, 50};
    std::vector<int> output;

    for (int val : input) {
        bool ok = queue.try_enqueue(val);
        std::cout << "Enqueue " << val << ": " << ok << ", size=" << queue.size() << std::endl;
        assert(ok);
    }

    for (int i = 0; i < (int)input.size(); ++i) {
        int expected = input[i];
        std::cout << "Attempting dequeue #" << i << " (expect " << expected << ")..." << std::endl;
        auto value = queue.try_dequeue();
        std::cout << "  has_value=" << value.has_value() << std::endl;
        if (!value.has_value()) {
            std::cout << "ERROR: Dequeue #" << i << " returned null!" << std::endl;
            throw std::runtime_error("Dequeue failed");
        }
        assert(value.value() == expected);
        output.push_back(value.value());
    }

    assert(input.size() == output.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(input[i] == output[i]);
    }

    std::cout << "  PASSED: FIFO ordering" << std::endl;
}

int main() {
    test_basic_operations();
    test_queue_full();
    test_fifo_ordering();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
