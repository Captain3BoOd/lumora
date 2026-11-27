# lis

`core/lis/lis.h` and `lis.cpp` (`lumora::lis`) give three answers about a sequence `a[0 .. n)` of `u32` values: the length `L` of the longest strictly increasing subsequence, how many longest ones there are (modulo 2^64) and the canonical one, the lexicographically smallest INDEX list (not the smallest value list). Limit: at most `262144` values.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `46fbb3d`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers. The header "Decisions" block was treated as data.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no large automatic arrays. The source includes `lis.h` only. All working memory is the caller's scratch.

## Check order and writes

- `lis_scratch_words`: a null `out_words` is NullArgument; then `n > 262144` is TooLarge (nothing written). Otherwise `*out_words = 16 * n + 8` (8 for `n = 0`).
- `lis_run`: NullArgument for a null `a`, `scratch` or `out_idx` only while `n > 0`, and for a null `out_len` or `out_count` always (the condition uses the given `n`, also above the limit); then `n > 262144` is TooLarge; then, only while `n > 0`, `scratch_words < 16 * n + 8` is ScratchTooSmall. On any failure nothing is written. On success only `*out_len`, `*out_count` and `out_idx[0 .. L)` are written; the entries `out_idx[L .. n)` stay as they were. More than `16 * n + 8` scratch words are accepted. `n = 0` gives length 0, count 1 and writes no index; `a`, `scratch` and `out_idx` may be null and `scratch_words` is ignored.
- Scratch contents after a call are unspecified. The tests never assert them.

## Method

The method uses `10 * n + 6` of the `16 * n + 8` scratch words, in this layout (word counts): `tails` n, `len` n, `off` n + 2, `fill` n + 2, `lidx` n, `cum_lo` n, `cum_hi` n, `rlen` n, `roff` n + 2, `rlist` n.

1. Forward patience pass. `tails[p]` is the smallest last value of an increasing subsequence of length `p + 1`. For each `i`, a binary search for the first `p` with `tails[p] >= a[i]` gives `len[i] = p + 1`, the longest increasing subsequence that ends at `i`. `L` is the final number of tails.
2. Levels. The elements with equal `len` form a level. Inside one level the values never increase with the index: if `j < k` in the same level had `a[j] < a[k]`, then `k` would have `len[k] >= len[j] + 1`. A counting sort of `len` gives `off[l]`, the start of level `l` in the index-ordered arrays `lidx`, `cum_lo` and `cum_hi`.
3. Counting pass, left to right. `cnt[i] = 1` when `len[i] = 1`, else the sum of `cnt[j]` over `j < i`, `a[j] < a[i]`, `len[j] = len[i] - 1`. Level `len[i] - 1`, as filled so far, holds exactly the earlier elements of that level. Its values do not increase along the segment, so the `j` with `a[j] < a[i]` are a suffix, found by a binary search for the first position with a value below `a[i]` (a suffix exists: some `j` with `len[j] = len[i] - 1` and `a[j] < a[i]` has to exist, and the last entry of the segment is the smallest value of the segment, so it qualifies). Each segment keeps prefix sums of `cnt` as two `u32` halves (`cum_lo`, `cum_hi`), wrapping modulo 2^64, so the suffix sum is `all - before`. The count is the last prefix sum of level `L`.
4. Backward pass. The same patience idea from the right (heads decrease with `p`; first `p` with `tails[p] <= x`) gives `rlen[i]`, the longest increasing subsequence that starts at `i`.
5. A counting sort of the indices by `rlen` gives `rlist`, with each group `rlen == r` in index order (`roff[r]` is its start; `fill` is reused for the group sizes).
6. Greedy. The first index is the first entry of group `rlen == L`. Each next index `t` is the first entry after the previous index in group `rlen == L - t`, found by a binary search. Correctness: let `prev` have `rlen[prev] = r + 1`. Then some `k > prev` has `rlen[k] = r` and `a[k] > a[prev]`. Inside group `rlen == r` the values do not increase with the index (two members `j < k` with `a[j] < a[k]` would give `j` a longer subsequence). So the first member `f` of the group after `prev` has `f <= k` and `a[f] >= a[k] > a[prev]`: it is a valid successor, and it is the smallest valid index. Smallest index at each step gives the lexicographically smallest index list.

## Bounds

Binary search: a search over an interval of `m` entries takes at most `floor(log2 m) + 1` loop iterations (one comparison each); with `m <= 262144` that is at most 19.

