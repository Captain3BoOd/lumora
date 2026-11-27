# lagrange

`core/lagrange/lagrange.h` and `lagrange.cpp` (`lumora::lagrange`) evaluate, modulo `kMod = 998244353` (a prime), the unique polynomial `f` of degree below `n` with `f(i) = y[i]` for the nodes `i = 0 .. n - 1`, at any `u32` point `x` (`n <= kMaxN = 2^20`). One call is `O(n)` modular multiplications plus ONE modular inverse (about 58 more multiplications); the extra memory is six residues. The node shortcut (`x mod kMod < n`) costs the range scan only.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `829ffc5`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no table (no prefix-product or factorial array). `y` is read only.

## Check order and writes

1. A null `y` or a null `out` is NullArgument (wins over everything, also `n = 0`).
2. `n == 0` is EmptyInput.
3. `n > kMaxN` is TooLarge (before `y` is read: a caller may pass a one-word buffer).
4. Any `y[i] >= kMod` is ValueOutOfRange. All `n` entries are scanned, so the result does not depend on which entry is bad or on `x` (a bad entry fails the call also when the request is a good node, also at `n = 1`).
5. `r = x % kMod`. When `r < n` the answer is `y[r]` (the node shortcut: no arithmetic).
6. Otherwise the recurrence below.

`*out` is written once, on success only. `y` is never modified. `x` is used as its residue (`x = kMod` acts as `0`, `x = kMod + 2` as `2`, `x = 2^32 - 1` as `2^32 - 1 - 4 * kMod`).

## Method (exact recurrence)

Let `r = x mod kMod >= n` (so `n <= r < kMod`). Lagrange's formula gives `f(r) = sum_i y_i * prod_{j != i} (r - j) / (i - j)`. Three identities, all over the integers:
- `prod_{j != i} (i - j) = i! * (n - 1 - i)! * (-1)^(n-1-i)`.
- `prod_{j != i} (r - j) = P / (r - i)` with `P = prod_{j < n} (r - j)`.
- `C(n-1, i) = A_i / B_i` with `A_i = (n-1)(n-2) .. (n-i)` (`A_0 = 1`) and `B_i = i!`.

Hence `f(r) = P / (n-1)! * sum_i s_i * y_i * C(n-1, i) / (r - i)` with `s_i = (-1)^(n-1-i)`. Term `i` is the fraction `a_i / b_i` with `a_i = s_i * y_i * A_i` and `b_i = B_i * (r - i)`.

The sum is kept as ONE fraction `N / D` over the nodes seen so far. State before the loop: `N = 0`, `D = 1`, `P = 1`, `A = 1`, `B = 1`. At node `i` (for `i > 0` first `A <- A * (n - i)`, `B <- B * i`, so `A = A_i`, `B = B_i`):
- `d = r - i`, `b = B * d`, `a = s_i * y_i * A`,
- `N <- N * b + a * D`, `D <- D * b`, `P <- P * d`.

What the running values hold after node `i` is finished (loop invariant, proved by induction): `D_i = prod_{t <= i} b_t`, `P_i = prod_{t <= i} (r - t)` and `N_i = sum_{t <= i} a_t * prod_{u <= i, u != t} b_u`, so `N_i / D_i = sum_{t <= i} a_t / b_t`. Base: before node 0, `N = 0`, `D = 1` (empty sum, empty product). Step: `N_i / D_i = N_{i-1} / D_{i-1} + a_i / b_i = (N_{i-1} * b_i + a_i * D_{i-1}) / (D_{i-1} * b_i)`, which is exactly the two updates. After the last node (`i = n - 1`) `B = (n-1)!`, `N / D = sum_i a_i / b_i`, and
`f(r) = P * N / (D * (n-1)!)`.
There is NO per-node inverse: the single inverse is of `D * (n-1)! mod kMod`, by Fermat: `base^(kMod-2)`, square and multiply over the 30 bits of `kMod - 2`.

