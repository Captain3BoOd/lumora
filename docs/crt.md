# crt

`core/crt/crt.h` and `crt.cpp` (`lumora::crt`) solve a system of up to 64 congruences `x = r[i] (mod m[i])` for moduli that need not be coprime (generalized Chinese remainder theorem). `crt_solve(n, r, m, out_r, out_m)` returns the smallest non-negative solution `out_r` and the lcm `out_m` of all moduli, when that lcm fits in `uint64_t`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `acc9b7b`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers.

Integer only: no floating point, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state. The source includes `<cstdint>` only.

## Check order

1. Null pointer: `out_r` or `out_m` always; `r` or `m` only while `n > 0` (the given `n` is used, before `TooLarge`).
2. `n > 64`: TooLarge, before any array element is read.
3. One scan over all `n` moduli: any zero is ZeroModulus (before any consistency work).
4. All pairs `i < j`: the residues must agree modulo `g = gcd(m[i], m[j])`, that is `r[i] % g == r[j] % g`; else NoSolution. This is exact mathematics and does not depend on the size of the lcm, so NoSolution beats Overflow.
5. Merge, left to right; the exact lcm above `2^64 - 1` is Overflow.

On any failure `out_r` and `out_m` are not written. The empty system gives `(0, 1)`. Residues of any size are reduced; the order of the equations never changes the result.

## Method

The state is `x` modulo `L` with `x < L`, starting at `x = 0, L = 1`. Merging `x = r (mod m)` with `r` reduced modulo `m`: `g = gcd(L, m)`, `m' = m / g`. If `m' = 1`, `m` divides `L` and the equation is already implied (the pair checks made it consistent). Otherwise, if `L > (2^64 - 1) / m'` the new lcm `L * m'` would exceed `2^64 - 1`: Overflow (a solution exists, because the system is pairwise consistent, which is sufficient for solvability). No product is formed for this test. Else `g` divides `r - x` (consistency), `d = ((r - x) / g) mod m'` is computed on magnitudes without signed numbers, `t = d * inverse((L / g) mod m', m') mod m'`, `x += L * t`, `L *= m'`. Since `t < m'` and `x < L`, `x + L * t < L * m'`, which fits. `L / g` and `m'` are coprime, so the inverse exists.

Helpers, all ordinary `uint64_t` code: `gcd64` (Euclid with remainder); `add_mod(a, b, m) = a >= m - b ? a - (m - b) : a + b` (no sum above `2^64 - 1`); `mul_mod` (64 steps of double-and-add over the bits of `b`, high to low, using `add_mod`); `inverse_mod` (extended Euclid on magnitudes: the signed coefficients alternate in sign, so `t(k+1) = t(k-1) + q * t(k)` on magnitudes, never above `n`; the sign is the step parity).

## Fixed bounds

- gcd: at most 93 steps (cap 128). By Lamé, `F(93) = 12200160415121876738 < 2^64 < F(94)`, so a pair below `2^64` needs at most 91 steps, plus the first swap when the first argument is smaller: at most 92.
- inverse: the same cap, and in fact at most 46 steps, because `L * m' <= 2^64 - 1` was tested first: the arguments are `a < n` with `a * n <= 2^64 - 1`, and `k` steps need `a >= F(k+1)`, `n >= F(k+2)`; `F(48) * F(49) > 2^64 - 1 >= F(47) * F(48)`.
- at most 63 merges, at most 2016 pair checks (64 * 63 / 2), 64 steps per `mul_mod`. Total per call: at most 2016 gcds of at most 92 steps, 64 merges each with at most one gcd, one inverse and one `mul_mod`. Every loop has a constant bound; there is no data-dependent recursion.

## Tests

`tests/crt/crt_tests.cpp`, 18 test cases. Expected values come from three labelled sources. (1) DIGESTS from `tools/crt_ref.py` (Python integers; method A is a brute force over `x`, method B is exact merging with `math.lcm` and `pow(x, -1, m)`; the script cross-checks them and `--check` verifies that the block spliced in the test file is current); the tests re-run each scenario against the real function and recompute the 64-bit digest and the counts. (2) An ORACLE inside the test that shares no code with the implementation: the exact lcm in a 6-limb big integer (schoolbook multiply, shift-subtract remainder, binary gcd), NoSolution as the pairwise test with a binary gcd, Overflow as "consistent and exact lcm above `2^64 - 1`", and for a success the checks that `out_m` is the exact lcm, `out_r < out_m` and `out_r = r[i] (mod m[i])` for every `i`; when the lcm is at most 5000 a brute force over `x` must agree. (3) CONSTANTS for the hand rows (computed once with Python integers) and the null, TooLarge, ZeroModulus and check-order rows (written from the header text).

