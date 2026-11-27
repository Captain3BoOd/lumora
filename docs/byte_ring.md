# Byte ring

`core/ring/byte_ring.h` and `byte_ring.cpp` (`lumora::ring`) are a fixed-capacity byte FIFO (4096 bytes) for staging bytes between two stages on one thread: `reset`, `size`, `free_space`, `write`, `read`, `peek` and `skip`.

Evidence label: unit tests only (CI-verified).

Integer only: no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation, no global state, no locking. The source includes `<cstdint>` and `<cstring>`; the tests add `<catch2/...>`, `<cstddef>` and `<vector>`. Every index is kept below `kCapacity` before an addition, so no sum can overflow whatever `n` is.

## State

`ByteRing { uint8 data[4096]; uint32 head; uint32 count; }`. `head` is the index of the oldest byte and `count` the number of stored bytes. The newest byte goes to index `(head + count) % 4096`. A default ring is `head == 0` and `count == 0`; its data bytes are not initialized and never read before they are written.

## Validity

A ring is valid when `head < 4096` and `count <= 4096`. Every function given a null or invalid ring returns 0 and changes nothing, except `reset`, which empties any non-null ring (an invalid one too). A null byte pointer with `n > 0` returns 0 and changes nothing; with `n == 0` the pointer is not read and the call returns 0. Rings with invalid fields can only be made by writing the struct fields directly; the tests do that.

## Check order

All functions check in this order and stop at the first failure.

- `reset(ring)`: null ring is ignored; otherwise `head = 0`, `count = 0`, data untouched.
- `size(ring)`: 0 for null or invalid, else `count`. `free_space(ring)`: 0 for null or invalid, else `4096 - count`.
- `write(ring, src, n)`:
  1. null or invalid ring, null `src` or `n == 0`: return 0.
  2. `k = min(n, 4096 - count)`; `k == 0` (full ring): return 0.
  3. `start = (head + count) % 4096`. Copy `min(k, 4096 - start)` bytes to `data + start`, then the rest (if any) to `data[0..]`. The first bytes of `src` are kept.
  4. `count += k`; return `k`. `head` does not change.
- `read(ring, dst, n)`:
  1. null or invalid ring, null `dst` or `n == 0`: return 0.
  2. `k = min(n, count)`; `k == 0`: return 0, `dst` untouched.
  3. Copy `k` bytes from `head` in at most two segments.
  4. `head = (head + k) % 4096`, `count -= k`; return `k`. When the ring empties, `head` keeps its value.
- `peek(ring, dst, n, offset)`:
  1. null or invalid ring, null `dst`, `n == 0` or `offset >= count`: return 0, `dst` untouched.
  2. `k = min(n, count - offset)`; copy from `(head + offset) % 4096`.
  3. The ring is never changed; return `k`.
- `skip(ring, n)`: as `read` without the copy.

`dst` is written for exactly the returned count and not at all on a return of 0. The data array is changed only by `write`; `read`, `peek` and `skip` change at most `head` and `count`. Buffers that point into `ring->data` are not supported and not tested.

## Bounds

| Item | Bound |
| --- | --- |
| capacity | 4096 |
| `n`, `offset` | any `uint32_t`; larger than the space or the stored bytes is clamped, never an error |
| `head` of a valid ring | 0 to 4095 |
| `count` of a valid ring | 0 to 4096 |
| `head + offset`, `head + count`, `head + k` | at most 8191 after the checks, so no overflow |
| `read` and `skip` on an empty ring | return 0, `head` unchanged |
| `peek` with `offset == count` | returns 0 |
| aliasing of `src` or `dst` with `ring->data` | not supported, not tested |

## Tests

`tools/byte_ring_ref.py` is an independent reference in Python (a plain list model of the ring, with no shared code or formulas with the C++). It prints a generated block with the head, count and `n` tables and FNV-1a 64 digests that the tests recompute. It runs in about 3 seconds.

```
python3 tools/byte_ring_ref.py                                             # print the block
python3 tools/byte_ring_ref.py --check tests/ring/byte_ring_tests.cpp
```

