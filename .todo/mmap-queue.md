# Fix blocking rvalue enqueue bug in MmapSPSC

As a library user of MmapSPSC, I want blocking rvalue enqueue to move only on
success, so that movable-only types (e.g., `std::unique_ptr`) are enqueued
correctly under contention without being moved-from multiple times.

## Definition of Done

- [ ] Add a unit test that exercises blocking rvalue enqueue with
      `std::unique_ptr<int>` under a full-queue scenario.
- [ ] Test fails before the change and passes after the fix.
- [ ] Implement fix so `enqueue(T&&, timeout)` does not repeatedly move the same
      rvalue across retries (move only on success).
- [ ] No behavior regressions in existing tests.
- [ ] Code formatted via clang-format per AGENTS.md.
- [ ] Brief note in README if behavior clarification is needed (no API
      rename/add required).

## Goals

- [ ] Correct the blocking rvalue enqueue semantics to avoid moved-from
      re-enqueues.
- [ ] Preserve low-latency spinning behavior and existing memory order
      semantics.
- [ ] Keep API surface stable (no breaking changes).

## Non-goals

- Robustness changes (relaxed loads for empty()/size(), cacheline padding, fd
  flags).
- Performance changes (bulk ops double-mapping optimizations, emplace APIs).
- Full exception-safety policy changes for bulk copies.
- Broad documentation overhaul beyond a short clarification if needed.

## Acceptance Criteria

1. Given a full queue and a producer calling the blocking rvalue enqueue with
   `std::unique_ptr<int>` while a consumer drains slowly, When the producer
   eventually succeeds enqueuing, Then the consumer observes a non-null
   `unique_ptr` with the expected value (not moved-from).
2. Given the existing test suite, When the fix is applied, Then all tests pass
   without regressions.
3. Given the new rvalue-blocking test on the pre-fix implementation, When the
   test is run, Then it fails due to moved-from re-enqueues; and given the
   post-fix implementation, Then the same test passes.

## Implementation Steps

- [ ] Add a failing test in `tests/test_mmap_spsc.cpp`:
  - Use `std::unique_ptr<int>` in a producer with blocking rvalue enqueue.
  - Keep the queue full briefly to force at least one retry.
  - Validate the consumed pointer is non-null and holds the original value.
- [ ] Implement the fix in `include/qbuf/mmap_spsc.hpp`:
  - Option A (preferred): attempt move only when space is available
    (move-on-success), avoiding pass-by-value overhead.
  - Option B: accept by value and forward to lvalue path; ensure no extra copies
    for movable-only types in the uncontended case.
- [ ] Re-run all tests; ensure the new test passes and existing tests remain
      green.
- [ ] Format changed files with clang-format.
- [ ] Add a brief note to in-line documentation clarifying the rvalue blocking
      behavior is move-on-success.

## Assumptions

- Tests follow the style in `tests/test_mmap_spsc.cpp` with assert-based checks.
- Public API names remain unchanged; the fix adjusts internal semantics only.

## Risks

- Minor performance impact if pass-by-value is chosen; mitigated by preferring
  move-on-success.
- Timing-sensitive tests could flake under heavy load; mitigate with short
  yields and deterministic queue sizing to force retries.
