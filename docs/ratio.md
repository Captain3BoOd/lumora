# ratio

`core/ratio/ratio.h` and `ratio.cpp` (`lumora::ratio`) are exact rational numbers over `int64_t`: `ratio_make`, `ratio_add`, `ratio_sub`, `ratio_mul`, `ratio_div`, `ratio_compare`, `ratio_floor`, `ratio_ceil`. A valid value is `num / den` with `den > 0` and `gcd(|num|, den) = 1`, so zero is exactly `0 / 1`. A result is the exact value in lowest terms or `Overflow`; it is never rounded or wrapped.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no 128-bit type, no compiler builtin or intrinsic or extension, no allocation, no recursion, no global state. The source includes `<cstdint>` only. The tests use no 128-bit type or builtin either.

## Method and the two-word helper

Every value is handled as a sign plus an unsigned magnitude (`uint64_t`), so `INT64_MIN` is never negated as a signed number (`|INT64_MIN| = 2^63` is formed with `~u + 1` on the unsigned value). Products and sums that can reach about 2^127 use an internal struct `Wide {hi, lo}` of two `uint64_t` words, in an anonymous namespace: add, subtract, compare, 64 x 64 multiply from 32-bit limbs, and division with remainder by a `uint64_t` (shift and subtract). It is ordinary C++ integer code, not a compiler 128-bit type.

Every loop has a fixed bound, so the running time has no data-dependent loop:
- the 32-bit-limb multiply has no loop;
- the division runs exactly 128 shift-subtract steps;
- the gcd (Euclid with `%` on `uint64_t`) is capped at 128 steps. For values below 2^63 it needs at most 91 (see the gcd bound below), so the cap is never reached and the answer is exact.

- add and subtract: with `g = gcd(db, dd)` the sum `a/b + c/d` has numerator `a * (d/g) + c * (b/g)` and denominator `(b/g) * d` (each product below 2^126, the sum below 2^127). A common factor of the numerator and the denominator can only divide `g`, so the reduction is by `gcd(g, numerator mod g)`. Subtraction flips the sign of the second operand (sign and magnitude, no negation of a signed value).
- multiply and divide: cancel crosswise first (`gcd(|a|, d)` and `gcd(|c|, b)`); the product is then in lowest terms. Divide multiplies by the reciprocal, with the sign of `b.num`.
- fits: the reduced numerator must be in `[-2^63, 2^63 - 1]` (so `-2^63 / 1` fits and `2^63 / 1` does not) and the reduced denominator at most `2^63 - 1` (so `1 / 2^63` does not fit). Overflow is decided on this exact result, so an intermediate above 2^64 never causes an Overflow.
- compare: signs first, then the exact products `|a| * den_b` and `|b| * den_a` in `Wide`.
- floor and ceil: `num / den` with a positive `den` (no `INT64_MIN / -1`), then `-1` for a negative remainder (floor) or `+1` for a positive one (ceil); a nonzero remainder means `den > 1`, so neither step can leave the int64 range.
- on any failure the output is never written: the result is built in a local and copied at the end (and the helpers write `*out` only after the fit check passes).

gcd bound. By Lamé's theorem, Euclid on `a > b > 0` taking `n` steps needs `b >= F(n+1)` where `F` is the Fibonacci sequence. `F(92) = 7540113804746346429` is below `2^63` and `F(93) = 12200160415121876738` is above it, and every gcd argument here is at most `2^63`. So `n + 1 <= 92`, that is at most 91 steps. The worst case is `gcd(F(91), F(92))`: 91 steps (checked in Python, along with 90 for `gcd(F(92), F(91))`). A cap of 91 or more is therefore enough; the source uses 128.

## Check order

- `ratio_make`: null `out` gives NullArgument; then `den == 0` gives ZeroDivision (also for `num = 0`); then Overflow if the reduced, sign-normalized value does not fit. `INT64_MIN / INT64_MIN` is `1 / 1`, `INT64_MIN / 2` is `-2^62 / 1`, `(1, INT64_MIN)` and `(-1, INT64_MIN)` are Overflow (reduced denominator 2^63), `(INT64_MIN, -1)` and `(INT64_MIN, -3)` are Overflow.
- `ratio_add`, `ratio_sub`, `ratio_mul`, `ratio_div`: a null pointer (a, b or out) gives NullArgument; then an invalid `a` or `b` gives BadRatio; then (divide only) `b.num == 0` gives ZeroDivision; then Overflow. An invalid `b` such as `0 / 5` is BadRatio, not ZeroDivision.
- `ratio_compare`, `ratio_floor`, `ratio_ceil`: NullArgument, then BadRatio.
- Valid: `den > 0` and `gcd(|num|, den) == 1`. `INT64_MIN / 1`, `INT64_MIN / 3` and `INT64_MIN / INT64_MAX` are valid; `INT64_MIN / 2`, `INT64_MIN / INT64_MIN`, any `den <= 0` (including `INT64_MIN`), `0 / 2` and `2 / 4` are BadRatio.
- On any failure the output (`*out`) is untouched. The pointers may alias (`out` equal to `a` or `b`); on a failure the aliased input is unchanged too.

