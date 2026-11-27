# Fenwick tree

`core/fenwick/fenwick.h` and `fenwick.cpp` (`lumora::fenwick`) keep a Fenwick tree (binary indexed tree) in a caller array of `n` int64 values: `init`, `add`, `prefix_sum`, `range_sum`, `first_prefix_at_least`.

Evidence label: unit tests only (CI-verified). Windows runs only in CI.

Integer only: no floating point, no libm, no 128-bit types, no builtins, no allocation, no recursion, no global state. The source includes only its header.

## Rules

- `n` is valid when it is at most 2^24 and `tree` is non-null when `n` is above 0. An invalid argument, a null `out`, or an index or range outside the tree returns false and changes nothing (also not `*out`).
- Arithmetic wraps modulo 2^64: elements and sums are computed on unsigned values and read back as two's complement, so `add` never fails on overflow. The conversions are written without relying on implementation-defined behavior.
- `tree[k - 1]` holds the sum of the elements `k - lowbit(k) .. k - 1` (the usual layout). Only these functions may write the array after `init`.
- `first_prefix_at_least` needs non-negative elements and a total of at most INT64_MAX; it walks down the tree by powers of two. A target of 0 or less gives 0; no prefix reaching the target gives `n + 1`.

## Bounds

| Item | Bound |
| --- | --- |
| `n` | 0 to 2^24 |
| index | 0 to `n - 1` |
| cost | O(log n) per call |

## Tests

`tools/fenwick_ref.py` is an independent Python model: a plain list of elements and sums by direct addition modulo 2^64, with no tree logic. It prints the digests that `tests/fenwick/fenwick_tests.cpp` recomputes: 12 wrap streams of 600 operations (deltas include INT64_MAX, INT64_MIN, 1 and -1, point adds, prefix, range and single-element sums) and 12 search streams of 300 operations. The test file also has hand cases, every size from 1 to 70 against a naive sum, failure cases that must change nothing, and an `init` test on a tree that held old values.

No array of 2^24 elements is allocated. Invalid-size calls use a one-element dummy buffer that must stay untouched.

## Mutation check

67 hand-written single-edit source mutants of `fenwick.cpp` (validity rule, sign conversions, loop steps of the prefix walk and of `add`, loop bounds in `init`, every argument check, range arithmetic, and each step of the search descent) were built against the unchanged tests. 61 were killed (two of them by a timeout, since the loop no longer ends) and 6 survive:

- Validity with `n < 2^24` instead of `<=`: not equivalent. It differs only at exactly `n = 2^24`, which needs a 128 MiB array for any call that reads or writes. Not killed, and not run.
- Sign conversion with `v < INT64_MAX` or `v <= INT64_MAX - 1` instead of `<=`: at `v == INT64_MAX` the other branch computes `-(int64)(~v) - 1`, which wraps to the same value in two's complement. Same result in a plain build and with UBSan here, but the mutant's path negates INT64_MIN, which the language does not define. Not killed, and not proved equivalent for every compiler.
- `v > 0` instead of `v >= 0` in the unsigned conversion: for 0 the other branch gives the bit pattern 0 as well. Equivalent.
- Search start `step <= n` instead of `step <= n / 2`: the extra doubling gives a step above `n`, which the `n - pos >= step` test never takes. Equivalent.
- Search start without the `n >= 2` test: for `n` of 0 or 1, `n / 2` is 0 and `step` is already above it. Equivalent.

## Not run

No call at `n = 2^24` (memory), no thread tests, no aliasing tests.
