# modarith

`core/modarith/modarith.h` and `modarith.cpp` (`lumora::modarith`) are exact modular arithmetic on 64-bit values for every modulus up to 2^64 - 1: `mulmod_u64`, `powmod_u64`, `modinv_u64` and a deterministic `is_prime_u64`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state. The source includes `<cstdint>` only. The tests use no 128-bit type or builtin either.

## Check order

For every function: a null `out` gives NullArgument; then a modulus of 0 gives ZeroModulus (`is_prime_u64` has no modulus); then the function-specific failure (NotInvertible for `modinv_u64`). `*out` is untouched on every failure. `a`, `b` and `base` may be any 64-bit value and are used as their residues modulo `m`.

## Algorithms

- `mulmod_u64`: the residues `a % m` and `b % m` are formed first. For `m <= 0xFFFFFFFF` the product of two residues is below 2^64, so `a * b % m` is one plain multiplication. Otherwise a 64-step double-and-add over the bits of `b` from the top: `r = 2r mod m`, then `r = r + a mod m` for a set bit, where every partial value stays below `m` and `x + y mod m` is formed as `x >= m - y ? x - (m - y) : x + y`, so a sum above 2^64 - 1 is never made. Both paths are exact.
- `powmod_u64`: square and multiply over the bits of `exp` from the lowest, at most 64 squarings and 64 multiplications through `mulmod`, so the time is proportional to the bit length of `exp`. `exp == 0` gives `1 % m` (0 when `m == 1`, 1 for `0 ^ 0` when `m > 1`).
- `modinv_u64`: `m == 1` gives `*out = 0` and Ok (every residue is 0). Otherwise the extended Euclidean algorithm on `(m, a mod m)` keeps the coefficients as residues in `[0, m)` (`t = t0 - q * t1` is formed as `t0 - qt` or `t0 + (m - qt)` with `qt = q * t1 mod m` through `mulmod`), so no signed value and no overflow appear for any `m` up to 2^64 - 1. A final remainder other than 1 (so `a` a multiple of `m`, including 0, for `m > 1`) is NotInvertible. The result is in `[0, m)`.
- `is_prime_u64`: 0 and 1 are not prime. Trial division by the 12 primes 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 (equal to one of them: prime; divisible: composite); below 41 * 41 = 1681 what is left is prime. Otherwise `n - 1 = d * 2^s` with `d` odd and a strong probable prime test (Miller-Rabin) to each of those 12 bases, all multiplications through `mulmod`.

## Why the primality answer is exact for every 64-bit n

The 12 prime bases 2 to 37 are enough because no composite below 2^64 is a strong pseudoprime to all of them: the smallest odd composite that is a strong pseudoprime to the first 12 prime bases is 318665857834031151167461, which is above 2^64 (OEIS A014233, a(12); it has 24 digits; the same sequence gives a(11) = 3825123056546413051, below 2^64, a strong pseudoprime to every prime base up to 31, which is why base 37 is needed). The result is from J. Sorenson and J. Webster, "Strong pseudoprimes to twelve prime bases", Mathematics of Computation 86 (2017), 985-1003, DOI 10.1090/mcom/3134, https://www.ams.org/mcom/2017-86-304/S0025-5718-2016-03134-8/ (also arXiv 1509.00864, https://arxiv.org/abs/1509.00864); the OEIS entry is https://oeis.org/A014233. We read the OEIS entry and the abstract and the references list of the paper page; we did not re-prove the result or check it independently.

So exactness for ALL 64-bit values rests on that published result. The tests in this repository validate it only on the sets listed below; they do not prove it, and everything from 2^32 up is NOT exhaustive.

## Bounds

| Item | Bound |
| --- | --- |
| `a`, `b`, `base`, `exp`, `m`, `n` | any 64-bit value (`m` above 0) |
| `mulmod` | one multiplication (`m <= 0xFFFFFFFF`) or 64 double-and-add steps |
| `powmod` | at most 64 squarings and 64 multiplications through `mulmod` |
| `modinv` | at most about 93 Euclid steps for 64-bit `m` (each step one `mulmod`) |
| `is_prime` | at most 12 trial divisions, then at most 12 bases x 64 squarings |

## Tests

`tools/modarith_ref.py` is an independent reference in Python with Python integers: `(a * b) % m`, `pow(a, e, m)`, a gcd test then `pow(a, -1, m)`, and its own Miller-Rabin with trial division by the primes below 100 and the 7 bases 2, 325, 9375, 28178, 450775, 9780504, 1795265022 (a DIFFERENT base set from the C++ one). Every primality value the script evaluates is cross-checked, each time it runs, against a second Miller-Rabin with the 12 prime bases; the values below 2^20 are checked against a sieve and the window near 2^32 against trial division. It prints a block of digests, counts and known-answer tables that the tests recompute:

