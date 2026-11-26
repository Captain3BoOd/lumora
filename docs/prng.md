# PRNG

`core/prng/prng.h` and `prng.cpp` (`lumora::prng`) give a PCG32 generator (PCG-XSH-RR, 64-bit state, 32-bit output): seeding, the next output, a bounded draw and an O(64) jump-ahead.

Evidence label: unit tests only (CI-verified). PCG32 is not a cryptographic generator.

Integer only: no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation. The source includes `<cstdint>`; the tests add `<catch2/...>` and `<cstddef>`.

## State

`Pcg32 { std::uint64_t state; std::uint64_t inc; }`. A default object is `state = 0, inc = 1`. Seeding makes `inc` odd. A hand-built object may hold any values, including an even `inc`; it is used as stored, never repaired. No aliasing and no thread-safety promise is made or tested.

## Step

One step computes the output from the OLD state, then moves the state:

```
old   = state
state = old * 6364136223846793005 + inc            (mod 2^64)
x     = (uint32)(((old >> 18) ^ old) >> 27)
rot   = (uint32)(old >> 59)
out   = (x >> rot) | (x << ((0 - rot) & 31))       (uint32 arithmetic)
```

## Seeding

`pcg32_seed(rng, initstate, initseq)` returns false for a null `rng` and changes nothing. Otherwise it overwrites both fields: `state = 0`, `inc = (initseq << 1) | 1`, one step, `state += initstate`, one step, and returns true. The top bit of `initseq` is lost on purpose, so `initseq` and `initseq ^ 2^63` give the same generator. The old contents of `*rng` are not an input. `seed(42, 54)` gives `inc = 109` and the outputs 0xa15c02b7 0x7b47f409 0xba1d3330 0x83d2f293 0xbfa4784b 0xcbed606e.

## next

`pcg32_next(rng)` returns 0 for a null `rng`, otherwise one step and its output.

## bounded

`pcg32_bounded(rng, bound)`:

1. A null `rng` returns 0.
2. `bound == 0` returns 0 and does not advance.
3. `threshold = (0 - bound) % bound` in `uint32_t` arithmetic.
4. Draw up to `kMaxDraws = 64` outputs with the step. Stop at the first output `r >= threshold`.
5. Return `r % bound`, where `r` is the last output drawn.

When an output is accepted the result is the usual rejection-sampling result. Bound 1 returns 0 after consuming exactly one output. A bound that is a power of two has threshold 0 and takes the first output.

The cap is a bounded-bias fallback. If all 64 outputs are below the threshold, the function stops having consumed exactly 64 outputs and returns the 64th output `% bound`, which can be biased. So the result is unbiased only when the cap is not reached. No probability is claimed for reaching it with a seeded stream and it is not expected in practice. A forged generator can reach it. This keeps the function terminating for every state.

A forged generator with `state 0, inc 0` emits 0 forever and never moves. It reaches the cap for every bound with a positive threshold (3, 5, 6, 0x80000001) and returns 0, and it never reaches the cap for a bound with threshold 0 (2, 4, 0x80000000), which accepts the first output. Both leave the state at 0. Because that state is a fixed point, the consumed count (64 or 1) cannot be seen in the state there, only in the result being 0. The cap cases we know of use forged states. The exact consumed count is tested on forged cycle states (see Tests).

## advance

`pcg32_advance(rng, delta)` returns false for a null `rng`. Otherwise `state` becomes the state after `delta` calls of `next` (outputs discarded), found by repeated squaring of the affine map `s -> mult * s + inc` in at most 64 rounds, and it returns true. Delta 0 changes nothing. `inc` is never changed. Advances compose: `advance(a); advance(b)` equals `advance(a + b)` modulo 2^64. `advance(2^64 - 1)` steps a seeded stream (odd `inc`) back by one exactly. For a forged even `inc` it is only `delta` applications of the step.

## Bounds

| Item | Bound |
| --- | --- |
| `initstate`, `initseq` | any `uint64_t` (top bit of `initseq` ignored) |
| `bound` | any `uint32_t`; 0 returns 0 without a draw |
| outputs drawn by `bounded` | 1 to 64 |
| `delta` | any `uint64_t` |
| `state`, `inc` of a hand-built object | any values |

## Tests

`tools/prng_ref.py` is an independent reference in Python (unbounded integers, a 2x2 matrix power for the jump). It prints a generated block with the seed, bound and delta lists, FNV-1a 64 digests, and exact cases that the tests recompute. It builds the cap cases from forged states of cycle length 2^j (`inc = (1 - mult) * c`, `state = c + 2^(62 - j)`, j = 1 to 4), whose outputs all stay below the threshold, and re-verifies two fixed j = 5 cases found by the search below.

```
python3 tools/prng_ref.py                                          # print the block
python3 tools/prng_ref.py --check tests/prng/prng_tests.cpp
```

