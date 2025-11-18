# AGENTS.md

## Code Formatting Policy

**All code changes MUST be reformatted using the appropriate formatter before committing.**

- For C++ files (`.cpp`, `.hpp`, `.h`), use `clang-format` with the project's `.clang-format` configuration file located at the workspace root.
- Format individual files with: `clang-format -i <file>`
- Format all C++ source files with: `./scripts/reformat-code.sh`
- Agents must run the formatter on any files they modify or create to ensure consistency with the project's code style.
- This is non-negotiable: unformatted code submissions will be rejected.

## Development Policies

- If there is need for temporary files or scripts, put them in the `temp` directory for easy cleanup later.

## Project Map

- `include/qbuf/spsc.hpp` is the header-only library for a single-producer single-consumer queue, `SPSC`, exported via the CMake `qbuf` INTERFACE target.
  - `SPSC<T, Capacity>` is the primary queue implementation.
  - `SPSC<T, Capacity>::Sink` and `SPSC<T, Capacity>::Source` are role-based handle classes that expose restricted APIs for type safety.
- `include/qbuf/mutex_queue.hpp` is the header-only library for a mutex-based circular buffer, `MutexQueue`, with API surface compatible with SPSC.
  - `MutexQueue<T, Capacity>` uses `std::mutex` and `std::condition_variable` for synchronization.
  - `MutexQueue<T, Capacity>::Sink` and `MutexQueue<T, Capacity>::Source` provide role-based access.
  - Reserves one slot (max occupancy = Capacity - 1); Capacity can be any value > 1 (no power-of-two requirement).
- `include/qbuf/mmap_spsc.hpp` is the header-only library for a memory-mapped single-producer single-consumer queue, `MmapSPSC`, with API surface mirroring SPSC.
  - `MmapSPSC<T, Capacity>` uses double-mapped virtual memory pages (Linux memfd) to eliminate wrap-around logic.
  - `MmapSPSC<T, Capacity>::Sink` and `MmapSPSC<T, Capacity>::Source` provide role-based access.
  - Factory method `MmapSPSC<T, Capacity>::create()` returns `std::pair<Sink, Source>`.
  - On non-Linux platforms, falls back to regular heap allocation without double-mapping optimization.
  - Requires `Capacity` to be a power of two and reserves one slot (max occupancy = Capacity - 1).
- `tests/test_main.cpp` is the entry point for the test runner; it delegates to `run_all_spsc_tests()` from `test_spsc.hpp`, `run_all_mmap_spsc_tests()` from `test_mmap_spsc.hpp`, and `run_all_mutex_queue_tests()` from `test_mutex_queue.hpp`.
- `tests/test_spsc.cpp` bundles all assertion-based tests; add new test functions here and register them in `run_all_spsc_tests()`.
- `tests/test_spsc.hpp` declares the `run_all_spsc_tests()` function.
- `tests/test_mmap_spsc.cpp` bundles all memory-mapped queue tests; add new test functions here and register them in `run_all_mmap_spsc_tests()`.
- `tests/test_mmap_spsc.hpp` declares the `run_all_mmap_spsc_tests()` function.
- `tests/test_mutex_queue.cpp` bundles all MutexQueue tests; add new test functions here and register them in `run_all_mutex_queue_tests()`.
- `tests/test_mutex_queue.hpp` declares the `run_all_mutex_queue_tests()` function.
- `src/benchmark.cpp` compares performance of SPSC vs MutexQueue; add new benchmark runs here.
- `scripts` contains utility scripts:
  - `reformat-code.sh` reformats all C++ source files using `clang-format`.
- `.clang-format` contains formatting rules matching the project's code style.
- Guix manifests in `manifest.scm` and `guix.scm` pin GCC 11 + CMake; prefer them when reproducing CI-like environments.
- `TODO.md` contains a checklist of high-level tasks left to do.

## Development Workflow

- Create a Guix development container using `guix shell -CWm manifest.scm --user=agent-$RANDOM`
- Once in a Guix development container:
  - Configure with `cmake -S . -B build` (use `--fresh` on first configure of session)
  - Build with `cmake --build build` (use `--target clean` on first build of session)
  - Test with `ctest --output-on-failure --test-dir build`
  - Run benchmarks with `./build/benchmark`

