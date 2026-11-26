# Fixed step

`core/timing/fixed_step.h` and `fixed_step.cpp` (`lumora::timing`) are the fixed-timestep accumulator of "Timing model" in `docs/android-lifecycle.md`. Simulation runs in fixed steps. The frame delta is clamped after a pause, the number of catch-up steps per frame is capped (no spiral of death), and rendering gets an interpolation factor between the last two simulation states.

Evidence label: unit tests only (CI-verified).

Integer arithmetic on nanoseconds only: no clock reads, no floating point, no libm, no 128-bit types (MSVC has none). The caller reads the clock and passes deltas.

## Types

| Type | Field | Meaning |
| --- | --- | --- |
| `StepConfig` | `step_ns` | one step, at least 1 (default 16666667) |
| | `max_steps` | most steps per `advance()`, at least 1 (default 5) |
| | `max_delta_ns` | larger frame deltas are clamped, any value including 0 (default 250000000) |
| `StepState` | `accumulator_ns` | time not yet simulated, below `step_ns` after every `advance()` |
| | `dropped_ns` | total time discarded by clamping and by the catch-up cap, saturating |
| `StepResult` | `steps` | steps to run now, 0 to `max_steps` |
| | `alpha_q16` | `floor(accumulator * 65536 / step_ns)`, 0 to 65535 |

`valid(config)` is true when `step_ns >= 1` and `max_steps >= 1`. `max_delta_ns` is not checked.

## advance

A null `state`, a null `out` or an invalid config returns false. `state` and `*out` are left untouched (`out` is not zeroed). Otherwise, in order:

1. `clamped = min(delta_ns, max_delta_ns)`. The excess `delta_ns - clamped` is added to `dropped_ns`, saturating at `UINT64_MAX`.
2. `accumulator = accumulator + clamped`, saturating at `UINT64_MAX`. A forged huge state cannot wrap. Time lost to this saturation is not counted in `dropped_ns`.
3. `wanted = accumulator / step_ns` (64-bit quotient). `steps = min(wanted, max_steps)`.
4. `accumulator -= steps * step_ns`. This cannot overflow because `steps * step_ns <= accumulator`.
5. If `wanted > max_steps` (64-bit compare, equality drops nothing), the catch-up cap was hit. The whole steps still in the accumulator are discarded: `remainder = accumulator % step_ns`, `dropped_ns` gets `accumulator - remainder` (saturating) and the accumulator becomes `remainder`.
6. `alpha_q16 = floor(accumulator * 65536 / step_ns)`.

When both caps trigger, the rule 1 excess is added to `dropped_ns` first and the rule 5 discard second, each saturating. `dropped_ns` stays at `UINT64_MAX` once it gets there. The accumulator is below `step_ns` afterwards even if the caller passed a larger value in. `reset_accumulator` sets the accumulator to 0, keeps `dropped_ns` and ignores a null state.

## Interpolation without 128-bit arithmetic

The accumulator is below `step_ns` when alpha is computed. The code keeps a remainder `r` (starting at the accumulator, always below `step_ns`) and runs 16 rounds. Each round doubles `r`: if `2r >= step_ns` it subtracts `step_ns` and sets the next quotient bit, otherwise it keeps `2r`. The test `2r >= step_ns` is written `r >= step_ns - r`, which cannot overflow even for `step_ns` near `UINT64_MAX`; the subtraction `r - (step_ns - r)` equals `2r - step_ns`. After 16 rounds the quotient is `floor(r0 * 2^16 / step_ns)`, below 65536 because `r0 < step_ns`.

## Bounds

| Item | Bound |
| --- | --- |
| `step_ns` | 1 to `UINT64_MAX` |
| `max_steps` | 1 to `UINT32_MAX`; `steps` fits in 32 bits because it is at most `max_steps` |
| accumulator | in: any 64-bit value; out: below `step_ns` |
| `steps * step_ns` | at most the accumulator, so no overflow |
| `alpha_q16` | 0 to 65535 |

## Tests

`tools/fixed_step_ref.py` is an independent reference in Python with unbounded integers. It uses an explicit `min(2**64 - 1, ...)` for saturation and the exact big-integer quotient for alpha. It prints a generated block with the sweep parameters and FNV-1a 64 digests, which the test recomputes from the C++ functions.

```
python3 tools/fixed_step_ref.py                                          # print the block
python3 tools/fixed_step_ref.py --check tests/fixed_step/fixed_step_tests.cpp
```

`tests/fixed_step/fixed_step_tests.cpp` compares exact values only:

- hand-computed frames: exact steps, `max_steps` boundary, clamp boundary, both caps, `max_delta_ns` 0, the default config at 60 Hz;
- extremes: `step_ns` 1 and `UINT64_MAX`, a forged accumulator of `UINT64_MAX`, saturation of the accumulator and of `dropped_ns`, `max_steps` `UINT32_MAX`, quotients above 2^32, alpha at 16-bit edges;
- null pointers, invalid configs, and that nothing is written on failure;
- an exhaustive small sweep, 1,240,000 cases (`step_ns` 1..8, `max_steps` 1..4, `max_delta_ns` 0..24, accumulator 0..24, two initial `dropped_ns` values including `UINT64_MAX - 2`, delta 0..30), checked against a step-by-step simulation that takes and discards steps one at a time, and against the digest;
- an extreme grid, 552,960 cases built from 16 values around 0, 2^16, 2^32, 2^63 and `UINT64_MAX`, checked against the digest and, for alpha, against a 128-bit comparison done with 32-bit limbs and binary search;
- 20,000 seeded random `(step_ns, accumulator)` pairs of every bit length for alpha, against the oracle and a digest, plus the first accumulator that reaches each of several alpha values for twelve step sizes.

## Mutation check

63 hand-written source mutants of `fixed_step.cpp` (saturation, clamp, wanted and steps, the cap test and discard, interpolation rounds and compare, validation, null rules, writes on failure, reset) were each compiled against the unchanged tests. 58 were killed and 5 survived. Five of the kills are crashes only (null checks removed), not failed assertions.

Survivors, each argued:

- saturating add `>=` instead of `>`: at `a == UINT64_MAX - b` the sum is exactly `UINT64_MAX`, the same value. Equivalent.
- clamp `<=` instead of `<`: at equality both branches give the same value. Equivalent.
- steps `<=` instead of `<`: at `wanted == max_steps` both give that value. Equivalent.
- cap test `>=` instead of `>`: at `wanted == max_steps` the accumulator is already below `step_ns`, so the remainder equals the accumulator and nothing is discarded. Equivalent.
- cap test always true: when `wanted <= max_steps` the accumulator is below `step_ns` after rule 4, so the discard is 0 and the accumulator is unchanged. The branch only matters when the cap was hit. Equivalent.

Not run: Windows (CI only), Android, Vulkan/GLES, the editor, `tools/ci/test.py`, fuzzing, a full local build.
