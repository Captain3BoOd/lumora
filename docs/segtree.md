# segtree

`core/segtree/segtree.h` and `segtree.cpp` (`lumora::segtree`) are range add and range sum over an array of up to `kMaxLen` = 65536 `uint64_t` values in caller storage of `8 * n` words: `seg_storage_words`, `seg_init`, `seg_range_add`, `seg_range_sum`. Every addition and sum wraps modulo 2^64. Ranges are half-open `[l, r)`, valid when `l <= r <= n`; an empty range is valid.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion (the helpers are plain loops), no global state. The source includes `<cstdint>` only. The tests use no 128-bit type or builtin either.

## Layout (internal, not asserted)

The header leaves the storage contents unspecified, so no test looks at them after a success. This implementation is an iterative lazy segment tree. `size` is the smallest power of two that is at least `n` (`size >= 1` for `n >= 1`) and `levels = log2(size)`. The storage holds `sum[0 .. 2 * size)` followed by `lazy[0 .. size)`, `3 * size` words. `sum[k]` is the sum under node `k` with the tags above `k` still pending, leaf `size + i` is element `i`, and `lazy[k]` is a delta not yet pushed to the two children of `k`. Elements `n .. size - 1` are zero leaves that no valid range includes.

Storage size argument. For `n >= 1`, `size / 2 < n` (otherwise a smaller power of two would do), so `size < 2n`, and `3 * size < 6n <= 8n`. The tight cases are `n = 1` (`size = 1`, 3 words of 8) and `n` just above a power of two (`n = 2^k + 1`: `size = 2^(k+1) = 2n - 2`, `3 * size = 6n - 6` words of `8n`). Every access is inside `storage[0 .. 3 * size)`, so inside `storage[0 .. 8n)`. For `n = 0` nothing is read or written. A query uses the storage as working memory (it pushes pending deltas down the two boundary paths), which the header allows ("a query may use the storage as working memory, which is why storage is not const").

## Check order

For `seg_init`, `seg_range_add`, `seg_range_sum`: NullArgument, then TooLarge, then ScratchTooSmall, then (add and sum only) OutOfRange. The need for a null check is "n above 0", also for `n` above `kMaxLen`: a null storage (any function), a null `values` (init) or a null `out` (sum) with `n > 0` is NullArgument before TooLarge. For `n = 0` the storage and `values` may be null. A null `out` for `seg_range_sum` is NullArgument for every `n`, including 0. ScratchTooSmall is `storage_words < 8 * n`, computed in 64 bits. OutOfRange is `l > r` or `r > n` (so `l > r` fails even when `r <= n`). `seg_storage_words`: a null `out_words` is NullArgument, then TooLarge. On `seg_init` and `seg_range_add` failures the passed buffer is untouched (tests compare it byte for byte); on a `seg_range_sum` failure only `*out` is untouched. A successful sum of an empty range, or any sum for `n = 0`, writes 0 to `*out`.

## Algorithms and the O(log n) argument

- `seg_init` copies the values into the leaves, zeroes the padding leaves and the tags, and builds the sums bottom up (linear in `size`, once).
- `seg_range_add` and `seg_range_sum`: first the pending deltas of the nodes on the two boundary paths are pushed down from the root (at most `2 * levels` pushes, each two O(1) applies). Then the usual bottom-up loop walks `levels` steps and applies the delta to (add) or reads (sum) at most two nodes per level. An add finally recomputes the sums of the nodes on the two boundary paths (at most `2 * levels` pulls). With `levels = ceil(log2 n) <= 16`, an operation is at most about `6 * levels` node visits, so O(log n) in the worst case. The bound is an argument: no test measures time or counts steps.
- Delta arithmetic: `apply` adds `delta * len` (unsigned, wraps) to the node sum and `delta` to its tag.

## Bounds

| Item | Bound |
| --- | --- |
| `n` | 0 to 65536 (524,288 storage words at the limit) |
| `l`, `r` | `0 <= l <= r <= n` |
| work per operation | at most about 6 * 16 node visits |

## Tests

`tools/segtree_ref.py` is an independent reference in Python that is NOT a segment tree: a plain list, each add and each sum a loop over the range, values kept modulo 2^64. Its block (digests, case and operation counts, the sizes) is spliced into the tests and recomputed there.

```
python3 tools/segtree_ref.py                                            # print the block
python3 tools/segtree_ref.py --check tests/segtree/segtree_tests.cpp
```

The tests also carry a naive oracle that shares no code with the implementation: a `std::vector` with a loop over the range for every add and sum. Buffers are heap buffers of exactly `8 * n` words (the shorter buffer for the ScratchTooSmall cases), so AddressSanitizer sees any access past the end. Init is run with two different storage prefills (alternating) and again over the same storage with a different `n`. Successful results are checked only through sums, never through the storage.

