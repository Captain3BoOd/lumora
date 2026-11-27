# checksum

`core/checksum/checksum.h` and `checksum.cpp` (`lumora::checksum`) are CRC-32 (IEEE 802.3, reflected polynomial `0xEDB88320`, as in zip and png) and Adler-32 (zlib), each as a one-shot call and an incremental `update` that takes the finished value of the earlier part.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no libm, no 128-bit types, no wide character types, no allocation, no mutable global state. The source includes `<cstdint>` only; the tests add `<catch2/...>`, `<cstddef>`, `<cstdint>`, `<initializer_list>` and `<vector>`. The functions are pure over their input: nothing is shared, so there is nothing to alias, and no thread tests were written.

## Algorithms and bounds

- CRC-32: the finished value is un-xored on entry (`c = crc ^ 0xFFFFFFFF`), each byte is `c = table[(c ^ byte) & 0xFF] ^ (c >> 8)`, and the result is `c ^ 0xFFFFFFFF`. The 256-entry table is a `constexpr` constant built at compile time (the reflected polynomial, 8 shifts per entry). Any `uint32_t` is accepted as a starting value and evaluated literally. Starting value 0 means "nothing yet".
- Adler-32: `s1` is the low 16 bits and `s2` the high 16 bits, started at 1 and 0 by `adler32`. Each half of a starting value is reduced modulo 65521 first (the decision in the header: no failure), so the result always has both halves below 65521. The modulo is deferred: at most 5552 bytes are summed between reductions. With both sums below 65521 at the start of a block, `255 * n * (n + 1) / 2 + (n + 1) * 65520 <= 2^32 - 1` holds for `n = 5552`, so a 32-bit sum cannot overflow. The tests run the block edge (5551, 5552, 5553 and their multiples) with all-0xFF bytes, which is the worst case.
- `data == nullptr` with `n > 0` reads nothing and returns `crc` unchanged (CRC) or the value after the reduction (Adler). `n == 0` does the same whatever `data` is. `crc32` and `adler32` are exactly `update(0, ...)` and `update(1, ...)`, so a null pointer gives 0 and 1.
- `n` is any `uint32_t`. Loop counters are `uint32_t` and never pass `n`, so there is no overflow near 2^32 - 1.

| Item | Bound |
| --- | --- |
| `n` | 0 to `0xFFFFFFFF` (only up to 70,000 was run, see Not run) |
| CRC start | any `uint32_t` |
| Adler start | any `uint32_t`; each half is reduced modulo 65521 |
| Adler result | both halves below 65521 |

## Tests

`tools/checksum_ref.py` is an independent reference in Python, written from the header text with Python integers: a bitwise CRC with no table and a per-byte-modulo Adler. No value comes from zlib. It builds fixed inputs (all 0xFF, all 0x00, an LCG stream, a counter pattern) and prints a block with FNV-1a 64 digests that the tests recompute, plus five vectors. It runs in about 2 seconds.

```
python3 tools/checksum_ref.py                                           # print the block
python3 tools/checksum_ref.py --check tests/checksum/checksum_tests.cpp
python3 tools/checksum_ref.py --zlib                                    # extra cross-check, not used by the tests
```

Header anchors (the values written in `checksum.h`) are tested separately in their own case: crc32 of `123456789` is `0xCBF43926`, crc32 of the empty input is 0, adler32 of the empty input is 1, adler32 of `Wikipedia` is `0x11E60398`. All other expected values are reference-derived: the five vectors in the generated block (`123456789` has adler32 `0x091E01DE`, which the header does not give) and the digests below.

zlib cross-check (an extra, not an oracle in the tests): `python3 tools/checksum_ref.py --zlib` compares the reference with Python's `zlib.crc32` and `zlib.adler32` for the single-byte cases, the length sweep (every CRC start value) and the long inputs, and for the Adler starts whose halves are both below 65521. It passed with 1,443 CRC and 1,443 Adler comparisons. It skipped 1,176 Adler cases whose start has a half of 65521 or more, because zlib does not define that case the way the header does. The five vectors were compared too. The chunked cases and the C++ were not compared with zlib; the C++ is compared with the reference and the oracle.

`tests/checksum/checksum_tests.cpp` has 14 test cases and 59 assertions (sweeps count mismatches and assert once per sweep). The oracle shares no code or table with the implementation: a bit-at-a-time CRC and a per-byte-modulo Adler in 64 bits. Inputs are heap vectors of exactly `n` bytes, so AddressSanitizer sees any read past the end.