`tests/ring/byte_ring_tests.cpp` has 12 test cases and 8,659 assertions. Written cases: constants and defaults, null ring, null pointers and `n == 0` for every function, `write` (basic, partial, full, wrap across the end of data), `read` (order, partial, empty ring keeps `head`), `peek` (offsets, clamping, ring unchanged), `skip`, invalid rings (forged `head` and `count` fields) for every function with the ring bytes compared before and after, `reset` (including an invalid ring), and a write then read round trip at every `head`. The buffers are filled with a pattern and a sentinel byte after the destination, so a write past the returned count is seen.

Two sweeps compare an in-test naive oracle (a `size_t` array model that shares no code with the implementation) and the reference digest:

- scripted operations: 4,000 operations from an LCG (seed 51) with all functions, including forged valid and invalid rings; the test also checks that the run is not trivial (more than 1,000 writes, 500 reads, 200 peeks, 100 skips, 20 partial writes, 100 wraps, 100 clamped calls, 10 forged rings and 10 resets);
- table: 2,688 cases (8 heads x 7 counts x every function and the `n` and `offset` tables).

A guard in the sweep stops on a corrupted ring (head at least 4096 or count above 4096) before sizing a buffer, so a broken implementation fails an assertion and does not exhaust memory.

## Mutation check

119 hand-written source mutants of `byte_ring.cpp` (the validity conditions, each null and `n == 0` check, the `min` choice, the modulo in `index_of` and `advance_head`, the copy lengths and sources of both segments, the wrap split, the order of the updates, the offset check in `peek`, the clamp and return values of every function) were compiled against the unchanged tests.

Plain build (mutated file and the prebuilt test object): 104 killed, 15 survived. Same with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated file only: 104 killed, the same 15 survived. A kill by a crash alone (no failing assertion) was run individually and its observed message is:

- null pointer (0, 40, 56): `member access within null pointer`; (57, 83, 96): `null pointer passed as argument ..., which is declared to never be null`;
- `heap-buffer-overflow` (5, 6, 9, 11, 12, 27, 55, 60, 61, 62, 63, 70, 73, 95, 99, 102);
- `SEGV on unknown address` (3, 8, 13, 82, 109);
- 67 (write without the wrap split), 86 (read with `k` replaced by `n`) and 100 (peek offset checked against capacity): the plain and the mutated-file-only sanitizer builds ended with a signal or an exit code and no sanitizer line. They are plain crashes. Built with the tests also instrumented, all three report a sanitizer error (`stack-buffer-overflow` for 67 and 100, `heap-buffer-overflow` for 86).

Mutant 30 (`copy_out`, first segment length `k` instead of `first`) survives both builds above, because the test object is not instrumented and the over-read stays inside the same stack object. Built with the source and the tests both instrumented, AddressSanitizer kills it (`stack-buffer-overflow` in `memcpy`). It is a kill by a sanitizer only, with undefined behavior in the mutant; no assertion catches it. It is not claimed equivalent.

The other 14 survivors are each equivalent, with these arguments:

- 10: `min_u32` with `a <= b` instead of `a < b`: the two differ only when `a == b`, and the returned value is the same.
- 15 and 38: `% 4096` written as `& 4095` in `index_of` and `advance_head`: 4096 is a power of two and the operands are unsigned and small, so the results are equal.
- 23 and 75: `k > first` as `k >= first` in `copy_out` and in `write`: at `k == first` the second copy has length 0 to valid pointers, which changes nothing.
- 58, 84, 97, 111: the `n == 0` check removed from `write`, `read`, `peek`, `skip`: with `n == 0`, `k` is 0 and the next check (or the 0-byte copy and the 0-byte update) returns 0 with nothing changed. In `peek`, `k = 0` copies 0 bytes and returns 0.
- 59, 85, 112: the `k == 0` check removed from `write`, `read`, `skip`: a 0-byte copy to valid pointers, and `count` and `head` change by 0, return 0.
- 91: `read` advancing before the copy: the start index is read into the call first, and advancing changes only `head` and `count`, not the data.
- 98: `peek` with `offset > count`: at `offset == count`, `k` is 0 and the call returns 0 with a valid index, the same as before.

## Not run

- Windows (MSVC) is covered by CI only; it was not run locally.
- No test of threads, and no test of buffers that overlap `ring->data`.