`tests/segtree/segtree_tests.cpp` has 15 test cases and 329 assertions (the sweeps count mismatches and assert once per sweep, plus the case count, the operation count and the digest).

| Scenario | Coverage | Against |
| --- | --- | --- |
| EXHAUSTIVE: n in 0..12, fixed values (including 2^64 - 1); on a fresh array every (l, r) in 0..n+1 for sum; every (l, r) in 0..n+1 with delta 1 and 2^64 - 1 for add, then (when valid) every (l, r) in 0..n+1 for sum; for n in 0..6 every ordered pair of valid adds (deltas 2^63 + 1 and 3) then every valid sum (3,637 cases, 158,728 operations) | exhaustive over this stated extent | oracle and reference digest |
| boundary sizes n in 1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1000: points 0, 1, 2, n/2, n-1, n; every sum over a pair of points, then on one running array every add over a pair of points with deltas 1, 2^64 - 1, 2^63, each followed by all those sums (23 cases, 27,979 operations) | NOT exhaustive | oracle and reference digest |
| 400 pseudo-random seeds, n 0 to 300, 40 operations each (400 cases, 16,000 operations) | NOT exhaustive | oracle and reference digest |
| random operations at n in 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 1023, 1024, 1025 (3,000 operations each, sums compared at every sum and at checkpoints) | SAMPLED, NOT exhaustive | oracle only |
| n = 65535 and n = 65536 (524,288 words), 20,000 seeded operations each (a third random ranges, a third ranges of up to 64 elements, a third prefixes, suffixes and halves); every 10th sum plus a checkpoint every 1,000 operations (32 sampled ranges, the full range, 8 end elements) | SAMPLED, NOT exhaustive | oracle only |
| n = 65536 hand-checked totals (all elements 2^64 - 1, the wrapped sum, an add to everything, adds at both ends) | hand-written | explicit numbers |
| hand-checked wrapping at n = 3 | hand-written | explicit numbers |
| TooLarge at n = 65537 and UINT32_MAX on all four functions with tiny never-read buffers; the same n with null and non-null pointers in the order tests | all functions, two sizes | return codes, untouched outputs |
| order matrix: every adjacent pair of checks has a test with the lower-priority error present as well; null storage for n = 1, 2, 3; `n = 0` with null pointers; `l > r` with `r <= n`; empty ranges at 0, inside and at n; UINT32_MAX endpoints; storage_words above the need accepted | hand-written | return codes, buffer and `*out` checks |

Only the first row is exhaustive, and only over the stated extent. Everything at or above 14 elements, the random rows and the max-size rows are sampled and labelled NOT exhaustive.

## Mutation check

115 hand-written source mutants of `segtree.cpp` (the tree size and level count, the layout offsets, apply, push, pull, the boundary-path pushes, every check and its order, the init loops, the add and sum loops and their pulls, the range checks), each compiled against the prebuilt object of the FINAL test file and run with `-a`, plain and with `-fsanitize=address,undefined -fno-sanitize-recover=all`. All 115 were run on the final test file in both builds (one mutant first survived plain and sanitizer, null storage with n = 1, so a test was added and everything was re-run). Timeouts are kept out of the kill totals. The batch runner used a 20 second timeout (the unmutated tests run in about 1 second).

| Build | Killed by a failing assertion | Killed by a crash or sanitizer report | Timeouts (NOT kills) | Survivors |
| --- | --- | --- | --- | --- |
| plain | 87 | 10 | 1 | 17 |
| sanitizer | 88 | 11 | 2 | 14 |

Crash-only kills, run alone with their observed messages. Plain: SIGSEGV after Catch2's FAILED line (no other message) for 38 and 39 (null storage never an error or not for n = 1), 56 and 57 (init null values and null storage unchecked), 58 (init null checks joined with and), 59 (init `n == 0` return removed), 72 (add: `r > n` unchecked), 93, 94 and 95 (sum null out, null storage). Sanitizer: UBSan "store to null pointer of type 'uint64_t'" (38, 39, 57, 58, 59, 93, 95), UBSan "load of null pointer of type 'const uint64_t'" (56), AddressSanitizer heap-buffer-overflow (62, init reads `values[n]`), SIGSEGV inside a failing test (72 and 94). Found to be an ordinary failing assertion when run alone: 3 (plain: tree size step 4) and 101 (sanitizer: sum `r > n` unchecked).

### Timeouts

