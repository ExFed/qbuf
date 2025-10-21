#include <iostream>
#include <vector>
#include <cassert>
#include <qbuf/spsc.hpp>

using namespace qbuf;

void test_basic() {
    std::cout << "Test 1: basic..." << std::endl;
    SPSC<int, 8> queue;
    assert(queue.try_enqueue(42));
    auto value = queue.try_dequeue();
    assert(value.has_value());
    assert(value.value() == 42);
    std::cout << "  PASSED" << std::endl;
}

void test_full() {
    std::cout << "Test 2: full..." << std::endl;
    SPSC<int, 8> queue;
    for (int i = 0; i < 7; ++i) {
        assert(queue.try_enqueue(i));
    }
    assert(!queue.try_enqueue(999));
    std::cout << "  PASSED" << std::endl;
}

void test_fifo() {
    std::cout << "Test 3: FIFO ordering..." << std::endl;
    SPSC<int, 8> queue;
    
    std::vector<int> input = {10, 20, 30, 40, 50};
    std::vector<int> output;

    for (int val : input) {
        assert(queue.try_enqueue(val));
    }

    for (int expected : input) {
        auto value = queue.try_dequeue();
        std::cout << "    Trying to dequeue, has_value=" << value.has_value() << std::endl;
        assert(value.has_value());
        assert(value.value() == expected);
        output.push_back(value.value());
    }

    assert(input.size() == output.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(input[i] == output[i]);
    }
    
    std::cout << "  PASSED" << std::endl;
}

int main() {
    test_basic();
    test_full();
    test_fifo();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
