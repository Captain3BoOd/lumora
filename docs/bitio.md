# bitio

`core/bitio/bitio.h` and `bitio.cpp` (`lumora::bitio`) read and write LSB-first bit fields in a caller buffer: `write_bits`, `read_bits`, `align_to_byte`, `bits_left`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no libm, no 128-bit types, no wide character types, no allocation, no global state. The source includes `<cstdint>` only; the tests add `<catch2/...>`, `<cstddef>`, `<cstdint>` and `<vector>` as used. No thread tests.

## Layout

Bit position `p` is bit `p % 8` of byte `p / 8` (bit 0 least significant). A field of `n` bits at `p` holds value bit 0 at `p`, bit 1 at `p + 1`, and so on. A field is moved one byte at a time, so only the bytes that overlap it are read or written, and the buffer is never accessed past byte `cap - 1`. A write replaces the field bits and keeps every other bit, also in the touched bytes.

## Check order

The header says every failure changes nothing and never touches the buffer. The order below is an implementation choice, not part of the contract.

- `write_bits`: null `buf` or `pos`; invalid span (`cap > kMaxBytes` or `*pos > 8 * cap`); `nbits > 32` or `*pos + nbits > 8 * cap` (64-bit sum). Then the field is written and `*pos` advanced. `nbits` 0 succeeds and changes nothing.
- `read_bits`: null `buf`, `pos` or `out`; the same span and field checks. `*out` and `*pos` change only on success. `nbits` 0 gives `*out = 0`.
- `align_to_byte`: null `pos` or invalid span: false. Otherwise `*pos` is rounded up to a multiple of 8 (never above `8 * cap`).
- `bits_left`: null `pos` or invalid span: 0. Otherwise `8 * cap - *pos`.

## Bounds

| Item | Bound |
| --- | --- |
| `cap` | 0 to `kMaxBytes` (2^28); above is invalid |
| `*pos` | 0 to `8 * cap` (at most 2^31) |
| `nbits` | 0 to 32 |
| field end | `*pos + nbits <= 8 * cap`, exactly equal succeeds |
| `value` bits above `nbits` | ignored |
| overlap of `buf` with `pos` or `out` | not supported, not tested |

## Tests

`tools/bitio_ref.py` is an independent Python bit-array model (one list entry per bit, no byte logic shared with the C++). It prints a generated block with case counts and FNV-1a 64 digests that the tests recompute.

```
python3 tools/bitio_ref.py                                           # print the block
python3 tools/bitio_ref.py --check tests/bitio/bitio_tests.cpp
```

`tests/bitio/bitio_tests.cpp` has 15 test cases and 1,393 assertions (sweeps assert once per sweep). Sweeps use a naive in-test oracle that shares no code with the implementation:

- table: 98,301 cases (caps 0, 1, 2, 5, 9, three backgrounds, every position, `nbits` 0 to 34 and `0xFFFFFFFF`, several values, reads, align, `bits_left`), compared with the oracle and the reference digest;
- limit: 2,236 cases with huge caps and a 16-byte dummy buffer that must not be touched;
- random: 100 scripted sequences of 40 steps (4,000 steps), LCG seed 81.

No allocation of 2^28 bytes is made. Near the top of the range the tests use small buffers: at cap 2^28, a 32-bit field ending one bit past 2^31, a position of 2^31 + 1 and cap 2^28 + 1 are rejected with a buffer that must not be touched; `bits_left`, `align_to_byte` and the zero-bit operations are tested at the maximum position (they need no buffer); successful field operations at cap 2^28 are tested only at small positions with a buffer that covers just the field bytes; exact-end successes are tested at small caps.

## Mutation check

127 hand-written source mutants of `bitio.cpp` (limits and comparisons, the `&&`/`||` of each check, null checks, the field check, offsets, room, take, masks, byte index, shifts, position and counter updates, `bits_left`, `align_to_byte`) were compiled against the unchanged test object. Plain build: 120 killed, 7 survived. With `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented): 120 killed, the same 7 survive.

Plain build: 92 kills by a failing assertion, 25 by a crash alone and 3 by a timeout (mutants 37, 38, 80, which loop forever). The crash-only kills were run individually: SIGSEGV with the Catch2 message (17: 1, 8, 13, 25, 26, 29, 30, 68 to 71, 73, 74, 98, 99, 106, 118), SIGSEGV with no message (3: 62, 63, 64), SIGABRT with no message (5: 19, 28, 32, 33, 42).

Sanitizer build: 74 kills by a failing assertion, 3 timeouts (the same), 43 by a sanitizer report, run individually: shift exponent 32 too large (9: 1, 12, 40, 41, 72, 82, 88, 95, 98), load or store of a null pointer (10: 25, 26, 29, 68, 69, 70, 73, 74, 106, 118), heap-buffer-overflow (17: 13, 19, 28, 32, 33, 42, 55, 57, 62, 63, 64, 76, 81, 92, 93, 97, 99), SEGV on an unknown address (7: 3, 8, 15, 18, 30, 31, 71). The kills that are crashes show the test would also catch a buffer or null access by the implementation, but they are not assertion failures.

Survivors, each equivalent by argument:

- 16, `fits` with a 32-bit sum: `valid_span` is checked first, so `*pos <= 2^31`, and `nbits <= 32` is checked before the sum (`&&`). The sum is at most `2^31 + 32 < 2^32`, so it does not wrap and the comparison gives the same result.
- 17, `fits` with `8u * cap`: reached only after `valid_span`, so `cap <= 2^28` and `8u * cap <= 2^31` does not wrap.
- 126, `bits_left` through 64 bits: after `valid_span`, `8 * cap - *pos` is between 0 and 2^31, so the value is the same.
- 49 and 50, write `|` replaced by `^` or `+`: `old & ~mask` has no bit inside `mask`, and `(value << offset) & mask` has only bits inside `mask`. The two operands share no bit, so `|`, `^` and `+` give the same result (no carry).
- 89 and 90, read `|=` replaced by `^=` or `+=`: each term is below `2^take` shifted by `got`, so it occupies bits `got .. got + take - 1`, and the earlier terms are below `2^got`. The operands share no bit and `got + take <= nbits <= 32`, so there is no carry and no loss.

Not run: a successful nonzero field ending exactly at 2^31 bits (cap 2^28) was NOT run, since it needs a 256 MiB buffer. Windows runs only in CI. Aliasing of `buf` with `pos` or `out` is untested. No thread tests.
