# MmapSPSC TODO Plan

## P1 – Correctness
- Fix blocking rvalue enqueue bug
  - Problem: enqueue(T&&, timeout) repeatedly std::move(value) across loop, enqueuing moved-from T.
  - Change: accept by value and pass to lvalue overload, or split path to move only on success.
  - Tests: add blocking rvalue test with movable type (e.g., std::unique_ptr<int>) under contention.
  - Acceptance: test fails before change, passes after; no regressions in existing tests.

## P2 – Robustness/maintainability
- empty()/size() memory orders
  - Consider relaxed loads (advisory only). Validate no consumer/producer visibility issues in tests.
- Cacheline isolation
  - Add padding after head_/tail_ or wrap in padded structs to avoid incidental sharing with other members.
  - Benchmark impact.
- Exception safety for bulk copies
  - If T copy/move may throw, document noexcept requirement for best guarantees, or add cleanup on partial construction.
- Linux fd handling
  - Use MFD_CLOEXEC when available; provide fallback define if missing. Ensure behavior unchanged on older glibc.

## P3 – Performance/clarity
- Exploit double-mapping in bulk ops
  - Replace split loops with single contiguous memcpy/memmove when safe (trivial types), or contiguous construct loops.
  - Keep semantics identical; benchmark vs current split approach.
- Emplace APIs (optional)
  - Add try_emplace/emplace with perfect forwarding; move/construct only when space is available.

## Testing additions
- Blocking rvalue enqueue with movable type under full queue; ensure correct value observed.
- Exception path (optional): custom type that throws on Nth copy to validate cleanup/documented constraints.
- Smoke + stress remain; add benchmarks to compare changes.

## Documentation
- README: note reserved-slot, power-of-two Capacity, Linux double-mapping behavior, and any new APIs.
- AGENTS.md: no changes unless formatting/build steps change.

## Work breakdown (checklist)
- [ ] Implement blocking rvalue enqueue fix
- [ ] Add unit test for blocking rvalue enqueue (unique_ptr)
- [ ] Consider relaxed loads in empty()/size() (measure, gate behind comment)
- [ ] Add cacheline padding around indices (measure impact)
- [ ] Use MFD_CLOEXEC for memfd_create (fallback for older libc)
- [ ] Optimize bulk ops leveraging double mapping (benchmark)
- [ ] Decide and document exception-safety policy; implement if chosen
- [ ] Update README snippets if APIs/semantics change
- [ ] Run/record benchmarks before/after

## Risks/notes
- Header-only: downstream must rebuild; no ABI stability concerns.
- Spinning semantics must remain low-latency; avoid adding locks/conds.
- Keep one-slot reservation and power-of-two invariant; reflect in tests/docs.
