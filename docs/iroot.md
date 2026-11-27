# iroot

`core/iroot/iroot.h` and `iroot.cpp` (`lumora::iroot`) give exact integer roots and powers over `uint64_t`: `iroot_u64(x, k, out)` (floor of the k-th root, k in 1..64), `perfect_power_u64(x, out_base, out_exp)` (the largest exponent with `x = base^exp`) and `ipow_u64(base, exp, out)` (exact power, Overflow above `2^64 - 1`). Results are exact; nothing is rounded, wrapped or estimated.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `244f467`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers.

Integer only: no floating point, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state.

## Check order

- `iroot_u64`: null `out` gives NullArgument, then `k == 0` or `k > 64` gives BadExponent, then the value. On failure `*out` is not written.
- `perfect_power_u64`: either output null gives NullArgument (nothing is written). There is no other failure. `x < 2` gives base `x`, exponent 1; so does every `x` that is not a perfect power.
- `ipow_u64`: null `out` gives NullArgument, then Overflow when the exact value is above `2^64 - 1`. `0^0 = 1`. On Overflow `*out` is not written.

## Method

- `pow_le(r, k, limit)`: builds `r^k` by repeated multiplication, after testing `p > limit / r` before each product `p * r`. For `r >= 1` the test is exact (`p * r > limit` iff `p > floor(limit / r)`), so no product wraps.
- Root, `x >= 2`, `2 <= k <= 63`: binary search for the largest `r` with `r^k <= x` over `1 .. 2^ceil(bits / k) - 1`, where `x < 2^bits`. The root is below `2^ceil(bits / k)` because `r >= 2^ceil(bits / k)` gives `r^k >= 2^bits > x`. The upper end is at most `2^32 - 1` since `k >= 2`. The step uses `mid = lo + (hi - lo + 1) / 2` and keeps `lo^k <= x`. `x < 2` and `k == 1` give `x`; `k == 64` gives 1 (`2^64` is above every `u64`).
- perfect_power, `x >= 2`: exponents from 63 down to 2; the first exact power found has the largest exponent, hence the smallest base. An exponent with `x < 2^ee` is skipped (a base of at least 2 would exceed `x`). For each other exponent: `r = root(x, ee)`; the answer is `(r, ee)` when `r >= 2` and `r^ee == x`.
- ipow: `exp == 0` gives 1, base 0 gives 0, base 1 gives 1; otherwise `exp > 63` is Overflow (`2^64` is already above `2^64 - 1`) and the loop multiplies, testing `r > (2^64 - 1) / base` before each product.

## Early exits (why they are exact)

- Power: all factors are at least 2 (the base 0 and 1 cases return before the loop), so the partial product only grows. A partial product above the limit can never come back below it, so Overflow (or "above x") is exact, and every partial of a fitting result fits.
- Root: for a candidate `mid`, `mid^k > x` means the candidate is too large, and every larger candidate is too large too (the power is increasing); so `hi = mid - 1` loses nothing. In `pow_le` for `r = 1` the product stays 1 and never exits.
- perfect_power: `r = 1` never matches, because `1 != x` for `x >= 2`; skipped exponents cannot hold a base of at least 2.

## Fixed bounds

- ipow: at most 63 multiplications.
- root: at most 32 search steps, each at most 63 multiplications (the range has at most `2^32 - 1` values, so 32 halvings finish it).
- perfect_power: at most 62 exponent steps (63 down to 2), each one root.
- No data-dependent unbounded loop, no recursion.

## Tests

`tests/iroot/iroot_tests.cpp`, 17 test cases. Expected values come from three labelled sources. (1) DIGESTS from `tools/iroot_ref.py` (Python integers; roots by a float guess with exact correction, perfect powers by an exact power test, cross-checked by prime factorization for `x <= 20000`; `--check` verifies that the block spliced into the test file is current); the tests re-run each scenario against the real functions and recompute the 64-bit digest and the counts. (2) An ORACLE inside the test that shares no code with the implementation (a naive linear root search and big-integer style power checks; perfect_power maximality: no larger exponent works, checked on every 5th pp_pow case, `b^e == x` on all). (3) CONSTANTS for the header values and hand rows.

Every grid below is labelled with its exact extent. Squares and cubes are NOT exhaustive over all bases: "exhaustive only over the stated subsets, not all square and cube bases" (the test names say so too). Where grids overlap, one grid is enough.

