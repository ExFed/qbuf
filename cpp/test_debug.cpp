#include <iostream>
#include <vector>
#include <cassert>
#include <qbuf/spsc.hpp>

using namespace qbuf;

int main() {
    SPSC<int, 8> queue;
    
    std::vector<int> input = {10, 20, 30, 40, 50};
    std::vector<int> output;

    std::cout << "Enqueuing " << input.size() << " elements..." << std::endl;
    for (int val : input) {
        bool ok = queue.try_enqueue(val);
        std::cout << "  Enqueue " << val << ": " << (ok ? "OK" : "FAILED") << std::endl;
        if (!ok) std::cerr << "ERROR: Enqueue failed!" << std::endl;
    }

    std::cout << "Queue size after enqueue: " << queue.size() << std::endl;
    std::cout << "Queue empty: " << queue.empty() << std::endl;

    std::cout << "Dequeuing " << input.size() << " elements..." << std::endl;
    for (int i = 0; i < (int)input.size(); ++i) {
        auto value = queue.try_dequeue();
        std::cout << "  Dequeue " << i << ": has_value=" << value.has_value();
        if (value.has_value()) {
            std::cout << ", value=" << value.value() << std::endl;
            output.push_back(value.value());
        } else {
            std::cout << " - EMPTY!" << std::endl;
            return 1;
        }
    }

    std::cout << "SUCCESS: All elements dequeued" << std::endl;
    return 0;
}
