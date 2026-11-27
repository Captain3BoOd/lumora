# sortedops

`core/sortedops/sortedops.h` and `sortedops.cpp` (`lumora::sortedops`) are operations on ascending (non-decreasing) arrays of u32 keys: `lower_bound_u32`, `upper_bound_u32`, `merge_sorted_u32` and `unique_sorted_u32`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no libm, no 128-bit types, no allocation, no recursion, no global state. The source includes `<cstdint>` only. The functions keep no state, so there is nothing to share between threads and no thread tests were written.

## Contract notes

Sorted input is a precondition. It is not checked, and unsorted input is never tested and nothing is claimed about it. Input arrays must not overlap an output array (not tested).

## Check order

- Every function: a null pointer argument gives NullArgument, then a count above `kMaxCount` (2^26) gives TooLarge, then (merge only) BufferTooSmall. A null pointer wins even when another argument is oversized. An array pointer counts only while its length is above 0; an out pointer always counts; for merge, `dst` counts while `na + nb` is above 0 (also when `dst_cap` is above 0). On any failure nothing is written: output arrays and out pointers are untouched.
- `lower_bound_u32(keys, n, key, out_index)`: `*out_index` is the smallest `i` in `[0, n]` with `keys[i] >= key` (`n` when every key is smaller). `n == 0` is Ok with index 0 and `keys` may be null. Reads only, and only below `n`.
- `upper_bound_u32`: the same with `keys[i] > key`.
- `merge_sorted_u32(a, na, b, nb, dst, dst_cap, out_len)`: each of `na` and `nb` is checked against `kMaxCount` on its own. The sum `na + nb` (at most 2^27, it fits in 32 bits) is computed in 64 bits for the null rule and the capacity check: `dst_cap < na + nb` is BufferTooSmall, `dst_cap == na + nb` succeeds. All elements are kept, duplicates included; `*out_len = na + nb`. At most `na + nb` elements of `dst` are written. With `na == nb == 0` and the out pointers set the call is Ok, no array is touched and null arrays and a null `dst` are fine.
- `unique_sorted_u32(keys, n, out_len)`: keeps the first of each run of equal keys, in place. `keys[0 .. *out_len)` holds each distinct key once, ascending. `n == 0` gives `*out_len` 0 (`keys` may be null). The elements from `*out_len` on are unspecified and are not tested; the tests check that nothing at or above `n` is written.

| Item | Bound |
| --- | --- |
| each input count | 0 to `kMaxCount` (2^26) |
| `na + nb` | 0 to 2^27, computed without overflow |
| `dst_cap` | any `uint32_t` |
| aliasing of inputs and outputs | not supported, not tested |

## Tests

`tools/sortedops_ref.py` is an independent reference in Python, written from the header text with Python integers: bounds by a linear scan for the first index that satisfies the definition, merge as `sorted(a + b)`, unique with `itertools.groupby`. It shares no binary search, merge loop or in-place write with the C++. It prints a generated block with 64-bit digests that the tests recompute. It runs in under a second.

```
python3 tools/sortedops_ref.py                                           # print the block
python3 tools/sortedops_ref.py --check tests/sortedops/sortedops_tests.cpp
```

The tests also carry a naive oracle that shares no code with the implementation: bounds by a linear scan, merge by the rank of each element (its index plus the number of elements of the other array that go before it), unique by comparing each element with its neighbour in the input. Inputs are heap buffers of exactly the stated length, so AddressSanitizer sees any access past the end; the write-rule runs add canaries after the output and fill the output range with canaries, so a write to a failed call or past `na + nb` or `n` is seen. Inputs are compared with copies after every call (reads only).

`tests/sortedops/sortedops_tests.cpp` has 8 test cases and 125 assertions (sweeps count mismatches and assert once per sweep, plus the case count, the operation count and the digest). The 4-value alphabet is {0, 1, 2, FFFFFFFF}; the 7 search keys are 0, 1, 2, 3, 80000000, FFFFFFFE, FFFFFFFF.

