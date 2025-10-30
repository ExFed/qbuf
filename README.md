# QBuf

This project is intended to research thread-safe buffers and queues in C and
C++. The project uses [CMake](https://cmake.org/) for build configuration and
[GNU Guix](https://guix.gnu.org/) for reproducible development environments.

## Building

### Requirements

* GNU Guix (preferred) or system-wide packages:
  * CMake >= 4.0
  * Make or Ninja build system
  * A C++ compiler with C++23 support (GCC >= 15, Clang >= 17, MSVC toolset >= v19.44)

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

// Create a queue with sink and source handles
auto [sink, source] = qbuf::SPSC<int, 256>::make_queue();

// Non-blocking single element operations
if (sink.try_enqueue(42)) {
    auto value = source.try_dequeue();
    if (value) {
        std::cout << *value << std::endl;
    }
}

// Blocking single element operations with timeout
using namespace std::chrono_literals;
if (sink.enqueue(42, 1s)) {  // Blocks up to 1 second
    auto value = source.dequeue(1s);  // Blocks up to 1 second
    if (value) {
        std::cout << *value << std::endl;
    }
}

// Non-blocking bulk operations (partial results OK, returns count)
std::vector<int> buff(100);
std::size_t enqueued = sink.try_enqueue(buff.data(), buff.size());
std::vector<int> output(100);
std::size_t dequeued = source.try_dequeue(output.data(), 100);

// Blocking bulk operations with timeout (entire buff guaranteed or timeout)
std::vector<int> buff2(100);
if (sink.enqueue(buff2.data(), buff2.size(), 1s)) {  // Blocks up to 1 second
    std::vector<int> output2(100);
    std::size_t dequeued2 = source.dequeue(output2.data(), 100, 1s);  // Blocks up to 1 second
}
```

#### Sink and Source Handles

The `make_queue()` factory method returns a pair of handles (`Sink` and `Source`)
that expose only the appropriate operations for each role. The handles own a
shared pointer to the underlying queue:

```cpp
#include <qbuf/spsc.hpp>

// Create queue with handles
auto [sink, source] = qbuf::SPSC<int, 256>::make_queue();

using namespace std::chrono_literals;

// Sink can only enqueue...
{
    // single element, non-blocking
    if (sink.try_enqueue(42)) {
        std::cout << "Non-blocking enqueued 42" << std::endl;
    }

    // single element, blocks up to 1 second
    if (sink.enqueue(1337, 1s)) {
        std::cout << "Blocking enqueued 1337" << std::endl;
    }

    // bulk, non-blocking
    std::vector<int> buff(100);
    std::size_t nb_elems = sink.try_enqueue(buff.data(), buff.size());
    std::cout << "Non-blocking elements enqueued: " << nb_elems << std::endl;

    // bulk, blocks up to 1 second
    std::vector<int> buff2(100);
    bool ok = sink.enqueue(buff2.data(), buff2.size(), 1s);
    std::cout << "Blocking enqueue success: " << ok << std::endl;

    std::cout << "Queue size: " << sink.size() << std::endl;
}

// Source can only dequeue
auto value = source.try_dequeue();
auto value2 = source.dequeue(1s); // blocks up to 1 second
std::cout << "Queue empty: " << source.empty() << std::endl;

// Compile error - type checking prevents misuse:
// sink.try_dequeue();  // Error: Sink has no such method
// source.try_enqueue(42);  // Error: Source has no such method
```

### MutexQueue

A mutex-based circular buffer queue providing an API surface compatible with SPSC.
Uses `std::mutex` and `std::condition_variable` for synchronization. Suitable for
scenarios where lock-free guarantees are not required, or where portability and
simplicity are prioritized over maximum performance.

**Key differences from SPSC:**
- Uses mutex-based synchronization instead of lock-free atomics
- Capacity can be any value > 1 (no power-of-two requirement)
- Reserves one slot to distinguish full from empty (max occupancy = Capacity - 1)
- Blocking operations use condition variables for efficient waiting

**Usage:**

```cpp
#include <qbuf/mutex_queue.hpp>

// Create a queue with sink and source handles
auto [sink, source] = qbuf::MutexQueue<int, 256>::make_queue();

// Same API as SPSC
sink.try_enqueue(42);
auto value = source.try_dequeue();
```

The API methods (`try_enqueue`, `enqueue`, `try_dequeue`, `dequeue`, `empty()`, `size()`)
are identical to SPSC. See the SPSC examples and API overview below for complete method signatures.

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
