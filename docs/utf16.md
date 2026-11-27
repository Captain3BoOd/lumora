# utf16

`core/utf16/utf16.h` and `utf16.cpp` (`lumora::utf16`) are strict UTF-16 over host-order 16-bit units: `validate` with the offset of the first ill-formed sequence, `count_code_points`, and single code point `decode_one` and `encode_one`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no libm, no wide character types, no allocation, no global state. The source includes `<cstdint>` only. The functions have no mutable state, so there is nothing to share between threads and no thread tests were written.

## Well-formed sequences

As in the header: one unit outside `D800-DFFF`, or a high surrogate `D800-DBFF` followed by a low surrogate `DC00-DFFF` (a pair). A low surrogate that does not follow a high one, a high surrogate followed by anything else, and a high surrogate at the end of the text are malformed. U+0000, U+FEFF and the noncharacters (`FFFE`, `FFFF`) are valid. Units are host order; there is no byte order mark handling and no byte order tests.

## Check order

- `validate(s, n, bad_offset)`: `s` null with `n > 0`: NullArgument (also when `n` is above the maximum). Then `n > kMaxUnits` (2^27): TooLarge, nothing read. Then the text is scanned from the start. The first ill-formed sequence gives Malformed and `*bad_offset` is the index of its first unit (the high surrogate for a high surrogate with a wrong or missing second unit, the unit itself for a lone low surrogate). Ok sets `*bad_offset = n`; empty text is Ok (also with a null `s`) and sets it to 0. `bad_offset` may be null at every outcome. On NullArgument and TooLarge it is not written.
- `count_code_points(s, n, out)`: `out` null: NullArgument always. `s` null with `n > 0`: NullArgument. Then `n > kMaxUnits`: TooLarge. Then the scan; an ill-formed sequence gives Malformed. `*out` is written only on Ok and is untouched on any failure. Empty text gives 0.
- `decode_one(s, n, cp, len)`: `s`, `cp` or `len` null: NullArgument, before the `n == 0` check. Then `n == 0`: Malformed. Then at most `min(n, 2)` units are read: the second unit only when `n` holds it. An ill-formed or cut-off sequence is Malformed. Units after the first sequence are ignored. On Ok `*cp` and `*len` (1 or 2) are set; on any failure both are untouched.
- `encode_one(cp, dst, dst_cap, len)`: `dst` null with `dst_cap > 0`, or `len` null: NullArgument. Then `cp` above U+10FFFF or in D800-DFFF: Malformed. Then the needed length (1 up to FFFF, else 2) above `dst_cap`: BufferTooSmall (`dst` may be null with `dst_cap` 0; a valid value then never gives Ok). On Ok exactly `*len` units are written and not one unit more. On any failure `dst` and `*len` are untouched.

| Item | Bound |
| --- | --- |
| `validate` and `count_code_points` input | 0 to `kMaxUnits` (2^27) units |
| `kMaxCodePoint` | `0x10FFFF` |
| `decode_one` length | 1 or 2 |
| `dst_cap` | any `uint32_t`; 2 always suffices |
| aliasing of `cp` and `len` | not supported, not tested |

## Tests

`tools/utf16_ref.py` is an independent reference in Python, written from the header text with Python integers: surrogate ranges, shifts and masks, and the first unit of a sequence as the offset. The codecs module is not used for any value. It prints a generated block with 64-bit digests that the tests recompute, and 149 vectors with their offsets. It runs in about 6 seconds.

```
python3 tools/utf16_ref.py                                           # print the block
python3 tools/utf16_ref.py --check tests/utf16/utf16_tests.cpp
python3 tools/utf16_ref.py --codecs                                  # extra cross-check, not used by the tests
```

The tests also carry a naive oracle that shares no code with the implementation: it recognises surrogates by bit masks and decodes by the surrogate formula, and it checks the encoder by value. Inputs are heap buffers of exactly `n` units, so AddressSanitizer sees any read past the end.

`tests/utf16/utf16_tests.cpp` has 14 test cases and 168 assertions (sweeps count mismatches and assert once per sweep). The alphabet A has 24 values: 0000, 0001, 0041, 007F, 0080, 07FF, 0800, 8000, D7FE, D7FF, D800, D801, DBFE, DBFF, DC00, DC01, DFFE, DFFF, E000, E001, FEFF, FFFD, FFFE, FFFF. The 24 boundary highs are D800, D801, D802, D83D, D87F, D880, D8FF, D900, D93F, D940, D9FF, DA00, DA3F, DA40, DABF, DB00, DB3F, DB40, DB7F, DB80, DBBF, DBC0, DBFE, DBFF.

