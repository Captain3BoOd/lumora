# Frame pacing and render scale

`core/lifecycle/pacing.h` and `pacing.cpp` (`lumora::lifecycle`) implement the frame interval and the render-scale notch controller of `docs/android-lifecycle.md` sections 10.1 and 10.4, including the thermal guard of 10.1 item 3. Two pure parts: the interval the loop paces to, and a controller that steps the render scale down after a sustained frame-time overrun or a platform throttle report.

Evidence label: unit tests only (CI-verified). The recovery rule (stepping back up after a run of fast frames) is provisional, as the header says.

Integer only: no clock reads, no floating point, no libm, no 128-bit types (MSVC has none).

## Frame interval

`cap_period_ns`: Fps30 33333333, Fps60 16666667, Fps90 11111111, Fps120 8333333 (one second divided by the rate, rounded half up). Auto and any value outside the enum give 0.

`frame_interval_ns(in, out)` returns false and leaves `*out` untouched when `out` is null, the cap is outside the enum, or `vsync_period_ns` is 0. The checks have no order that matters, since all three only fail. Then, in this order:

1. `desired` is `vsync` for Auto, else `cap_period_ns(cap)`.
2. With battery saver, `desired = max(desired, 33333333)`.
3. `desired = max(desired, vsync)`.
4. `q = desired / vsync`, `r = desired % vsync`, `tolerance = vsync / 20` (integer division), `k = q + (r > tolerance ? 1 : 0)`, result `k * vsync`.

Because step 4 rounds to the nearest multiple with a tolerance, the result can be slightly below `desired` (for example Fps120 with battery saver on an 8333333 ns panel gives 33333332, four periods). A remainder exactly equal to the tolerance rounds down. The result is never below one vsync period.

No overflow: `q >= 2` only when `desired` is one of the constants (below 2^26), so `k * vsync` is small. When `desired == vsync`, `q == 1` and `r == 0`. When a constant is above `vsync`, `vsync` is below 33333333, so the product is small.

Evidence: the header example (90 Hz panel, Fps60) gives 22222222.

## Scale controller

`valid(config)`: `1 <= max_notch <= 15`, `overrun_frames >= 1`, `recover_frames >= 1`, `100 <= overrun_percent <= 1000`, `10 <= fast_percent <= 100`.

Thresholds: `floor(budget * percent / 100)`, computed as `(budget / 100) * percent + (budget % 100) * percent / 100` with a check before the multiplication and the addition. The value saturates at `UINT64_MAX` only when the exact value does not fit. The fast threshold (at most 100 percent) never saturates. A saturated overrun threshold can never be exceeded, since `frame_ns > UINT64_MAX` is false. The two classes cannot coincide: overrun is strictly above the overrun threshold, fast is at or below the fast threshold, and the overrun threshold is at least the fast one.

### on_frame

Returns false and changes nothing (state and `*changed`) when `state` is null, the config is invalid or `budget_ns` is 0. Otherwise, in this order:

1. The caller's notch is remembered. A notch above `max_notch` is clamped to `max_notch`. The runs are not touched by the clamp.
2. `frame_ns > overrun threshold`: `overrun_run += 1` (saturating at `UINT32_MAX`), `fast_run = 0`. If `overrun_run >= overrun_frames` and `notch < max_notch`: `notch += 1`, `overrun_run = 0`. At `max_notch` the run keeps counting.
3. Else `frame_ns <= fast threshold`: `fast_run += 1` (saturating), `overrun_run = 0`. If `fast_run >= recover_frames` and `notch > 0`: `notch -= 1`, `fast_run = 0`. At notch 0 the run keeps counting.
4. Else both runs reset to 0.

On success `*changed` (if not null) is written: true when the final notch differs from the notch passed in. A clamp alone therefore reports true (notch 9 with `max_notch` 3, an in-between frame). A frame count limit of `UINT32_MAX` fires on reaching it, so a run already saturated at `UINT32_MAX` fires too.

### on_thermal_throttle

