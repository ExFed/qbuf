# C++ Thread-Safe Buffers and Queues

Header-only C++17 implementations of lock-free thread-safe buffers and queues.

## Quick Build

```bash
mkdir -p build && cd build
cmake ..
make
ctest
```

With Guix:

```bash
guix shell -m manifest.scm -- sh -c 'cd build && cmake .. && make && ctest'
```

## Features

- **C++17**: Modern C++ with `std::optional`, structured bindings
- **Lock-free**: SPSC queue using atomic operations
- **Header-only**: Easy integration, no build required

## Project Structure

```
include/ringbuf/spsc_queue.hpp  - SPSC queue implementation
tests/test_main.cpp              - Tests
CMakeLists.txt                   - Simple CMake config
guix.scm                         - Guix package definition
manifest.scm                     - Guix dev environment
```

## SPSCQueue

Single-producer single-consumer lock-free queue.

**Usage:**

```cpp
#include <ringbuf/spsc_queue.hpp>

ringbuf::SPSCQueue<int, 256> queue;

// Producer
queue.try_push(42);

// Consumer
auto value = queue.try_pop();
if (value) {
    std::cout << *value << std::endl;
}
```

**Features:**

- Lock-free atomic operations
- Fixed-size ring buffer (power-of-2 capacity)
- Cache-line aligned pointers
- Move semantics support
- `std::optional` return values

## License

MIT/Expat
