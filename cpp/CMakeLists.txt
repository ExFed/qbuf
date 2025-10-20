cmake_minimum_required(VERSION 3.15)
project(qbuf-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only library
add_library(ringbuf INTERFACE)
target_include_directories(ringbuf INTERFACE include)

# Install headers
install(
  DIRECTORY include/ringbuf
  DESTINATION include
  FILES_MATCHING PATTERN "*.hpp"
)

# Tests
enable_testing()
add_executable(test_runner tests/test_main.cpp)
target_link_libraries(test_runner PRIVATE ringbuf)
add_test(NAME tests COMMAND test_runner)
