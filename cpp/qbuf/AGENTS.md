# AGENTS.md

## Code Formatting Policy

**All code changes MUST be reformatted using the appropriate formatter before committing.**

- For C++ files (`.cpp`, `.hpp`, `.h`), use `clang-format` with the project's `.clang-format` configuration file located at the workspace root.
- Format individual files with: `clang-format -i <file>`
- Format all C++ source files recursively with: `find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i`
- Agents must run the formatter on any files they modify or create to ensure consistency with the project's code style.
- This is non-negotiable: unformatted code submissions will be rejected.

## Development Policies

- If there is need for temporary files or scripts, put them in the `temp` directory for easy cleanup later.

## Project Map

- `include/qbuf/spsc.hpp` is the header-only library for a single-producer single-consumer queue, `SPSC`, exported via the CMake `qbuf` INTERFACE target.
  - `SPSC<T, Capacity>` is the primary queue implementation.
  - `SPSC<T, Capacity>::Sink` and `SPSC<T, Capacity>::Source` are role-based handle classes that expose restricted APIs for type safety.
- `tests/test_main.cpp` is the entry point for the test runner; it delegates to `run_all_spsc_tests()` from `test_spsc.hpp`.
- `tests/test_spsc.cpp` bundles all assertion-based tests; add new test functions here and register them in `run_all_spsc_tests()`.
- `tests/test_spsc.hpp` declares the `run_all_spsc_tests()` function.
- `src/benchmark.cpp` compares performance and is the place to script new performance runs.
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

## Testing & Extensions

- Tests depend on `assert` and simple `std::cout` summaries; keep new checks in the same style so failures remain obvious.
- Stress cases cover wrap-around, partial bulk transfers, and blocking wakeups; mirror these patterns when introducing new edge cases.
- Any additional source file must be registered in `CMakeLists.txt`; header-only additions can live under `include/qbuf/` with matching tests.
- Benchmarks should favor template capacities (e.g., 64, 4096) to stay consistent with existing comparisons.
- Update documentation snippets in `README.md` if the public API surface (methods or semantics) changes.
- Update instructions in `AGENTS.md` before committing.