## SPSC Design Notes

- `SPSC<T, Capacity>` requires `Capacity` to be a power of two and reserves one slot, so maximum occupancy is `Capacity - 1`.
- `SPSC<T, Capacity>::Sink` wraps an `SPSC` reference and exposes only producer-side operations: `try_enqueue` (single and bulk), `enqueue` (blocking single and bulk), `empty()`, and `size()`.
- `SPSC<T, Capacity>::Source` wraps an `SPSC` reference and exposes only consumer-side operations: `try_dequeue` (single and bulk), `dequeue` (blocking single and bulk), `empty()`, and `size()`.
- Both handles use pass-by-reference to the underlying queue; they incur no runtime overhead and exist purely for type-safe API restriction.
- Head and tail indices are `std::atomic<std::size_t>` aligned to 64 bytes; preserve this to avoid false sharing when extending the structure.
- All enqueue/dequeue paths use relaxed/acquire/release atomics; mirror the existing memory orders for correctness.
- Bulk operations avoid `%` by splitting into up-to-two segments; maintain that pattern when adding new bulk helpers.
- Blocking APIs rely on spinning with `std::this_thread::yield()`; changes here must respect the low-latency intent (documented in comments).

## MutexQueue Design Notes

- `MutexQueue<T, Capacity>` uses `std::mutex` for synchronization; Capacity can be any value > 1 (no power-of-two requirement).
- Reserves one slot to distinguish full from empty (max occupancy = Capacity - 1).
- `MutexQueue<T, Capacity>::Sink` and `MutexQueue<T, Capacity>::Source` provide role-based access mirroring SPSC's API.
- Both handles wrap a reference to the underlying queue; non-copyable, move-constructible, no move-assign.
- Blocking operations use `std::condition_variable` with `wait_until` and while-predicate loops to handle spurious wakeups.
- Notifications (`cv_not_empty_`, `cv_not_full_`) occur after state changes; unlock-then-notify pattern reduces contention.
- Bulk operations split transfers into up-to-two contiguous segments using modulo arithmetic.
- Internal helpers: `next(i)`, `size_unlocked()`, `free_unlocked()`, `full_unlocked()` maintain consistency with circular buffer semantics.

## MmapSPSC Design Notes

- `MmapSPSC<T, Capacity>` uses the double-mapping trick where two adjacent virtual memory regions point to the same physical memory (via Linux memfd_create).
- This eliminates wrap-around logic during bulk operations—a contiguous write/read in the virtual address space automatically wraps via the double mapping.
- Requires `Capacity` to be a power of two and reserves one slot (max occupancy = Capacity - 1), mirroring SPSC behavior.
- `MmapSPSC<T, Capacity>::Sink` and `MmapSPSC<T, Capacity>::Source` provide role-based access with identical APIs to SPSC.
- Factory method `MmapSPSC<T, Capacity>::create()` returns `std::pair<Sink, Source>` for convenient setup.
- Head and tail indices are `std::atomic<std::size_t>` aligned to 64 bytes to avoid false sharing.
- All enqueue/dequeue paths use relaxed/acquire/release atomics matching SPSC's memory ordering.
- On Linux, uses `memfd_create` + double `mmap(MAP_FIXED)` to create mirrored regions; falls back to regular heap allocation on non-Linux platforms.
- Buffer size is rounded up to page size for mmap compatibility.
- Cleanup in destructor unmaps both regions and closes the memfd file descriptor.
- Blocking APIs spin with `std::this_thread::yield()` for low latency, consistent with SPSC design.

## Testing & Extensions

- Tests depend on `assert` and simple `std::cout` summaries; keep new checks in the same style so failures remain obvious.
- Stress cases cover wrap-around, partial bulk transfers, and blocking wakeups; mirror these patterns when introducing new edge cases.
- Any additional source file must be registered in `CMakeLists.txt`; header-only additions can live under `include/qbuf/` with matching tests.
- Benchmarks should favor template capacities (e.g., 64, 4096) to stay consistent with existing comparisons.
- Update documentation snippets in `README.md` if the public API surface (methods or semantics) changes.
- Update instructions in `AGENTS.md` before committing.
