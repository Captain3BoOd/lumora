# Wallpaper offset filter

`core/input/offset_filter.h` and `offset_filter.cpp` (`lumora::input`) smooth one two-axis Q16 value (65536 is 1.0) with a dead zone, for the launcher offset and tilt that feed the parallax input (`docs/android-lifecycle.md` section 10.1).

Evidence label: unit tests only (CI-verified).

Integer only: no clock reads, no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation. The source includes `<cstdint>`; the tests add `<catch2/...>` and `<cstddef>`.

## State and config

`OffsetFilter` holds `x`, `y` (the smoothed value, within +/- 65536 once written by `step`) and `primed` (false until the first successful `step`). `OffsetFilterConfig` holds `smooth_percent` (0 to 99, default 80) and `deadzone` (0 to 32768, default 1966).

`valid(config)`: `smooth_percent <= 99` and `0 <= deadzone <= 32768`, both ends inclusive.

## reset

Returns false when `f` is null. Otherwise `x = 0`, `y = 0`, `primed = false`, returns true.

## step

Order of checks: `f` null, `out_x` null, `out_y` null, config invalid. Any failure returns false and writes nothing (not the filter, not the outputs, no repair). Otherwise, for each axis independently:

1. Clamp the input to `[-65536, 65536]`. `INT32_MIN` and `INT32_MAX` therefore act as -65536 and 65536.
2. Dead zone: `m = |v|`. If `m <= deadzone` the value is 0. Otherwise it is `sign(v) * ((m - deadzone) * 65536 / (65536 - deadzone))`, integer division of the 64-bit non-negative product (floor of the magnitude), so the sign is applied after the division and full scale stays full scale. Deadzone 0 gives `v` unchanged. Deadzone 32768 maps 32769 to 2 and 65536 to 65536.
3. Hand-built state repair: the filter's `x` and `y` are clamped to `[-65536, 65536]` (`primed` is kept as given). This happens on every successful step; on an unprimed filter it cannot be seen because the state is overwritten.
4. Smoothing: an unprimed filter takes the step 2 value on both axes, whatever `x` and `y` held. A primed filter gets `state += trunc((value - state) * (100 - smooth_percent) / 100)`, C++ division (toward zero), in 64-bit. At `smooth_percent` 99 the state stalls within 99 units of the target (for example 0 toward 99 stays at 0, 0 toward 100 moves to 1); at 0 it equals the value.

Then `primed = true` and `*out_x`, `*out_y` receive the new state. If `out_x` and `out_y` are the same pointer, `out_x` is written first and `out_y` last; the tests do not alias them.

## Bounds

| Item | Bound |
| --- | --- |
| input | any `int32_t`, clamped to +/- 65536 |
| `smooth_percent` | 0 to 99, else false |
| `deadzone` | 0 to 32768, else false |
| state on input | any `int32_t`; clamped to +/- 65536 before use |
| state and outputs | within +/- 65536 |
| products | at most 65536 * 65536 and 131072 * 100, in 64-bit |

## Tests

`tools/offset_filter_ref.py` is an independent reference in Python (exact fractions: the dead zone is a floor of the exact quotient, the smoothing a truncation toward zero of the exact quotient). It prints a generated block with the grids and FNV-1a 64 digests (return value, outputs and the whole filter after every call) that the test recomputes.

```
python3 tools/offset_filter_ref.py                                          # print the block
python3 tools/offset_filter_ref.py --check tests/offset_filter/offset_filter_tests.cpp
```

`tests/offset_filter/offset_filter_tests.cpp` has 16 test cases and 5,462 assertions:

- written cases: constants, defaults and `valid` over both bounds and 0 to 130 percent, `reset`, the check order with every null combination and failures that leave a hand-built out-of-range filter unrepaired, dead zone examples for deadzones 0, 1966 and 32768 (just inside, just outside, full scale, both signs), clamp edges at `INT32_MIN`, `INT32_MIN + 1`, `INT32_MAX - 1`, `INT32_MAX` and +/- 65537, axis independence, first sample pass-through over a junk state, truncation toward zero in both directions (half steps, 1 and 99 unit stalls), convergence to 65536 - 99 at smooth 99 over 5,000 steps, and repair of states 65537, 70000, `INT32_MAX` and `INT32_MIN`;
- sweeps against a naive oracle that shares no code with the implementation (binary search for each quotient instead of division): every deadzone 0 to 150, 32668 to 32768, 1946 to 1986 and three more, at inputs around the dead zone edge and the clamp edge with both signs, plus a full input sweep from -65600 to 65600 for selected deadzones; every smooth 0 to 99 with 12 states and 11 inputs;
- digests from the reference: 234 dead zone cases (9 deadzones x 26 inputs), 86,400 smoothing cases (100 percents x 3 deadzones x primed or not x 16 hand-built states x 9 inputs, states and inputs including `INT32_MIN` and `INT32_MAX`), 11 invalid config cases (nothing written, sentinel outputs untouched) and 4 scripted sequences of 500 steps (2,000 cases, resets every 97 steps, one in 11 inputs from the extreme list), the last two also checked against the oracle.

## Mutation check

78 hand-written source mutants of `offset_filter.cpp` (each clamp bound, the dead zone comparison, subtraction, divisor, rounding, 32-bit product and sign, the smoothing sign, constants, divisor and rounding, every `valid` comparison, `reset` field and return, each null check, the check order, which input and dead zone feeds which axis, the state repair, the primed test and the first sample, the smoothing of each axis, the primed write, the outputs and the return) were compiled against the unchanged tests. 73 were killed by a plain build and 5 survived. Four kills are crashes only (a null check removed).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented, the test object unchanged), the same 73 are killed and the same 5 survive; no mutant was hidden from the plain build. The sanitizers explain the crash-only kills with observed messages:

- null `f` check removed in `reset` or `step`: `member access within null pointer of type 'struct OffsetFilter'`.
- null `out_x` or `out_y` check removed: `store to null pointer of type 'int32_t'`.

The unmutated source passes all tests under the same flags.

Survivors in both builds, each argued:

- clamp `v >= 65536` instead of `>`: at `v == 65536` both return 65536. Equivalent.
- clamp `v <= -65536` instead of `<`: at `v == -65536` both return -65536. Equivalent.
- dead zone `m < deadzone` instead of `<=`: at `m == deadzone` the formula gives `0 * 65536 / (65536 - deadzone) = 0`, the same as the early return. Equivalent.
- dead zone `m <= deadzone - 1`: the same argument at `m == deadzone`. Equivalent.
- dead zone sign test `v <= 0` instead of `v < 0`: `v == 0` has `m == 0 <= deadzone` and returns before the sign is used. Equivalent by unreachability.

Not run: Windows (CI only), Android, the editor, `tools/ci/test.py`, fuzzing, a full local build.