```
python3 tools/modarith_ref.py                                              # print the block
python3 tools/modarith_ref.py --check tests/modarith/modarith_tests.cpp
```

The tests carry a naive oracle that shares no code with the implementation and uses plain 64-bit arithmetic only: mulmod for moduli below 2^16 by one multiplication, powmod by repeated multiplication, modinv by trying every residue, and primality by a sieve of smallest prime factors.

`tests/modarith/modarith_tests.cpp` has 11 test cases and 3,432 assertions (sweeps count mismatches and assert once per sweep, plus the case count, the operation count and the digest; the primality helper asserts once per call). Every call presets `*out` to a sentinel and a failure must leave it untouched.

| Scenario | Cases | Against |
| --- | --- | --- |
| boundary grid: 27 values (0, 1, 2, 3, 4, 5, 7, 10, 10^9 + 7, 2^16 +- 1 and 2^16, 2^31 +- 1 and 2^31, 2^32 +- 1 and 2^32, 4294967291, 4294967311, 2^63 +- 1 and 2^63, 2^64 - 1, 2^64 - 2, 2^64 - 3, 18446744073709551557): mulmod and powmod over every (a, b, m), modinv over every (a, m); every modinv result is also checked by `a * x mod m == 1` and `x < m` | 40,095 | reference digest |
| small: mulmod, powmod, modinv for a, b, exp in 0..15 and m in 0..12 (m = 0 included) | 6,864 | oracle, reference digest |
| oracle sweep: m in 1..64, a and b in 0..m + 3, exponents 0..40 (not hashed) | 205,216 | oracle |
| 400 pseudo-random seeds (NOT exhaustive): four 64-bit values of random bit length, mulmod, powmod, modinv | 1,200 | reference digest |
| primality, every n below 2^20 | 1,048,576 | sieve, reference digest of the primes |
| primality, every n in [2^32 - 512, 2^32 + 512), [2^63 - 512, 2^63 + 512), [2^64 - 1024, 2^64 - 1] (NOT exhaustive beyond these windows) | 3,072 | reference digest (two Miller-Rabin base sets, trial division near 2^32) |
| primality table (NOT exhaustive): Mersenne numbers 2^k - 1 for k = 2 to 63, Fermat numbers 2^(2^k) + 1 for k = 0 to 5, squares and products of primes near 2^32, 2^64 - 1, the 12 largest primes below 2^64 (including 18446744073709551557), other large primes, Carmichael numbers, and the odd composites of OEIS A014233 below 2^64 (the smallest strong pseudoprimes to the first prime bases) | 108 (31 prime, 77 composite) | Python-generated expectations |
| primality, every odd composite below 2^22 that is a strong pseudoprime to base 2 (104 of them) and every Carmichael number below 2^22 (69), found by a sieve and Korselt's criterion in both languages | 173 | digest of both lists, `is_prime` is false for each |

The first three and the fifth rows are exhaustive over their stated sets. The pseudo-random row, the 2^32 and up windows, and the table are NOT exhaustive. Known-answer tables (17 mulmod, 16 powmod and 21 modinv vectors with answers from Python integers) cover products of two values near 2^64, a modulus above 2^63, a modulus of 2^32 and 2^32 + 1, `exp` of 2^64 - 1, `0 ^ 0`, m equal to 1 and 0, and non-invertible inputs. Worst paths (every base tried) are checked for correctness only, with no timing assertion: 18446744073709551557 (prime), 2^64 - 1, 3825123056546413051 (composite, only base 37 rejects it) and 341550071728321.

## Mutation check

