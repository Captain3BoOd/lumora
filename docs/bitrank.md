# bitrank

`core/bitrank/bitrank.h` and `bitrank.cpp` (`lumora::bitrank`) are rank and select over a bit vector of 64-bit words, up to `kMaxBits` = 2^24 bits: `bitrank_dir_words`, `bitrank_build` (a directory of `(nbits + 63) / 64 + 1` u32 words), `bitrank_rank1` and `bitrank_select1`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state. The source includes `<cstdint>` only, and so do the tests (apart from `<cstddef>`, `<memory>` and `<vector>` for buffers).

## Directory layout (internal, not asserted)

The header leaves the directory contents open, so they are an implementation detail and no test asserts them. For the record, this implementation stores in `dir[w]` the number of set bits in words `0 .. w-1` (the last word counted only below `nbits`), and `dir[words]` is the total. `bitrank_build` writes only `dir[0 .. need)` where `need = words + 1`. Nothing is asserted about directory words at or past `need` (a longer directory is accepted and the extra words are never read), and a failed build leaves the directory untouched. A stale or corrupt directory is NOT tested: the directory is trusted.

## Check order

For `bitrank_build`, `bitrank_rank1` and `bitrank_select1`: NullArgument (a null `dir`, a null output, or a null `bits` when `nbits > 0`; `bits` may be null for `nbits == 0`), then TooLarge (`nbits > kMaxBits`), then ScratchTooSmall (`dir_words < need`), then OutOfRange (`i > nbits` for rank, `k >= total` for select). `bitrank_dir_words` checks the null output, then TooLarge. The outputs are untouched on every failure.

## Algorithms

- Popcount: a SWAR sequence (pairs, nibbles, bytes, then a multiplication that sums the byte counts into the top byte; the unsigned multiplication wraps modulo 2^64).
- Padding: bits above `nbits` in the last word are never counted. Reading `bits[words - 1]` is allowed; `bits[words]` is never read.
- `bitrank_rank1(i)`: `dir[i / 64]` plus the popcount of the bits of word `i / 64` below `i % 64`. For `i % 64 == 0` (including an aligned `i == nbits`) no vector word is read.
- `bitrank_select1(k)`: a binary search over the directory for the word `lo` with `dir[lo] <= k < dir[lo + 1]`, then the lowest set bit is cleared `k - dir[lo]` times and the position of the lowest set bit left is found by shifting.
- Time: build is linear in the words, rank is constant, select is logarithmic in the words plus at most 63 steps in the word.

## Bounds

| Item | Bound |
| --- | --- |
| `nbits` | 0 to 2^24 (262,144 words; 262,145 directory words) |
| `i` for rank | 0 to `nbits` |
| `k` for select | 0 to total - 1 |
| rank result | at most 2^24 (fits u32) |

## Tests

`tools/bitrank_ref.py` is an independent reference in Python with Python integers: the vector is one integer, rank counts with `int.bit_count` on a masked integer, select takes the k-th element of the list of set positions and checks it against `bisect` on the prefix counts. Its block (digests, case and operation counts, the shape sizes) is spliced into the tests and recomputed there.

```
python3 tools/bitrank_ref.py                                           # print the block
python3 tools/bitrank_ref.py --check tests/bitrank/bitrank_tests.cpp
```

The tests also carry a naive oracle that shares no word or popcount logic with the implementation: the vector is one char per position, and rank and select are plain loops over positions. Buffers are heap buffers of exactly the stated size, so AddressSanitizer sees any access past the end: `bits` has exactly `(nbits + 63) / 64` words, the directory exactly `need` words (pre-filled with a sentinel), and outputs are pre-set to a sentinel to see untouched outputs.

`tests/bitrank/bitrank_tests.cpp` has 11 test cases and 153 assertions (the sweeps count mismatches and assert once per sweep, plus the case count, the operation count and the digest).

| Scenario | Coverage | Against |
| --- | --- | --- |
| every bit vector of nbits 0 to 14 (65,534 vector and padding runs: 32,767 vectors), zero and all-ones padding, rank for every i in 0..nbits+1 and select for every k in 0..total+1 | EXHAUSTIVE | oracle and reference digest |
| nbits in 1, 63, 64, 65, 127, 128, 129, 191, 192, 193, 256, 257: empty, full, even, odd, every single bit, every pair (nbits up to 65), every prefix run and suffix run; zero and all-ones padding; every i and k | exhaustive over the listed shapes only | oracle and reference digest |
| 400 pseudo-random seeds, nbits 0 to 4096, four density modes; 73 sampled rank positions and 73 sampled select ranks per seed including the endpoints | SAMPLED, NOT exhaustive | oracle and reference digest |
| nbits = 2^24 (262,144 words, 262,145 directory words): full, empty, alternating, last bit only, first bit only; rank at every word boundary and its neighbours, at `nbits`, `nbits + 1` and on a stride of 4099; select at 0, total / 2, total - 1, total and 0xFFFFFFFF (plus a stride for full, empty and alternating) | SAMPLED, NOT exhaustive | closed-form expectations |
| nbits = 2^24 - 1 with all-ones padding in the last word (full and empty vectors), sampled the same way | SAMPLED, NOT exhaustive | closed-form expectations |
| TooLarge at nbits = 2^24 + 1 and UINT32_MAX, on every function (`dir_words`, build, rank, select), with tiny buffers that must not be read | all four functions, two sizes | return codes, untouched outputs |
| null and order matrix: Null beats TooLarge, TooLarge beats ScratchTooSmall, ScratchTooSmall beats OutOfRange; null `bits` with nbits 1, 2, 64, 65 is an error and with nbits 0 is not; a longer directory is accepted; outputs untouched on every failure | hand-written | return codes |
| hand-checked vectors (65 bits with padding set, 64 bits, 3 bits in an all-ones word, empty) | hand-written | explicit numbers |

