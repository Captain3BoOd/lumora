# Ordered dither

`core/dither/dither.h` and `dither.cpp` (`lumora::dither`) give an ordered 8x8 Bayer threshold, a 16-bit to 8-bit quantizer that uses it, and a row helper.

Evidence label: unit tests only (CI-verified).

Integer only: no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation. The source includes `<cstdint>`; the tests add `<catch2/...>`, `<cstddef>` and `<vector>`.

## State

None. All three functions are pure except that `dither_row_u16` writes its destination.

## threshold

`threshold(x, y)` returns the 8x8 Bayer value (0 to 63) at `(x & 7, y & 7)`. With `d = (x ^ y) & 7` and `yy = y & 7`, the index is the bit interleave `d0 yy0 d1 yy1 d2 yy2` (most significant first). Only the low 3 bits of `x` and `y` matter, so `x` and `y` may be any `uint32_t`. Every value 0 to 63 appears exactly once per 8x8 tile. Rows 0 and 1 are `0 32 8 40 2 34 10 42` and `48 16 56 24 50 18 58 26`.

## quantize_u16

With `scaled = v * 255`, `q = scaled / 65535`, `rem = scaled % 65535` and `t = threshold(x, y)`, the result is `q + 1` when `rem * 128 >= (2t + 1) * 65535`, else `q`. All intermediate values fit `uint32_t`: `v * 255 <= 16,711,425`, `rem * 128 <= 8,355,840` and `(2t + 1) * 65535 <= 8,322,945`. 0 maps to 0 and 65535 to 255 at every pixel. The result is monotone in `v` at a fixed pixel and is `q` or `q + 1`. Over one 8x8 tile the sum of the results is `64 * q + (k + 1) / 2` with `k = rem * 128 / 65535`, for every `v`.

## dither_row_u16

`dither_row_u16(src, dst, count, x0, y)` writes `dst[i] = quantize_u16(src[i], x0 + i, y)` for `i = 0..count-1`. Checks, in this order; every failure writes nothing:

1. `count == 0`: returns true and touches nothing (the pointers are not looked at).
2. `count > kMaxRow` (16,777,216): returns false.
3. `src` or `dst` null: returns false.
4. Overlap of the byte ranges `[src, src + 2 * count)` and `[dst, dst + count)`, compared as `uintptr_t` and half-open: returns false. Touching ranges do not overlap. Identical addresses overlap. There is no alignment check on `src`.

Otherwise it returns true. `x0 + i` wraps modulo 2^32. Only the low 3 bits of `y` matter.

## Bounds

| Item | Bound |
| --- | --- |
| `x`, `y`, `x0` | any `uint32_t` (only low 3 bits of `x + i` and `y`) |
| `v` | 0 to 65535 |
| `threshold` result | 0 to 63 |
| `quantize_u16` result | 0 to 255 |
| `count` | 0 accepted, up to `kMaxRow` = 16,777,216 accepted, above rejected |
| `rem * 128` | at most 8,355,840 (fits `uint32_t`) |

## Tests

`tools/dither_ref.py` is an independent reference in Python (the Bayer matrix by recursive doubling, and the identity 65535 = 255 * 257 for the quantizer). It prints a generated block with grids and FNV-1a 64 digests that the tests recompute.

```
python3 tools/dither_ref.py                                          # print the block
python3 tools/dither_ref.py --check tests/dither/dither_tests.cpp
```

`tests/dither/dither_tests.cpp` has 15 test cases and 873 assertions. Written cases: constants, the header check values and one occurrence of each value per tile, repeat every 8 pixels including across the `uint32_t` wrap, endpoints and hand-computed examples (v = 1, 128, 256, 257, 514, 65534), an exhaustive match against the oracle for every `v` at all 64 tile positions with the bounds and monotone checks, the exact tile sum for every `v` (also on a tile shifted across the wrap), row basics with a wrapping `x0`, count 0 with null and overlapping pointers, every rejection with the buffer unchanged, an overlap table (destination offsets 0 to 40 bytes against a 8-byte source, touching ranges accepted, identical addresses rejected, one byte of overlap rejected on either side, nothing else written) and `count == kMaxRow` against the oracle (about 48 MB of buffers, run once) with `kMaxRow + 1` rejected.

The oracle shares no code with the implementation (Bayer table built by doubling at test time, `q = v / 257`, `r = 255 * (v % 257)`). Digests from the reference:

- threshold: 900 cases (extreme coordinates);
- golden: 4,194,304 cases (every `v` at all 64 tile positions);
- extreme: 1,035,000 cases (coordinates and values from the edge lists);
- row: 2,000 cases (4 source patterns, counts, `x0` and `y` lists, guard bytes checked).

## Mutation check

89 hand-written source mutants of `dither.cpp` (each bit term and shift of the threshold index, the use of `x` and `y`, masks, the scaling and divisors, `rem * 128`, the comparison and `2t + 1`, the round-up, the row check order, the null and `kMaxRow` checks, the overlap end computations and comparisons, the loop, `x0 + i`, `y`, and the return value) were compiled against the unchanged tests. 78 were killed by a plain build and 11 survived. Four kills (null checks removed or weakened) lost the plain report to a crash (SIGSEGV).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented), the same 78 are killed and the same 11 survive. Run directly under the sanitizers, the crash-only kills report:

- null check removed or `||` changed to `&&`: `load of null pointer of type 'const uint16_t'` (source) and `store to null pointer of type 'uint8_t'` (destination only);
- `kMaxRow` check removed, or `> kMaxRow + 1`: `AddressSanitizer: heap-buffer-overflow` in `dither_row_u16`;
- loop one iteration too long: `AddressSanitizer: heap-buffer-overflow`.

The 11 survivors are argued as equivalent:

- `d` or `yy` masked with 15 instead of 7 (2 mutants): the index uses only bits 0 to 2 of each.
- the index bits combined with `^` or `+` instead of `|` (2): the six terms occupy disjoint bits.
- `>=` changed to `>`: equality needs `2^7 * 255 * j = (2t + 1) * 65535`. The left side is even and the right side odd, so it never holds.
- comparison divisor 65536 instead of 65535, and `rem` replaced by `rem + 1` (2): checked over all 65,536 `v` and 64 thresholds with Python, no result differs.
- count 0 check placed after the `kMaxRow` check: 0 is never above `kMaxRow`.
- `kMaxRow` check placed after the null check: both return false and write nothing, so the order is not observable.
- an added alignment check on `src`: only observable with a misaligned `uint16_t*`, which is undefined behavior to read, so the tests do not make one.
- `x0 + i` computed in 64 bits and narrowed: the same value modulo 2^32.

## Not run

- Windows (MSVC) is covered by CI only; it was not built locally.
- No performance or hardware measurements.
- No test with a misaligned `src`, see the alignment survivor.
