# Span set

`core/spans/spans.h` and `spans.cpp` (`lumora::spans`) keep a set of up to 16 disjoint integer intervals `[begin, end)` (dirty ranges, free ranges, selection runs): `clear`, `add`, `remove`, `contains`, `total_length`, `size`, `get`.

Evidence label: unit tests only (CI-verified). Windows runs only in CI.

Integer only: no floating point, no libm, no 128-bit types, no allocation, no recursion, no global state. The source includes only its header.

## Rules

- Well-formed set: `count <= 16`, every span has `begin < end`, ascending order, and a gap of at least one integer between neighbours. Spans that overlap or touch are always one span.
- `add`: empty interval (`begin >= end`) is true and changes nothing. Otherwise every span with `s.begin <= end` and `s.end >= begin` merges with the interval. If the result needs a 17th slot the call returns false and the set is unchanged (no collapse).
- `remove`: a span strictly containing the interval is split in two, a fully covered span disappears (no slot needed), a partly covered span is cut. If more than 16 spans would result: false, set unchanged.
- A null or corrupt set (`count > 16`) returns false from `add` and `remove` (before the empty-interval check), false from `contains`, 0 from `total_length` and `size`, false from `get`. `clear` repairs a corrupt set.
- Differences and sums use 64 bits, so spans reaching INT32_MIN and INT32_MAX are fine. `end` is exclusive.

## Bounds

| Item | Bound |
| --- | --- |
| spans | 0 to 16 |
| coordinates | full int32 range |
| total length | up to 2^32 per set (64-bit sum) |

## Tests

`tools/spans_ref.py` is an independent Python point-set model (a Python set of integers on a small window, plus interval arithmetic for large values). It prints the digest constants that `tests/spans/spans_tests.cpp` recomputes: 12 xorshift streams of 500 operations that reach 16 spans and include capacity failures. The test file has 11 cases and hand cases for touching and overlapping merges, splits, exact boundaries, full-set failures, int32 extremes, null and corrupt sets.

## Mutation check

50 hand-written single-edit source mutants of `spans.cpp` (comparison operators, the overlap test, min and max updates, placement order, every capacity guard, count update, split pieces, contains bounds, length arithmetic, null and corrupt checks, get index) were built against the unchanged tests. 42 were killed and 8 survive. A first run left the corrupt check in `add` alive; the tests now also call `add` and `remove` with an empty interval on corrupt sets, and it is killed.

Survivors (not killed; equivalence argued):

- `add`, `sp.end < begin` as `<=`: the span reaching this test does not overlap, so `sp.end == begin` was already merged by the overlap test. Unreachable difference.
- `add`, capacity guards before the placed span and before a preceding span (two mutants): the guard counts only spans before position i, so `n <= 15` there. The guard cannot fire. The guard that matters (after the placed span) is killed.
- `remove`, `sp.end < begin` and `sp.begin > end` in the keep test (two mutants): at equality the span falls through to the cut code, which emits the same span unchanged.
- `remove`, guard before the left piece: `n <= i <= 15` when a span intersects, so the guard cannot fire. The right-piece guard is killed.
- `total_length`, `d >= 0`: adds zero. Equivalent.
- `total_length`, no `d > 0` test: differs only for a hand-built inverted span, which the header leaves unspecified.