| Coverage | Inputs | Against |
| --- | --- | --- |
| every 1-unit input | 65,536 | oracle and reference digest |
| every (high, low) pair | 1,048,576 | oracle and reference digest |
| every pair with one unit from A and the other any unit (either side) | 3,145,728 | oracle and reference digest |
| each of the 24 highs followed by every unit | 1,572,864 | oracle and reference digest |
| every string of length 0 to 4 over A | 346,201 | oracle and reference digest |
| every scalar value (0 to 10FFFF, 1,112,064 valid) plus 5 values above the range | 1,114,117 | oracle, round trip, reference digest |
| encode capacity table | 55 | oracle and reference digest |
| reference vectors with offsets | 149 | reference offsets, oracle and digest |

The high-surrogate-by-any-unit coverage is NOT exhaustive: only the 24 highs above are followed by every one of the 65,536 units, not all 1,024 highs. The full sweep (about 67 million inputs) was deliberately not run, to keep the runtime reasonable. Every 1-unit input, every high/low pair, every unit paired with A on either side, and all strings up to length 4 over A are exhaustive over their stated sets. Also tested: argument order for all functions with sentinels, canary units around `encode_one` output, header anchors, and a long text with the first bad offset far from the start.

`n == kMaxUnits` is accepted and `kMaxUnits + 1` is TooLarge, tested with a 1-unit buffer: a first unit that is ill-formed makes `validate` stop after one unit at `n = kMaxUnits`, so the buffer is not read past its end. The maximum-size test relies on the scan stopping at the first ill-formed sequence. No 256 MiB buffer is allocated.

codecs cross-check (an extra, not an oracle in the tests): `python3 tools/utf16_ref.py --codecs` compares the reference with Python's strict UTF-16 decoder for 6,179,054 inputs and with `chr(cp).encode("utf-16-le")` for all scalar values. Result: 0 disagreements about validity, 0 differences in offsets, 0 disagreements in encoding. The C++ was not compared with codecs; it is compared with the reference and the oracle.

## Mutation check

133 hand-written source mutants of `utf16.cpp` (the surrogate range bounds, the sequence cut-off, the order and result of every check in `validate`, `count_code_points`, `decode_one` and `encode_one`, offsets, step and remaining length, every shift, mask and operator of decode and encode, the length threshold, and the capacity check) were compiled against the unchanged tests. Plain build: 126 killed, 7 survived. With `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented): 126 killed, the same 7 survive.

Plain build: 105 kills by a failing assertion, 19 by a crash alone and 2 by a timeout (mutants 37 and 58, the malformed check removed in `validate` and `count_code_points`, loop forever). The crash-only kills were run individually and all report SIGSEGV (Catch2 "Segmentation violation signal"): null checks removed or weakened in `validate` (4, including `bad_offset`), `count_code_points` (5), `decode_one` (4) and `encode_one` (4), the `count_code_points` loop `<=`, and the capacity check removed or off by one (2).

Sanitizer build: 101 kills by a failing assertion, 2 timeouts (the same), 23 by a sanitizer report, run individually: heap-buffer-overflow (4: cut-off `n < 1`, `count_code_points` loop `<=`, `count_code_points` remaining length wrong, `decode_one` branch swap), load of a null pointer (6: `validate` loop `<=`, `validate`, `count_code_points` and `decode_one` null checks), store to a null pointer (13: `bad_offset`, `out`, `cp`, `len` and `dst` null checks, and the capacity check removed or off by one). Several of these are killed by an assertion in the plain build too (for example 17, 34, 68 and 79); they are listed as the sanitizer saw them.

Survivors, each equivalent by argument:

- `is_high` upper bound `DC00` instead of `DBFF` (mutant 3): `is_high` is only called after `is_low(u)` has returned false in `seq_len`, and `DC00` is low, so the extra value never reaches it.
- Decode, `+` replaced by `|` or `^` for the low half (mutants 88, 89): `(hi - D800) << 10` has its low 10 bits clear and `s[1] - DC00` is at most `0x3FF` (the second unit is a low surrogate, checked by `seq_len`), so the operands share no bit and `+`, `|`, `^` agree.
- Encode, `0xD800 + (v >> 10)` as `|` or `^` (mutants 122, 129): `0xD800` has its low 10 bits clear and `v >> 10` is at most `0x3FF` for `v <= 0xFFFFF`, so no bit is shared.
- Encode, `0xDC00 + (v & 0x3FF)` as `|` or `^` (mutants 127, 128): `0xDC00` has its low 10 bits clear and `v & 0x3FF` is at most `0x3FF`, so no bit is shared.

The mutant that needs the maximum size (mutant 29, `n >= kMaxUnits` returns TooLarge) is killed by the 1-unit buffer test above, so it does not need a 256 MiB buffer.

## Not run

A successful validation at exactly `kMaxUnits` (2^27) units was NOT run (it needs a 256 MiB buffer); the maximum-size test relies on the scan stopping at the first ill-formed sequence. The high-surrogate-by-any-unit sweep covers only 24 highs, not all 1,024 (not exhaustive). Windows runs only in CI. Aliasing of `cp` and `len`, threads and byte order are untested.
