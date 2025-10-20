# AGENTS.md

## Project Map

- `include/qbuf/spsc.hpp` is the header-only library exported via the CMake `qbuf` INTERFACE target.
- `tests/test_main.cpp` bundles every assertion-based test; add helpers or new sections inside this file rather than introducing a new test framework.
- `src/benchmark.cpp` compares performance and is the place to script new performance runs.
- Guix manifests in `manifest.scm` and `guix.scm` pin GCC 11 + CMake; prefer them when reproducing CI-like environments.

## Build & Test Workflow

- Use a Guix development container to isolate changes: `guix --container --development --no-cwd --user=agent-$RANDOM --file guix.scm`.
- Once in a Guix development container:
  - Configure with `cmake -S . -B build` (use `--fresh` on first configure of session)
  - Build with `cmake --build build` (use `--target clean` on first build of session)
  - Test with `ctest --output-on-failure --test-dir build`
  - Run benchmarks with `./build/benchmark`

## SPSC Design Notes

- `SPSC<T, Capacity>` requires `Capacity` to be a power of two and reserves one slot, so maximum occupancy is `Capacity - 1`.
- Head and tail indices are `std::atomic<std::size_t>` aligned to 64 bytes; preserve this to avoid false sharing when extending the structure.
- All enqueue/dequeue paths use relaxed/acquire/release atomics; mirror the existing memory orders for correctness.
- Bulk operations avoid `%` by splitting into up-to-two segments; maintain that pattern when adding new bulk helpers.
- Blocking APIs rely on spinning with `std::this_thread::yield()`; changes here must respect the low-latency intent (documented in comments).

## Testing & Extensions

- Tests depend on `assert` and simple `std::cout` summaries; keep new checks in the same style so failures remain obvious.
- Stress cases cover wrap-around, partial bulk transfers, and blocking wakeups; mirror these patterns when introducing new edge cases.
- Any additional source file must be registered in `CMakeLists.txt`; header-only additions can live under `include/qbuf/` with matching tests.
- Benchmarks should favor template capacities (e.g., 64, 4096) to stay consistent with existing comparisons.
- Update documentation snippets in `README.md` if the public API surface (methods or semantics) changes.
