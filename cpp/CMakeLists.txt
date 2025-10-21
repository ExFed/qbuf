cmake_minimum_required(VERSION 3.15)
project(qbuf-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find threads library for std::thread support
find_package(Threads REQUIRED)

# Header-only library
add_library(qbuf INTERFACE)
target_include_directories(qbuf INTERFACE include)

# Install headers
install(
  DIRECTORY include/qbuf
  DESTINATION include
  FILES_MATCHING PATTERN "*.hpp"
)

# Tests
enable_testing()
add_executable(test_runner tests/test_main.cpp tests/test_spsc.cpp)
target_include_directories(test_runner PRIVATE tests)
target_link_libraries(test_runner PRIVATE qbuf Threads::Threads)
# Disable optimization for test_runner due to GCC optimization bug with atomics
target_compile_options(test_runner PRIVATE -O0)
add_test(NAME tests COMMAND test_runner)

# Benchmark
add_executable(benchmark src/benchmark.cpp)
target_link_libraries(benchmark PRIVATE qbuf Threads::Threads)
target_compile_options(benchmark PRIVATE -O3)