Plain, 1 timeout: 27 (boundary-path pushes start at `levels - 1`). Run alone: n = 2, 3 and 8 finish at once with the right sums, n = 1 (`levels = 0`) was still running after 40 seconds. The counter `levels - 1` wraps to 4,294,967,295 and the loop counts down from there, so it is finite by construction (about 4.3 billion iterations, each with a shift of 32 or more, which is undefined) but it was NOT shown to finish. Classified: slow only, not size dependent, completion not shown. It is killed in the sanitizer build.

Sanitizer, 2 timeouts: 113 and 114, the time-only mutants below, which are correct but linear per operation (the sanitizer build of the max-size tests with 20,000 operations does not finish in 20 seconds). Slow only, with the doubled-size series below.

### Time-only mutants

Mutants 113 and 114 push every tree node before each add (113) or each sum (114). The results are the same, so no correctness test can kill them; both pass the plain test run and are NOT killed and NOT equivalent: they make an operation linear instead of logarithmic. The complexity is covered only by the argument above. One local timing series, 2,000 alternating random add and sum operations at -O0 on one machine (local evidence only, not asserted anywhere):

| n | original | mutant 113 | mutant 114 | both |
| --- | --- | --- | --- | --- |
| 16,384 | 1.3 ms | 115.5 ms | 112.1 ms | 156.0 ms |
| 32,768 | 1.5 ms | 222.1 ms | 217.7 ms | 304.1 ms |
| 65,536 | 1.6 ms | 442.0 ms | 433.7 ms | 605.9 ms |

The mutants roughly double with `n`; the original barely moves. The checksums of all variants were equal.

### Survivors

Argued equivalent (results never differ): 
- 1, `size <= n` in the size loop: for a power of two `n` the tree is twice as large. `size <= 2n`, so `3 * size <= 6n` words and every access stays inside `8n` words; the extra leaves are zero padding that no valid range reads. Equivalent.
- 5, the tags stored at offset `3 * size`: `4 * size < 8n` for `n >= 1` (and `n = 2^k + 1` gives `4 * size = 8n - 8`), the two arrays stay disjoint and inside `8n` words. Equivalent (layout only).
- 11, `k <= size` in `apply`: only node `size` (leaf 0) writes `lazy[size]`, the word at offset `3 * size`, inside `8n` words (`3 * size < 8n`), and no code reads `lazy[size]` (reads are `lazy[k]` for `k < size`, init clears `lazy[0 .. size)`). Equivalent.
- 14, `push` without the zero test: pushing a zero delta adds 0 to both children and sets the tag to 0. Equivalent.
- 24, `pull` adding `lazy[k]`: a pull runs only on a node of a boundary path, and `push_paths` has just pushed exactly the same set of nodes (same condition, same nodes), so `lazy[k]` is 0 there. Equivalent.
- 28 and 33, the alignment test removed from the pushes: pushing an extra node never changes an element (it only moves a pending delta one level down) and the order is still top down. Equivalent.
- 34, `r >> i` for `(r - 1) >> i`: the push happens only when `r` is not a multiple of `2^i`, and then both are the same node. Equivalent.
- 47, a 32-bit comparison in the scratch check: `n <= 65536` here (TooLarge came first), so `8n < 2^32` and the casts change nothing. Equivalent.
- 60, `sum[0]` not cleared: node 0 is never read. Equivalent.
- 61 and 63, the padding leaves set to 1 or left as they were: a valid range lies in `[0, n)`; the nodes a query reads or a pull sums are fully inside the range or on a boundary path below it, and a node that contains a padding leaf is never fully inside `[0, n)`, so no result depends on the padding. Equivalent.
- 73, the early return for an empty range removed: with `lo == hi` the loops add or read nothing, the extra pushes only move deltas down and the pulls recompute sums from correct children. For `n = 0` the tree pointers are formed from the (possibly null) storage but nothing is read or written. Equivalent in results (for `n = 0` it does form a pointer offset from a null storage, which the language leaves undefined; the sanitizer did not flag it).
- 91, `hi >> i` for `(hi - 1) >> i`: pulled only when `hi` is not a multiple of `2^i`, where both are the same node. Equivalent.

Survives plain, killed by the sanitizer, NOT equivalent: 62 (`i <= n` reads `values[n]`, one element past the caller's array).

Survivors NOT killed and NOT proved equivalent: none, apart from the two time-only mutants above (113 and 114 survive the plain build).

## Not run

Windows was not run locally. The storage contents, storage words past `8n`, aliasing of `values` and `storage`, a storage the caller changed between calls, and a storage that did not come from `seg_init` with the same `n` are not tested (the header leaves them open or forbids them). Exhaustive coverage stops at n = 12 (n = 6 for ordered pairs of adds); larger sizes, including the 65535 and 65536 maximum, are sampled. No timing or step-count test exists: the O(log n) bound is an argument, with one local timing series for the time-only mutants. One plain timeout (27) was not shown to finish.