103 hand-written source mutants of `modarith.cpp` (the add-with-carry test, the residue reduction, the fast-path bound, the double-and-add loop range, shift and bit test, the powmod initial value, reduction, bit test, shift and squaring, the strong test's early exits and loop range, every null and zero-modulus rule and their order, the modinv initial values, remainder, coefficient step, gcd test and output, and the primality small cases, base list, trial loop, small bound, `d` and `s`, and the base loop). Each replaces one unique text and is compiled against the prebuilt test object, then the whole binary runs with a 40 second timeout.

Plain build: 77 kills by a failing assertion, 7 by a crash alone, 2 timeouts, 17 survivors. Sanitizer build (`-fsanitize=address,undefined -fno-sanitize-recover=all`): 74 kills by a failing assertion, 10 by a sanitizer report or crash, 2 timeouts, the same 17 survivors.

The 2 timeouts are NOT claimed as kills and were not investigated: mutant 29 (powmod without the shift of `exp`) and mutant 64 (a wrong modinv remainder `r0 - r1`).

The crash-only kills were run individually. Plain: SIGSEGV ("Segmentation violation signal") for the four null-pointer mutants (46 to 49), and SIGFPE (return code -8, no message printed) for the three zero-modulus mutants (50 to 52). Sanitizer: UBSan "store to null pointer" (46 to 49), UBSan "division by zero" (50 to 52) and UBSan "shift exponent ... too large for 64-bit type" (16, 17, 18).

Survivors, 13 argued equivalent (a wrapping `uint64_t` computation is exact modulo 2^64):
- 4, `x - m + y` for `x - (m - y)`: the same value modulo 2^64. Equivalent.
- 69, `t0 - qt + m` for `t0 + (m - qt)`: the same value modulo 2^64. Equivalent.
- 9, `m < 0xFFFFFFFF` for `<=`, and 12, a fast-path bound that never holds: both send more moduli to the 64-step loop, which is exact for every `m > 0`, so the result is the same. Equivalent.
- 26, powmod without `base %= m`: the first use of `base` is `mul_mod(result, base, m)` or `mul_mod(base, base, m)`, and `mul_mod` reduces both arguments first. Equivalent.
- 30, powmod squaring `base` once more after the last bit: `base` is not used again. Equivalent.
- 57, the `m == 1` block removed: for `m == 1` the Euclid loop starts with `r0 = 1`, `r1 = a % 1 = 0`, skips the loop, `r0 == 1` and `*out = t0 = 0`, the same Ok and 0. Equivalent.
- 59, modinv without `a % m`: the first step has `q = m / a = 0`, `r2 = m`, `qt = 0`, which only swaps `(r0, t0)` and `(r1, t1)` into `(a, 1)` and `(m, 0)`; the next step has `r2 = a % m` and `t2 = 1`, the state the original starts the second step with. If `a % m == 0` the remainder is 0 and `r0` is `m` or `a`, not 1, so NotInvertible as before. Equivalent.
- 63, `r0 % r1` for `r0 - (r0 / r1) * r1`: the same value. Equivalent.
- 67, `t0 > qt` for `t0 >= qt`: the only difference is `t0 == qt`, where the mutant makes `m` instead of 0 (the same residue). A coefficient equal to `m` is only ever reduced by `mul_mod`, compared with `>` against a `qt` below `m`, or subtracted (`m - qt` is the correct residue of `0 - qt`), so every nonzero residue is formed canonically and the final coefficient is not a multiple of `m` when `r0 == 1` (a multiple of `m` times `a` is 0, not 1). Equivalent.
- 73, `r0 > 1u` for `r0 != 1u`: `r0` is the gcd, at least 1 (`m` itself when the loop is skipped). Equivalent.
- 91 and 92, the small bound lowered to 37 * 37 and removed: every n from 38 to 1680 with no prime factor up to 37 is prime (a composite below 41 * 41 has a prime factor of at most 37, found by trial division), and a prime passes every strong test. Equivalent.

Survivors, 4 NOT killed and NOT proved equivalent: 39, 40 and 97 (the strong test's squaring loop runs one extra step: from 0, to `s`, or with `s` started at 1) and 99 (the base 2 skipped in the Miller-Rabin loop). The extra step in 39, 40 and 97 only adds the test `a ^ (n - 1) == -1 (mod n)` after the usual ones. It can only turn a rejection into an acceptance for a composite `n` for which all 12 bases pass in that form; no such composite exists below 2^20 (the exhaustive sweep) and none was found in the other tested sets, but this is not a proof for all 64-bit values. Skipping base 2 (99) changes the answer only for a composite that is a strong pseudoprime to the 11 odd prime bases 3 to 37 and not to base 2; none is in the tested sets and none is known to us. Dropping only the base 37 (mutant 98) IS killed (by 3825123056546413051). We did not search for a killing input for these four.

## Not run

Windows was not run locally. Primality exactness for all 64-bit values rests on the published result cited above; the tests do not exhaust 2^64 and check exhaustively only below 2^20 (and the strong pseudoprime and Carmichael lists below 2^22), plus windows near 2^32, 2^63 and 2^64 - 1 and the table. Mutants 29 and 64 timed out and were not investigated. Mutants 39, 40, 97 and 99 are not killed and not proved equivalent. No timing is asserted.
