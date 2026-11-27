# utf8

`core/utf8/utf8.h` and `utf8.cpp` (`lumora::utf8`) are strict UTF-8 (RFC 3629, the Unicode table of well-formed sequences): `validate` with the offset of the first ill-formed sequence, and single code point `decode_one` and `encode_one`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no libm, no 128-bit types, no wide character types, no allocation, no global state. The source includes `<cstdint>` only; the tests add `<catch2/...>`, `<cstddef>`, `<cstdint>`, `<initializer_list>`, `<memory>` and `<vector>`. The functions have no mutable state, so there is nothing to share between threads and no thread tests were written.

## Well-formed sequences

As in the header: 1 byte `00-7F`; 2 bytes `C2-DF` then `80-BF`; 3 bytes `E0` then `A0-BF` then `80-BF`, `E1-EC` or `EE-EF` then `80-BF` twice, `ED` then `80-9F` then `80-BF`; 4 bytes `F0` then `90-BF` then `80-BF` twice, `F1-F3` then `80-BF` three times, `F4` then `80-8F` then `80-BF` twice. Overlong forms, surrogates, values above U+10FFFF, a lone continuation byte and the bytes `C0`, `C1`, `F5-FF` are malformed. U+0000, U+FEFF and the noncharacters are valid; there is no byte order mark handling.

## Check order

- `validate(s, n, bad_offset)`: `s` null with `n > 0`: NullArgument (also when `n` is above the maximum). Then `n > kMaxInput` (2^28): TooLarge, nothing read. Then the text is scanned from the start. The first ill-formed sequence gives Malformed and `*bad_offset` is the index of its first byte: the lead byte when a lead byte has a missing or wrong continuation byte (including a wrong second byte for `E0`, `ED`, `F0`, `F4`, and a sequence cut off by the end of the text), the byte itself for a continuation byte or an invalid lead. Ok sets `*bad_offset = n`; empty text is Ok (also with a null `s`) and sets it to 0. `bad_offset` may be null at every outcome. On NullArgument and TooLarge it is not written.
- `decode_one(s, n, cp, len)`: `s`, `cp` or `len` null: NullArgument, before the `n == 0` check. Then `n == 0`: Malformed. Then at most `min(n, 4)` bytes are read: the second and later bytes only when `n` holds the whole sequence. An ill-formed or cut-off sequence is Malformed. Bytes after the first sequence are ignored. On Ok `*cp` and `*len` are set; on any failure both are untouched.
- `encode_one(cp, dst, dst_cap, len)`: `dst` null with `dst_cap > 0`, or `len` null: NullArgument. Then `cp` above U+10FFFF or in D800-DFFF: Malformed. Then the needed length (1 up to 7F, 2 up to 7FF, 3 up to FFFF, else 4) above `dst_cap`: BufferTooSmall (`dst` may be null with `dst_cap` 0; a valid value then never gives Ok). On Ok exactly `*len` bytes are written, the shortest form, and not one byte more. On any failure `dst` and `*len` are untouched.

| Item | Bound |
| --- | --- |
| `validate` input | 0 to `kMaxInput` (2^28) bytes |
| `kMaxCodePoint` | `0x10FFFF` |
| `decode_one` length | 1 to 4 |
| `dst_cap` | any `uint32_t`; 4 always suffices |
| aliasing of `cp` and `len` | not supported, not tested |

## Tests

`tools/utf8_ref.py` is an independent reference in Python, written from the header text with Python integers: a table of continuation byte ranges per lead byte, shifts and masks for decoding, and the lead byte as the offset. The codecs module is not used for any value. It prints a generated block with 64-bit digests that the tests recompute, the grids, and 337 vectors with their offsets (22 valid texts, and 35 ill-formed classes each placed alone, after `A`, and after `A B U+20AC`, and before nothing, `Z` and `U+00E9 Z`). It runs in about 6 seconds.

```
python3 tools/utf8_ref.py                                           # print the block
python3 tools/utf8_ref.py --check tests/utf8/utf8_tests.cpp
python3 tools/utf8_ref.py --codecs                                  # extra cross-check, not used by the tests
```

The tests also carry a naive oracle that shares no code with the implementation: it decodes with the lead byte pattern and shifts, then rejects overlong forms, surrogates and values above U+10FFFF by value, so it does not use the table of ranges. Its encoder works in groups of 6 bits. Inputs are heap buffers of exactly `n` bytes, so AddressSanitizer sees any read past the end.

`tests/utf8/utf8_tests.cpp` has 17 test cases and 126 assertions (sweeps count mismatches and assert once per sweep). Header anchors (U+20AC is `E2 82 AC`, U+10FFFF is `F4 8F BF BF`, U+0000, U+FEFF and the noncharacters are valid) have their own case. Every input class is compared with the oracle; an accepted sequence must also re-encode to the same bytes.

| Coverage | Inputs | Against |
| --- | --- | --- |
| every 1-byte input | 256 | oracle and reference digest |
| every 2-byte input | 65,536 | oracle and reference digest |
| every 3-byte input | 16,777,216 | oracle (and 61,440 accepted 3-byte sequences) |
| 3-byte inputs, third byte in the grid {00, 7F, 80, BF, C0} | 327,680 | oracle and reference digest |
| 4-byte inputs, third and fourth bytes in the grid | 1,638,400 | oracle and reference digest |
| 4-byte inputs, every lead and second byte, third and fourth from {00, 7F, 80, 8F, 90, 9F, A0, BF, C0, C1, F4, F5, FF} | 11,075,584 | oracle |
| every scalar value (0 to 10FFFF, 1,112,064 valid) plus 5 values above the range | 1,114,117 | oracle, round trip, reference digest |
| encode capacity table (14 values x caps 0 to 5, null `dst` with cap 0) | 98 | oracle and reference digest |
| reference vectors with offsets | 337 | reference offsets, oracle and digest |

