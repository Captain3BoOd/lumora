# ratapprox

`core/ratapprox/ratapprox.h` and `ratapprox.cpp` (`lumora::ratapprox`) give the best rational approximations of an exact rational `num / den` (any `int64_t` num, any nonzero `int64_t` den) with denominator at most `max_den`: `ratapprox_bounds` (the Farey neighbours `lo <= x <= hi`) and `ratapprox_nearest` (the closest candidate).

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `ac68c54`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers.

Integer only: no floating point, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state. The source includes `<cstdint>` only.

## Method

The input is reduced with unsigned magnitudes (`|INT64_MIN| = 2^63` is `~u + 1` on the unsigned value; nothing is negated as a signed number): `x = sign * P / Q` in lowest terms, `P <= 2^63`, `1 <= Q <= 2^63`. If `Q <= max_den` then `x` is a candidate and `lo = hi = nearest = x`. Otherwise the neighbours of `|x|` come from a Stern-Brocot walk with batched steps. The bounds `L = a / b` and `U = c / d` start as `floor(|x|) / 1` and `(floor(|x|) + 1) / 1` with `c*b - a*d = 1`, and the residuals `eL = P*b - a*Q` and `eU = c*Q - P*d` are above 0 while `x` lies strictly between. One batch moves the bound on the far side of the mediant by the largest multiple `k` that keeps it on its side of `x` and keeps its denominator at most `max_den`; `k` comes from a division, never from a product that can overflow. The walk stops when `b + d > max_den`; then `L` and `U` are Farey neighbours of order `max_den` around `x`. A negative `x` is solved on `|x|` and mirrored (`lo = -U`, `hi = -L`).

Nearest: `eL*d + eU*b = Q`, so `|x - L| = eL / (Q*b)` and `|U - x| = eU / (Q*d)` compare as `eL*d` against `eU*b`. Ties: the smaller denominator, then the smaller VALUE in the ORIGINAL orientation (for `-1/2` at bound 1 the result is `-1/1`; for `1/2` it is `0/1`). This is the maintainer's answer.

## Check order

Null pointer (`lo` or `hi`; `out`), then `den == 0` (ZeroDivision), then `max_den == 0` or above `INT64_MAX` (BadBound), then Overflow. On any failure the outputs are not written (results are built in locals and copied at the end).

## Overflow rule, and why every other result fits in int64

Overflow: `num == INT64_MIN`, `den` negative and odd with magnitude `d` in `1..INT64_MAX`, and `d <= max_den`. Then `x = +2^63 / d` is in lowest terms, is a candidate, and every result would need the numerator `+2^63`. Both functions. Everything else succeeds, and every result fits:
- Denominators are at most `max_den <= INT64_MAX`.
- If `x` is a candidate the results are `x`, with numerator `P`. `P = 2^63` happens only for `num = INT64_MIN` with a negative odd `den`. A positive `den` gives a negative `x` with numerator `-2^63`, which fits. A negative even `den` cancels a power of two, so `P <= 2^62`. A candidate `x < 0` has numerator `-P >= -2^63`, which fits.
- If `x` is not a candidate then `Q > max_den >= 1`, so `Q >= 2`. For `|x| < 1`, `P < Q <= 2^63`, so `P <= 2^63 - 1`, and both neighbours have numerators at most `P`. For `|x| > 1` the neighbours are ancestors of `P / Q` in the Stern-Brocot tree: their numerators are below `P` (and `floor|x| + 1 <= P` because `Q >= 2`), so at most `2^63 - 1`. The mirror keeps magnitudes at most `2^63 - 1`. `INT64_MIN / -d` with `d > max_den` falls here, so those calls succeed (tested).
- Intermediate values in uint64: numerators never exceed `P`, denominators never exceed `max_den`, residuals never exceed `Q <= 2^63`. The products `k*c`, `k*d`, `k*eU`, `k*eL` are bounded by the new bound value or by the old residual (`k` is a floor quotient). For nearest, `eL*d + eU*b = Q <= 2^63`, so each product fits in a plain `uint64_t`.

## Fixed bounds

