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

// Individual operations
queue.try_enqueue(42);
auto value = queue.try_dequeue();
if (value) {
    std::cout << *value << std::endl;
}

// Bulk operations (optimized for performance)
std::vector<int> batch(100);
queue.try_enqueue_bulk(batch.data(), batch.size());
std::vector<int> output(100);
queue.try_dequeue_bulk(output.data(), 100);
```

**Features:**

- Lock-free atomic operations
- Fixed-size ring buffer (power-of-2 capacity)
- Cache-line aligned pointers
- Move semantics support
- `std::optional` return values
- **Bulk operations** with optimized vectorization

## Performance

Bulk operations provide up to **2-3x throughput improvement** for medium to large batch sizes compared to individual enqueue/dequeue calls. See [BENCHMARK.md](BENCHMARK.md) for detailed performance analysis.

```bash
# Run benchmarks
cd cpp && guix shell -m manifest.scm -- sh -c 'cd build && cmake .. && make && ./benchmark'
```

## License

MIT/Expat