Returns false and changes nothing when `state` is null or the config is invalid. Otherwise: clamp to `max_notch`, `notch = min(notch + 1, max_notch)`, both runs reset on every call (even at `max_notch`). "Step down" means a higher notch number. `*changed` compares the final notch with the original, so an oversized notch clamped to `max_notch` reports true.

`to_string(FrameCap)` gives "Auto", "Fps30", "Fps60", "Fps90", "Fps120", and "unknown" for any other value (never null).

## Decisions taken from the header

These were confirmed by the module owner before coding: `*changed` and the clamp, the throttle step order, failure leaving state and `*changed` untouched, the exact saturating thresholds, and runs saturating at `UINT32_MAX` while counting at the edge notches.

## Tests

`tools/pacing_ref.py` is an independent reference in Python on unbounded integers. It uses a different formulation for the interval (the smallest `k` with `k * vsync + tolerance >= desired`) and `min(UINT64_MAX, budget * percent // 100)` for thresholds. It prints a generated block with the grids and FNV-1a 64 digests that the test recomputes from the C++ functions.

```
python3 tools/pacing_ref.py                                      # print the block
python3 tools/pacing_ref.py --check tests/pacing/pacing_tests.cpp
```

`tests/pacing/pacing_tests.cpp` compares exact values only:

- written cases: the defaults, every cap period and name, the interval examples (including vsync 1, `UINT64_MAX`, 144 Hz, battery saver), the tolerance boundary (remainder equal to and one above the tolerance), failures leaving `*out` untouched, every `valid()` bound, every failure of `on_frame` and `on_thermal_throttle` with both values of a `*changed` sentinel, threshold rounding, saturating thresholds at the exact edge, counters at `UINT32_MAX - 1`, `UINT32_MAX` and at the edge notches, notches above `max_notch` (including `UINT32_MAX`), and a long scripted run;
- sweeps checked against a naive oracle that shares no code with the implementation (32-bit limb arithmetic for thresholds, a step-by-step search for the vsync multiple, a plain state machine) and against digests from the reference: the interval for vsync 0..400 plus 27 extremes, 10 caps (5 valid, 5 not) and both saver settings (8,560 cases; the oracle walks the multiples in the 3,095 cases where that takes at most 200,000 steps, the digest covers the rest); `valid()` over a 6,272-point grid; `on_frame` over 6 configs, 23 budgets up to `UINT64_MAX`, frames at each threshold minus one, equal and plus one, 7 start notches and 5 values for each run (198,975 cases); `on_thermal_throttle` over the same state grid (1,050 cases); and every sequence of 7 symbols (overrun frame, in-between frame, fast frame, throttle) for 3 configs and 5 start notches (245,760 sequences, every step compared).

## Mutation check

108 hand-written source mutants of `pacing.cpp` (each `valid()` bound, both thresholds and their saturation, the saturating counters, every cap period, the tolerance rule and the three `max` steps of the interval, battery saver, null and failure handling, the clamp, each run reset and limit comparison, the step rules at both edge notches, `*changed`, the throttle rules and the `to_string` text) were each compiled against the unchanged tests. 105 were killed and 3 survived. Four of the kills are crashes only (a null check or a vsync 0 check removed): they die on a null dereference or a division by zero, not on a failed assertion.

Survivors, each argued:

- second saturation check `part >= MAX - whole` instead of `>`: the two differ only when `whole + part == UINT64_MAX`, and then saturating returns the same value. Equivalent.
- `q != 0` guard removed from the first saturation check: with `q == 0` the source divides by zero, which is undefined. GCC at -O0 turns the comparison into a multiplication overflow test, so no division happens and the result is the same. It is a real source defect that this compiler hides, and the tests cannot see it. The guard is kept for that reason.
- `return k != 0` instead of `return true` after the write: `k` is at least 1, so it is always true. Equivalent.

Not run: Windows (CI only), Android, the editor, `tools/ci/test.py`, fuzzing, a full local build.
