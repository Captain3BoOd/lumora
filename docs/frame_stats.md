# Frame statistics and fps overlay text

`core/lifecycle/frame_stats.h` and `frame_stats.cpp` (`lumora::lifecycle`) keep a window of the 120 most recent frame times, summarize it (average, worst, nearest-rank p99, fps in tenths) and write the diagnostics overlay line of `docs/android-lifecycle.md` section 10.1 into a caller buffer.

Evidence label: unit tests only (CI-verified).

Integer only: no clock reads, no floating point, no libm, no 128-bit types (MSVC has none), no allocation, no standard formatting or I/O functions. The sources include `<cstddef>` and `<cstdint>`; the tests add `<string>` and `<vector>`.

## State

`FrameStats` holds `samples[120]` (nanoseconds), `count` (valid samples) and `next` (the slot written next). Valid state: `count <= 120` and `next < 120`. Anything else (`count > 120` or `next >= 120`) is invalid and is read as empty.

The window is the `count` most recent slots ending at `next - 1` with wrap-around, that is slots `(next - count + i) mod 120` for `i = 0..count-1`. `record_frame` always produces `count == next` until the window is full, so for objects it builds the window is slots `0..count-1`, and after that every slot. A hand-built object with `count < 120` and `next != count` reads exactly the slots above. Slots outside the window are never read. A zero inside a hand-built window is an ordinary sample.

## record_frame

Returns false and changes nothing when `stats` is null or `frame_ns` is 0 (an invalid object is not repaired on failure). Otherwise, in this order:

1. An invalid object is made empty (`count = 0`, `next = 0`). Old slots are not cleared.
2. `samples[next] = frame_ns`.
3. `next` advances by one and wraps from 119 to 0.
4. `count` grows by one up to 120. Once full, the oldest sample is the one overwritten.

## summarize

Returns false and leaves `*out` untouched when `stats` or `out` is null. Otherwise it returns true and writes `*out`: a zeroed summary for an empty or invalid object, and for a window of `c` samples:

| Field | Value |
| --- | --- |
| `count` | `c` |
| `average_ns` | `floor(sum / c)`, with the sum saturating at `UINT64_MAX` |
| `worst_ns` | the largest sample |
| `p99_ns` | the sample of rank `c - floor(c / 100)` in ascending order (1-based, equal to `ceil(0.99 * c)`) |
| `fps_tenths` | `floor(10000000000 / average_ns)`, saturated at `UINT32_MAX`; 0 when `average_ns` is 0 |

Ranks: `c` 1 to 99 gives the largest sample, 100 gives the 99th smallest, 101 the 100th, 120 the 119th. The sum is a running saturating addition, so several `UINT64_MAX` samples give `floor(UINT64_MAX / c)`. `10000000000` fits in 64 bits, so the division needs no wide type. Any average of 3 or more gives at most 3333333333 tenths, so only averages of 1 and 2 reach the saturation (5000000000 and 10000000000 tenths). A copy of the window (at most 120 values, 960 bytes) is sorted by insertion; nothing is allocated.

## format_overlay

`"<fps>.<tenth> fps  avg <ms>.<hh> ms  p99 <ms>.<hh> ms  worst <ms>.<hh> ms"`, for example `59.9 fps  avg 16.69 ms  p99 18.20 ms  worst 25.04 ms` (two spaces between the groups). The text is built from the summary fields as given, with no recomputation or validation. `fps` is `fps_tenths / 10`, `tenth` is `fps_tenths % 10`. Each time is `h = ns / 10000` (hundredths of a millisecond, truncated, never rounded) printed as `h / 100`, a dot and the two digits of `h % 100`. Numbers are unsigned decimal with no padding. A summary with `count == 0` writes `no frames` whatever its other fields hold; any other `count` (up to `UINT32_MAX`) writes the full line.

Order of checks:

1. `out` null or `capacity` 0: return 0 and write nothing. This is a deliberate departure from `snprintf`, which returns the length at capacity 0.
2. The full text is built in a local buffer of 160 characters. The longest possible line is 95 characters (`UINT32_MAX` tenths and `UINT64_MAX` times: `429496729.5 fps  avg 18446744073709.55 ms  p99 18446744073709.55 ms  worst 18446744073709.55 ms`).
3. The first `min(length, capacity - 1)` characters are copied and a NUL follows. Capacity 1 writes only the NUL. Nothing is written past the NUL.
4. The return value is the full length, excluding the NUL, whatever the capacity.

