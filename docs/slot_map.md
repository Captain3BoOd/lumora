# Slot map

`core/slotmap/slot_map.h` and `slot_map.cpp` (`lumora::slotmap`) are a generational handle allocator with 256 slots, for scene resources: `init`, `allocate`, `release`, `is_live` and `size`. It stores no payload, only which slots are in use and their generation.

Evidence label: unit tests only (CI-verified).

Integer only: no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation, no global state, no locking. The source includes `<cstdint>`; the tests add `<catch2/...>`, `<algorithm>`, `<cstddef>` and `<vector>`.

## State and handles

`SlotMap { uint32 gen[256]; uint8 used[256]; uint32 count; }`. A handle is `(generation << 8) | index`: `index = handle & 255`, `generation = handle >> 8` (24 bits, so at most `0xFFFFFF`). Valid generations are `1 .. 0xFFFFFF`. A handle with generation 0 is never live, so 0 is a safe "no handle" value. `init` sets every slot free with generation 1.

`count` is informational. It is used by `size` and for the corrupt check (`count > 256`). Allocation and liveness use the `used` flags and the stored `gen`, never `count`.

## Check order

- `init(map)`: a null map is ignored; otherwise every `gen[i] = 1`, `used[i] = 0` and `count = 0`, whatever the map held before (also a corrupt count).
- `allocate(map, handle)`:
  1. `map` or `handle` null: false. `count > 256` (corrupt): false.
  2. Scan `i = 0 .. 255` for the first `used[i] == 0` (flags only). None: false.
  3. `*handle = ((gen[i] & 0xFFFFFF) << 8) | i`, `used[i] = 1`, `count += 1`, true.
  `*handle` is untouched on every false return. The stored generation is not checked or repaired: a hand-built slot with generation 0 gives a handle with generation 0 (not live), and a slot above `0xFFFFFF` gives a masked handle (not live, since the stored value differs from the masked one). `count == 256` with a free flag allocates and leaves `count == 257`, which is corrupt from then on. A map whose flags are all set but `count < 256` returns false.
- `is_live(map, handle)`, in this order: null map or `count > 256`: false; generation field 0: false; `used[index] == 0`: false; otherwise `gen[index] == generation` with the full stored 32-bit value (no masking), so a stored generation above `0xFFFFFF` never matches any handle. Any nonzero `used` value counts as in use.
- `release(map, handle)`: false (nothing changes) when the handle is not live as above (this includes a null map, a corrupt count, a second release of the same handle, a stale handle and a free slot). Otherwise `used[index] = 0`, `gen[index] = 1` if it was exactly `0xFFFFFF` and `gen[index] + 1` otherwise, and `count -= 1` only when `count > 0`; true.
- `size(map)`: `count`, or 0 for a null map or `count > 256`. A `count` that disagrees with the flags is returned as stored.

## Bounds

| Item | Bound |
| --- | --- |
| slots | 256, handle index = low 8 bits |
| generation | 24 bits, valid `1 .. 0xFFFFFF`; `0xFFFFFF` wraps to 1, never 0 |
| `count` | `0 .. 256` is valid; above 256 is corrupt (`size` 0, `allocate`, `release` and `is_live` refuse); 256 with a free flag can still allocate, to 257 |
| `used[i]` | 0 is free; any other value is in use; `allocate` writes 1 |
| generation reuse | a handle kept while its slot is released and allocated 16,777,215 times is live again (24-bit period); tested |

## Tests

`tools/slot_map_ref.py` is an independent reference in Python (plain lists, each rule written out one condition at a time). It prints a generated block with FNV-1a 64 digests that the tests recompute, the operation statistics and the handles around the generation wrap. It runs in about 1 second.

```
python3 tools/slot_map_ref.py                                         # print the block
python3 tools/slot_map_ref.py --check tests/slotmap/slot_map_tests.cpp
```

