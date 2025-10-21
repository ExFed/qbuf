#include <iostream>
#include <cassert>
#include <qbuf/spsc.hpp>
using namespace qbuf;

int main() {
    SPSC<int, 8> queue;
    
    // Test with -O3: this exact sequence fails
    assert(queue.try_enqueue(10));
    assert(queue.try_enqueue(20));
    assert(queue.try_enqueue(30));
    assert(queue.try_enqueue(40));
    assert(queue.try_enqueue(50));
    
    std::cout << "After enqueue: size=" << queue.size() << ", empty=" << queue.empty() << std::endl;
    
    auto v1 = queue.try_dequeue();
    std::cout << "Dequeue 1: has_value=" << v1.has_value() << std::endl;
    if (!v1.has_value()) {
        std::cerr << "FAILURE: Expected to dequeue but got empty!" << std::endl;
        return 1;
    }
    
    return 0;
}
