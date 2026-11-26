# Vsync period estimator

`core/lifecycle/vsync.h` and `vsync.cpp` (`lumora::lifecycle`) keep a ring of the 16 most recent vsync deltas and report their lower median as the vsync period estimate.

Evidence label: unit tests only (CI-verified).

Integer only: no clock reads, no floating point, no libm, no 128-bit types, no wide types, no allocation. The sources include `<cstddef>` and `<cstdint>`; the tests add `<initializer_list>`, `<utility>` and `<vector>`.

## State

`VsyncEstimator` holds `last_ns` (previous timestamp, meaningful only when `has_last`), `has_last`, `deltas[16]`, `count` (valid deltas) and `next` (the slot written next). Valid state: `count <= 16` and `next < 16`. Anything else is invalid.

The window is the `count` most recent slots ending at `next - 1` with wrap-around, that is slots `(next - count + i) mod 16` for `i = 0..count-1`. Slots outside the window are never read.

## reset

Returns false when `e` is null. Otherwise sets `*e = VsyncEstimator{}` (everything zero, `has_last` false) and returns true.

## on_vsync

Order of checks:

1. `e` null: return false.
2. Invalid state (`count > 16` or `next >= 16`): repair by `has_last = false`, `count = 0`, `next = 0`. No other field is touched (`last_ns` and the delta slots stay).
3. `has_last` false: store `last_ns = timestamp_ns`, set `has_last`, return true. `count`, `next` and the window are left alone. A first timestamp of 0 is accepted.
4. `timestamp_ns <= last_ns` (equal, backwards, or a later 0): return false, nothing changes.
5. `delta = timestamp_ns - last_ns`. If `delta > 1000000000` (strictly), `count = 0`, `next = 0`, `last_ns = timestamp_ns`, return true. A delta of exactly 1e9 is recorded.
6. Otherwise `deltas[next] = delta`, `next` wraps from 15 to 0, `count` grows to at most 16 (the oldest delta is overwritten when full), `last_ns = timestamp_ns`, return true.

The repair in step 2 happens even when the call then returns false.

## estimate_vsync_ns

Returns false and leaves `*out` untouched when `e` or `out` is null, the state is invalid, `count < 4`, or the median is 0. Otherwise `*out` is the element at index `(count - 1) / 2` of the window sorted ascending (lower median) and the result is true. `has_last` is not checked. A copy of the window (at most 16 values) is sorted by insertion.

## Bounds

| Item | Bound |
| --- | --- |
| window | 16 deltas |
| minimum for an estimate | 4 deltas |
| recorded delta | 1 to 1000000000 ns |
| timestamp | any `uint64_t`; the first may be 0, later ones must exceed `last_ns` |
| `count`, `next` on input | any 32-bit value; out of range is invalid |

## Tests

`tools/vsync_ref.py` is an independent reference in Python on unbounded integers (it sorts a list and takes index `(count - 1) // 2`). It prints a generated block with the grids and FNV-1a 64 digests that the test recomputes.

```
python3 tools/vsync_ref.py                                              # print the block
python3 tools/vsync_ref.py --check tests/vsync/vsync_tests.cpp
```

`tests/vsync/vsync_tests.cpp` has 12 test cases and 414 assertions. Written cases: constants and defaults, `reset` from every kind of state and null, first timestamp (including 0), rejection of equal, backward and zero timestamps, the gap rule at 1e9 and 1e9 + 1, ring wrap and count growth to 16 and beyond, repair of invalid objects touching only the named fields, lower median for counts 4 to 16 and thresholds, window selection by `next`, `has_last` ignored, invalid state, zero median, and an end-to-end run. Sweeps are checked against a naive oracle that shares no code with the implementation and against reference digests:

- 4 scripted timestamp sequences of 300 steps (1,004 cases including the estimate after each step);
- 1,296 estimate cases over hand-built objects (12 counts, 9 `next` values, 6 delta patterns);
- 4,536 `on_vsync` cases over hand-built objects (whole struct hashed).

## Mutation check

75 hand-written source mutants of `vsync.cpp` (range check, null checks, repair, first timestamp, comparison and gap thresholds, delta slot and wrap, count growth, window indexing, sort, median index, zero median, minimum count) were compiled against the unchanged tests. 72 were killed by a plain build and 3 survived. Nine kills are crashes only (null checks removed, `in_range` weakened, the repair removed, an out-of-range slot read or written, a too-small window array).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented, the test object unchanged), 73 are killed and 2 survive. Observed UBSan messages for the extra kill and for crash-only kills:

- `j > 0` dropped from the sort: `index 18446744073709551615 out of bounds for type 'long unsigned int [16]'`. The plain build passes.
- window array of 8 instead of 16: `index 8 out of bounds for type 'long unsigned int [8]'`.
- invalid state accepted in `estimate_vsync_ns`: `index 16 out of bounds for type 'long unsigned int [16]'`.

The unmutated source passes all tests under the same flags.

Survivors in both builds, each argued:

- insertion sort `>=` instead of `>`: equal values may swap, the sorted values are identical. Equivalent.
- the repair also zeroing `last_ns`: the next step (`has_last` is false) overwrites `last_ns` with the timestamp, so it is never observable. Equivalent.

Not run: Windows (CI only), Android, the editor, `tools/ci/test.py`, fuzzing, a full local build.
