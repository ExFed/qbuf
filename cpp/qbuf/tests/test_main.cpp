#include "test_spsc.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    std::cout << "\n=== Running SPSC Tests ===" << std::endl;
    try {
        run_all_spsc_tests();
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