`tests/slotmap/slot_map_tests.cpp` has 16 test cases and 1,449 assertions. Written cases: constants, `init` over garbage and null, null map and null handle pointer for every function, lowest-free allocation of all 256 slots then a full map (handle untouched), release and reuse of the lowest hole with the new generation, a second release, a stale handle, a wrong generation, generation 0 and a free slot, `is_live` with generation 0 and the index bits, the wrap with forged generations (`0xFFFFFE` to `0xFFFFFF` to 1), forged generation 0 and above the maximum on `allocate`, `used` values 2 and 255, a `count` that disagrees with the flags (including release never underflowing, `count == 256` with a free flag, and all flags set with `count < 256`), every corrupt count (257, 1000, `0xFFFFFFFF`) with the map unchanged and `init` repairing it, and all 256 indexes against generations 0, 1, 2 and `0xFFFFFF`.

The full generation loop runs in the plain native build: one slot is allocated and released 16,777,218 times (3 cycles past the wrap). Every handle is compared with `k % 0xFFFFFF + 1` shifted left by 8 (naive arithmetic, not the implementation's rule), the previous handle must be stale, and the handles at 8 probe cycles around the wrap must equal the values printed by the reference (`0x100`, `0x200`, `0x300`, `0xFFFFFE00`, `0xFFFFFF00`, `0x100`, `0x200`, `0x300`). The same loop also runs under the sanitizers (about 0.4 s at `-O1`); the forged near-wrap cases above run in every build.

Two sweeps compare a naive oracle that shares no code with the implementation (`std::vector` arrays, `std::find` for the lowest free slot, `%` and `/` for the handle, a remainder for the generation step) and the reference digest after every step, comparing the whole map:

- scripted operations: 12,000 operations from an LCG (seed 61): allocate, release (live, stale and random handles), `is_live` (two generators), `size`, forging (generations `0xFFFFFE`, `0xFFFFFF`, 0, `0x1000005` and random, `used` 2 and 0, count set to 0, 255, 256 or 257) and `init`. The test checks the statistics against the reference exactly: 2,600 successful allocates, 419 failed, 469 successful releases, 2,500 failed, 160 live and 1,831 not-live answers, 2 releases of a slot at generation `0xFFFFFF`, 2,040 forgings, 250 inits and 1,629 steps with a corrupt count;
- table: 12,936 cases (every combination of 7 counts, 6 `used` patterns and 7 generation patterns, with `size`, `allocate` and, for 21 handles, `is_live` and `release`).

## Mutation check

84 hand-written source mutants of `slot_map.cpp` (the corrupt check and its position, each null check, the `init` fields and loop bound, the scan start, bound and flag test of `allocate`, the mask, shift and index of the handle, the `used` write and the count update, every condition of `is_live`, the index mask, the `release` conditions, the wrap value and the wrap comparison, the generation step, the count guard and decrement, and the `size` conditions) were compiled against the unchanged tests. 81 were killed by a plain build and 3 survived. Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented), the same 81 are killed and the same 3 survive.

Kills that need a crash or a sanitizer message were run individually. Plain builds: null-check mutants (`init`, `allocate` map and handle, `is_live`, `size`) end in SIGSEGV with no assertion output; four more (the `allocate` handle and count condition changed to `&&`, `release` with the liveness check removed or inverted, `size` with `||`) fail assertions and then crash. Sanitizer builds report `member access within null pointer` (`init`, `allocate`, `is_live`, `release`, `size` mutants), `store to null pointer of type 'uint32_t'` (the `allocate` handle null check removed, and the `allocate` handle and count condition changed to `&&`), and `index 256 out of bounds for type 'unsigned char [256]'` (index mask 511, which fails assertions without a message in the plain build).

The 3 survivors are each equivalent, with these arguments:

- `allocate` without the `& 0xFFFFFF` mask: the shift is on a `uint32_t`, so `(g << 8)` drops the top 8 bits, which is exactly what masking the low 24 bits first does.
- `allocate` with `+ i` instead of `| i`: the low 8 bits of `g << 8` are 0 and `i < 256`, so the sum and the or are equal.
- `release` with `>= kMaxGeneration` instead of `== kMaxGeneration`: `release` reaches that line only after `is_live` accepted the handle, which needs `gen[index] == generation` with `generation <= 0xFFFFFF`, so a generation above `0xFFFFFF` is never seen there.

## Not run

- Windows (MSVC) is covered by CI only; it was not run locally.
- No test of threads.
