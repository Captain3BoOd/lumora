# 16.16 fixed point

`core/fixed/fixed.h` and `fixed.cpp` (`lumora::fixed`) give 16.16 fixed-point arithmetic (`Q16 = int32_t`, value = raw / 65536): `from_int`, `to_int_floor`, `mul`, `div`, `lerp`, `sqrt`, and an exact integer square root `isqrt_u64`. Every function is defined for every input and none traps.

Evidence label: unit tests only (CI-verified).

Integer only: no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation. All intermediate values fit `int64_t` or `uint64_t`. The source includes `<cstdint>`; the tests add `<catch2/...>` and `<cstddef>`.

## State

None. All functions are pure.

## Rounding and saturation

"Round half away from zero": the exact result is rounded to the nearest integer and an exact half goes to the larger magnitude. "Saturate": clamp to `[kMin, kMax]`. `mul` and `div` compute the exact result, round it, then saturate, so a result that rounds to 2^31 is `kMax` and a result of exactly -2^31 is `kMin`. Rounding is done on the magnitude (`(magnitude + 32768) >> 16` for `mul`) and the sign is applied afterwards.

## Functions

| Function | Rule |
| --- | --- |
| `from_int(n)` | `n * 65536` in 64 bits, saturated. Exact for -32768 to 32767. |
| `to_int_floor(a)` | floor(`a` / 65536), computed by division of the magnitude (no shift of a negative value). `kMin` gives -32768, `kMax` gives 32767. |
| `mul(a, b)` | `a * b` in 64 bits (at most 2^62 in magnitude), rounded `/ 65536`, saturated. |
| `div(a, b)` | `b == 0`: `kMax` for `a > 0`, `kMin` for `a < 0`, 0 for `a == 0` (checked first). Otherwise `|a| * 65536 / |b|` rounded (the tie test is `2 * rem >= |b|`), sign is the product of the signs, saturated. `div(kMin, -1)` is `kMax`. |
| `lerp(a, b, t)` | `d = b - a` in 64 bits, product `d * t` (below 2^63 in magnitude), only the product term rounded `/ 65536`, then `a` is added in 64 bits and the sum saturated. The sum is not rounded again. `lerp(a, b, 0)` is `a` and `lerp(a, b, kOne)` is `b` exactly. `t` is not clamped. |
| `isqrt_u64(v)` | floor(sqrt(`v`)) for every `uint64_t`, by the bitwise restoring method. `UINT64_MAX` gives 4294967295. |
| `sqrt(a)` | 0 for `a <= 0`, else `isqrt_u64(uint64(a) << 16)`. The largest result (for `kMax`) is 11863283, so the cast to `Q16` never clamps. |

## Bounds

| Item | Bound |
| --- | --- |
| `a`, `b`, `t` | any `int32_t` |
| `mul` product | magnitude at most 2^62 |
| `div` numerator | magnitude at most 2^47 |
| `lerp` difference | -(2^32 - 1) to 2^32 - 1 |
| `lerp` product | magnitude at most 2^63 - 2^31 |
| `isqrt_u64` input | any `uint64_t`, result at most 4294967295 |
| `sqrt` result | at most 11863283 |

## Tests

`tools/fixed_ref.py` is an independent reference in Python (exact `Fraction` rounding, `math.isqrt`). It prints a generated block with the edge list, the square-root bases and FNV-1a 64 digests (each result as a 32-bit word) that the tests recompute.

```
python3 tools/fixed_ref.py                                          # print the block
python3 tools/fixed_ref.py --check tests/fixed/fixed_tests.cpp
```

`tests/fixed/fixed_tests.cpp` has 18 test cases. Written cases: constants, known values and ties for every function (half ulp positive and negative, just below the half, 1.5 and 2.5), saturation boundaries (`kMin * -kOne`, `kMin * kMin`, `-65535 * kMin`, `div(kMin, -1)`, `div` by zero for all three signs, `lerp` across the whole range and `t` outside 0 to `kOne`), `from_int` and `to_int_floor` round trip for all of -32768 to 32767, `to_int_floor` over -200000 to 200000 against the floor inequality, `isqrt_u64` around every square up to 70000^2 and for 20,000 random 64-bit values against `r * r <= v < (r + 1)^2`, `sqrt` monotone with its bracket property for 0 to 99999, and algebraic checks on the edge list (`mul(a, kOne) == a`, commutativity, `div(a, kOne) == a`, `lerp(a, b, 0) == a`, `lerp(a, b, kOne) == b`).

Sweeps use a naive oracle that shares no code with the implementation (sign and magnitude with a remainder test instead of add-and-shift, a binary search for the square root, floor by quotient and remainder) and digests from the reference:

- `mul`: 21,600 cases (40 x 40 edge pairs and 20,000 random pairs with random magnitude scale);
- `div`: 21,600 cases (same layout);
- `lerp`: 84,000 cases (40 x 40 x 40 edge triples and 20,000 random triples);
- `to_int_floor`: 420,041 cases (edge list, -200000 to 200000, random);
- `from_int`: 220,041 cases (-100000 to 100000, edge list, random);
- `sqrt`: 90,041 cases (edge list, 0 to 70000, random);
- `isqrt_u64`: 22,082 cases (0 to 2000, three values around each of 25 bases squared, the 2^62 and 2^63 neighbours, `UINT64_MAX`, random).

## Mutation check

102 hand-written source mutants of `fixed.cpp` (the magnitude helper, every saturation comparison, return value and sign in the helper, the `from_int` scale and clamps, the `to_int_floor` branches and divisors, the `mul` rounding constant, shift and sign, the `div` zero rules, shift, tie test and sign, the `lerp` difference, rounding, sign, sum and clamps, every step of the `isqrt_u64` loop, and the `sqrt` guard and shift) were compiled against the unchanged tests. 89 were killed by a plain build and 13 survived. Two kills (a magnitude off by one and `div` using the wrong denominator) lost the plain test report to a crash (SIGFPE); the failed assertions were seen when run directly.

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented), the same 89 are killed and the same 13 survive. Run directly under the sanitizers, four kills report only a crash:

- magnitude off by one, `div` using `|a|` as denominator, and the zero check changed to `b < 0`: `runtime error: division by zero`;
- `lerp` with the whole sum rounded: `signed integer overflow` in 64 bits.

The 13 survivors are argued as equivalent:

- `saturate`: `>=` to `>` for the negative clamp (magnitude 2^31 gives -2^31 either way), and `>` to `>=` or `> kMax - 1` for the positive clamp (magnitude `kMax` gives `kMax` either way).
- `from_int`: `v >= kMax` is never true for a multiple of 65536 that equals `kMax`, and `v <= kMin` gives `kMin` either way.
- `to_int_floor`: `v > 0` instead of `v >= 0` (0 goes to the other branch and still gives 0), and a right shift in place of the division for negative values (the shift is arithmetic in C++20 and later, so the value is the same).
- `mul` and `lerp`: sign test `p <= 0` instead of `p < 0`: a zero product has a zero rounded magnitude.
- `lerp`: `sum >= kMax` or `sum <= kMin` return the same value as the value they clamp.
- `isqrt_u64`: skipping the leading `while (bit > v)` loop only shifts a zero result.
- `sqrt`: `a < 0` instead of `a <= 0` (0 gives `isqrt_u64(0)` which is 0).

## Not run

- Windows (MSVC) is covered by CI only; it was not built locally.
- No performance measurements.
