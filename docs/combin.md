# combin

`core/combin/combin.h` and `combin.cpp` (`lumora::combin`) give exact counts over `uint64_t`: `binom_u64(n, k)` (any `n` and `k`), `multichoose_u64(n, k)`, `factorial_u64(n)` (up to 20) and `catalan_u64(n)` (up to 36). A result is the exact value or the status Overflow; it is never wrapped or rounded. Overflow is decided on the exact value, even when intermediate products exceed `2^64`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `4285424`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers.

Integer only: no floating point, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state. The source includes `<cstdint>` only.

## Check order

For all four functions: a null `out` gives NullArgument (even when the value would overflow), then Overflow when the exact value does not fit. On failure `*out` is not written (the value is built in a local and stored last). `binom_u64(n, k)` with `k > n` is 0 and Ok.

## Method

Let `kk` be the smaller symmetric argument and `a` the other part, so that the result is `C(a + kk, kk) = prod_{i=1..kk} (a + i) / i`.
- binom: `kk = min(k, n - k)` (`k <= n`, so `n - k` does not wrap), `a = n - kk`.
- multichoose, `n >= 1`: with `m = n - 1`, `multichoose(n, k) = C(m + k, k) = C(m + k, m)`; `kk = min(k, m)` and `a = max(k, m)` (because `m + k - kk` is the larger one). The upper argument `m + k` can need 65 bits, but `a` always fits in 64 bits, and only the factor `a + i` can need a 65th bit. The factors follow the symmetry branch (`a` is the larger argument, not always `m`). `multichoose(0, 0) = 1`, `multichoose(0, k > 0) = 0`.
- The partial value `r(i) = C(a + i, i)` is an integer for every `i`, grows with `i` (`a >= 1`), and is at most the final value. So a partial that does not fit means the result does not fit, and every partial of a fitting result fits in `uint64_t`.
- Each step computes `r(i) = r(i-1) * (a + i) / i` exactly: the product `r(i-1) * (a + i) = i * r(i)` is formed in two words (`mul_wide`: four 32-bit partial products, no sum can exceed `2^64 - 1`), plus `r(i-1)` added to the high word when the factor needs a 65th bit; any carry out of the high word is Overflow, it is never wrapped. The division by the small `i` (at most 36) is exact two-limb schoolbook division (`div_small`).
- Bound argument for the high word: for a step whose result fits, the exact numerator is `i * r(i) <= i * (2^64 - 1) < i * 2^64`, so its high word is below `i`. A high word of `i` or more means `r(i) >= 2^64`: Overflow. This is the only test; it is applied after the (checked) carry.
- `kk > 33`: Overflow without a loop. For `34 <= kk <= (a + kk) / 2` the value is at least `C(68, 34) = 28453041475240576740 > 2^64 - 1` (`C(2j, j)` grows with `j`, and `C(N, kk)` grows with `N`).
- factorial: an ordinary loop for `n <= 20`; `n > 20` is Overflow (`21! > 2^64 - 1`).
- catalan: `C(i+1) = C(i) * (4i + 2) / (i + 2)` with the same two-word step (the binomial `C(2n, n)` overflows from `n = 34` although `C_36` fits, so it is not used). Each `C(i)` is an integer, increasing, and `C_37 > 2^64 - 1`; `n > 36` is Overflow without a loop.

## Fixed bounds

- binom, multichoose: at most 33 loop iterations; each iteration is one `mul_wide` (constant), one carry test and one `div_small` (two limb steps). `kk > 33` returns before the loop.
- factorial: at most 20 iterations. catalan: at most 36 iterations. No data-dependent loop, no recursion.

## Tests