- header anchors, reference vectors, null data and zero length (with a real pointer and with null, `n` up to `0xFFFFFFFF` with null data), the one-shot functions against `update(0 / 1)`;
- Adler start reduction, hand-written: `0xFFFFFFFF` gives `0x000E000E`, 65521 gives 0 in either half, 65520 is kept, start 65526 plus byte 1 gives `0x00060006`, and the result is a valid value for 10 starts, several lengths and null data;
- CRC start values (5, any `uint32_t`) against the oracle;
- single bytes 0 to 255 for both algorithms (every table entry is used once), with the reference digest;
- length sweep: 4 input kinds x 98 lengths (0 to 80, 127 to 129, 255 to 257, 1000, 4096, 5550 to 5554, 11103 to 11105, 16656) x 3 CRC starts and 6 Adler starts, plus lengths 65521, 65536 and 70000 for two kinds: 1,182 CRC and 2,358 Adler cases, each against the oracle and the reference digest (an Adler result is also checked to be a valid value);
- block edge: all-0xFF data of 5551, 5552, 5553, 11103, 11104, 11105, 16655, 16656, 16657, 22207, 22208 and 22209 bytes with Adler starts 1, 65520 in both halves, 65520 low only and 65520 high only, against the oracle;
- split independence: every split point of inputs up to 80 bytes (3 kinds, both algorithms), splits at 0, 1, 5551, 5552, 5553, 11103 to 11106 of an 11,106-byte input, 400 random multi-part sequences (parts of 1 to 60 bytes, with empty parts using a real pointer and a null pointer in the middle) against the one-shot value and the oracle, and chunked streaming (2 kinds x 48 lengths x chunk sizes 1, 2, 3, 7, 64, 5552) against the one-shot value and a reference digest that chains in Python (576 CRC and 576 Adler cases).

## Mutation check

78 hand-written source mutants of `checksum.cpp` (the polynomial, the table build and its bounds, the null and zero-length checks, each entry and final xor, the table index mask, the shifts and the combine operator, the loop bounds, the Adler base, block size, half extraction and swap, the reduction of each half, the block take and loop conditions, data indexing, the sums and their reductions, the position and counter updates, the packing, and the one-shot start values) were compiled against the unchanged tests. Plain build: 72 killed, 6 survived. With `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented): 72 killed, the same 6 survive.

Plain build: 60 kills by a failing assertion, 10 by a crash alone and 2 by a timeout (block size 0 and `left >= 0`, which loop forever). The crash-only kills were run individually and all report SIGSEGV (Catch2 "Segmentation violation signal"): the CRC null check removed or weakened (4), the table index mask removed, the Adler null check removed, `take` always the block size or the maximum, and `left` not reduced or reduced by 1.

Sanitizer build: 55 kills by a failing assertion, 2 timeouts, 15 by a sanitizer report, run individually: load of a null pointer (6: CRC null checks and the Adler null check, removed or inverted), table index out of bounds (2: mask removed gives index 4294967279, mask `0xFFF` gives 4046), heap-buffer-overflow (7: CRC loop `<= n`, Adler `take` always the block size or the maximum, inner loop `<= take`, data index `+ 1`, `left` not reduced, `left` reduced by 1). Mutants 33 (mask `0xFFF`), 40 (`<= n`), 51 (inverted Adler null check), 59 (inner `<= take`) and 61 (index `+ 1`) are killed by an assertion in the plain build, and the sanitizer reports above are the undefined behavior they also cause; none is claimed equivalent.

Survivors, each equivalent by argument:

- Block size 5551 and block size 1: a smaller block only reduces more often. The modulo is applied to both sums after every block and the sums are linear, so the final residues are the same, and a smaller block cannot overflow if 5552 does not. Block sizes above 5552 (5553, 5600, 6000, 65536) are killed.
- CRC check `n == 0u` removed: for `n == 0` the loop does not run, so the result is `(crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF`, which is `crc`, the value the check returns.
- `take` with `left <= kBlock` instead of `left < kBlock`: the two differ only when `left == kBlock`, where both choices give `kBlock`.
- Adler packing `(s2 << 16) + s1` and `(s2 << 16) ^ s1` instead of `|`: `s1` and `s2` are below 65521 here, so `s2 << 16` fits in 32 bits and its low 16 bits are zero. `s1` is below 2^16, so the two operands share no bit and `|`, `+` and `^` agree.

Not run: no input near 2^32 - 1 bytes (that needs 4 GiB); the longest input is 70,000 bytes. The loops use `uint32_t` counters below `n`, so they cannot wrap, but this was argued and not run. Windows runs only in CI. Aliasing and threads are untested.