The maintainer answers to the gates, relayed via Instinct, are what this document and the code follow.

## Bounds

| Item | Bound |
| --- | --- |
| numerator of a result | `-2^63` to `2^63 - 1` |
| denominator of a result | 1 to `2^63 - 1` |
| largest intermediate | below 2^127 (two 64-bit words) |
| gcd steps | at most 91 (cap 128) |
| division steps | exactly 128 |

## Tests

`tools/ratio_ref.py` is an independent reference in Python (`fractions.Fraction` for the exact value, plain integers for the checks; floor is `n // d`, ceil is `-(-n // d)`). It prints the grids and the 64-bit digests, spliced into the tests and recomputed there.

```
python3 tools/ratio_ref.py                                          # print the block
python3 tools/ratio_ref.py --check tests/ratio/ratio_tests.cpp
```

Where each expected value comes from:
1. DIGEST: the block from `ratio_ref.py` (every exhaustive grid and the random scenarios): the tests re-run the scenario against the real functions and recompute the digest and the counts.
2. ORACLE: a naive oracle inside the test that shares no code with `ratio.cpp`: a sign-magnitude big integer (6 limbs of 32 bits, schoolbook multiply, binary gcd; no 128-bit type). It forms the exact unreduced `N / D`, finds `g = gcd(N, D)`, decides Overflow when `N / g` or `D / g` does not fit, and checks the returned value with `num * g = N` and `den * g = D`. Floor and ceil are checked against their defining inequalities. The oracle runs on every pair of every grid below (including the random ones).
3. CONSTANTS: the hand-picked extreme rows (written in the test as constants, computed once with Python Fractions), and the null, BadRatio, check-order and aliasing tests (written by hand from the header).

After a failing call the output is compared byte for byte with the sentinel that was passed (the header promises it is untouched). `tests/ratio/ratio_tests.cpp` has 19 test cases and 5,228 assertions (the grids count mismatches and assert once per grid, plus the case count, the operation count and the digest).

The grids (all exact definitions are printed in the block; sizes are computed):
- Ns, 41 numerators: -8 to 8; plus and minus each of 2^31 - 1, 2^31, 2^31 + 1, 2^32 - 1, 2^32, 2^32 + 1, 2^62 - 1, 2^62, 2^62 + 1; `INT64_MIN`, `INT64_MIN + 1`, `INT64_MIN + 2`, `INT64_MAX - 2`, `INT64_MAX - 1`, `INT64_MAX`.
- Ds, 20 denominators: 1 to 8; 2^31 - 1, 2^31, 2^31 + 1, 2^32 - 1, 2^32, 2^32 + 1, 2^62 - 1, 2^62, 2^62 + 1; `INT64_MAX - 2`, `INT64_MAX - 1`, `INT64_MAX`.
- Dm, 42 make denominators: Ds, minus Ds, 0, `INT64_MIN`, `INT64_MIN + 1`, `INT64_MIN + 2` (minus `INT64_MAX` and minus `INT64_MAX - 1` are `INT64_MIN + 1` and `+ 2`, counted once).
- G: every `(n, d)` in Ns x Ds with `gcd(|n|, d) = 1` (zero only as `0 / 1`): |G| = 555 valid ratios.
- B: every `(n, d)` in Ns x Dm that is NOT valid: |B| = 1,167.
- Partners P: 12 valid ratios (`0/1`, `1/1`, `-1/1`, `2/3`, `-3/2`, `INT64_MIN/1`, `INT64_MAX/1`, `1/INT64_MAX`, `-1/INT64_MAX`, `INT64_MIN/3`, `INT64_MAX/2`, `1/2`).
- D12: every valid `(n, d)` with n in -12..12 and d in 1..12: 183 ratios.

