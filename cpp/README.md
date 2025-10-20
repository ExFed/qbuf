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

// Non-blocking operations (try_* methods)
if (queue.try_enqueue(42)) {
    auto value = queue.try_dequeue();
    if (value) {
        std::cout << *value << std::endl;
    }
}

// Blocking operations (enqueue/dequeue methods)
queue.enqueue(42);  // Blocks until successful
int value = queue.dequeue();  // Blocks until element available
std::cout << value << std::endl;

// Blocking bulk operations (entire batch guaranteed)
std::vector<int> batch(100);
queue.enqueue_bulk(batch.data(), batch.size());  // Blocks until all enqueued
std::vector<int> output(100);
queue.dequeue_bulk(output.data(), 100);  // Blocks until all dequeued

// Non-blocking bulk operations (partial results OK)
std::vector<int> batch(100);
std::size_t enqueued = queue.try_enqueue_bulk(batch.data(), batch.size());
std::vector<int> output(100);
std::size_t dequeued = queue.try_dequeue_bulk(output.data(), 100);
```

**Features:**

- Lock-free atomic operations
- Fixed-size ring buffer (power-of-2 capacity)
- Cache-line aligned pointers
- Move semantics support
- `std::optional` return values for non-blocking operations
- **Blocking API** (enqueue/dequeue) for simplified synchronization
- **Bulk operations** with optimized vectorization
- Minimal memory overhead and latency

## API Overview

| Method | Type | Returns | Behavior |
|:---|:---|:---|:---|
| `try_enqueue(value)` | Non-blocking | `bool` | Returns immediately, false if full |
| `try_dequeue()` | Non-blocking | `std::optional<T>` | Returns immediately, nullopt if empty |
| `enqueue(value)` | Blocking | `void` | Blocks until enqueued |
| `dequeue()` | Blocking | `T` | Blocks until element available |
| `try_enqueue_bulk(ptr, count)` | Non-blocking | `std::size_t` | Enqueues up to `count` elements |
| `try_dequeue_bulk(ptr, count)` | Non-blocking | `std::size_t` | Dequeues up to `count` elements |
| `enqueue_bulk(ptr, count)` | Blocking | `void` | Blocks until all `count` elements enqueued |
| `dequeue_bulk(ptr, count)` | Blocking | `void` | Blocks until all `count` elements dequeued |
| `empty()` | Non-blocking | `bool` | Approximate check |
| `size()` | Non-blocking | `std::size_t` | Approximate size |

## Performance

Bulk operations provide up to **2-3x throughput improvement** for medium to large batch sizes compared to individual enqueue/dequeue calls. See [BENCHMARK.md](BENCHMARK.md) for detailed performance analysis.

```bash
# Run benchmarks
cd cpp && guix shell -m manifest.scm -- sh -c 'cd build && cmake .. && make && ./benchmark'
```

## License

MIT/Expat
