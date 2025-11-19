cmake_minimum_required(VERSION 3.15)
project(qbuf-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Support tools like clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

function(add_test_executable name source)
  add_executable(${name} ${source})
  target_include_directories(${name}
    PRIVATE ${CMAKE_SOURCE_DIR}/tests
    PUBLIC ${CMAKE_SOURCE_DIR}/include
  )
  target_link_libraries(${name} PRIVATE qbuf Threads::Threads)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_test_executable(test_spsc tests/test_spsc.cpp)
add_test_executable(test_mmap_spsc tests/test_mmap_spsc.cpp)
add_test_executable(test_mutex_queue tests/test_mutex_queue.cpp)

# Benchmark
add_executable(benchmark src/benchmark.cpp)
target_link_libraries(benchmark PRIVATE qbuf Threads::Threads)
target_compile_options(benchmark PRIVATE -O3)
