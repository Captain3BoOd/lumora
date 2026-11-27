# lyndon

`core/lyndon/lyndon.h` and `lyndon.cpp` (`lumora::lyndon`) give two answers about a byte string `s[0 .. n)`: the lengths of its Lyndon factors in order (`lyndon_factor`) and the smallest start of its least rotation (`lyndon_min_rotation`). Limit: at most `262144` bytes.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `d05ae58`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers. The header "Decisions" block was treated as data.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no large automatic arrays. The source includes `lyndon.h` only. The library needs no memory beyond the caller's `lens` array; the 1 MiB `lens` array for `n = 262144` lives in the tests.

## Check order and writes

- `lyndon_factor`: NullArgument for a null `s` or `lens` only while `n > 0`, and for a null `out_count` always (the condition uses the given `n`, also above the limit); then `n > 262144` is TooLarge. On failure nothing is written. On success only `lens[0 .. k)` and `*out_count` are written; the entries at `k .. n - 1` are checked unchanged. `n = 0` is Ok, `*out_count = 0`, nothing is written to `lens`, and `s` and `lens` may be null.
- `lyndon_min_rotation`: NullArgument for a null `s` only while `n > 0` and for a null `out_start` always; then TooLarge. On failure `*out_start` is untouched. `n = 0` gives 0.
- Bytes are compared as unsigned values. A Lyndon word is strictly smaller than each proper suffix, so `aa` is the two factors `a`, `a` and `abab` is `ab`, `ab`. For a periodic string the smallest index among equal least rotations is returned (`abab` gives 0, `baba` gives 1). Overlapping arrays are not specified and not tested.

## Method

Duval's algorithm. A round starts at `i`, scans `j` forward with a comparison index `k` (start `j = i + 1`, `k = i`): while `s[k] <= s[j]`, `k` becomes `i` if `s[k] < s[j]` and `k + 1` if they are equal, and `j` moves on. The round stops at `j = n` or at the first `s[k] > s[j]`. The repeated Lyndon word has length `len = j - k`; it is emitted once per copy while `i <= k` (`i` moves by `len` each time). The next round starts at the new `i`, which is at most `len - 1` before `j`.

`lyndon_min_rotation` runs the same rounds over the string followed by itself (index `x >= n` reads `s[x - n]`, so no copy exists), with the scan limit `2 * n`, and returns the start `i` of the last round that begins below `n`. Why that is the least rotation: the Lyndon factorization of `ss` starts at positions that are minimal among the suffixes before them, so the last round start below `n` is the start of the least rotation, and because `i` only visits round starts (a run of equal factors is consumed in one round) it is the first, that is the smallest, index when the string is periodic.

## Bounds

Count the evaluations of the scan test `s[k] <= s[j]` (the second compare `s[k] < s[j]` runs only after a successful test, so it adds at most the same number again).
- Per round starting at `i` with `c` = the amount `i` advances (`c >= len`): the successful tests are `j - i - 1`, there is at most one failing test, so at most `j - i` tests. Since `j - i = c + (j - i_new)` and `j - i_new < len <= c`, a round makes at most `2 * c - 1` tests.
- `lyndon_factor`: the `c` of all rounds sum to `n`, so at most `2 * n - rounds <= 2 * n - 1` tests, and at most `n` writes to `lens`. Every other step is a constant number of integer operations per round or per test.
- `lyndon_min_rotation`: the `c` of all rounds sum to the final `i`, which is at most the last `j <= 2 * n`, so at most `4 * n - rounds <= 4 * n - 1` tests.
- Observation, not a claim: exhaustively over all binary strings up to length 14 and ternary strings up to length 9 (a Python model counting the same tests) the maxima were far below these bounds: at most 21 tests for factoring at `n = 14` and 39 for the rotation at `n = 14`, and the rotation maximum grows as `3 * n - 3` on `0 ... 0 1 0`.
- No loop counter wraps in 32 bits: `i`, `j`, `k <= 2 * n = 524288`.

### One-time scaling table (observation, not asserted)

Best of 3 runs, one thread, seconds, g++ `-std=c++23`, `-O0` / `-O2`. Machine class: Intel Xeon Platinum 8375C at 2.90 GHz, 1 vCPU visible, Linux 7.3.4. It was run once outside the test suite; no timing test exists. Doubling `n` roughly doubles the time on every pattern. Each cell is `lyndon_factor` then `lyndon_min_rotation`.

