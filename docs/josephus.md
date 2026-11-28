# josephus

`core/josephus/josephus.h` and `josephus.cpp` (`lumora::josephus`) solve the Josephus problem for `n` people (`n <= kMaxN = 2^24`) and any step count `k` (any `u32` above 0): `josephus_survivor` returns the person left when `n - 1` people have been removed, `josephus_removed` returns the person removed at a given 0-based step. One call is O(n) steps (one modulo per circle size), with no simulation of the circle, no table and no recursion.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `ca464db`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no array.

## Check order and writes

For both functions: a null `out` is NullArgument; then `n == 0` or `k == 0` is BadArgument (so `n = 2^24 + 1` with `k = 0` is BadArgument); then `n > kMaxN` is TooLarge; then, for `josephus_removed` only, `step >= n` is OutOfRange (so `n = 2^24 + 1` with `step = UINT32_MAX` is TooLarge, `step = n - 1` is accepted, `step = n` is OutOfRange; for `n = 1`, step 0 gives 0 and step 1 is OutOfRange). `*out` is written once, on success only: on every failure it keeps its value (the tests compare a sentinel).

## Method and derivation

Label the people of a circle of size `m` by their distance from the person where the count starts: `0 .. m - 1`. The `k`-th person counted has label `(k - 1) mod m` and is removed. The count restarts at the NEXT person, whose label in the circle of size `m` is `k mod m` (the removed person has label `(k - 1) mod m`, the next one is one further, so `k mod m`; this is the restart point). In the circle of size `m - 1` (the removed person is gone) that person is relabelled 0, the next one 1, and so on, so a person with label `idx` in the smaller circle has label `(idx + k) mod m` in the larger one (the count from the restart point goes `idx` places, the restart point itself being `k mod m`). That is the lift map

`lift(idx, k, m) = (idx + k) mod m`.

Survivor: with one person the label is 0, and lifting through the sizes `m = 2 .. n` gives the survivor: `J(1) = 0`, `J(m) = (J(m - 1) + k) mod m`.