Scenarios (counts computed by the reference and recounted by the tests):
- `n = 1`, EXHAUSTIVE: 4160 cases.
- `n = 2`, EXHAUSTIVE: moduli 1..24, every residue pair: 90000 cases.
- `n = 3`, EXHAUSTIVE: moduli 1..8, every residue triple and every ordering of the equations: 46656 cases.
- `n = 4`, EXHAUSTIVE: moduli in {1, 2, 3, 4, 6}, every residue and every ordering: 65536 cases.
- residues above the modulus, EXHAUSTIVE over the list `RS(m)` (including `UINT64_MAX`): 24336 cases.
- big moduli pairs, EXHAUSTIVE over `BM x BM` (12 moduli: 1, 2, 3, `2^32 - 1`, `2^32`, `2^32 + 1`, `2^63 - 1`, `2^63`, `2^63 + 1`, `2^64 - 3`, `2^64 - 2`, `2^64 - 1`) with residues from `RS`: 11628 cases.
- big moduli triples, EXHAUSTIVE over `BT^3` (8 moduli) and 4 values of `X` (consistent, and the last residue shifted): 4096 cases.
- gcd-heavy pairs, EXHAUSTIVE over `GS x AS x AS` (6 gcds, 10 cofactors; `m1 = g*a`, `m2 = g*b` when both fit), products above `2^64` with an lcm that fits, consistent and inconsistent: 2052 cases.
- Fibonacci moduli, EXHAUSTIVE over its pairs only (not over all systems): `F(a), F(b)` for `3 <= a < b <= 93` (`F(93)` is the largest below `2^64`), with consistent residues and with the last residue shifted: 8190 cases. The mutants with a gcd cap of 91 and an inverse cap of 45 are killed, so 92 gcd steps and 46 inverse steps are reached by the suite.
- 64 equations, NOT exhaustive: 87 cases. The first 64 primes (Overflow); 64 divisors of `2^63` (success, lcm `2^63`); a large system with one conflicting pair (NoSolution although the lcm would overflow); 64 equal moduli (1, 7, `2^64 - 1`, `2^63`), consistent and with one residue shifted; the seven coprime factors of `2^64 - 1` repeated in 7 rotations (success), and each with the last modulus replaced by 2 (Overflow). 64 distinct primes are pairwise coprime and cannot conflict, so the NoSolution row is built differently.
- random, NOT exhaustive: 300 seeds x 10 = 3000 cases (up to 6 equations; moduli random or sharing a factor; residues consistent or random).
- hand rows, NOT exhaustive: 44 constant systems (codes: 22 Ok, 12 Overflow, 6 NoSolution, 2 ZeroModulus), including the boundary rows: modulus `2^64 - 1` alone succeeds; the seven coprime factors 3, 5, 17, 257, 641, 65537, 6700417 (their product is `2^64 - 1`) as seven equations succeed; the same system plus an equation with modulus 2 is Overflow; `2^63` with 3 (lcm `3 * 2^63`) is Overflow; `2^32` with `2^32 - 1` fits. No row claims an lcm of exactly `2^64`: it cannot occur for `uint64_t` moduli.
- hand-written: the empty system, the null matrix, TooLarge with arrays that must not be read, ZeroModulus before NoSolution and Overflow at every position, NoSolution beats Overflow, Overflow needs a consistent system, outputs byte-compared after every failure.
- Not asserted: anything the header leaves unspecified (overlapping arrays).

## Mutation testing

Real source mutants of `crt.cpp` (105), each compiled with the prebuilt test object and run on the FINAL test file, one at a time (one core), in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. Result, identical in both builds: 89 killed, 16 survived. No timeout occurred.

Crash-only kills (the failure is the crash, with no assertion expansion), each re-run alone. Plain build: SIGFPE (division by zero) for `gcd: stop on a`, `gcd: returns b`, `zero: skip last`; SIGSEGV for `null: out_r only`, `null: out_m only`, `null: r only`, `null: m only`, `null: n > 1`. Sanitizer build: UBSan "division by zero" for the same three SIGFPE mutants, and UBSan "store to null pointer" (`null: out_r only`, `null: out_m only`) or "load of null pointer" (`null: r only`, `null: m only`, `null: n > 1`) for the rest. In the sanitizer build `gcd: no swap` is killed by an assertion (not crash-only); in the plain build it is also an assertion failure. The mutated source is instrumented; the test object is not.