| Scenario | Coverage | Against |
| --- | --- | --- |
| EXHAUSTIVE: `ratio_make` over every (num in Ns, den in Dm) (1,722 cases) | exhaustive over this stated extent | oracle and digest |
| EXHAUSTIVE: add, sub, mul, div and compare over every ordered pair of G (308,025 pairs, 1,540,125 operations; division by `0/1` included) | exhaustive over G x G | oracle and digest |
| EXHAUSTIVE: floor and ceil of every element of G (555) | exhaustive over G | oracle and digest |
| EXHAUSTIVE: every x in B against every p in P, both argument orders, all five two-argument functions, plus floor and ceil of every x in B (15,171 cases, 141,207 operations) | exhaustive over B x P | oracle and digest |
| EXHAUSTIVE: all five operations over every ordered pair of D12 (33,489 pairs, 167,445 operations) | exhaustive over D12 x D12 | oracle and digest |
| 400 pseudo-random seeds, 10 pairs of random valid ratios with bit lengths up to 63, all five operations (4,000 cases, 20,000 operations) | NOT exhaustive | oracle and digest |
| 400 pseudo-random seeds, 10 random `(num, den)` with bit lengths up to 64 for `ratio_make` (4,000 cases) | NOT exhaustive | oracle and digest |
| Fibonacci ratios `F(k) / F(k+1)` and their negatives, k = 60 to 91, every ordered pair, all five operations plus floor and ceil, and unreduced Fibonacci pairs for make (the longest gcd chains) | hand-chosen set, NOT exhaustive | oracle only |
| 64 extreme add, sub, mul, div rows (products near 2^126, sums with an overflowing intermediate and a fitting result, results needing `den > INT64_MAX`), 34 make rows, 20 compare rows (each also swapped), 20 floor and ceil rows | hand-written | explicit constants (computed with Python Fractions) |
| validity vectors (17 invalid, 14 valid values including `INT64_MIN` over odd and even denominators), BadRatio on every function as first and second argument against every valid vector | hand-written | return codes, untouched outputs |
| check order: null beats BadRatio beats ZeroDivision beats Overflow, null `out` beats `den = 0` in make, every pointer alone for all eight functions | hand-written | return codes, untouched outputs |
| aliasing: `out == a`, `out == b` and `a == b == out` for add, sub, mul, div over D12 x D12, every 7th element of G and P (success equals the unaliased result, failure leaves the aliased input unchanged) | sampled, NOT exhaustive | unaliased call |

Only the rows marked EXHAUSTIVE are exhaustive, and only over the stated extents. The pseudo-random scenarios, the Fibonacci set and the aliasing sweep are NOT exhaustive. The running time is not tested; the absence of data-dependent loops is argued above.

## Mutation check

179 hand-written source mutants of `ratio.cpp` (the magnitude, the gcd and its cap, every part of the 64 x 64 multiply, compare, add, subtract and divide of `Wide`, the validity test, the fit checks, the add, subtract, multiply, divide, make, compare, floor and ceil bodies and their check order and sign handling). Each was compiled against the prebuilt object of the FINAL test file and run with `-a`, plain and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source (the test object is not instrumented). All counts below come from the final run on the final test file.

| Build | Killed by a failing assertion | Killed by a crash or sanitizer report | Timeouts (NOT kills) | Survivors |
| --- | --- | --- | --- | --- |
| plain | 144 | 10 | 0 | 25 |
| sanitizer | 137 | 17 | 0 | 25 |

Crash-only kills, each run alone with the observed message. Plain: SIGSEGV after Catch2's FAILED line (no other message) for 115 (make without the null check), 129, 130 and 131 (add without the null check for a, b, out), 161 (compare), 167 (floor) and 173 (ceil) without the null check of `out`; SIGFPE for 10 (gcd stops on the wrong variable, division by zero), 168 and 174 (floor and ceil without the BadRatio check, `den = 0` in the division). Sanitizer: the same ten, plus 11 (gcd returns `b`, `division by zero`), 40 and 41 (divmod bit boundary: `shift exponent 64 is too large`, `shift exponent -1 is negative`), 42 and 43 (hi and lo shift: `shift exponent 64 is too large`, `shift exponent -1 is negative`), 52 (quotient high word boundary: `shift exponent 64 is too large`), 163 (floor adjusts on a zero remainder: `signed integer overflow: -9223372036854775808 - 1`). The sanitizer messages for 10, 115, 129, 130, 131, 161, 167, 168, 173 and 174 are `division by zero`, `store to null pointer` and `member access within null pointer`. The plain first pass at four parallel jobs on a one-core machine produced 52 spurious 60-second timeouts; each of those 52 was re-run alone (all finished within the limit) and the final plain classification uses the alone runs. Mutant 57 (validity without the `den > 0` test) was an assertion kill when run alone and an abnormal exit in the parallel pass (its behaviour past the first failing assertion is undefined); it is counted as an assertion kill.