The 4-byte coverage is a grid and is NOT exhaustive: the third and fourth bytes only take the grid values. All 1-, 2- and 3-byte inputs and all scalar values are exhaustive. Also tested: ten valid code points (U+0080, U+07FF, U+0800, U+FFFF, U+10000, U+10FFFF, U+D7FF, U+E000, U+20AC, U+1F600) cut at every point (Malformed at the lead byte, outputs untouched), bytes after `n` never used (a sequence completed by bytes beyond `n` is still cut off), argument order for all three functions with sentinels, canary bytes around `encode_one` output, and a 100,000-byte text with the first bad byte far from the start.

`n == kMaxInput` is accepted and `kMaxInput + 1` is TooLarge, tested with a 1-byte buffer: a first byte that is ill-formed (`80`, `C0`, `FF`) makes `validate` stop after one byte at `n = kMaxInput`, so the buffer is not read past its end. No 256 MiB buffer is allocated.

codecs cross-check (an extra, not an oracle in the tests): `python3 tools/utf8_ref.py --codecs` compares the reference with Python's strict `bytes.decode("utf-8")` for 2,032,209 inputs (every 1- and 2-byte input, the 3- and 4-byte grids, and the 337 vectors) and with `chr(cp).encode("utf-8")` for all 1,114,112 values from 0 to 10FFFF. Result: 0 disagreements about validity, 0 differences between the reference offset and the codec's `UnicodeDecodeError.start`, 0 disagreements in encoding. The C++ was not compared with codecs; it is compared with the reference and the oracle.

## Mutation check

167 hand-written source mutants of `utf8.cpp` (each lead range bound and range choice, the second byte limits of `E0`, `ED`, `F0` and `F4`, the cut-off check, the continuation loop, every check and result in `validate`, `decode_one` and `encode_one` and their order, the offset values, the step and remaining length, every shift and mask of decode and encode, the length thresholds, and the capacity check) were compiled against the unchanged tests. Plain build: 156 killed, 11 survived. With `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented): 156 killed, the same 11 survive.

Plain build: 141 kills by a failing assertion, 14 by a crash alone and 1 by a timeout (mutant 74, `validate` with the malformed check removed, loops forever on the first ill-formed byte). The crash-only kills were run individually and all report SIGSEGV (Catch2 "Segmentation violation signal"): the `validate` null check removed or weakened (2), `bad_offset` null checks removed (2), the `decode_one` null checks (4), the `encode_one` null checks (4), and the capacity check removed or off by one (2).

Sanitizer build: 135 kills by a failing assertion, 1 timeout (the same), 20 by a sanitizer report, run individually: heap-buffer-overflow (6: 2-byte lead `&&` to `||`, cut-off check one short or removed, continuation loop one long, `validate` loop `<=`, `validate` remaining length wrong), load of a null pointer (4: `validate` and `decode_one` null checks), store to a null pointer (10: `bad_offset`, `cp` and `len` null checks, `dst` null checks, and the capacity check removed or off by one). Several of these are killed by an assertion in the plain build too (mutants 8, 43, 44, 56, 71 and 86); they are listed as the sanitizer saw them.

Survivors, each equivalent by argument:

- 3-byte lead range starting at `DF`, and 4-byte lead range starting at `EF`: `DF` is taken by the 2-byte branch and `EF` by the 3-byte branch first (an `else if` chain), so the changed bound is never reached.
- Continuation loop starting at `k = 1`: the loop only adds a re-check of `s[1]`. `s[1]` is already inside `lo..hi`, which is inside `80..BF`, so it passes the continuation test.
- Decode, 2-byte lead mask `0x3F` instead of `0x1F`: the lead is `C2-DF`, whose bit 5 is 0, so the extra mask bit adds nothing. 3-byte lead mask `0x1F` instead of `0x0F`: the lead is `E0-EF`, bit 4 is 0. 4-byte lead mask `0x0F` instead of `0x07`: the lead is `F0-F4`, bit 3 is 0.
- Decode, continuation mask `0x7F` instead of `0x3F`: it is applied only to bytes already checked to be `80-BF`, where bit 6 is 0, so clearing bit 7 or bits 6 and 7 gives the same value.
- Decode 2-byte `|` replaced by `+`, and encode 2-byte lead `0xC0 + (cp >> 6)` instead of `|`: the operands share no bit (`(lead & 0x1F) << 6` has its low 6 bits clear and the continuation is below 64; `cp >> 6` is below 32 for `cp <= 0x7FF`), so there is no carry.
- Decode branch `l <= 2` instead of `l == 2`, and encode branch `need <= 2` instead of `need == 2`: the case 1 is handled by the earlier branch, so the `<=` branch only sees 2.

Mutant 66 (`n >= kMaxInput` returns TooLarge) is the one mutant that needs the maximum size. It is killed by the 1-byte buffer test above, so it does not need a 256 MiB buffer.

## Not run

A successful validation of a well-formed text of exactly `kMaxInput` bytes was NOT run (it needs a 256 MiB buffer). The 4-byte inputs are a grid, not exhaustive (see the table). Windows runs only in CI. Aliasing of `cp` and `len` and threads are untested.