Why the common denominator is never zero modulo `kMod` (so the inverse exists and the fractions are all legal): `kMod` is prime.
- Every factor of `b_t` is nonzero: `b_t = t! * (r - t)`. Here `r - t` is a positive integer below `kMod` (`r >= n > t`, `r < kMod`, so no wrap and no zero), and `t! = 1 * 2 * .. * t` has all factors in `1 .. t` with `t < n <= 2^20 < kMod`, so each factor is nonzero modulo the prime `kMod`.
- `D` is a product of the `b_t`, so it is nonzero. `(n-1)!` is a product of numbers below `kMod`, so it is nonzero. Their product is nonzero.
- The `A_i` factors `n - i` lie in `1 .. n - 1`, also below `kMod`; `A_i` may only matter through the numerator, never a denominator.
In short: every denominator factor is `r - i` with `r >= n` and `i < n`, or a factorial of a number below `kMod`.

Arithmetic bounds: residues are below `kMod < 2^30`; a product is a `u64` (`< kMod^2 < 2^60`) reduced by one `%`. The sum of two residues is below `2 * kMod < 2^32` and needs one conditional subtraction. `sub_mod` is used only as `0 - a` (the sign `s_i = -1` when `n - 1 - i` is odd).

## Step counts (exact, structural)

For `n >= 1` and `r >= n`, per call: `n` range comparisons; `n` modular adds; `floor(n / 2)` modular subtractions (the signs); `8n + 59` modular multiplications: per node 6 (`b`, `a`, two in `N`, `D`, `P`) plus 2 for the advances of `A` and `B` when `i > 0` (that is `8n - 2`), plus 3 at the end (`D * B`, `N * inverse`, `P * ...`), plus 58 inside the inverse (30 squarings and `popcount(kMod - 2) = 28` multiplications). A node request (`r < n`) does `n` comparisons and nothing else. These were checked with a counting copy of the real source (a text substitution adds a counter to `add_mod`, `sub_mod`, `mul_mod` and the range compare; nothing else changes), for every `n = 1 .. 5000` and for `n = 2^16, 2^19, 2^19 + 1, 2^20 - 1, 2^20` and others: all counts equal the formulas (for example `n = 2^20`: 8388667 multiplications, 1048576 adds, 524288 subtractions, 1048576 compares). The counts are structural, not measured, and the counting copy and its driver are not part of the change.

### One-time scaling table (observation, not asserted)

Best of 3, milliseconds for one call (not a node request), a pseudo-random `y`, this machine, single thread, noisy (two runs of the same binary differed by up to 40 percent). There is no timing assertion in the tests.

| n | -O0 | -O2 |
|---|---|---|
| 2^16 | 1.2 | 0.4 |
| 2^18 | 5.0 | 1.7 |
| 2^20 | 20.1 | 7.0 |

## Tests