`tests/prng/prng_tests.cpp` has 16 test cases and 189 assertions. Written cases: defaults, the `seed(42, 54)` vector, seeding details (a hand-built `inc`, the lost top bit, odd `inc` for edge `initseq` values, reseeding a used generator), null handling for every function, `next` from forged states (even `inc` as stored, the old-state rule), bound 0 and bound 1, power-of-two bounds, advance versus repeated `next`, composition and reversal, and the vector after a jump of 3.

Exact-state cases, each checking the result and the state after the call (and `inc` unchanged):

- 7 cap cases on forged cycle states with bounds 0x80000001, 0x80000005 and 0x8000FFFF (cycle lengths 2, 4, 8, 16 and, for two cases found by an offline search, 32). After 64 draws the state is back at its start (64 is a multiple of every cycle length), so a cap of 63 or 65 ends in a different state or result;
- 10 cases on `state 0, inc 0` (positive thresholds reach the cap, thresholds of 0 accept the first output);
- 18 rejection runs of k rejections followed by an acceptance;
- 66 cases with an output equal to the threshold minus 1, the threshold, and the threshold plus 1.

Sweeps use a naive oracle that shares no code with the implementation (a 64-bit product from 32-bit limbs, a rotation one bit at a time, `(2^32 - bound) % bound`, a counted draw loop) and digests from the reference:

- seed and next: 9 seeds, 1000 outputs each, 9,000 outputs, with the state after each;
- bounded: 4 seeds, 14 bounds, 200 calls, 11,200 calls with the result and exact state after each;
- advance: 9 seeds, a seeded and a forged-even `inc`, 20 deltas (360 cases), plus 2,000 random cases.

## Search for a long cycle (caps 32, 48, 80, 128)

Question: is there a forged state whose bounded draw separates a cap of 32, 48, 80 or 128 from 64? The cycle length of any state is a power of two. Separating the cap needs a cycle that does not divide the difference from 64: at least 32 for caps 48 and 80, 64 for cap 32 and 128 for cap 128. Every output on the cycle must be below the threshold, so the cap path is taken. The largest threshold any bound can have is 0x7FFFFFFF (bound 0x80000001), so each output is below it with probability a little under 1/2.

Method: states `c + 2^(62 - j)` with `inc = (1 - mult) * c` have an exact cycle of length 2^j (`state - c` is multiplied by `mult` every step). Outputs depend only on state bits 27 to 63, so only those 37 bits of `c` matter. A C++ search over random `c` checked the 2^j outputs of each candidate against 0x7FFFFFFF (bound 0x80000001) and the return to the start state.

Result:

- j = 4 (cycle 16): found easily (285,000,000 candidates, 4,339 hits).
- j = 5 (cycle 32): 9,332,000,000 candidates, 2 hits (c = 432382947758619778 and c = 18302666269181243910). Both are in the tests. They kill caps 48 and 80.
- j = 6 and 7 (cycles 64 and 128): not searched. The expected count in this family is about 2^37 * 2^-64, far below 1, so no hit is expected. This is an argument about this family only, not a proof that no such state exists.

Caps 32 and 128 were therefore not killed, and they are not proved equivalent: no state with a long enough cycle and all-rejected outputs was found by this search.

## Mutation check

93 hand-written source mutants of `prng.cpp` (the multiplier, every shift and operator of the step, the rotation, the old-versus-new state rule, each seeding step and the `(initseq << 1) | 1` term, every null check, the bound 0 and bound 1 rules, the threshold, `>=`, the cap value, the final modulo, and every term, loop limit and the delta handling of `advance`) were compiled against the unchanged tests. 89 were killed by a plain build and 4 survived. Six kills (null checks removed, and a modulo by zero) lost the plain report to a crash (SIGSEGV or SIGFPE) and two (no cap, and a delta that is never shifted off) were killed by the 25 second timeout (the loop does not end).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented), 90 are killed and 3 survive; the extra kill is the unmasked rotate, which is undefined behavior only. Caps 48 and 80 are killed by the cap test in both builds. Run directly under the sanitizers, the crash-only kills report:

- null checks removed (4): `member access within null pointer` or `reference binding to null pointer`;
- bound 0 check removed, and modulo by `bound + 1`: `division by zero`;
- shift 58 for the rotation count, and the rotate `32 - rot`: `shift exponent 61` or `32 is too large for 32-bit type`.

The 3 sanitizer survivors:

- `r` initialised to 1 instead of 0: equivalent. The loop runs at least once, so `r` is always assigned before use.
- Cap 32 and cap 128 instead of 64: not killed and not proved equivalent: no state with a long enough cycle and all-rejected outputs was found by the documented search above.

The fourth plain survivor is `(32 - rot)` instead of `((0 - rot) & 31)` in the rotation: for `rot = 0` it is a shift by 32, undefined in C++ but the same result on x86 (shift count is taken modulo 32); the sanitizer run kills it.

## Not run

- Windows (MSVC) is covered by CI only; it was not run locally.
- No test for a hand-built `inc` that is even beyond the documented cases, no test of aliasing or threads.
- No statistical test of the output quality.