The maximum-size and random coverage is sampled, not exhaustive. Max-size buffers are reused (one 2 MiB vector and one directory, refilled) and freed. Correctness only, no timing.

## Mutation check

109 hand-written source mutants of `bitrank.cpp` (the popcount constants and shifts, the word count, the last-word mask and its condition, the null, TooLarge, ScratchTooSmall and OutOfRange conditions and order, the directory loop, the final total, the rank branch for `i % 64 == 0` and its mask, the select binary search bounds, midpoint and comparison, the skip loop, the position loop and `lo * 64`), each compiled against the prebuilt test object and run with `-a`.

Plain build: 71 kills by a failing assertion, 10 by a crash alone, 25 timeouts, 3 survivors. Sanitizer build (`-fsanitize=address,undefined -fno-sanitize-recover=all`): 67 kills by a failing assertion, 14 by a sanitizer report, a crash or a failing assertion discovered only when run alone (see below), 25 timeouts, 3 survivors. The per-mutant batch counts were taken before one test was added (null `bits` with nbits 1) to kill a mutant that had survived; that mutant is then counted as a crash kill in both builds (plain SIGSEGV; sanitizer: "load of null pointer of type 'const uint64_t'"). The other mutants were not re-run after that test was added, which can only add kills.

The 25 timeouts per build are NOT claimed as kills and were not investigated. Plain: 2 (pop pair shift 2), 4 (pop nibble shift), 5 (pop byte shift), 8 (pop final shift 55), 39 (build unmasked popcount), 43 (build dir[words] total+1), 45 (build dir[0] nonzero), 77 (sel range >), 79 (sel hi = words-1), 81 (sel lo = 1), 82 (sel loop > 0), 83 (sel loop > 2), 85 (sel mid = lo), 86 (sel mid = hi/2), 87 (sel < instead of <=), 88 (sel > instead of <=), 89 (sel lo = mid+1), 90 (sel hi = mid-1), 91 (sel hi = mid+1), 93 (sel word lo+1), 94 (sel skip k), 95 (sel skip off by one low), 96 (sel skip off by one high), 99 (sel skip clears top bit), 102 (sel pos shift 2). Sanitizer: the same list with 1 (pop pair no subtract) and 38 (build dir[w] after add) in place of 4 and 93. Mutants 43, 45 and 81 were also run alone: they still hang (the junit run recorded a termination signal inside a test), so they are timeouts here and not kills.

Crash-only kills, run individually. Plain: SIGSEGV ("Segmentation violation signal", no other message) for mutants 23 and 24 and 25 (null checks of `bits`, `dir` and the combined check), 26 (null `bits` with nbits 1), 46 (rank null output), 47 (rank `bits` null), 56 (rank range check removed), 60 (rank branch `r != 0` always taken), 69 (select null output) and 70 (select `dir` null). Sanitizer, by observed message: UBSan "load of null pointer of type 'const uint64_t'" (23, 25, 26), "store to null pointer of type 'uint32_t'" (24, 46, 69), AddressSanitizer heap-buffer-overflow (40 build loop `<= words`, 44 build writes `dir[words + 1]`, 55 rank range `nbits + 1`, 56, 57 rank `w = i / 63`, 60, 93 select reads word `lo + 1`), a SIGSEGV inside a failing test (70). Found to be ordinary failing assertions when run alone: 34 (build scratch `<=`), 47 and 62 (rank mask `r + 1`).

Survivors, 3 argued equivalent:
- 80, `hi = words + 1` in the select search: `dir[words]` is the total and `k < total`, so a probe at `mid == words` takes the `hi = mid` branch and the search ends at the same `lo` (the largest `lo` with `dir[lo] <= k`); every probed index is at most `words`, inside the `need` words. Same results, same reads. Equivalent.
- 84, the midpoint rounded up: with `hi - lo > 1`, `lo < mid < hi` still holds, so the invariant `dir[lo] <= k < dir[hi]` is kept and the loop ends with `hi == lo + 1`, where `lo` is unique because `dir` is non-decreasing. Equivalent.
- 92, select reading the unmasked `bits[lo]`: only the last word can have padding, and padding sits above every real position of that word. The k-th lowest set bit that select looks for exists among the masked bits (since `k < total`), so the lowest `skip` bits removed and the next lowest bit are the same with or without the padding. Equivalent.

Survivors NOT killed and NOT proved equivalent: none.

## Not run

Windows was not run locally. Directory contents, directory words past `need` and stale or corrupt directories are not asserted, as the header leaves them open. Exhaustive coverage stops at 14 bits (all vectors) and the listed word-boundary shapes; larger sizes, including the 2^24 maximum, are sampled. No timing or performance tests. The 25 mutant timeouts per build were not investigated.
