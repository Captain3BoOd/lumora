# Varint

`core/varint/varint.h` and `varint.cpp` (`lumora::varint`) give canonical LEB128 variable-length integers for 64-bit values, a strict decoder, the zigzag mapping, and signed wrappers, for compact binary streams (save state and telemetry records).

Evidence label: unit tests only (CI-verified).

Integer only: no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation. The source includes `<cstdint>`; the tests add `<catch2/...>`, `<cstddef>` and `<vector>`.

## State

None. All functions are pure except that `encode`, `decode` and the signed wrappers write through their pointers on success.

## Format

Seven value bits per byte, least significant group first. Bit 7 of a byte is set when another byte follows. The canonical (shortest) encoding is the only accepted one.

## encoded_size and encode

`encoded_size(v)` is 1 for 0 to 127, 2 for 128 to 16383, and so on: `2^(7k) - 1` takes `k` bytes and `2^(7k)` takes `k + 1`. Values with bit 63 set take 10 bytes.

`encode(v, out, capacity, written)` returns false, writes no byte and leaves `*written` as it was when `out` or `written` is null or `capacity < encoded_size(v)`. Otherwise it writes the canonical encoding, sets `*written` and returns true. Bytes beyond the encoding are never touched, whatever the capacity.

## decode

`decode(in, length, value, consumed)` reads at most `kMaxBytes` (10) bytes and never more than `length`. Bytes after the first complete encoding are not read. It returns false and leaves `*value` and `*consumed` as they were when:

- `in`, `value` or `consumed` is null;
- `length` is 0;
- the data ends while a continuation bit is still set (truncated), including ten bytes that all have the continuation bit even when `length` is exactly 10, and any longer encoding;
- the 10th byte is above 1 (bits beyond 64). A 10th byte of exactly 1 is accepted as bit 63;
- the last byte is 0 and the encoding has more than one byte (padding such as `0x80 0x00`). A zero payload byte in the middle of an encoding (`0x80 0x01` is 128) is canonical.

All failures are ordinary false returns, so which one fires first is not observable. On success it writes the value and the number of bytes used and returns true. A successful decode always re-encodes to the same bytes.

## Zigzag

`zigzag_encode(v)` is `2 * v` for `v >= 0` and `2 * (-(v + 1)) + 1` for `v < 0`, so 0, -1, 1, -2, 2 map to 0, 1, 2, 3, 4. `INT64_MIN` maps to `UINT64_MAX` and `INT64_MAX` to `UINT64_MAX - 1`. `zigzag_decode(u)` is `u / 2` for even `u` and `-(u / 2) - 1` for odd `u`, for every `uint64_t` (`UINT64_MAX` gives `INT64_MIN`). No shift of a negative value and no signed overflow.

## Signed wrappers

`encode_signed(v, ...)` is `encode(zigzag_encode(v), ...)` with the same failure rules. `decode_signed` decodes into temporaries and publishes `*value` and `*consumed` only after full success. A null `value` or `consumed` fails like a null in `decode`. On any failure both are unchanged.

## Bounds

| Item | Bound |
| --- | --- |
| encoding size | 1 to 10 bytes |
| `capacity`, `length` | any `uint32_t` (at most 10 bytes are looked at) |
| 10th byte | 0x01 only (0x00 is padding, above 1 is out of range) |
| `v` | any `uint64_t` / `int64_t` |
| aliasing of the buffers with `value`, `consumed`, `written` | not promised and not tested; callers pass distinct objects |

## Tests

`tools/varint_ref.py` is an independent reference in Python (7-bit groups from the binary string, a scan for the first byte without the continuation bit, then a classification of the bytes it found). It prints a generated block with the boundary lists and FNV-1a 64 digests that the tests recompute.

```
python3 tools/varint_ref.py                                          # print the block
python3 tools/varint_ref.py --check tests/varint/varint_tests.cpp
```

`tests/varint/varint_tests.cpp` has 16 test cases and 328 assertions. Written cases: constants, `encoded_size` at every 7-bit boundary, known encodings (0, 1, 127, 128, 300, 16383, 16384, `UINT64_MAX`, 2^63, 2^63 - 1) with the bytes after the encoding checked untouched, every encode failure with the buffer and `*written` unchanged and the exact-capacity successes, known decodes (padding in the middle, the 10-byte maximum, bytes after the encoding), 17 failing inputs (length 0, truncated, padding, complete only past `length`, 10th byte 0, 2 and 0x7F, ten continuation bytes, 11 bytes, a length of four billion) with sentinels unchanged, null pointers, a check that bytes after `length` and after the tenth byte are never read (exact-size heap buffers, which AddressSanitizer checks), round trip with every shorter length a truncation, zigzag known values and the bijection, and the signed wrappers (including every failure with both outputs unchanged).

Sweeps use a naive oracle that shares no code with the implementation (division and multiplication by 128, a scan for the end byte, complement for zigzag) and digests from the reference. Every accepted decode is also checked to re-encode to the same bytes:

- `encode`: 22,042 values (boundaries, 0 to 2000, 20,000 random with random bit length);
- `decode`: 66,210 inputs (all 256 one-byte inputs, all 65,536 two-byte inputs, 216 three-byte inputs from the alphabet 00 01 7F 80 81 FF, and long patterns of 9 to 12 bytes with three prefix bytes, six last bytes and three length values, and `80 01 05` at lengths 0 to 3);
- decode after encode: 22,042 encodings followed by other bytes;
- zigzag: 46,106 cases (boundaries, -2000 to 2000, random, and the decode of every value above);
- signed round trip: 24,064 cases.

## Mutation check

92 hand-written source mutants of `varint.cpp` (every comparison, constant and shift in `encoded_size`, the null and capacity checks and the group, continuation and `*written` rules of `encode`, the null, length, limit, tenth-byte, padding, shift, `*value` and `*consumed` rules and a read past `length` in `decode`, both zigzag formulas, and the null checks, temporaries and publishing order of the signed wrappers) were compiled against the unchanged tests. 86 were killed by a plain build and 6 survived. Ten kills (the null checks) lost the plain report to a crash (SIGSEGV).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented), the same 86 are killed and the same 6 survive. Run directly under the sanitizers, the crash-only kills report:

- null checks removed or weakened (10): `store to null pointer` (`uint8_t`, `uint32_t`, `uint64_t` or `int64_t`) or `load of null pointer of type 'const uint8_t'`;
- `limit` ignoring `length`, and a read one byte past `length`: `AddressSanitizer: heap-buffer-overflow` in `decode`;
- shift `8 * i` or `7 * (i + 1)`: `shift exponent 64` or `70 is too large for 64-bit type`;
- zigzag odd branch with `-2` or a dropped sign: `signed integer overflow`.

The 6 survivors are argued as equivalent:

- `encode` mask `0xFF` instead of `0x7F`: the continuation bit is or-ed in for every byte but the last, and the last byte is below 128.
- `decode` without the `length == 0` check: the limit is 0, the loop does not run, and it returns false.
- `decode` limit 11 or no limit of 10 (2): the loop can never pass index 9, because a 10th byte is either above 1 (false) or at most 1 (no continuation bit, so the loop ends).
- `result +=` instead of `|=`: the 7-bit groups occupy disjoint bits (the 10th group is at most 1 at bit 63).
- a padding check on any zero byte after the first: a zero byte has no continuation bit, so it is the last byte, the same as the original check.

## Not run

- Windows (MSVC) is covered by CI only; it was not built locally.
- No performance measurements.
- No test of overlap between the buffers and the `value`, `consumed` or `written` pointers (the header makes no promise).