The ACTUAL code runs these searches: `n` in the forward pass, at most `n` in the counting pass (only for `len > 1`), `n` in the backward pass and `L - 1 <= n - 1` in the greedy pass, so at most `4n - 1` searches and at most `19 * (4n - 1)` search iterations. The other loops (clears, prefix sums, counting sorts, outer loops) run `6n + 6L + 7` iterations at most (step by step: the forward pass `n`; step 2 `2L + 3 + n`; the counting pass `n`; the backward pass `n`; step 5 `3L + 5 + 2n`; the greedy loop `L - 1`), at most `12n + 7` because `L <= n`. In total at most `88n + 7` loop iterations, each with a constant number of operations: O(n log n) comes from the searches, and the bound proved here is linear in `n` because every search is capped at 19 iterations. No recursion, no allocation.

### One-time scaling table (observation, not asserted)

Single run, best of 3, `lis_run` only, machine class Intel Xeon Platinum 8375C 2.90 GHz, 1 vCPU, Linux, g++ (these are observations, not guarantees; the test suite asserts no timing).

| input | n | -O0 (s) | -O2 (s) |
|---|---|---|---|
| increasing | 32768 | 0.0045 | 0.0016 |
| increasing | 65536 | 0.0095 | 0.0034 |
| increasing | 131072 | 0.0203 | 0.0074 |
| increasing | 262144 | 0.0429 | 0.0157 |
| decreasing | 32768 | 0.0010 | 0.0004 |
| decreasing | 65536 | 0.0020 | 0.0009 |
| decreasing | 131072 | 0.0041 | 0.0019 |
| decreasing | 262144 | 0.0083 | 0.0040 |
| constant | 32768 | 0.0010 | 0.0004 |
| constant | 65536 | 0.0020 | 0.0008 |
| constant | 131072 | 0.0040 | 0.0019 |
| constant | 262144 | 0.0082 | 0.0037 |
| random | 32768 | 0.0075 | 0.0036 |
| random | 65536 | 0.0159 | 0.0076 |
| random | 131072 | 0.0342 | 0.0164 |
| random | 262144 | 0.0751 | 0.0353 |
| sawtooth 1000 | 32768 | 0.0064 | 0.0027 |
| sawtooth 1000 | 65536 | 0.0134 | 0.0052 |
| sawtooth 1000 | 131072 | 0.0270 | 0.0108 |
| sawtooth 1000 | 262144 | 0.0573 | 0.0233 |

## Tests

`tests/lis/lis_tests.cpp`, 11 test cases, 407 assertions. Expected values come from three labelled sources. (1) DIGESTS from `tools/lis_ref.py` (independent Python: every index subset, an O(n^2) dynamic programme with exact integers, and a Fenwick tree over ranks with patience for the backward lengths; the generated block is spliced into the test, `python3 tools/lis_ref.py --check tests/lis/lis_tests.cpp` verifies it, and the test recomputes each digest from the C++ results; each fold covers `n`, `L`, the count modulo 2^64 and the `L` indices). (2) An ORACLE inside the test that shares no code with `lis.cpp`: every index subset up to 12 values, the quadratic programme up to 400, a Fenwick tree plus patience above. (3) Closed forms and exact integers for the large and the 2^63..2^65 cases.

Every successful call runs with 8 sentinel entries before and 16 after `out_idx`, with `out_idx[L .. n)` unchanged, and with exactly `16 * n + 8` scratch words. On failure all outputs must be untouched. Extents, with their labels:
- EXHAUSTIVE: every sequence of length 0 to 8 over `{0, 1, 2}` (9841); length 0 to 6 over `{0, 1, 0x80000000, 0xFFFFFFFF}` (5461); every permutation of `0 .. n - 1` for `n` = 0 to 7 (5914); the empty sequence, all 256 one-value sequences and all 65536 two-value sequences over 0..255 (65793).
- SAMPLED, fixed seeds 3004, 3016 and 3032: 600 random sequences each of length 17 to 400 over 4, 16 and 2^32 values (1800 sequences), checked against the quadratic oracle.
- Listed large patterns (18, SAMPLED patterns, checked by the Fenwick oracle and the digests): increasing (262144, 262143), decreasing, constant 0 (262144), constant 0xFFFFFFFF (262143), alternating 0/1, alternating extremes, sawtooth 1000 (262144) and 512 (262142), each value twice and three times, 131072 pairs, random over 2, 16 and 2^32 values (262144, 262143), a random permutation and a random sequence of length 1000.
- True counts above 2^63 (exact integers in the Python reference): 63 pairs (`2^63`), 64 pairs (`2^64`, residue 0, length 64, canonical list the first index of every pair), 65 pairs (`2^65`, residue 0) and 63 pairs followed by `129, 128, 127` (`3 * 2^63`, residue `2^63`).
- Closed forms at 262144: increasing (`L = n`, count 1), decreasing and constant (`L = 1`, count `n`, index 0), 131072 pairs (count 0 modulo 2^64).
- Hand rows (24): ties, duplicates, a first index that is not the smallest value, 0 and 0xFFFFFFFF. The expected values come from the Python brute force, not from the C++ code.
- Scratch rules: `lis_scratch_words` gives 8, 24, 88 and `16 * 262144 + 8`; `16 * n + 7` words give ScratchTooSmall with every output untouched; `16 * n + 8` and larger sizes succeed; `n = 0` with null `a`, `scratch` and `out_idx`.
- Check-order matrix: every null argument, null before TooLarge, TooLarge (`kMaxLen + 1` and `0xFFFFFFFF`) before ScratchTooSmall, nothing written on failure.
- Not asserted: scratch contents after a call, anything else the header leaves open, running time.