The gcd is Euclid with remainder on `uint64_t`, capped at 128 steps. By Lamé, `n` steps on `a > b > 0` need `b >= F(n+1)`; `F(92) = 7540113804746346429 < 2^63 < F(93)`, so at most 91 steps (90 plus the first swap when `mn < md`). The walk runs at most 128 iterations and needs at most 89 batches: a batch finishes one partial quotient of `|x|` (or is cut by the denominator bound and the next iteration breaks), `P / Q` has at most 90 quotients by the same bound, and the last one is never taken in full. The Fibonacci scenario reaches 89 (a cap of 88 is killed, a cap of 89 survives and is shown equivalent below). No data-dependent loop remains.

## Tests

`tests/ratapprox/ratapprox_tests.cpp`. Expected values come from three labelled sources. (1) DIGESTS from `tools/ratapprox_ref.py` (Python `Fraction`; a brute force over every denominator and a continued fraction method, cross-checked by the script; `--check` verifies the generated block in the test file). (2) In-test ORACLES that share no code with the implementation: a 6-limb big integer that checks every result against the definition (lowest terms, `lo <= x <= hi`, determinant 1, denominator sum above `max_den`, exact distance comparison and tie rule), and a plain int64 brute force over every denominator for small cases. (3) CONSTANTS: hand rows computed with Python, and hand-written null, check-order, BadBound and Overflow rows.

Grids (unique counts computed by the reference and recounted by the tests): `NI` = 65 numerators, `DI` = 64 denominators, `NB` = 31 bounds (listed in the generated block of the test file).
- grid A, EXHAUSTIVE: every `NI x DI` pair at every `NB` bound plus `d-1, d, d+1` around the reduced denominator `d` (where in `1..INT64_MAX`; the bound `INT64_MAX` is in NB for the `2^63` denominators). 132023 cases run, 273 Overflow cases skipped (covered below).
- grid B, EXHAUSTIVE: 1959 values (0 and `+-p/q`, `1 <= p,q <= 40`, lowest terms) in two forms `(n, d)` and `(-3n, -3d)` at every bound 1..64; 250752 cases.
- ties, EXHAUSTIVE: midpoints of consecutive Farey terms of order N = 1..16, plus j = 0..2, both signs, two forms, at bound N; 5964 cases.
- overflow family, EXHAUSTIVE over its list: 18 odd `d` (1, 3, ..., 19, 2^31 +- 1, 2^32 +- 1, 2^62 +- 1, INT64_MAX - 2, INT64_MAX) at `d-1, d, d+1, 1, 2, INT64_MAX`; 102 cases. Overflow rows are outside the grid A digest. `INT64_MIN / -1` is covered here and by hand.
- Fibonacci chains, NOT exhaustive: 5376 cases. Random, NOT exhaustive: 400 seeds x 10 = 4000 cases. Hand rows, NOT exhaustive: 144 constants, including `INT64_MIN` over -2, -4, -6, 1, 3.
- Hand-written: null matrix, ZeroDivision before BadBound before Overflow, BadBound values (0, 2^63, 2^63 + 1, UINT64_MAX), outputs byte-compared after every failure, the Overflow family by hand (d = 1, 3, 5, INT64_MAX at bounds below, equal to and above d, and at 1, 2, INT64_MAX), the tie examples.
- Not asserted: anything the header leaves unspecified.

## Mutation testing

Real source mutants of `ratapprox.cpp`, each compiled with the prebuilt test object and run on the FINAL test file, one at a time (one core), in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. The test object itself is NOT sanitizer-instrumented in either build. Total mutants: 120. Killed: 101 in both builds. Survived: 19 in both builds (the same set). No mutant timed out and none failed to compile.

Crash-only kills (the failure is the crash, with no assertion expansion), each re-run alone. Plain build: SIGFPE for `gcd: 64 steps`, `gcd: stop on a`, `gcd: returns b`, `reduce: no gcd`, `reduce: q not reduced`, `reduce: p not reduced`, `a0: p / q + 1`, `init: el from a0`, `init: eu = q-el-1`, `stepL: b uses b`, `stepL: el uses el`, `stepU: eu uses eu`; SIGSEGV for `bounds: null lo only`, `bounds: null hi only`, `nearest: null`. Sanitizer build: `runtime error: division by zero` for `gcd: 64 steps`, `gcd: stop on a`, `gcd: returns b`, `reduce: no gcd`, `cand: never`, `init: eu = q-el-1`, `stepL: b uses b`, `stepL: el uses el`, `stepU: eu uses eu`; `runtime error: store to null pointer` for the three null-pointer mutants. Every other kill is a failed assertion with an expansion in at least one build.