| pattern | 32768 | 65536 | 131072 | 262144 |
| --- | --- | --- | --- | --- |
| repeated byte | 0.0002 / 0.0008 and 0.0001 / 0.0002 | 0.0003 / 0.0015 and 0.0002 / 0.0004 | 0.0006 / 0.0030 and 0.0003 / 0.0009 | 0.0012 / 0.0060 and 0.0006 / 0.0017 |
| alternating | 0.0001 / 0.0007 and 0.0001 / 0.0002 | 0.0002 / 0.0014 and 0.0001 / 0.0004 | 0.0005 / 0.0028 and 0.0003 / 0.0008 | 0.0009 / 0.0056 and 0.0006 / 0.0016 |
| Fibonacci | 0.0001 / 0.0008 and 0.0001 / 0.0003 | 0.0003 / 0.0017 and 0.0002 / 0.0005 | 0.0005 / 0.0031 and 0.0004 / 0.0010 | 0.0010 / 0.0064 and 0.0008 / 0.0020 |
| random bytes | 0.0001 / 0.0006 and 0.0001 / 0.0002 | 0.0002 / 0.0012 and 0.0001 / 0.0004 | 0.0003 / 0.0025 and 0.0003 / 0.0008 | 0.0007 / 0.0050 and 0.0005 / 0.0016 |

Read each cell as `-O0 factor / -O0 rotation and -O2 factor / -O2 rotation`.

## Tests

`tests/lyndon/lyndon_tests.cpp`, 9 test cases, 390 assertions. Expected values come from three labelled sources. (1) DIGESTS from `tools/lyndon_ref.py` (independent Python: the factorization by the definition, every rotation against every other, a verifier for the lengths, and a two-pointer rotation with the failure function; its self-check compares them on every binary string up to length 12, every ternary string up to 8 and 3000 random strings; `--check` verifies the block spliced into the test file is current). The tests rerun every string against the real functions and recompute the 64-bit digests (`n`, `k`, the `k` lengths, the least rotation) and the case counts. (2) An ORACLE inside the test sharing no code with the implementation: a verifier that the returned lengths are positive, sum to `n`, every factor is a Lyndon word (two-pointer minimal rotation at 0 and primitive by the failure function) and neighbours do not increase (a non-increasing run of Lyndon words is the factorization); the same two-pointer rotation reduced modulo the smallest period; and for strings up to 24 bytes the definitions (the longest Lyndon prefix repeatedly, every rotation against every other). (3) CONSTANTS written from the header: 24 hand rows, closed forms at the limit, the check-order matrix.

Every successful call runs with sentinel entries before and after `lens`; the entries at `k .. n - 1` must stay unchanged and on failure nothing may change. Extents, with their labels:
- EXHAUSTIVE: every string of length 0 to 16 over `{0, 255}` (131071 strings); length 0 to 10 over `{0, 128, 255}` (88573); length 0 to 6 over `{0, 1, 127, 128, 254, 255}` (55987); the empty string, all 256 one-byte strings and all 65536 two-byte strings (65793). Bytes 0 and 255 are adjacent in the first group and in the hand rows.
- SAMPLED, fixed seeds 3002, 3003 and 3256: 600 random strings each of length 17 to 400 over 2, 3 and 256 symbols (1800 strings), checked against the oracle (definitions only up to 24 bytes).
- Listed large patterns (27 strings, SAMPLED patterns, checked by the verifier and the digests): one repeated byte (`0` at 262144, 262143, 262142 and 32768, `255` at 262144), alternating `0/255` (262144, 262143, 262142), alternating `a/b`, period 3 (262144, 262143, 262142), Fibonacci (262144, 262143, 262142), Fibonacci rotated by 100003, `0 ... 0 1` (one Lyndon word), `1 0 ... 0`, descending bytes `255 .. 0` repeated, `1 0 ... 0 2`, a random 512-byte block repeated (plain and rotated by 300, so the least rotation is periodic and not at 0), random over 2 symbols (262144, 262143), random over 256 symbols (262144, 262143, 1000).
- Closed forms at `262144` and `262143`: one repeated byte gives `n` factors of length 1 and rotation 0; `0 ... 0 1` gives one factor of length `n`; `1 0 ... 0` gives `n` factors and rotation 1.
- Hand rows (24): periodic strings (`abab`, `baba`, `aabaab`), equal neighbours, descending runs, runs of 0 and 255, ties for the rotation. The expected values were taken from the definition (Python brute force), not from the C++ code.
- Check-order matrix: null arguments, `n = 0` with null `s` and `lens`, TooLarge (`kMaxLen + 1` and `0xFFFFFFFF`), null before TooLarge, nothing written on failure.
- Not asserted: anything the header leaves unspecified (overlapping arrays, which error comes first inside one class, running time).