- Lists. SQ (k = 2): bases 1..70000 (includes the windows around 65535 and 65536), `2^31 - 2000 .. 2^31 + 2000`, `2^32 - 2000 .. 2^32 - 1`: 76001 bases. CU (k = 3): 1..70000, `2642245 +- 2000`, `2^20 +- 2000`, `2^21 +- 2000` (so `2^21 - 1` and `2^21` are in): 82003 bases.
- iroot, x 0..5000, every k 1..64: EXHAUSTIVE, 320064 cases.
- iroot, k 4..64: EVERY base with `b^k` fitting, x = `b^k - 1`, `b^k`, `b^k + 1`: EXHAUSTIVE, 227073 cases. This includes the largest fitting base for every k from 2 to 64 and the next base (tables and hand rows).
- iroot k = 2, bases in SQ, three x each: 228003 cases (exhaustive only over SQ). k = 3, bases in CU: 240009 cases (exhaustive only over CU).
- iroot, fixed-seed sample: 20000 bases for k = 2 (range 1..`2^32 - 1`) and 20000 for k = 3 (range 1..2642245), x = `b^k + d`, d in -2..2 where it fits: 200000 cases (SAMPLED, not exhaustive).
- iroot edges: k 1..64 with `x = 2^64 - 1 - j` (j 0..199), `2^63 + j` (j -100..100), `2^p - 1`, `2^p`, `2^p + 1` (p 0..63): 37952 cases.
- perfect_power, every x 0..200000: EXHAUSTIVE, 200001 cases.
- perfect_power, `b^e` and neighbours (+-1): e 4..63 EVERY fitting base; e = 2 and e = 3 only a subset of SQ and CU (bases up to 1000, every 16th list entry, and bases within 100 of 65535, 65536, `2^20`, `2^21`, 2642245, `2^31`, `2^32`). This covers every b 2..300 with every fitting e. REDUCED on purpose, for the run time of the mutant runs: the full SQ and CU lists are used for the root, not here. 265149 cases (exhaustive only over the stated subsets).
- perfect_power, products of distinct prime powers: `p^a q^b` (first 12 primes) and `p^a q^b r^c` (first 6 primes, exponents 1..8), where they fit: 25215 cases. Large rows (`3^40`, `2^63`, `2^64 - 1`, squares of the 10 primes below `2^32`, each +-1): 41 cases.
- ipow: bases 0..300 with exponents 0..70: 21371 cases. Largest fitting base per exponent with its neighbours: 193. Bases 0, 1, 2, 3 with exponents 0..200 plus a few large bases: 828. Large bases with exponents 0..4, 63, 64, `2^32 - 1`: 48.
- Hand rows: the header values, the null and BadExponent matrix, outputs untouched on failure.
- Not asserted: anything the header leaves unspecified.

## Mutation testing

Real source mutants of `iroot.cpp` (105), each compiled with the prebuilt test object and run on the FINAL test file, one at a time (one core), in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. Plain: 88 killed, 17 survived. Sanitizer: 90 killed, 15 survived. No timeout occurred. The two extra sanitizer kills (`pp: ee stops at 1`, `const: kMaxExp 64`) are caught only by UBSan (a shift by 64), and in the plain build they survive as shown below.

Crash-only kills (the failure is the crash, with no assertion), each re-run alone. Plain: `iroot: null check dropped` (SIGSEGV), `iroot: k==0 allowed` (SIGFPE, division by zero), `pp: null both -> only base`, `pp: null both -> only exp`, `pp: null && instead of ||`, `pow: null check dropped` (all SIGSEGV). Sanitizer, same list plus: `iroot: k==1 dropped`, `pp: ee stops at 1`, `pp: skip x>>(ee+1)`, `const: kMaxExp 64` (UBSan: shift exponent 64 too large), `pow: base 0 dropped`, `pow: overflow wrong divisor` (UBSan: division by zero); the null-store mutants report "store to null pointer", `k==0 allowed` reports "division by zero". The mutated source is instrumented; the test object is not.

Survivors (17 plain, 15 sanitizer), each equivalent, with the proof:
- `root: hi = 2^sh`, `root: lo starts 0`: the search only needs an upper bound at least the root and at most `2^32` values; `2^sh` is one still valid bound, and `lo = 0` never tests `mid = 0` (mid is at least 1 while `hi >= 1`) and ends at the root, since the root is at least 1 for `x >= 2`. A range of at most `2^32` values is finished by 32 halvings.
- `root: 33 iterations`, `root: lo > hi break`, `root: no lo>=hi break`: after 32 steps the range is one value; extra steps with `lo == hi` give `mid = lo`, which passes, and change nothing. `lo > hi` cannot occur (`hi = mid - 1` only when `mid > lo`).
- `root: returns hi`: after the 32 steps `lo == hi`.
- `root: hi = mid` (instead of `mid - 1`): `hi` is then either the initial untested bound or a `mid` that failed. If a step ever failed, `hi` is a failing value, the root is below `hi`, and a range of 2 left at the end is `[lo, lo + 1]` with `lo + 1 = hi` failing, so `lo` is right; sizes are at most `floor(n / 2) + 1`, so the range is at most 2 after 32 steps. If no step failed, every step halved with `ceil`, and the range is 1.
- `iroot: x<2 to x<1`: `x = 1` goes to the search with `lo = hi = 1` and gives 1.
- `iroot: k==64 dropped`: for `k = 64`, `bits <= 64` gives `sh = 1`, `hi = 1`, result 1.
- `pp: x>=2 to x>=3`, `pp: x>=2 to x>=1`: for `x = 2` every exponent from 2 up is skipped (`2 >> 2 == 0`), and for `x = 1` too; the default `(x, 1)` is the answer.
- `pp: ee stops at 1` (plain only): `ee = 1` finds `r = x`, `p == x`, which is the same `(x, 1)`. In the sanitizer build, `x >= 2^63` shifts by 64 and is killed.
- `pp: skip dropped`: for `x < 2^ee` the root is 1, and `r >= 2` fails. `pp: skip x>>(ee-1)`: it skips only `x < 2^(ee-1) < 2^ee`, where no base of at least 2 exists.
- `pp: r>=2 to r>=1`, `pp: r>=2 dropped`: `r >= 1` always; `r = 1` gives `p = 1 != x` for `x >= 2`.
- `const: kMaxExp 64` (plain only): `pow_le` never runs for `k = 64`; ipow with `exp = 64` overflows in the loop anyway (the 64th product); `perfect_power` with `ee = 64` finds root 1. The shift by 64 is caught by UBSan.

## Not run

- Windows: CI-only.
- No timing test.
- No exhaustive run over all square and cube bases, and no exhaustive run over all `u64`.
- The mutant test object is not sanitizer-instrumented; only the mutated source is.