| Scenario | Cases | Operations | Against |
| --- | --- | --- | --- |
| lower and upper bound: every ascending array of length 0 to 6 over the alphabet (210 arrays) x 7 keys | 1,470 | 2,940 | oracle, reference digest |
| merge: every pair of ascending arrays of length 0 to 5 (126 x 126) x every `dst_cap` from 0 to `na + nb + 1`, each also with canary padding | 158,760 | 158,760 (and as many padded) | oracle, reference digest |
| unique: every ascending array of length 0 to 7 over the alphabet, each also with canary padding | 330 | 330 (and as many padded) | oracle, reference digest |
| pseudo-random: 300 seeds, two arrays of up to 300 elements (three gap sizes), 40 search keys, merge with the exact capacity and one less, unique of both | 300 | 25,200 | oracle, reference digest |

The first three rows are exhaustive over their stated sets. The pseudo-random scenario uses a fixed 64-bit LCG (seeds 1 to 300) and is NOT exhaustive. Probing representative search keys (the 7 keys above, and for the random arrays an element, the element plus or minus one, and a random value) is NOT exhaustive over uint32 values. Other cases: argument checks for every null combination and the order Null, TooLarge, BufferTooSmall (a null pointer wins over an oversized count, also when `na + nb` wraps 32 bits), untouched out values and output arrays on failures, equal-key bounds, every length 0 for merge with null pointers, merge with one empty side, duplicates kept by merge, and header anchors.

The maximum counts are tested only where no array is read: a count of `kMaxCount + 1` or more with 1-element arrays gives TooLarge, and merge of two arrays of `kMaxCount` elements with `dst_cap` below `2^27` (for example `2^27 - 1`, `kMaxCount`, 0) gives BufferTooSmall without reading or writing an array. No 256 MiB array is allocated.

## Mutation check

