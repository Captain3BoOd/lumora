# decfmt

`core/decfmt/decfmt.h` and `decfmt.cpp` (`lumora::decfmt`) are decimal text for 64-bit integers: `format_u64`, `format_i64` (into a caller buffer) and strict `parse_u64`, `parse_i64` (from bytes).

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no libm, no printf or strtol family, no 128-bit types, no compiler builtins, no allocation, no recursion, no global state. The source includes `<cstdint>` only.

## Format

The text is the usual decimal form: digits with no leading zeros (0 is the single digit "0") and a leading `-` for a negative `i64` only (no "-0", no `+`). No terminator is written. The text is at most 20 bytes (`-9223372036854775808` and `18446744073709551615` are both 20). `*out_len` is the text length. `format_i64` takes the magnitude with unsigned arithmetic (`0 - uint64(value)`), so `INT64_MIN` has no signed overflow.

Check order: a null pointer (`dst` only while `dst_cap > 0`, `out_len` always) gives NullArgument, then BufferTooSmall when `dst_cap` is below the length. The text is at least 1 byte, so capacity 0 is always BufferTooSmall and `dst` may be null there. A null `out_len` is NullArgument even with capacity 0.

Success writes only `dst[0 .. length)`: every byte at or above the length stays unchanged, inside the capacity and at or above it (no terminator, no padding; maintainer answer relayed via Instinct). On BufferTooSmall no byte of `dst` is written. `*out_len` after ANY format failure (BufferTooSmall or NullArgument) is UNSPECIFIED (maintainer answer): the tests never assert it, and the implementation happens to leave it untouched.

## Parse

`parse_u64` accepts one or more ASCII digits and nothing else. `parse_i64` accepts an optional single `-` and then one or more digits. No `+`, no spaces, no separators, no terminator; a NUL byte is a bad byte, not an end. A byte is a digit only if it is 0x30 to 0x39 (compared as `unsigned char`, so 0x80 to 0xFF are bad). Leading zeros are allowed and have no effect, so only the value decides overflow: u64 0 to 2^64 - 1, i64 -2^63 to 2^63 - 1 (`-0` is 0, a lone `-` is Invalid, `-9223372036854775808` is valid).

Check order: a null pointer (`src` only while `n > 0`, `out` always) gives NullArgument (so `n == 0` with a null `src` is not a null argument, it is Invalid for an empty input); then `n > kMaxParseLen` (2^20) gives TooLarge (Null beats TooLarge, TooLarge beats Invalid; the claimed length is never read); then the whole input is checked for form (empty, a bad byte anywhere, a missing digit: Invalid); only then does a value out of range give Overflow. So Invalid wins over Overflow wherever the bad byte is. On any failure `*out` is untouched, Overflow included.

Implementation: a first pass over the whole input decides the form; a second pass accumulates the magnitude in a `uint64_t` and tests `v > (limit - d) / 10` before each `v * 10 + d`, with the limit 2^64 - 1 (u64), 2^63 - 1 (i64) or 2^63 (i64 after `-`).

| Item | Bound |
| --- | --- |
| text length | 1 to 20 bytes |
| `dst_cap` | any `uint32_t` |
| parse length `n` | 0 to `kMaxParseLen` (1,048,576); above it TooLarge |
| u64 range | 0 to 18446744073709551615 |
| i64 range | -9223372036854775808 to 9223372036854775807 |

## Tests

`tools/decfmt_ref.py` is an independent reference in Python with Python integers as the oracle (format is `str(int)`, parse is a regular-expression form check followed by `int()` and a range check). It prints a block of value tables and digests that the tests recompute:

```
python3 tools/decfmt_ref.py                                           # print the block
python3 tools/decfmt_ref.py --check tests/decfmt/decfmt_tests.cpp
```

The tests also carry a naive oracle that shares no code with the implementation: format by repeated subtraction of powers of ten (no division or remainder), parse by a hand-written form check and a decimal-string comparison against the limit (length first, then lexicographic) before any value is computed.

`tests/decfmt/decfmt_tests.cpp` has 11 test cases and 245 assertions (sweeps count mismatches and assert once per sweep, plus the case count, the operation count and the digest). Parse inputs are heap buffers of exactly the stated length, so a read past the end shows up under AddressSanitizer. Format outputs are canary-filled buffers.

