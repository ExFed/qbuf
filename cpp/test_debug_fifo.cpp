#include <iostream>
#include <vector>
#include <cassert>
#include <qbuf/spsc.hpp>

using namespace qbuf;

int main() {
    SPSC<int, 8> queue;

    std::vector<int> input = {10, 20, 30, 40, 50};
    std::vector<int> output;

    for (int val : input) {
        bool ok = queue.try_enqueue(val);
        std::cerr << "Enqueue " << val << ": " << ok << std::endl;
        assert(ok);
    }

    std::cerr << "Before dequeue: ";
    queue.debug_print();

    for (int expected : input) {
        std::cerr << "Attempting dequeue for " << expected << "..." << std::endl;
        auto value = queue.try_dequeue();
        std::cerr << "  Result: has_value=" << value.has_value() << std::endl;
        if (!value.has_value()) {
            std::cerr << "ERROR: Queue returned empty when it shouldn't!" << std::endl;
            queue.debug_print();
            return 1;
        }
        assert(value.value() == expected);
        output.push_back(value.value());
    }

    std::cout << "SUCCESS" << std::endl;
    return 0;
}