## Mutation testing

Real source mutants of `lis.cpp` (140), each compiled with the prebuilt test object and run on the FINAL test file, one at a time, in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. Plain: 126 killed, 14 survived. Sanitizer build: 124 killed, 14 survived (the same 14), 2 timed out. No mutant was killed only by the sanitizer build.

Timeouts (30 s each) are NOT kills. `fwd: mid rounds up` and `bwd: mid rounds up` hang in the sanitizer build, also with the large and closed-form cases excluded: the interval no longer shrinks when `hi - lo = 1`, so the loop never ends. The plain build killed both (the first by a failing assertion, the second by SIGSEGV).

Crash-only kills (the run shows no failing assertion), 31 mutants in the union, each re-run alone. Plain: 27 end in SIGSEGV with Catch2's "Segmentation violation signal" message and no assertion, 4 end in failing assertions when run alone (`fwd: hi = top - 1 guard`, `cnt: base from off[l]`, `cnt: search hi = base + used + 1`, `bwd: tails > to >=`). Sanitizer build: 9 report `runtime error: load of / store to null pointer` (the null-argument mutants), 19 end in SIGSEGV, 2 end in failing assertions (`fwd: tails not stored`, `seg: fill not cleared`), and `bwd: mid rounds up` hangs.

Survivors, with the argument (the same 14 in both builds):
- `fwd: lo >= top` and `bwd: top condition >=` (instead of `==`): equivalent, `lo` is in `[0, top]` because the search starts with `hi = top`, so `lo > top` never happens.
- `seg: off loop to total_len`, `sort: roff clear to total_len` and `sort: fill clear to total_len`: equivalent in output. They skip clearing slot `L + 1` only. That slot is incremented or summed into but never read to decide anything: every read of `off`, `roff` and `fill` uses an index `<= L`.
- `seg: prefix stops early` and `sort: prefix stops early`: equivalent, the skipped step only writes slot `L + 1`, which is never read.
- `seg: prefix start 0` (first prefix step skipped) and `sort: prefix start 2`: equivalent, slot 0 holds 0 and slot 1 counts the elements with `len` (or `rlen`) 0, which does not exist, so `off[1] += off[0]` and `roff[1] += roff[0]` add 0.
- `cnt: search hi = base + used - 1` and `greedy: hi = roff[r] + fill[r] - 1`: equivalent. The last entry of the searched range always satisfies the search predicate (see the method: the last entry of the segment is the smallest value there and some entry has a value below `a[i]`; in the greedy pass a valid successor exists), so the first position satisfying it is never above `hi - 1`, and a search that cannot return `hi` ends at the same position.
- `cnt: answer adds` (`answer += cnt` for level `L`): equivalent, the last prefix sum of the level is the sum of its `cnt` values, both modulo 2^64.
- `greedy: > to >=`: equivalent, an entry of group `r` is never equal to `prev` (it is in group `r + 1`).
- `out: written before greedy`: equivalent in output, the greedy pass has no failure exit, so `*out_len` is the same at return.

## Not run

- Windows: CI-only.
- No timing assertion in the suite (maintainer decision); the scaling table is a one-time observation.
- No exhaustive run beyond the alphabets and lengths listed above; longer sequences are fixed-seed samples and listed patterns.
- Scratch contents after a call and overlapping arrays are not tested (unspecified).
- The mutant test object is not sanitizer-instrumented; only the mutated source is.
- The 2 timed-out sanitizer mutants were not killed in that build (the plain build killed them); there are no unproven survivors.