`tests/lagrange/lagrange_tests.cpp` has 9 test cases (31 assertions; most checks are accumulated and reported once per group, with the first failing case in the message). Expected values come from three places:
1. DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from `tools/lagrange_ref.py`, a standard-library Python program with three formulations (the definition with a modular inverse per factor, a barycentric form with one inverse per node, and Horner's rule / `pow` for polynomial and monomial values) that the script compares with each other. The tests recompute each digest (FNV-1a over 64-bit words, the file documents the fold order) and the case count from the real function:

| group | cases | digest |
|---|---|---|
| ex3 | 28932 | `0xA4654A0BB210D29B` |
| ex4 | 8368 | `0x97ED0D89F1C681D7` |
| poly8 | 9840 | `0x472362944C550E77` |
| rnd | 350 | `0x499AFBB934A789DF` |
| bigpoly | 30 | `0xDC3F140D6AC6D289` |
| bigmono | 9 | `0xE3EDA7D30BB260EA` |
| bigrnd | 2 | `0x8D7FC86810B53E4C` |

2. ORACLE inside the test, sharing no code with the implementation: the definition as a double loop with a Fermat inverse per factor, in `u64`.
3. HORNER and fast powers: for a polynomial `f` of degree below `n` the samples are `y[i] = f(i)` and the expected value is `f(r)` by Horner's rule, which needs no interpolation; for the monomials `y[i] = i^e` the expected value is `r^e` by fast powers.

Extents (labelled):
- EXHAUSTIVE against the oracle and the digest: every `y` over `{0, 1, p - 1}` for `n = 1 .. 6` and over `{0, 1, 2, p - 1}` for `n = 1 .. 4`, each at every `x` of the list `0 .. n + 6`, `p - 2 .. p + n + 1`, `2p - 1 .. 2p + 1`, `3p`, `4p`, `2^32 - 1`, `2^32 - 2` (so every node, the first non-nodes, the residue wraps and the top of `u32`).
- EXHAUSTIVE against Horner and the digest: every coefficient vector over `{0, 1, p - 1}` of length `n = 1 .. 8` (all polynomials of degree below `n`), sampled at the nodes, at every `x` of the same list.
- SAMPLED (fixed LCG seeds, entries biased to `0`, `1`, `p - 1`, `p - 2`) against the oracle and the digest: `n = 1 .. 64` and `n = 100, 255, 256, 257, 1000, 4096`, five values of `x` each.
- A degree 5 polynomial sampled at `n = 6 .. 40` nodes gives the same polynomial for every `n`.
- SAMPLED at `n = 2^20, 2^20 - 1, 2^19 + 1` (30 values of `x` each: `0, 1, n - 1, n, n + 1, p - 1, p, p + 5, p + n - 1, p + n, 2p, 2^32 - 1` and 18 seeded draws): the polynomials `1`, `p - 1`, `x`, `x^2` and random polynomials of degree `0, 1, 2, 3, 7, 40` against Horner; the monomials `y[i] = i^e` for `e = n - 1, n - 2, n / 2` against `r^e`; the node shortcut boundaries `x = 0, n - 1, n, p + n - 1, p + n, 2p, UINT32_MAX` are in the list. At `n = 2^20` also two random `y` at `x = n + 3` and `x = p + 12345` against the digest only (no independent oracle at that size beyond the Horner and monomial families).
- Every call at a large `n` compares `y` with a copy afterward.
- Hand rows: `n = 1` (`y[0]` for every `x`); `x = p`, `p + 2`, `2^32 - 1` against their residues; `y = [1, 2]` (`f = 1 + x`) at `x = p - 1`; `f = x^2` at `n = 3`; the root of `x - c` for `c` off the nodes; a polynomial with value `p - 1` everywhere.
- Failure matrix: null `y`, null `out` and both null for `n = 0, 1, 3, 2^20 + 1, UINT32_MAX` and `x = 0, 7, UINT32_MAX` (NullArgument, `*out` untouched); `n = 0` with a one-word `y` holding `kMod` (EmptyInput, `y` never read); `n = 2^20 + 1, 2^20 + 2, 2^31 - 1, UINT32_MAX` with a one-word `y` holding `kMod` (TooLarge before the scan); for `n = 1 .. 5` a bad entry (`kMod`, `kMod + 1`, `UINT32_MAX`) at the first, middle and last position with `x = 0, n - 1, pos, n, kMod + pos, UINT32_MAX` (good node, the bad node, off the nodes, also `n = 1`), and two bad entries; at `n = 2^20` a bad entry (`kMod`) at the first, middle and last position with `x = 0, 1, pos, n - 1, n, kMod + pos, 2 * kMod, UINT32_MAX` (requests that would hit the node shortcut); `n = 2^20 + 1` with a real buffer of that size (TooLarge); `kMod - 1` entries accepted. After EVERY call `y` is compared with a copy, and after every failure `*out` is compared with a sentinel.

Boundary witnesses of the modular add: the last add of `y = [1, 2]` at `x = p - 1` sums to exactly `kMod` (the root of `1 + x`); the constant polynomial `p - 1` at `n = 3`, `x = 7` reaches sums above `kMod` (`kMod + 8` appears); `y = [0, 1]` at `x = p - 1` gives sums `0` and `p - 1`. The equality variants of the add (`>` for `>=`, thresholds `kMod - 1` and `kMod + 1`) survive the mutation run: see the equivalence argument below.

## Mutation testing

128 real source mutants of `core/lagrange/lagrange.cpp` (one text change each: the modular add, subtract and multiply and their equality variants, the inverse (exponent, loop, bit test, squaring), the range scan, every check and its code and position, the residue `x % kMod`, the node shortcut, the loop bounds, `d`, `b`, `a`, the sign parity, the advance of `A` and `B`, every update of `N`, `D`, `P`, every initial value, the final product, and the final return code), each run on the FINAL test file (no staging) in two builds, serially, one mutant at a time, each as a complete run of the whole suite: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. The test object is the same plain object in both builds: it is NOT sanitizer-instrumented; only the mutated source is. Plain: 112 killed, 16 survived, 0 timed out. Sanitizer build: 112 killed, 16 survived, 0 timed out, the same sets. No mutant is killed only by the sanitizer build. A mutant that does not terminate is classified TIMEOUT and is never counted as a kill (none occurred). Catch2 runs the test cases in a random order, so which test case fails first differs between runs; the kill count does not depend on it.

Crash-only kills (the batch run of one build shows no failing assertion): 8 mutants (51 y null dropped, 52 out null dropped, 53 nulls joined with `&&`, 59 TooLarge limit `+ 1`, 60 TooLarge dropped, 65 range scan before the TooLarge check, 72 shortcut `r <= n`, 80 loop `i <= n`), each re-run alone in both builds. Plain, alone: 51, 52, 53, 59, 60 and 65 print a failing-assertion marker at the test `SIGSEGV - Segmentation violation signal` (rc 139); 72 and 80 fail assertions (rc 42). Sanitizer build, alone: UBSan `load of null pointer` for 51 and 53; UBSan `store to null pointer` for 52; AddressSanitizer `heap-buffer-overflow` for 59, 60, 65, 72 and 80 (rc 1 each).

Survivors (16), all argued equivalent. Facts used: the result of `add_mod` is consumed only as `N`, and `N` is used only as the operand of `mul_mod` (which widens to `u64` and reduces); `sub_mod` is called only as `sub_mod(0, a)` with `a < kMod`, and its result goes only to `mul_mod`.
- 0, 4 (add `>` for `>=`; threshold `kMod + 1`): they differ only when the sum is exactly `kMod`; the stored `N` is `kMod` instead of `0`, the same residue, and the next use is `mul_mod`, whose product reduces to the same value. The next add sees `kMod + t` and reduces it or not, but the value stays congruent and below `2^32` (it stays below `2 * kMod`).
- 1 (add without reduction): `N` can reach at most `2 * kMod - 2` (the sum of two residues), then goes through `mul_mod` (`u64` product below `2^63`, reduced) before it is added again; so `N` never grows beyond one unreduced sum. Same residue, no overflow.
- 10, 11 (`s % kMod`, `s + 0u`): `s < 2 * kMod`, so `% kMod` equals the single subtraction; `+ 0u` is the same value.
- 12, 15 (sub `>` for `>=`; `x >= y + 1`), 14 (sub always `x + kMod - y`): with `x = 0` they differ only when `a = 0`: the result is `kMod` instead of `0` (14: `kMod - a` always), the same residue; it goes to `mul_mod`.
- 19, 20 (non-wrap branch `y - x`; wrap branch `kMod - y`): with `x = 0` the non-wrap branch is reached only for `y = 0` (`0 - 0` either way), and the wrap branch `x + kMod - y` equals `kMod - y`.
- 22 (`(x + kMod - y) % kMod`): on the wrap branch `x < y`, so `0 < x + kMod - y < kMod` and `%` does nothing.
- 30 (`x % kMod` before the product): `x` is always a residue (inputs are checked first, every helper returns a residue or a congruent value below `2^32`; the unreduced `kMod` of the add and sub variants above does not occur in the real code).
- 48 (range scan returning at the first bad entry): the scan has no side effect and the caller writes nothing on failure, so scanning on is not observable.
- 64 (TooLarge checked before EmptyInput): `n == 0` and `n > kMaxN` cannot both hold.
- 106 (sign by `kMod - a`): `0` becomes `kMod`, the same residue, consumed by `mul_mod`.
- 126 (an extra `*out = add_mod(*out, 0)` after the result): adds `0` to a residue.

## Not run

Windows (CI only); any timing assertion; overlapping `y` and `out` (the header leaves them open; not tested); a full sweep of `n` up to `2^20` (the large extents are the three values of `n` named above); an independent oracle for random `y` at `n = 2^20` (the digest from the Python reference only); a non-prime modulus (not in the contract). The mutant test object is not sanitizer instrumented. Per-case runtimes of the unmutated suite under `-fsanitize=address,undefined` (`-O0`, this machine): the polynomial case at the three large `n` 22.5 s, the monomial case 6.4 s, every other case under 0.4 s; no case is over 30 s. Plain, the same large cases take about 10 s and 3.5 s. `python3 tools/lagrange_ref.py --check tests/lagrange/lagrange_tests.cpp` takes about 6 s here (`NAME...` checks only the named groups); generating the digests takes about 4 s.
