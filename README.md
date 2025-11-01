# QBuf

This project is intended to research thread-safe buffers and queues in C and
C++. The project uses [CMake](https://cmake.org/) for build configuration and
[GNU Guix](https://guix.gnu.org/) for reproducible development environments.

## Building

### Requirements

* GNU Guix (preferred) or system-wide packages:
  * CMake >= 4.0
  * Make or Ninja build system
  * A C++ compiler with C++23 support
    (GCC >= 15, Clang >= 17, MSVC toolset >= v19.44)

### Using Guix Shell

1. Configure the project.
2. Build the project.
3. Run tests.

```bash
guix time-machine -C channels.scm -- shell -m manifest.scm -- sh <<EOF
    set -euo pipefail
    cmake -S . -B build --fresh
    cmake --build build -j$(nproc)
    ctest --output-on-failure --test-dir build
EOF
```

### Using System Packages

1. Configure the project.
2. Build the project.
3. Run tests.

```bash
cmake -S . -B build --fresh
cmake --build build -j$(nproc)
ctest --output-on-failure --test-dir build
```

## Performance Benchmarks

1. Configure the project.
2. Build the benchmark target.
3. Run the benchmark.

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

## Queues and Shared API

QBuf provides multiple queue implementations that share the same high-level API via
role-based handles:
- SPSC<T, Capacity>: lock-free single-producer single-consumer ring buffer
- MmapSPSC<T, Capacity>: SPSC using double-mapped virtual memory (Linux) to simplify wrap-around
- MutexQueue<T, Capacity>: mutex/condition-variable based circular buffer

All three expose identical role-based handles:
- Sink: producer-only operations
- Source: consumer-only operations

Each implementation provides a factory that returns a pair (Sink, Source). See the
respective header for the exact factory name and usage examples.

### Common operations (shared across all queues)
- Non-blocking single element
  - Sink::try_enqueue(const T&), Sink::try_enqueue(T&&) -> bool
  - Source::try_dequeue() -> std::optional<T>
- Blocking single element (timeout)
  - Sink::enqueue(const T&, timeout), Sink::enqueue(T&&, timeout) -> bool
  - Source::dequeue(timeout) -> std::optional<T>
- Non-blocking bulk
  - Sink::try_enqueue(const T* data, std::size_t count) -> std::size_t enqueued
  - Source::try_dequeue(T* out, std::size_t count) -> std::size_t dequeued
- Blocking bulk (timeout)
  - Sink::enqueue(const T* data, std::size_t count, timeout) -> bool (all or timeout)
  - Source::dequeue(T* out, std::size_t count, timeout) -> std::size_t (up to count)
- Utilities
  - size() -> std::size_t (approximate)
  - empty() -> bool (approximate)

### Implementation notes (high level)
- SPSC: lock-free with atomics, reserves one slot; Capacity must be power-of-two.
- MmapSPSC: mirrors SPSC API; uses double mapping on Linux for contiguous virtual space.
- MutexQueue: same API, uses mutex + condition_variable; Capacity > 1, reserves one slot.

For full method signatures, memory ordering details, capacity semantics, and platform
behavior, see the in-source Doxygen comments in:
- include/qbuf/spsc.hpp
- include/qbuf/mmap_spsc.hpp
- include/qbuf/mutex_queue.hpp