`tests/combin/combin_tests.cpp`, 12 test cases. Expected values come from three labelled sources. (1) DIGESTS from `tools/combin_ref.py` (Python integers and `math.comb`; the script cross-checks Pascal's triangle up to row 80, binary-searched largest-fitting tables, the header values and the `kk > 70` shortcut; `--check` verifies that the block spliced into the test file is current); the tests re-run each scenario against the real functions and recompute the 64-bit digest and the counts. (2) Two ORACLES inside the test that share no code with the implementation: Pascal's triangle with a saturation flag (rows 0..141) for the small grids, and for every case an exact big-integer check (a vector-of-limbs integer; the numerator is the product of the factors, each an exact integer that may need 65 bits, the denominator is a factorial; Overflow exactly when `P > (2^64 - 1) * Q`, a success is checked as `value * Q == P`). Where `kk` is above 70 the oracle expects Overflow (argued above; checked directly for `kk <= 70`). (3) CONSTANTS for the header values and the hand rows.

Scenarios (counts computed by the reference and recounted by the tests). `S` is a list of 175 values: 0..40, 63..67, `2^32 - 1`, `2^32`, `2^32 + 1`, `2^63 - 1`, `2^63`, `2^63 + 1`, `2^64 - 1` .. `2^64 - 4`, and the tables: for `k = 2..33` the largest `n` with `C(n, k)` fitting and that `n + 1`; for `m = 1..33` the largest `k` with `C(m + k, m)` fitting and that `k + 1`, `k + 2`.
- binom, n 0..80 and k 0..n+2, EXHAUSTIVE: 3483 cases, each also against Pascal.
- binom, `S x S`, EXHAUSTIVE over the list (NOT over all `u64` pairs): 30625 cases.
- binom, `n` in `S` and `k = n - j` for `j = 0..40` (the symmetric side), EXHAUSTIVE over the list: 6355 cases.
- multichoose, n and k in 0..70, EXHAUSTIVE: 5041 cases, each also against Pascal.
- multichoose, `S x S` (both orders), EXHAUSTIVE over the list: 30625 cases.
- multichoose, `n` in `S` with `j = 0..40`, both `(n, j)` and `(j, n)`, EXHAUSTIVE over the list: 14268 cases.
- factorial `n` in 0..22, `2^31`, `2^32 - 1`: 25 cases. catalan `n` in 0..40, 1000, `2^31`, `2^32 - 1`: 44 cases.
- The `S x S` grids contain `k' = 1, 2, 3` with `n` and `k` near `2^64` in both orders, `n = 1` with huge `k`, huge `n` with `k = 0`, and the largest fitting values per `k` and per `m`. A 65-bit factor (`multichoose(2, 2^64 - 1)`, the factor `2^64`) is covered in the grid and in the hand rows.
- Hand rows (constants): the header values (C(67,33), C(68,34), C(2^64-1, 1), C(2^64-1, 2), Catalan 36 and 37, multichoose(0, 0)), `multichoose(2, 2^64 - 2) = 2^64 - 1` fits and `multichoose(2, 2^64 - 1) = 2^64` is Overflow, `multichoose(1, 2^64 - 1) = 1`, `C(2^32, 2)`, factorial 0, 20, 21, Catalan 0, 1, 5, the null matrix, and the untouched output after Overflow.
- Not asserted: anything the header leaves unspecified.

## Mutation testing

Real source mutants of `combin.cpp` (106), each compiled with the prebuilt test object and run on the FINAL test file, one at a time (one core), in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. Result, identical in both builds: 88 killed, 18 survived. No timeout occurred.

Crash-only kills (the failure is the crash, with no assertion expansion), each re-run alone: `binom: null check off`, `multi: null check off`, `fact: null off`, `cat: null off`. Plain build: SIGSEGV (null store). Sanitizer build: UBSan "store to null pointer". The mutated source is instrumented; the test object is not.

Survivors, each with a proof of equivalence in the stated sense (none is left as not proved). "In context" means for every input the public functions can reach, not as a stand-alone helper:
- `mul: lo or -> plus`: (p00 & kLow32) occupies bits 0..31 and (mid << 32) has bits 0..31 clear, so | and + agree. Equivalent.
- `mul: hi drops p01 high`: p01 >> 32 is nonzero only if a0 * b1 >= 2^32, which needs b1 >= 2, so fl >= 2^33. In a reachable step that means i = 1 (r = 1, so a0 = 1 and p01 = b1 < 2^32), or i = 2 (r = a + 1 >= 2^33 - 1 so a1 >= 1, hence the high word is at least p11 = a1 * b1 >= 2 = d, and both versions return false), or i >= 3, which is not reached because C(a + 2, 2) >= C(2^33 - 1, 2) > 2^64 returned false at i = 2. For fh = 1 see the carry mutants. Equivalent in context.
- `step: carry check <=`: when the factor needs a 65th bit (fh = 1, so a >= 2^64 - 33) the original returns false in every reachable call: for i = 1, r = 1 and the high word is exactly 1 = d; for i = 2, a = 2^64 - 2 is the only value that reaches it (a = 2^64 - 1 failed at i = 1), r = 2^64 - 1 and fl = 0, so the high word is r >= 2; for i >= 3 the partial C(a + 2, 2) already exceeded 2^64 and an earlier step returned false. The carry test h < w.hi never fires there (w.hi = 0, h = r), so removing, flipping or loosening it, or changing the multiplier by fh, still returns false. Equivalent in context.
- `step: carry check off`: when the factor needs a 65th bit (fh = 1, so a >= 2^64 - 33) the original returns false in every reachable call: for i = 1, r = 1 and the high word is exactly 1 = d; for i = 2, a = 2^64 - 2 is the only value that reaches it (a = 2^64 - 1 failed at i = 1), r = 2^64 - 1 and fl = 0, so the high word is r >= 2; for i >= 3 the partial C(a + 2, 2) already exceeded 2^64 and an earlier step returned false. The carry test h < w.hi never fires there (w.hi = 0, h = r), so removing, flipping or loosening it, or changing the multiplier by fh, still returns false. Equivalent in context.
- `step: carry check flipped`: when the factor needs a 65th bit (fh = 1, so a >= 2^64 - 33) the original returns false in every reachable call: for i = 1, r = 1 and the high word is exactly 1 = d; for i = 2, a = 2^64 - 2 is the only value that reaches it (a = 2^64 - 1 failed at i = 1), r = 2^64 - 1 and fl = 0, so the high word is r >= 2; for i >= 3 the partial C(a + 2, 2) already exceeded 2^64 and an earlier step returned false. The carry test h < w.hi never fires there (w.hi = 0, h = r), so removing, flipping or loosening it, or changing the multiplier by fh, still returns false. Equivalent in context.
- `step: mul by fh part`: when the factor needs a 65th bit (fh = 1, so a >= 2^64 - 33) the original returns false in every reachable call: for i = 1, r = 1 and the high word is exactly 1 = d; for i = 2, a = 2^64 - 2 is the only value that reaches it (a = 2^64 - 1 failed at i = 1), r = 2^64 - 1 and fl = 0, so the high word is r >= 2; for i >= 3 the partial C(a + 2, 2) already exceeded 2^64 and an earlier step returned false. The carry test h < w.hi never fires there (w.hi = 0, h = r), so removing, flipping or loosening it, or changing the multiplier by fh, still returns false. Equivalent in context.
- `choose: kMaxK 34`: for 34 <= kk <= kMaxK the loop runs and C(a + kk, kk) >= C(2 kk, kk) > 2^64 - 1, so a step returns false at or before i = kk (partials only grow); the result is Overflow, as with the early return. The loop bound stays a constant. Equivalent in result.
- `choose: kMaxK 35`: for 34 <= kk <= kMaxK the loop runs and C(a + kk, kk) >= C(2 kk, kk) > 2^64 - 1, so a step returns false at or before i = kk (partials only grow); the result is Overflow, as with the early return. The loop bound stays a constant. Equivalent in result.
- `choose: kMaxK 40`: for 34 <= kk <= kMaxK the loop runs and C(a + kk, kk) >= C(2 kk, kk) > 2^64 - 1, so a step returns false at or before i = kk (partials only grow); the result is Overflow, as with the early return. The loop bound stays a constant. Equivalent in result.
- `choose: carry <=`: fl = al + i with 1 <= i <= 33, so fl == al is impossible. Equivalent.
- `binom: kk <=`: the two arguments are equal in the tie case, so both branches give the same kk (and a). Equivalent.
- `multi: kk <=`: the two arguments are equal in the tie case, so both branches give the same kk (and a). Equivalent.
- `multi: other <=`: the two arguments are equal in the tie case, so both branches give the same kk (and a). Equivalent.
- `fact: i starts 1`: the extra factor is r *= 1. Equivalent.
- `cat: limit 37`: n = 37 enters the loop and the step at i = 36 (C_37 > 2^64 - 1) returns false: Overflow with the output untouched, the same result as the early return. Equivalent in result.
- `cat: step failure ignored`: for n <= 36 every step computes a fitting C(i + 1) (C_36 fits), so no step fails; the early return handles n > 36. Equivalent in context.
- `cat: loop starts i=1`: the skipped step i = 0 computes C_1 = C_0 * 2 / 2 = 1 = C_0, so the state is the same; for n = 0 the loop does not run in either version. Equivalent.
- `cat: loop cap kMax+1`: the loop breaks at i >= n and n <= 36 here, so i = 36 as a start is never reached with i < n. Equivalent.

## What was NOT run

Windows (CI only). No timing test. No exhaustive test over all `uint64_t` pairs (the grids are exhaustive over their named lists). The mutant test object is not sanitizer-instrumented. No 128-bit type or builtin is used in the source or the tests.