134 hand-written source mutants of `sortedops.cpp` (the binary search start, loop test, midpoint, comparison direction and both bound updates, every null rule, the TooLarge and BufferTooSmall tests and their order and results, the 64-bit sum, the merge loops, tail copies and tie rule, the unique start index, comparison, write index and length) were compiled against the unchanged tests. Plain build: 121 killed, 13 survived. With `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented): 121 killed, the same 13 survive.

Plain build: 89 kills by a failing assertion, 26 by a crash alone and 6 by a timeout at 25 seconds (mutants 3, 5, 6, 7, 19 and 22: the loop test `<=`, the midpoint without `lo`, the upper midpoint, the midpoint `hi / 2`, `lo = mid` and `hi = mid + 1`; the cause was not investigated, they presumably loop). The crash-only kills were run individually: 20 report SIGSEGV with the Catch2 message "Segmentation violation signal" (the bound start `n - 1` and `n + 1`, `hi = mid - 1`, the `search` TooLarge check `+ 1` or removed, null checks removed or weakened in `search` and `merge`, `merge` loop `<=`, tail b writing a, tail a without a step, and `unique` with `w = 0` and the `out_len` null check removed), 3 exit with SIGSEGV and no message (merge buffer check `+ 1`, `dst_cap < na`, and the `unique` TooLarge check removed), and 3 abort with SIGABRT and no message (merge loop `||`, merge tail loops `<=`).

Sanitizer build: 90 kills by a failing assertion, 5 timeouts (mutants 5, 6, 7, 19 and 22), 26 by a sanitizer report or crash. Run individually, 25 of them: load of a null pointer (9: null checks removed in `search` and `merge`, merge loop `||`, tail b `<=`, tail a writing b), store to a null pointer (5: `out_index`, `dst`, `out_len` null checks, `dst` null rule by `na`, `unique` `out_len` null check), heap-buffer-overflow (4: merge loop `i <= na`, `j <= nb`, tail a `<=`, tail b writing a), an AddressSanitizer SEGV report (4: bound `hi = n - 1`, search TooLarge `+ 1` and removed, `unique` `w = 0`), "attempting free on address which was not malloc()-ed" (1: `unique` TooLarge check removed) and an exit with SIGSEGV and no message (2: merge buffer check removed, tail a without a step). The remaining one, mutant 3 (the loop test `<=`), was an abnormal exit in the sweep but a 25 second timeout when run individually, so it is counted as a crash-only kill but not confirmed; in the plain build it is a timeout.

Survivors (13), each argued:

- Bound loop `lo != hi` (mutant 4): `lo <= hi` is an invariant (`lo = mid + 1` with `mid < hi`, `hi = mid` with `mid >= lo`), so `lo != hi` and `lo < hi` are the same test. Equivalent.
- Bound returns `hi` (mutant 23): the loop ends with `lo >= hi` and `lo <= hi`, so `lo == hi`. Equivalent.
- Bound midpoint `(lo + hi) / 2` (mutant 9): `lo + hi` is at most 2^27 (no wrap) and `floor((lo + hi) / 2) == lo + floor((hi - lo) / 2)`. Equivalent for every count the function accepts.
- Bound midpoint `lo` (mutant 8) and `lo + (hi - lo) / 3` (mutant 10): any `mid` in `[lo, hi)` keeps the invariant that the answer lies in `[lo, hi]` for an ascending array (the predicate `keys[i] < key`, or `<=`, is true on a prefix), and each step moves `lo` or `hi` strictly, so the loop ends with the same index; only the number of probes differs. Equivalent in results (the probe count is not part of the contract and is not tested).
- Merge buffer check `dst_cap < na + nb` in 32 bits (mutant 69): after the TooLarge check `na` and `nb` are at most 2^26, so the sum is at most 2^27 and does not wrap. Equivalent.
- Merge tie `b[j] <= a[i]` (mutant 77): the two elements are equal u32 values, so taking either gives the same output. Equivalent.
- Merge `*out_len = na + nb` (mutant 97): every loop step and tail step adds one to `k` and one to `i` or `j`, and the loops end with `i == na` and `j == nb`, so `k == na + nb`. Equivalent.
- Unique start `r = 0` (mutant 115): at `r = 0` the key is compared with `keys[w - 1] = keys[0]`, which is itself, so nothing is written. Equivalent.
- Unique compare with the input neighbour `keys[r - 1]` (mutant 119): `keys[r - 1]` still holds its original value at step `r` (a slot `p` is overwritten only at a step `r' >= p` with `r' < r`, so only `p = r - 1 = r'`, a write of the same value). In an ascending array `keys[r] != keys[r - 1]` is the start of a new run, the same as `keys[r] != keys[w - 1]`. Equivalent under the sorted precondition.
- Unique compare `keys[r] > keys[w - 1]` (mutant 120): `keys[w - 1]` is the last distinct key kept, at most `keys[r]` in an ascending array, so `!=` and `>` agree. Equivalent under the sorted precondition.
- TooLarge at `n >= kMaxCount` in `search` (mutant 32) and `unique` (mutant 105): these are NOT equivalent, they reject `n == kMaxCount`, which the header accepts. They are not killed because a successful call at exactly 2^26 elements needs a 256 MiB array and was not run. Not killed and not proved equivalent. (The same mutant in `merge`, `na >= kMaxCount` and `nb >= kMaxCount`, is killed by the dual-maximum BufferTooSmall test.)

## Not run

Successful calls at exactly `kMaxCount` (2^26) elements (bounds, unique, and merge with `na` or `nb` equal to `kMaxCount`, including the dual maximum with `dst_cap == 2^27`) were NOT run (256 MiB and more). The pseudo-random scenario and the probed search keys are not exhaustive. Unsorted input, overlap of inputs and outputs, and threads are untested. Windows runs only in CI.