Survivors, each with a proof of equivalence (none is left as not proved):
- `mag: v <= 0`: v = 0 gives ~0 + 1 = 0, the same as u. Equivalent.
- `mag: signed negate`: for v < 0, -(v + 1) is |v| - 1 (no overflow, v + 1 <= 0) and adding 1 gives |v|, also for INT64_MIN (2^63). Equivalent.
- `steps: 91`: both loops use this cap. Euclid on (mn, md) needs at most 91 steps (Lamé, see Fixed bounds) and the walk at most 89 batches, so a cap of 91 or more is never reached. Equivalent.
- `steps: 92`: both loops use this cap. Euclid on (mn, md) needs at most 91 steps (Lamé, see Fixed bounds) and the walk at most 89 batches, so a cap of 91 or more is never reached. Equivalent.
- `steps: 93`: both loops use this cap. Euclid on (mn, md) needs at most 91 steps (Lamé, see Fixed bounds) and the walk at most 89 batches, so a cap of 91 or more is never reached. Equivalent.
- `sign: or`: gcd(md, mn) equals gcd(mn, md); Euclid swaps the pair in one extra step when mn < md, so at most 92 steps, below the cap of 128. Equivalent.
- `sign: no p!=0 test`: p = 0 means x = 0 = 0 / 1, always a candidate (max_den >= 1); signed_num(true, 0) is ~0 + 1 = 0, so the output is 0 / 1 either way, and the Overflow test is unaffected. Equivalent.
- `loop: 90 batches`: a batch finishes one partial quotient of |x| (or is cut by the denominator bound and the next iteration breaks); P / Q has at most 90 quotients (Lamé) and the last one is never taken in full (it would make a bound equal to x, whose denominator Q is above max_den), so at most 89 batches run. The mutant with a cap of 88 IS killed and the one with 89 survives, so 89 is reached by the tests, and 90 and 92 are never reached. Equivalent.
- `loop: 92 batches`: a batch finishes one partial quotient of |x| (or is cut by the denominator bound and the next iteration breaks); P / Q has at most 90 quotients (Lamé) and the last one is never taken in full (it would make a bound equal to x, whose denominator Q is above max_den), so at most 89 batches run. The mutant with a cap of 88 IS killed and the one with 89 survives, so 89 is reached by the tests, and 90 and 92 are never reached. Equivalent.
- `loop: break > +1`: with b + d = max_den + 1 both batch multiples, (max_den - b) / d = (d - 1) / d and (max_den - d) / b = (b - 1) / b, are 0, so the extra iterations change nothing. Equivalent in result.
- `loop: break uses b only`: b <= max_den always, so the break never fires; but whenever b + d > max_den both multiples are 0 (as for break > +1), so the extra iterations change nothing. Equivalent in result; the loop then runs its constant bound of 128 iterations.
- `loop: break uses d only`: b <= max_den always, so the break never fires; but whenever b + d > max_den both multiples are 0 (as for break > +1), so the extra iterations change nothing. Equivalent in result; the loop then runs its constant bound of 128 iterations.
- `loop: no break`: b <= max_den always, so the break never fires; but whenever b + d > max_den both multiples are 0 (as for break > +1), so the extra iterations change nothing. Equivalent in result; the loop then runs its constant bound of 128 iterations.
- `step: el >= eu`: el = eu with b + d <= max_den cannot happen: the mediant of the bounds would have residual el - eu = 0, so it would equal x, and its denominator b + d <= max_den would make x a candidate. Equivalent.
- `stepL: el/eu -> (el-1)/eu`: the two differ only when eu divides el; then taking el / eu steps would make L equal x with a denominator at most max_den, impossible for a non-candidate, so the denominator bound is the smaller multiple and the min is unchanged. Equivalent.
- `stepU: eu-1 -> eu`: the two differ only when el divides eu; same argument (U would equal x). Equivalent.
- `nearest: compare <=`: the comparison sits in the branch where near_l != near_u, so <= and < agree. Equivalent.
- `nearest: tie den <=`: the comparison sits in the branch where b != d, so <= and < agree. Equivalent.
- `loop: 89 batches`: 89 is the maximum batch count (proof under loop: 90 batches; the Fibonacci scenario reaches it and a cap of 88 is killed). Equivalent.

## What was NOT run

Windows (CI only). No timing test. No exhaustive test over all `int64` inputs (the Fibonacci, random and hand scenarios are sampled). The mutant test object is not sanitizer-instrumented. No 128-bit type or builtin is used in the source or the tests.