| Scenario | Cases | Operations | Against |
| --- | --- | --- | --- |
| format: boundary values (0, 1, 9, 10, 11, 99, 100, 101, 10^k and its neighbours, 2^k and its neighbours, the limits) and 120 pseudo-random values, for u64 and for i64 (both signs), each with every capacity from 0 to length + 1 and 3 canaries after the capacity | 1,067 | 13,393 | oracle, reference digest |
| parse: every byte string of length 0 to 4 over 12 symbols ('0', '1', '5', '9', '-', '+', space, 'x', NUL, 0x80, 0xFF, '.'), through both parsers | 22,621 | 45,242 | oracle, reference digest |
| parse: 15 decimal strings around 2^63, 2^64 and 10^19, each with 5 prefixes (none, '-', '+', '--', 'x'), 4 zero runs (0, 1, 5, 30) and 4 tails (none, 'x' at the end, 'x' in the middle, ' ' at the end), through both parsers | 1,200 | 2,400 | oracle, reference digest |
| parse: 400 seeds, a string of 1 to 30 symbols, mostly digits, an optional '-' | 400 | 800 | oracle, reference digest |

The first three rows are exhaustive over their stated sets. The pseudo-random parts (the 120 values and the 400 seeds, fixed 64-bit LCG) are NOT exhaustive. Further cases: format then parse gives the value back for every table value; hand-checked texts and values; null rules and check order for all four functions (Null beats TooLarge and BufferTooSmall, TooLarge beats Invalid, `n == 0` with a null `src` is Invalid, capacity 0 with a null `dst` is BufferTooSmall, a capacity of 0xFFFFFFFF is accepted and nothing past the text is written); and one real 1 MiB buffer at exactly `kMaxParseLen`: all zeros, zeros and a final digit, `-` and zeros and a final digit, a bad byte at the end, a huge value (Overflow, and Invalid when a bad byte is at the end), and the largest u64 behind about a million zeros. `n = kMaxParseLen + 1` uses a 1-byte buffer (nothing is read) and gives TooLarge.

## Mutation check

138 hand-written source mutants of `decfmt.cpp` (the limits, the digit count, every digit write and the sign, the format length, capacity test and null rules and their order, the INT64_MIN negation, the parse TooLarge test and order, sign handling, the empty and lone-sign test, the digit range bounds, the signed-char reading of a byte, the limit choice, the overflow test and the accumulation, the out writes on failure, and every null rule). Each mutant replaces one unique text of `decfmt.cpp` and is compiled against the prebuilt test object, then the whole test binary is run with a 25 second timeout.

Plain build: 120 kills by a failing assertion, 17 by a crash alone, 1 survivor, no timeouts. Sanitizer build (`-fsanitize=address,undefined -fno-sanitize-recover=all`): 118 kills by a failing assertion, 19 by a sanitizer report or crash, 1 survivor, no timeouts. The crash-only kills were run individually. Plain: SIGSEGV for all 17 (mutants 6, 9, 11, 13, 15, 16, 21, 23, 35, 36, 37, 67, 114, 115, 116, 125, 126), no message printed. Sanitizer: ASan SEGV on an unknown address (6, 9, 11, 13, 15, 16, 21, 23), UBSan store to a null pointer (35, 36, 37, 43, 44, 116, 126) and UBSan load of a null pointer (67, 114, 115, 125).

Survivor (1), argued: parse form with a `signed char` instead of an `unsigned char` (mutant 86). The byte is promoted to `int` before it is compared with 48 and 57: for 0x00 to 0x7F the value is the same as with `unsigned char`; for 0x80 to 0xFF the `signed char` value is negative, so `c < '0'` is true and the byte is rejected, exactly as 0x80 to 0xFF is rejected by `c > '9'` with `unsigned char`. The result is Invalid either way. Equivalent.

## Not run

Windows was not run locally. Nothing in the contract was skipped for cost: the largest allocation is the one 1 MiB parse buffer; `kMaxParseLen + 1` is tested with a 1-byte buffer. `*out_len` after a format failure is unspecified and was not asserted.
