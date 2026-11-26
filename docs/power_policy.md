# Power policy

`core/lifecycle/power_policy.h` and `power_policy.cpp` (`lumora::lifecycle`) turn the battery-saver flag and the render-scale notch of `pacing.h` into a quality preset, a render-scale percentage and scaled pixel sizes (`docs/android-lifecycle.md` section 10.1 items 2 and 3). The frame cap side of battery saver stays in `pacing.h`.

Evidence label: unit tests only (CI-verified).

Integer only: no clock reads, no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation. The source includes `<cstdint>`; the tests add `<catch2/...>`, `<cstddef>` and `<string_view>`.

## Common rules

Valid `max_notch` is 1 to 15. In every function the checks run in this order: the output pointer is null, then the range (`max_notch` 1 to 15, or `percent` 1 to 100 for `scaled_size`). A failure returns false and writes nothing. A `notch` above `max_notch` is clamped to `max_notch` first and never fails, even `UINT32_MAX`.

## render_scale_percent

`100 - (50 * n) / max_notch` with integer division, where `n = min(notch, max_notch)`. Notch 0 gives 100 and `max_notch` gives 50. `max_notch` 3 gives 100, 84, 67, 50 (50 * 1 / 3 = 16, so 84, not the rounded 83). `max_notch` 15 gives 100, 97, 94, 90, 87, 84, 80, 77, 74, 70, 67, 64, 60, 57, 54, 50. The product is at most 750, so 32 bits suffice. There is no battery-saver input.

## quality_preset

`effective = max_notch` when `battery_saver` is true (the supplied notch is ignored, even an oversized one), else `min(notch, max_notch)`. Then, in this order:

1. `effective == 0`: High.
2. `effective * 2 >= max_notch` (64-bit): Low.
3. Otherwise Medium.

Examples: `max_notch` 1: notch 0 High, notch 1 Low. `max_notch` 2: 0 High, 1 and 2 Low. `max_notch` 3: 0 High, 1 Medium, 2 and 3 Low. `max_notch` 15: 0 High, 1 to 7 Medium, 8 to 15 Low. With battery saver the result is always Low.

## scaled_size

Returns false and writes nothing when `out` is null or `percent` is 0 or above 100. Otherwise `*out = max(1, floor(size * percent / 100))`, the product in 64-bit (at most `UINT32_MAX * 100`, which fits). Size 0 gives 1. `UINT32_MAX` at 100 percent stays `UINT32_MAX`; at 99 it gives 4252017622; at 1 it gives 42949672.

## to_string

Static strings, never null: `"High"`, `"Medium"`, `"Low"`; any other value gives lowercase `"unknown"`.

## Bounds

| Item | Bound |
| --- | --- |
| `max_notch` | 1 to 15, else false |
| `notch` | any 32-bit value, clamped |
| `percent` | 1 to 100, else false |
| `size` | any 32-bit value |
| render scale | 50 to 100 |
| scaled size | 1 to `UINT32_MAX` |

## Tests

`tools/power_policy_ref.py` is an independent reference in Python (exact rationals for the render scale and the Low threshold, unbounded integers for the scaling). It prints a generated block with the grids and FNV-1a 64 digests (return value and output after every call, with a sentinel for an untouched output) that the test recomputes.

```
python3 tools/power_policy_ref.py                                          # print the block
python3 tools/power_policy_ref.py --check tests/power_policy/power_policy_tests.cpp
```

`tests/power_policy/power_policy_tests.cpp` has 13 test cases and 316 assertions:

- written cases: the header examples, clamping (including `UINT32_MAX`), rejection of `max_notch` 0, 16, 255 and `UINT32_MAX`, battery saver for every `max_notch`, the quality thresholds for `max_notch` 1, 2, 3, 4 and 15, `scaled_size` at 0, 1, 99, 100, 199, 200, the 1920 and 1081 cases, 50 percent of 2 and 3, and `UINT32_MAX`, `UINT32_MAX - 1` and 2^31 at several percents, bad percents 0, 101, 102, 255 and `UINT32_MAX` with the output untouched, null outputs, and `to_string` including 3, 4 and 255;
- exhaustive: every `max_notch` 1 to 15 with notches 0 to 20 for the render scale (also checked non-increasing and within 50 to 100) and for the quality preset with and without battery saver, against an in-test naive oracle (repeated subtraction instead of division, a split `(size / 100) * p + ((size % 100) * p) / 100` for the scaling, `ceil(max / 2)` for the Low threshold) that shares no code with the implementation;
- sweep: `scaled_size` for every percent 1 to 100 with sizes 0 to 1000, plus 201 sizes at the top of the 32-bit range and 201 around 2^31;
- digests: 520 render cases (20 `max_notch` values x 26 notches), 1,040 quality cases and 33,285 scaled cases (317 sizes x 105 percents), recomputed from the C++ functions.

## Mutation check

63 hand-written source mutants of `power_policy.cpp` (the range limit and each comparison, the clamp, null checks, the scale constants and rounding, the saver handling, the High, Low and Medium conditions, the percent range, the 64-bit product, rounding of the scaling, the minimum of 1, and every string) were compiled against the unchanged tests. 61 were killed by a plain build and 2 survived. Five kills are crashes only (a null check removed, the range check removed so `max_notch` 0 divides by zero).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented, the test object unchanged), the same 61 are killed and the same 2 survive. No extra kills: none of the mutants was hidden from the plain build. The sanitizers explain the crash-only kills with observed messages:

- `max_notch` range check removed, or `||` instead of `&&` in it: `division by zero`.
- null `percent_out` check removed or null `scaled_size` out check removed: `store to null pointer of type 'uint32_t'`.
- null `Quality` out check removed: `store to null pointer of type 'Quality'`.

The unmutated source passes all tests under the same flags.

Survivors in both builds, each argued:

- clamp with `>=` instead of `>`: at `notch == max_notch` both return `max_notch`. Equivalent.
- clamp removed in `quality_preset`: the result depends only on `effective == 0` and `effective * 2 >= max_notch` (64-bit, no overflow). For `notch > max_notch` both the raw and the clamped value are positive and satisfy the second test. Equivalent. (The same removal in `render_scale_percent` is killed.)

Not run: Windows (CI only), Android, the editor, `tools/ci/test.py`, fuzzing, a full local build.
