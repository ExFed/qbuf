# C++ Thread-Safe Buffers and Queues

Header-only implementations of lock-free thread-safe buffers and queues.

## Quick Build

```bash
cmake -S . -B build --fresh
cmake --build build -j$(nproc)
ctest --output-on-failure --test-dir build
```

With Guix:

```bash
guix shell -m manifest.scm -- sh -c ' \
    cmake -S . -B build --fresh \
    && cmake --build build -j$(nproc) \
    && ctest --output-on-failure --test-dir build'
```

## Features

- **C++23**: Modern C++ with `std::optional`, structured bindings
- **Lock-free**: SPSC queue using atomic operations
- **Header-only**: Easy integration, no build required

## Project Structure

```text
include/qbuf/spsc.hpp   # SPSC queue implementation
tests/test_main.cpp     # Tests
CMakeLists.txt          # Simple CMake config
guix.scm                # Guix package definition
manifest.scm            # Guix dev environment
```

## SPSC

Single-producer single-consumer lock-free queue.

**Usage:**

```cpp
#include <qbuf/spsc.hpp>

qbuf::SPSC<int, 256> queue;

// Non-blocking single element operations
if (queue.try_enqueue(42)) {
    auto value = queue.try_dequeue();
    if (value) {
        std::cout << *value << std::endl;
    }
}

// Blocking single element operations with timeout
using namespace std::chrono_literals;
if (queue.enqueue(42, 1s)) {  // Blocks up to 1 second
    auto value = queue.dequeue(1s);  // Blocks up to 1 second
    if (value) {
        std::cout << *value << std::endl;
    }
}

// Non-blocking bulk operations (partial results OK, returns count)
std::vector<int> batch(100);
std::size_t enqueued = queue.try_enqueue(batch.data(), batch.size());
std::vector<int> output(100);
std::size_t dequeued = queue.try_dequeue(output.data(), 100);

// Blocking bulk operations with timeout (entire batch guaranteed or timeout)
std::vector<int> batch(100);
if (queue.enqueue(batch.data(), batch.size(), 1s)) {  // Blocks up to 1 second
    std::vector<int> output(100);
    std::size_t dequeued = queue.dequeue(output.data(), 100, 1s);  // Blocks up to 1 second
}
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
| `try_dequeue()` | Non-blocking | `std::optional<T>` | Returns immediately, `nullopt` if empty |
| `enqueue(value, timeout)` | Blocking | `bool` | Blocks up to timeout, returns success |
| `enqueue(T&&, timeout)` | Blocking | `bool` | Blocks up to timeout, returns success (move) |
| `dequeue(timeout)` | Blocking | `std::optional<T>` | Blocks up to timeout, returns element or `nullopt` |

### Bulk Operations

| Method | Type | Returns | Behavior |
|:---|:---|:---|:---|
| `try_enqueue(T*, count)` | Non-blocking | `std::size_t` | Enqueues up to `count` elements, returns count enqueued |
| `try_dequeue(T*, count)` | Non-blocking | `std::size_t` | Dequeues up to `count` elements, returns count dequeued |
| `enqueue(T*, count, timeout)` | Blocking | `bool` | Blocks up to timeout until all elements enqueued, returns success |
| `dequeue(T*, count, timeout)` | Blocking | `std::size_t` | Blocks up to timeout until all elements dequeued, returns count dequeued |

### Utility Methods

| Method | Type | Returns | Behavior |
|:---|:---|:---|:---|
| `empty()` | Non-blocking | `bool` | Approximate check |
| `size()` | Non-blocking | `std::size_t` | Approximate size |

## Performance

```bash
# Run benchmarks
guix shell -m manifest.scm -- sh -c ' \
    cmake -S . -B build --fresh \
    && cmake --build build -j$(nproc) \
    && ./build/benchmark \
    '
```
