# C++ Thread-Safe Buffers and Queues

Header-only C++23 implementations of lock-free thread-safe buffers and queues.

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

- **C++23**: Modern C++ with `std::optional`, structured bindings
- **Lock-free**: SPSC queue using atomic operations
- **Header-only**: Easy integration, no build required

## Project Structure

```
include/qbuf/spsc_queue.hpp  - SPSC queue implementation
tests/test_main.cpp              - Tests
CMakeLists.txt                   - Simple CMake config
guix.scm                         - Guix package definition
manifest.scm                     - Guix dev environment
```

## SPSCQueue

Single-producer single-consumer lock-free queue.

**Usage:**

```cpp
#include <qbuf/spsc_queue.hpp>

qbuf::SPSCQueue<int, 256> queue;

// Non-blocking single element operations
if (queue.try_enqueue(42)) {
    auto value = queue.try_dequeue();
    if (value) {
        std::cout << *value << std::endl;
    }
}

// Blocking single element operations
queue.enqueue(42);  // Blocks until successful
int value = queue.dequeue();  // Blocks until element available
std::cout << value << std::endl;

// Non-blocking bulk operations (partial results OK, returns count)
std::vector<int> batch(100);
std::size_t enqueued = queue.try_enqueue(batch.data(), batch.size());
std::vector<int> output(100);
std::size_t dequeued = queue.try_dequeue(output.data(), 100);

// Blocking bulk operations (entire batch guaranteed)
std::vector<int> batch(100);
queue.enqueue(batch.data(), batch.size());  // Blocks until all enqueued
std::vector<int> output(100);
queue.dequeue(output.data(), 100);  // Blocks until all dequeued
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

### Single Element Operations

| Method | Type | Returns | Behavior |
|:---|:---|:---|:---|
| `try_enqueue(value)` | Non-blocking | `bool` | Returns immediately, false if full |
| `try_enqueue(T&&)` | Non-blocking | `bool` | Returns immediately, false if full (move) |
| `try_dequeue()` | Non-blocking | `std::optional<T>` | Returns immediately, nullopt if empty |
| `enqueue(value)` | Blocking | `void` | Blocks until enqueued |
| `enqueue(T&&)` | Blocking | `void` | Blocks until enqueued (move) |
| `dequeue()` | Blocking | `T` | Blocks until element available |

### Bulk Operations

| Method | Type | Returns | Behavior |
|:---|:---|:---|:---|
| `try_enqueue(ptr, count)` | Non-blocking | `std::size_t` | Enqueues up to `count` elements, returns count enqueued |
| `try_dequeue(ptr, count)` | Non-blocking | `std::size_t` | Dequeues up to `count` elements, returns count dequeued |
| `enqueue(ptr, count)` | Blocking | `void` | Blocks until all `count` elements enqueued |
| `dequeue(ptr, count)` | Blocking | `void` | Blocks until all `count` elements dequeued |

### Utility Methods

| Method | Type | Returns | Behavior |
|:---|:---|:---|:---|
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
