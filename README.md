# QBuf

This project is intended to research thread-safe buffers and queues in C and
C++. The project uses [CMake](https://cmake.org/) for build configuration and
[GNU Guix](https://guix.gnu.org/) for reproducible development environments.

## Building

### Requirements

* GNU Guix (preferred) or system-wide packages:
  * CMake >= 4.0
  * Make or Ninja build system
  * A C++ compiler with C++17 support (GCC >= 7, Clang >= 5, MSVC >= 2017)

### Using Guix Shell

```bash
guix time-machine -C channels.scm -- shell -m manifest.scm -- sh <<EOF
    set -euo pipefail
    cmake -S . -B build --fresh
    cmake --build build -j$(nproc)
    ctest --output-on-failure --test-dir build
EOF
```

### Using System Packages

```bash
cmake -S . -B build --fresh
cmake --build build -j$(nproc)
ctest --output-on-failure --test-dir build
```

## Performance Benchmarks

```bash
guix shell -m manifest.scm -- sh <<EOF
    set -euo pipefail
    cmake -S . -B build --fresh
    cmake --build build -j$(nproc)
    ./build/benchmark
EOF
```

## Development VM

For a reproducible and isolated development environment using Guix System VM,
see [docs/dev-vm.md](docs/dev-vm.md).

## Project Overview

| Path                     | Description                        |
|--------------------------|------------------------------------|
| `docs/`                  | Documentation                      |
| `include/`               | Public API includes                |
| `src/`                   | Private API includes and sources   |
| `tests/`                 | Tests                              |
| `channels.scm`           | Guix channels definition           |
| `guix.scm`               | Guix package definition            |
| `manifest.scm`           | Guix dev environment definition    |
| `CMakeLists.txt`         | Root CMake config                  |
| `README.md`              | Project overview (this file)       |
| `AGENTS.md`              | AI agent policies and guidelines   |

### SPSC Queue

A single-producer single-consumer queue. *SPSC* is a lock-free, fixed-size ring
buffer with cache-line aligned pointers, move semantics support. The API
supports both single-element and bulk operations for efficient data transfer,
and both blocking and non-blocking variants.

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
std::vector<int> buff(100);
std::size_t enqueued = queue.try_enqueue(buff.data(), buff.size());
std::vector<int> output(100);
std::size_t dequeued = queue.try_dequeue(output.data(), 100);

// Blocking bulk operations with timeout (entire buff guaranteed or timeout)
std::vector<int> buff(100);
if (queue.enqueue(buff.data(), buff.size(), 1s)) {  // Blocks up to 1 second
    std::vector<int> output(100);
    std::size_t dequeued = queue.dequeue(output.data(), 100, 1s);  // Blocks up to 1 second
}
```

#### Sink and Source Handles

For type-safe producer-consumer patterns, use `Sink` and `Source` to expose only
the appropriate operations for each role:

```cpp
#include <qbuf/spsc.hpp>

qbuf::SPSC<int, 256> queue;

// Create handles from the same queue
qbuf::Sink<int, 256> producer(queue);
qbuf::Source<int, 256> consumer(queue);

using namespace std::chrono_literals;

// Producer can only enqueue...
{
    // single element, non-blocking
    if (producer.try_enqueue(42)) {
        std::cout << "Non-blocking enqueued 42" << std::endl;
    }

    // single element, blocks up to 1 second
    if (producer.enqueue(1337, 1s)) {
        std::cout << "Blocking enqueued 1337" << std::endl;
    }

    // bulk, non-blocking
    std::vector<int> buff(100);
    std::size_t nb_elems = producer.try_enqueue(buff.data(), buff.size());
    std::cout << "Non-blocking elements enqueued: " << nb_elems << std::endl;

    // bulk, blocks up to 1 second
    std::size_t b_elems = producer.enqueue(buff.data(), buff.size(), 1s);
    std::cout << "Blocking elements enqueued: " << b_elems << std::endl;

    std::cout << "Queue size: " << producer.size() << std::endl;
}

// Bulk enqueue with timeout (all or timeout)
bool ok = producer.enqueue(buff.data(), buff.size(), 1s);

// Consumer can only dequeue
auto value = consumer.try_dequeue();
auto value2 = consumer.dequeue(1s); // blocks up to 1 second
std::cout << "Queue empty: " << consumer.empty() << std::endl;

// Compile error - type checking prevents misuse:
// producer.try_dequeue();  // Error: Source has no such method
// consumer.try_enqueue(42);  // Error: Sink has no such method
```

### API Overview

#### Single Element Blocking Operations

| Method                  | Returns            | Behavior                                                    |
|:---                     |:---                |:---                                                         |
| `enqueue(T, timeout)`   | `bool`             | Blocks up to timeout, returns success                       |
| `enqueue(T&&, timeout)` | `bool`             | Blocks up to timeout, returns success (move)                |
| `dequeue(timeout)`      | `std::optional<T>` | Blocks up to timeout, returns element or `nullopt` if empty |

#### Single Element Non-Blocking Operations

| Method                  | Returns            | Behavior                                                    |
|:---                     |:---                |:---                                                         |
| `try_enqueue(T)`        | `bool`             | Returns immediately, false if full                          |
| `try_enqueue(T&&)`      | `bool`             | Returns immediately, false if full (move)                   |
| `try_dequeue()`         | `std::optional<T>` | Returns immediately, `nullopt` if empty                     |

#### Bulk Blocking Operations

| Method                        | Returns       | Behavior                                                                 |
|:---                           |:---           |:---                                                                      |
| `try_enqueue(T*, count)`      | `std::size_t` | Enqueues up to `count` elements, returns count enqueued                  |
| `try_dequeue(T*, count)`      | `std::size_t` | Dequeues up to `count` elements, returns count dequeued                  |
| `enqueue(T*, count, timeout)` | `bool`        | Blocks up to timeout until all elements enqueued, returns success        |
| `dequeue(T*, count, timeout)` | `std::size_t` | Blocks up to timeout until all elements dequeued, returns count dequeued |

#### Utility Methods

| Method    | Returns       | Behavior                |
|:---       |:---           |:---                     |
| `empty()` | `bool`        | Approximate empty check |
| `size()`  | `std::size_t` | Approximate size        |
