# AGENTS.md

## Project Map

- Root repo hosts multi-language queue experiments; only `cpp/` is active today.
- `cpp/include/qbuf/spsc.hpp` is the header-only library exported via the
  CMake `qbuf` INTERFACE target.
- `cpp/tests/test_main.cpp` bundles every assertion-based test; add helpers or
  new sections inside this file rather than introducing a new test framework.
- `cpp/src/benchmark.cpp` compares performance and is the place to script new
  performance runs.
- Guix manifests in `cpp/manifest.scm` and `cpp/guix.scm` pin GCC 11 + CMake;
  prefer them when reproducing CI-like environments.

## Build & Test Workflow

- Default local loop: `cmake -S cpp -B build`, then `cmake --build build`, then
  `ctest --output-on-failure --test-dir build`.
- Existing `build/` folders already contain configure artifacts; wipe them (or
  run the `C++: CMake: Clean` task) before switching toolchains.
- Minimal path build inside `cpp/`: `mkdir -p build && cd build && cmake .. &&
  make && ctest` mirrors the README.
- Benchmarks live at `cpp/build/benchmark`; run via `cmake --build build
  --target benchmark && ./build/benchmark` or the provided Guix shell command.
- When using Guix: `guix shell -m manifest.scm -- sh -c 'cmake -S . -B build &&
  cmake --build build && ctest'` executed from `cpp/` matches the documented
  workflow.

## SPSC Design Notes

- `SPSC<T, Capacity>` requires `Capacity` to be a power of two and reserves one
  slot, so maximum occupancy is `Capacity - 1`.
- Head and tail indices are `std::atomic<std::size_t>` aligned to 64 bytes;
  preserve this to avoid false sharing when extending the structure.
- All enqueue/dequeue paths use relaxed/acquire/release atomics; mirror the
  existing memory orders for correctness.
- Bulk operations avoid `%` by splitting into up-to-two segments; maintain that
  pattern when adding new bulk helpers.
- Blocking APIs rely on spinning with `std::this_thread::yield()`; changes here
  must respect the low-latency intent (documented in comments).

## Testing & Extensions

- Tests depend on `assert` and simple `std::cout` summaries; keep new checks in
  the same style so failures remain obvious.
- Stress cases cover wrap-around, partial bulk transfers, and blocking wakeups;
  mirror these patterns when introducing new edge cases.
- Any additional source file must be registered in `cpp/CMakeLists.txt`;
  header-only additions can live under `cpp/include/qbuf/` with matching tests.
- Benchmarks should favor template capacities (e.g., 64, 4096) to stay
  consistent with existing comparisons.
- Update documentation snippets in `cpp/README.md` if the public API surface
  (methods or semantics) changes.