## Mutation testing

Real source mutants of `lyndon.cpp` (85), each compiled with the prebuilt test object and run on the FINAL test file, one at a time, in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. Plain: 74 killed, 4 survived, 7 timed out. ASan/UBSan: 75 killed, 3 survived, 7 timed out. The two builds differ in one mutant (`scan j <= 2n`), explained below.

Timeouts (30 s each) are NOT kills. Six hang (the loop never ends, because `len` becomes 0 or `i` stops advancing, or the scan state is never reset to a valid value): `factor: reset to 0`, `factor: emit loop i < k`, `rot: j starts at i`, `rot: k starts at i + 1`, `rot: len is j - k - 1`, `rot: advance loop i < k`. They hang on the small tests too (observed once with the large cases excluded). The seventh, `factor: emit once (if instead of while)`, gives the right output (it re-scans the rest of a run of equal factors from the next round), so it passes every small test and every digest; it is quadratic on a repeated byte and times out only on the large strings. The suite asserts no timing by maintainer decision, so a timeout is the only evidence; none of the 7 is claimed as a kill.

Crash-only kills (the run shows no failing assertion), 17 mutants in the plain run and 24 in the ASan/UBSan run, each re-run alone. Plain: 11 end in SIGSEGV, 1 in SIGABRT with no message, and 4 in failing assertions when run alone (TooLarge checks dropped or widened, scan bound dropped); the crash message varies between runs and the crash is the kill. ASan/UBSan: 16 AddressSanitizer reports (heap-buffer-overflow or similar) and 8 UBSan stores through a null pointer (dropped null checks).

Survivors, with the argument:
- `rot: outer loop i < n to i <= n` (survives in both builds), equivalent in output. A round can start at `i = n` only if `n` is a factor start of the factorization of `ss`. Then the factorization of `ss` is `f(s)` followed by `f(s)` (the factors up to a boundary are the factorization of that prefix), and the factors `w1 >= ... >= wk` of `s` with `wk >= w1` (the factorization of `ss` does not increase across the boundary) are all equal, so `s = w^k` and `ss` is one run of `2k` equal factors that the round starting at 0 consumes completely, so `i` jumps from 0 to `2 * n` and never equals `n`.
- `rot: scan j < 2n to j < 2n - 1` (both builds), equivalent in output. The answer is set at the start of a round, so only the start of the next round matters. A scan that reaches `j = 2n - 1` ends with `i_new > j - len`; if `len >= n` then `i_new >= i + len >= n`; otherwise `i_new > j - len >= 2n - 1 - (n - 1) = n`, hence `i_new >= n` and the loop ends in both versions with the same last round start.
- `rot: scan j < 2n to j <= 2n`: plain survives, ASan/UBSan kills. The scan then reads `at(2n) = s[n]`, one byte past the string whenever a whole periodic string is scanned. That is undefined behaviour that a plain build does not see; the sanitizer build does. It survives only in the plain build, as an out-of-bounds read of one byte with the same result otherwise.
- `rot: advance by 1` (`i += 1` instead of `i += len` in the advance loop), survives in both builds and is NOT proven equivalent. After a round `i` becomes `k + 1` instead of the end of the last whole copy, so the next round may start inside the last copy of the repeated word, which in theory could add a later round start below `n`. The exhaustive and sampled extents did not find a difference. A Python model of this mutant (not part of the suite) found none over all binary strings up to length 20, ternary up to 11, four-letter up to 8, and 1.2 million random and near-periodic strings up to length 70. This is an honest limitation: no counter-example was found and no equivalence proof was completed; it is not claimed as a kill or as proven equivalent.

## Not run

- Windows: CI-only.
- No timing assertion in the suite (maintainer decision); the scaling table is a one-time observation.
- No exhaustive run beyond the alphabets and lengths listed above; longer strings are fixed-seed samples and listed patterns.
- Overlapping arrays are not tested (unspecified).
- The mutant test object is not sanitizer-instrumented; only the mutated source is.
- The 7 timed-out mutants were not killed; mutant `rot: advance by 1` is an unproven survivor.