## Bounds

| Item | Bound |
| --- | --- |
| window | 120 samples |
| frame time | 1 to `UINT64_MAX` through `record_frame`; any value in a hand-built object |
| `count`, `next` on input | any 32-bit value; out of range reads as empty |
| sum | saturates at `UINT64_MAX` |
| fps | at most `UINT32_MAX` tenths |
| overlay line | at most 95 characters plus the NUL |

## Tests

`tools/frame_stats_ref.py` is an independent reference in Python on unbounded integers. It uses `sorted()` and the smallest `r` with `r * 100 >= 99 * count` for the rank (not the `count - count / 100` form), `min(UINT64_MAX, sum(...))` for the sum and Python format strings for the text. It prints a generated block with the grids and FNV-1a 64 digests that the test recomputes from the C++ functions.

```
python3 tools/frame_stats_ref.py                                                 # print the block
python3 tools/frame_stats_ref.py --check tests/frame_stats/frame_stats_tests.cpp
```

`tests/frame_stats/frame_stats_tests.cpp` compares exact values only:

- written cases: defaults, wrap-around and the full window, rejection of 0 and null, invalid objects (count 121, next 120, `UINT32_MAX`) treated as empty with no repair on failure, a valid but inconsistent object (`count` 3, `next` 50), window selection by `next` including a wrapping window and ignored extreme slots, ranks for counts 1, 2, 50, 99, 100, 101, 119 and 120 in ascending and descending slot order, duplicates, sums at and over `UINT64_MAX` (exact `2^64` must give `UINT64_MAX / 2`), fps at 1, 2, 3 and 4 ns, at the 1e10 edge and at `UINT64_MAX`, zero samples, the header example, truncation at every capacity from 0 to length + 4, guard bytes past the capacity and after the NUL;
- sweeps checked against a naive oracle that shares no code with the implementation (a ring walk, rank counting instead of sorting, shift-subtract division, decimal long division, `std::to_string` text) and against digests from the reference: four scripted record sequences of 300 frames with a rejected zero every seventh frame, summary after every step (1,200 cases); 770 hand-built objects (5 sample patterns including zeros, ascending and descending, 14 counts up to `UINT32_MAX`, 11 `next` values); 90 `record_frame` calls on valid and invalid objects, with the whole struct hashed; and the overlay text over 115,680 summary and capacity combinations (every triple of 15 time values for two fps values, plus every count, fps and time on the diagonal, at 16 capacities from 0 to 200).

## Mutation check

85 hand-written source mutants of `frame_stats.cpp` (each range check, the digit and millisecond formatting, every text separator, field mix-ups, null and capacity handling, truncation, the NUL, the return value, the repair of invalid objects, wrap and count growth, window selection, sum saturation, worst, the sort, the p99 rank, the average and the fps saturation) were each compiled against the unchanged tests. 77 were killed by a plain build and 8 survived it. Nine of the kills are crashes only (a null check removed, a removed empty check, an out-of-range `next` written).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented, the test object unchanged), 79 are killed and 6 survive. The two extra kills:

- `kTextMax` 90 instead of 160: the 95 character line overflows the local buffer. Plain build passes; ASan reports a stack-buffer-overflow in `put_text`.
- the `j > 0` check removed from the insertion sort: reads `w[-1]` on the way to the front. Plain build passes; UBSan reports an index out of bounds at the loop.

The unmutated source passes all tests under the same flags.

Survivors in both builds, each argued:

- sum saturation `>=` instead of `>`: they differ only when `sum == UINT64_MAX - v`, where the exact sum equals `UINT64_MAX` and both give the same value. Equivalent.
- worst `>=` instead of `>`: equal values give the same maximum. Equivalent.
- insertion sort `>=` instead of `>`: equal values may swap, and the sorted values are identical. Equivalent.
- p99 index `(count * 99 + 99) / 100 - 1`: `ceil(99c / 100) = c - floor(c / 100)` for every `c`. Another spelling of the same value. Equivalent.
- fps saturation `>=` instead of `>`, and `> UINT32_MAX - 1`: `floor(1e10 / a)` is at most 3333333333 for `a >= 3`, and 5000000000 or 10000000000 for `a` of 2 or 1, so no average gives a value in `[UINT32_MAX - 1, UINT32_MAX]` and both thresholds behave like the original. Equivalent by unreachability.

Not run: Windows (CI only), Android, the editor, `tools/ci/test.py`, fuzzing, a full local build.
