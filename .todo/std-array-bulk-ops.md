# Adopt std::array for bulk queue operations

As a library maintainer, I want to refactor bulk queue operations to accept
std::array so that the API is safer and communicates batch intent.

## Goals

- [ ] Replace bulk `enqueue` and `try_enqueue` signatures with `std::array`
      parameters across `SPSC`, `MmapSPSC`, and `MutexQueue`.
- [ ] Update call sites, tests, and docs to match the new `std::array` API.
- [ ] Confirm performance parity or negligible regression relative to pointer
      loops.

## Non-goals

- Retouching single-item enqueue or dequeue paths beyond required refactors.

## Implementation Steps

- [ ] Update `SPSC` bulk operations and adjust internal helpers for
      `std::array`.
- [ ] Mirror changes in `MmapSPSC`, keeping mmap handling intact.
- [ ] Apply the same refactor to `MutexQueue` and its blocking paths.
- [ ] Revise tests and benchmarks to exercise the new overloads.
- [ ] Refresh documentation snippets referencing bulk operations.

## Assumptions

- Downstream users can adopt the `std::array` API without needing legacy
  pointer overloads.
- The queues can deduce array length via `std::array::size()` without harming
  performance.

## Risks

- API break for consumers expecting pointer/count overloads.

## Lessons Learned

- None.

## INVEST Check

- Independent: Yes - No other stories must land first; updates stay within the
  queue headers and tests.
- Negotiable: Yes - Implementation details, such as helper usage, remain open
  for discussion.
- Valuable: Yes - Safer API and clearer contracts improve user ergonomics.
- Estimable: Yes - Scope covers three queue implementations plus dependent
  tests.
- Small: Yes - Coordinated signature updates fit in a single iteration.
- Testable: Yes - Passing builds and docs that show `std::array` usage verify
  completion.
