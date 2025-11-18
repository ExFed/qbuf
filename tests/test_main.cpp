#include "test_mmap_spsc.hpp"
#include "test_mutex_queue.hpp"
#include "test_spsc.hpp"

#include <iostream>

int main() {
    try {
        run_all_spsc_tests();
        run_all_mmap_spsc_tests();
        run_all_mutex_queue_tests();
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