Survivors, each with a proof of equivalence (none is left as not proved). "Equivalent in context" means equal for every input the function can reach, not as a stand-alone helper:
- `steps: 92`: Euclid on (a, b) below 2^64 needs at most 92 steps (Lamé, see Fixed bounds), so a cap of 92 or more is never reached. Equivalent.
- `steps: 93`: Euclid on (a, b) below 2^64 needs at most 92 steps (Lamé, see Fixed bounds), so a cap of 92 or more is never reached. Equivalent.
- `add_mod: >`: the two differ only when a + b = m exactly; the mutant returns m instead of 0, and an operand equal to m acts as 0 in every later add_mod. In `mul_mod` the final value equals m only if the true product is 0 mod m; with 1 <= d < m' and an inverse coprime to m' that product is never 0. Equivalent in context.
- `add_mod: plain sum`: a + b wraps only when a + b >= 2^64, so m > 2^63. Then L * m' <= 2^64 - 1 forces L = 1, g = 1, the inverse is 1 and mul_mod(d, 1, m') only adds d to 0. No wrap. For m <= 2^63 the sum is below 2^64. Equivalent in context.
- `mul_mod: 63 bits`: bit 63 of b (the inverse, below m') is set only for m' > 2^63, which forces L = 1 and inverse 1. Equivalent in context.
- `mul_mod: add via a>b`: (r << 1) % m differs from add_mod(r, r, m) only when r >= 2^63, which needs m > 2^63, forcing L = 1 and b = 1, so r stays 0 until the last add. Equivalent in context.
- `inverse: cap 46`: at most 46 inverse steps occur (see Fixed bounds: a * n <= 2^64 - 1), so a cap of 46 or more is never reached; caps 40, 44 and 45 are killed. Equivalent.
- `inverse: cap 47`: at most 46 inverse steps occur (see Fixed bounds: a * n <= 2^64 - 1), so a cap of 46 or more is never reached; caps 40, 44 and 45 are killed. Equivalent.
- `merge: r not reduced`: (r - x) / g modulo m': replacing r by r + k * m changes it by k * m / g = k * m', which is 0 modulo m'; g divides both forms of r - x (pair checks), and unsigned subtraction in the right order never wraps. Equivalent.
- `merge: mp==1 no skip`: with m' = 1: the overflow test L > MAX / 1 is false, (L / g) mod 1 = 0, d = 0, t = 0 (shortcut), x and L unchanged. Equivalent.
- `overflow: mp / max`: L > floor(MAX / m') iff L * m' > MAX iff m' > floor(MAX / L), for positive integers. Equivalent.
- `lp: no divide`: without the final mod the argument a can exceed n (a = n is impossible, they are coprime and n > 1). Step 1 of the Euclid loop then has q = 0 and swaps to (a, n) with t = (1, 0); step 2 has q = a / n, r = a mod n, t = 1 + q * 0 = 1, giving the state (n, a mod n, 0, 1) of the correct run after an even number of extra steps (parity unchanged). Equivalent, also checked on 182465 random pairs in Python.
- `d: ri>x flipped`: ri > x instead of ri >= x differs only for ri = x: the else branch gives d = 0 % m' = 0 and no negation. Equivalent.
- `d: no mod mp`: in the branch ri >= x, (ri - x) / g <= ri / g < m / g = m' (ri is reduced below m), so the mod is a no-op. Equivalent.
- `d: neg always negate`: for d = 0 the mutant sets d = m'; mul_mod(m', inv, m') has add_mod(r, m', m') = r (r >= m' - m' = 0 gives r - 0), so the result is 0, as with the shortcut. Equivalent.
- `t: zero shortcut off`: for d = 0 the mutant computes mul_mod(0, inverse(lp, m'), m') = 0 and the inverse call has valid arguments (lp >= 1 as it is coprime with m' > 1). Equivalent.

## What was NOT run

Windows (CI only). No timing test. No exhaustive test over all `uint64_t` systems (the 64-equation, random and hand scenarios are sampled). The mutant test object is not sanitizer-instrumented. No 128-bit type or builtin is used in the source or the tests.