Removed at step `s` (0-based): at that step `s` people are already gone, so the circle has `m0 = n - s` people, counting starts at its label 0 and the removed person has label `(k - 1) mod m0`. Lifting that label through the sizes `m0 + 1 .. n` with the same map gives its label in the full circle, which is its person number (the full circle's labels are the person numbers: the count starts at person 0). For `s = n - 1`: `m0 = 1`, the label is 0 and the lift is the survivor chain, so `josephus_removed(n, k, n - 1) == josephus_survivor(n, k)` by construction. For `s = 0` there is no lift and the answer is `(k - 1) mod n`.

Bounds: `idx < m` always holds (the initial labels are `0 < 2` and `(k - 1) mod m0 < m0`, and `lift` returns a value below `m`), `k < 2^32`, `m <= 2^24`, so `idx + k < 2^24 + 2^32 < 2^33`: the sum is a `u64` and one `u64 %` is taken; there is no multiplication, so no widened product. `k` is not reduced first. The modulus `m` is never 0: `m >= 2` in every lift, and `m0 = n - step >= 1` because `step < n`.

## Step counts (exact, structural)

`josephus_survivor`: `n - 1` modulos (one per size `2 .. n`), `n - 1` additions. `josephus_removed` at step `s`: `s + 1` modulos (one for the first label, `s` lifts); `josephus_removed(n, k, 0)` is one modulo and `josephus_removed(n, k, n - 1)` is `n`. No other arithmetic is counted. Checked with a counting copy of the real source (a text substitution adds a counter to `lift` and to the first modulo; nothing else changes): all `n = 1 .. 300`, `k` in `1, 2, 7, 4000000000`, every step: 181800 calls, no mismatch; and for `n = 1000, 65536, 2^20, 2^24 - 1, 2^24` (`k = 3`): survivor `n - 1`, removed at `n / 2` has `n / 2 + 1`, removed at 0 has 1 (for example `n = 2^24`: 16777215 and 8388609). The counts are structural, not measured. The counting copy and its driver are not part of the change.

### One-time scaling table (observation, not asserted)

Best of 3, milliseconds for one `josephus_survivor(n, 3)`, this machine, single thread. The `-O0` and `-O2` builds gave the same figures here (the loop is bound by the 64-bit modulo). There is no timing assertion in the tests.

| n | -O0 | -O2 |
|---|---|---|
| 2^16 | 0.36 | 0.36 |
| 2^20 | 5.7 | 5.7 |
| 2^24 | 91.9 | 91.7 |

## Tests

`tests/josephus/josephus_tests.cpp` has 13 test cases (53 assertions; most checks are accumulated and reported once per group, with the first failing case in the message). Expected values come from three places:
1. DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from `tools/josephus_ref.py`, a standard-library Python program with two formulations compared with each other by the script (the literal circle with `list.pop`, and the recurrence written with Python integers). The tests recompute each digest (FNV-1a over 64-bit words: `n`, `k`, the removed numbers, or `n`, `k`, `step`, value for sampled steps) and the count from the real functions:

| group | values | digest |
|---|---|---|
| ex_a | 1515000 | `0x8CD08BD914D1C1B5` |
| ex_b | 11476 | `0xE43E0F13A40DEDF4` |
| fen_s | 91930 | `0xC26D598C052ACCBB` |
| whole_a (n = 65535) | 655350 | `0x0E9D8131848036CF` |
| whole_b (n = 65536) | 655360 | `0x35991B2E9D11350E` |
| whole_c (n = 65537) | 655370 | `0x6A586FBE79D9270B` |
| fen_l | 180 | `0xF0AB23F76E94C5CE` |
| max_a | 54 | `0x33A83BAAA972FD4E` |
| max_b | 36 | `0x407F3A164EFC3EF7` |

2. ORACLES inside the test, sharing no code with the implementation: a literal simulation (a vector; the person at `(pos + k - 1) mod size` leaves; the count continues at the same index), and an order-statistic simulation (a Fenwick tree over the people still present, the person of a given rank found by descent, O(n log n)). A test case checks that the two oracles agree for `n = 1 .. 64` and eight values of `k`.
3. CLOSED FORMS and properties: `k = 1` removes `0, 1, 2, ..` (survivor `n - 1`); `k = 2` leaves `2 * (n - 2^a)` (`2^a` the largest power of two not above `n`); the first removed is `(k - 1) mod n`; the removal order is a permutation whose last element is the survivor; hand rows from the header (`n = 5, k = 2` removes 1, 3, 0, 4 and leaves 2) and the classics (`n = 7, k = 3`; `n = 41, k = 3` leaves 30).

Extents (labelled):
- EXHAUSTIVE, whole removal order against the vector oracle and the digest: `n = 1 .. 100`, every `k = 1 .. 300` (this contains `n = 1 .. 64` with `k = 1 .. 2n + 1`); and `n = 1 .. 40` with the 14 larger `k`: `2n, 2n + 1, 3n - 1, 3n, 3n + 1, 301, 1000, 2^16, 2^16 + 1, 2^31 - 1, 2^31, 2^31 + 1, 2^32 - 2, 2^32 - 1`.
- WHOLE order against the Fenwick oracle and the digest: `n = 1000, 4096, 4097`, each with the 10 values `k = 1, 2, 3, 7, n - 1, n, n + 1, 2n + 1, 2^31, 2^32 - 1`.
- PERMUTATION property: all `n = 1 .. 300` and eight seeded `n` in `301 .. 2048`, with those 10 values of `k` and 4, 5, 6.
- WHOLE removal order from production calls against the Fenwick oracle and the digest, ONE test case per `n`: `n = 65535`, `65536`, `65537`, each with the 10 values `k = 1, 2, 3, 7, n - 1, n, n + 1, 2n + 1, 2^31, 2^32 - 1` (every step of every order: 655350, 655360 and 655370 removed numbers, each also compared with the survivor call at the end). Cost: a whole order is `n^2 / 2` modulos of the library (2.1 * 10^9 at `n = 65537`, about 12 s per `k`), so each of the three cases takes about 2 minutes (see Not run for the runtimes).
- SAMPLED steps against the Fenwick oracle (its whole order) and the digest at `n = 2^20 - 1, 2^20, 2^20 + 1` with the 10 values of `k`, steps `0, 1, 2, n / 2, n - 2, n - 1`.
- SAMPLED at the maximum, against the digest and the closed forms: `n = 2^24` with `k = 1, 2, 3, 7, n - 1, n, n + 1, 2^31, 2^32 - 1`, and `n = 2^24 - 1` with `k = 3, 7, n - 1, n, n + 1, 2^32 - 1`; steps `0, 1, 2, n / 2, n - 2, n - 1` (the last is the survivor, and the survivor call is compared with it). Closed forms: step 0, `k = 1` (the step itself, survivor `n - 1`), `k = 2` (survivor).
- Failure matrix: `n` in `0, 1, 2, 5, 2^24, 2^24 + 1, UINT32_MAX`, `k` in `0, 1, 3, UINT32_MAX`, `step` in `0, 1, 5, 2^24 - 1, 2^24, UINT32_MAX`, with and without `out`, both functions, against a restatement of the check order; success values are compared with the vector oracle for `n <= 5` (larger successes are in the groups above). Every failure leaves the sentinel. Named precedences by hand (`n = 2^24 + 1` with `k = 0` is BadArgument, with `step = UINT32_MAX` TooLarge; `n = 1`: step 0 gives 0, step 1 is OutOfRange).
- The large cases carry the Catch2 tag `[big]` (the filter `~[big]` runs the rest).

The whole removal order is not checked above `n = 65537`: at `n = 2^20` a whole order is `5.5 * 10^11` modulos (about an hour per `k`), so `2^20` and the maximum are sampled. The Python reference at the maximum costs 16 million iterations per sampled step near `n - 1`.

## Mutation testing

97 real source mutants of `core/josephus/josephus.cpp` (one text change each: the lift (widening, reduction, modulus, additions of 1, `k` shifted, multiply, a mask, a truncated sum, equivalent rewrites), every check and its code and position, the loop starts, ends and strides of both functions, the initial values, the first label `(k - 1) mod m0`, `m0`, the lift arguments, `step >= n` and its variants, the written value, a write on failure), each run on the FINAL test file in two builds, serially, one mutant at a time: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. The test object is the same plain object in both builds: it is NOT sanitizer-instrumented; only the mutated source is. Staging (allowed by the maintainer, on the SAME unedited final test file): the first pass ran the suite with the Catch2 filter `~[big]` (everything except the tag `[big]`: the whole-order cases n = 65535, 65536, 65537, and the groups fen_l, max_a, max_b are omitted); every mutant that survived it was then run against the FULL suite in both builds before any survivor claim.
- First pass, plain: 90 killed, 7 survived, 0 timed out. Sanitizer: 90 killed, 7 survived, 0 timed out, the same sets.
- Full suite on the 7 survivors, both builds: 1 more killed (survivor with `TooLarge` at `n >= kMaxN`: only the `n = 2^24` group calls the survivor at the maximum), 6 survived.
- Total: 91 killed, 6 survived, 0 timed out, in both builds (the same sets). No mutant is killed only by the sanitizer build. A mutant that does not terminate is classified TIMEOUT and is never counted as a kill (none occurred). Catch2 runs the test cases in a random order, so which case fails first differs between runs.

Crash-only kills (the first-pass run shows no failing assertion): 7 mutants (survivor and removed with the null check dropped; survivor and removed with `TooLarge` dropped; removed with `step > n`; removed with `step >= n` dropped; removed with `m0 = n - step - 1`), each re-run alone in both builds with the same `~[big]` filter. Plain, alone: SIGSEGV for the two dropped null checks (rc 139, `FAILED: SIGSEGV - Segmentation violation signal`); rc 136 (a fatal error, signal 8: modulo by zero) for the five others. Sanitizer build, alone: UBSan `store to null pointer` for the two dropped null checks; UBSan `division by zero` for the five others.

Survivors (6), all equivalent:
- `k` reduced first in the lift, `(idx + k % m) % m` (and the same in `u32`): `(a + b % m) % m = (a + b) % m`; in `u32` the sum is `idx + k % m < (m - 1) + m <= 2^25`, no wrap.
- `idx + k + m` before the modulo: adds a multiple of `m`; the sum stays below `2^33 + 2^24`, no `u64` wrap.
- `idx % m` before adding: `idx < m` always holds (see the bounds), so the reduction does nothing.
- survivor loop starting at `m = 1`: `lift(0, k, 1) = k mod 1 = 0`, the initial value.
- survivor with an early exit for `n = 1` writing 0: the loop does not run for `n = 1` and the initial value is 0.

## Not run

Windows (CI only); any timing assertion; the whole removal order above `n = 65537` and a Fenwick oracle at `2^24`; other `n` at `2^24` than `2^24` and `2^24 - 1`; no test of the survivor for every `n` up to `2^24`. The mutant test object is not sanitizer instrumented. The first mutation pass omitted the `[big]` cases (see above). Per-case runtimes of the unmutated suite (13 cases, this machine, `-O0`):

| case | plain | `-fsanitize=address,undefined` (test and source instrumented) |
|---|---|---|
| whole order `n = 65537` | 120.4 s | 186.8 s |
| whole order `n = 65536` | 119.8 s | 187.1 s |
| whole order `n = 65535` | 120.1 s | 186.7 s |
| sampled `n = 2^20 - 1 .. 2^20 + 1` | 10.3 s | 15.5 s |
| `n = 2^24` | 3.8 s | 5.9 s |
| `n = 2^24 - 1` | 2.5 s | 4.0 s |
| permutation property | 1.3 s | 1.7 s |
| whole order `n = 1000, 4096, 4097` | 1.0 s | 1.5 s |
| whole order `n = 1 .. 100`, `k = 1 .. 300` | 0.7 s | 1.1 s |
| all others | under 0.1 s | under 0.1 s |

Three cases are over 30 s (the three whole-order cases): the library alone needs about 12 s per `k` there (measured on its own: 11.9 s plain, 12.3 s sanitized), the rest of the sanitized time is the instrumented Fenwick oracle. The whole suite takes about 6.7 minutes plain and about 10 minutes sanitized. `python3 tools/josephus_ref.py --check tests/josephus/josephus_tests.cpp` takes about 50 s here (`NAME...` checks only the named groups).