### Timeouts

None in the final runs, plain or sanitizer.

### Time-only mutants

None: no mutant gives the right answer in more than constant time (every loop is bounded by a constant).

### Survivors

All 25 survive in both builds. Each is argued equivalent (the results never differ for valid inputs, which every internal call has):
- 2, `v <= 0` in the magnitude: for `v = 0` both branches give 0.
- 3, signed negate in the magnitude: for `v < 0`, `-(v + 1) + 1` is `|v|` (also `2^63` for `INT64_MIN`, with no signed overflow since `v + 1` is `-2^63 + 1` at worst).
- 8 and 9, gcd cap of 92 and 93 steps: at most 91 steps are ever needed (gcd bound above), so the cap is never reached.
- 26, `<=` in the low-word compare of `wide_less`: differs only for equal values. In add, equal products give `num = t2 - t1 = 0` on either branch and the zero shortcut then fires; compare calls it only after `wide_equal` is false.
- 35, the carry of `wide_add` tested against `b.lo` instead of `a.lo`: the sum wraps exactly when the result is below either addend.
- 38, 127 division steps: the dividend is below 2^127 (each product is below `2^63 * 2^63`, since `|num| <= 2^63` and the other factor is a denominator quotient at most `2^63 - 1`, and the sum of two of them is below 2^127), so bit 127 is always 0.
- 44, the carry ignored in the division: the divisor is `g` or `g2`, at most `INT64_MAX`, so the running remainder stays below 2^63 and `r << 1` never carries.
- 47, the carry taken from bit 62: if `r >= 2^62` then `2r + bit >= 2^63 > d`, so the subtraction happens anyway; otherwise bit 62 is 0, as in the original.
- 49, `^` for `|` when shifting the bit in: the shifted value has a zero low bit.
- 50, `i - 63` for `i - 64` in the quotient high word: it matters only when the correct quotient has a high bit set, that is a quotient of 2^64 or more, which `store` rejects (`hi != 0` means Overflow) either way; no quotient with bit 127 is ever formed.
- 55, `d - r` for `r` as the remainder: here `d = g`, and `gcd(g, g - r) = gcd(g, r)`; for `r = 0` both are `g`.
- 61, `<= 1` for `== 1` in the validity test: the gcd of a positive denominator is at least 1.
- 80, `!wide_less(t2, t1)` for `wide_less(t1, t2)`: differs only for equal products, where the numerator is 0 on both branches.
- 85, no zero shortcut in add: a zero sum has `a = -b`, so both valid inputs have the same denominator `g`, `da1 = 1`, the denominator `da1 * db / g2` with `g2 = g` is 1 and the numerator 0, as the shortcut gives.
- 92, `num mod da` for `num mod g`: `g` divides `da`, so `gcd(g, x mod da) = gcd(g, x)`.
- 96 and 114, `store` writing straight to `out`: `store` writes only after every fit check has passed, so a failure never writes (the copy from a local is redundant). In 114 the following `*out = *out` is a no-op.
- 97, 98 and 99, the zero shortcut of multiply: a valid zero has denominator 1, so without the shortcut the cancellation gives `0 / (1 * 1)`.
- 126, `neg` without `rn != 0` in make: `0` stored as negative is still `~0 + 1 = 0`.
- 135, `a->num <= 0` for the sign in add: the zero case then has `na` true with magnitude 0, which gives the same value (the zero shortcut, or the other term alone with its own sign).
- 138, the flipped sign of a zero `b` in subtract: only the sign flag of a zero magnitude changes, and the result does not depend on it.
- 151, the zero-sign branch of compare: with both signs 0 both products are 0 and the equality test keeps the result 0.

Survivors NOT killed and NOT proved equivalent: none.

## Not run

Windows was not run locally. No timing or step-count test. Not tested: any behaviour the header leaves open (failure outputs of `ratio_make` beyond "untouched" are compared byte for byte only where the header promises it). The random scenarios, the Fibonacci set and the aliasing sweep are sampled and not exhaustive; exhaustive coverage stops at the stated grids (for example no exhaustive pair grid beyond G x G and D12 x D12). The mutants ran with a non-instrumented test object.
