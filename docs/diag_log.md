# Diagnostics log ring

`core/diag/diag_log.h` and `diag_log.cpp` (`lumora::diag`) keep the 64 most recent events (timestamp, code, two `int32_t`), a counter of overwritten events and an FNV-1a 64 digest of the whole log, for the in-app diagnostics screen (`docs/android-lifecycle.md` section 10.1 item 3).

Evidence label: unit tests only (CI-verified).

Integer only: no clock reads (the caller supplies the timestamp), no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation, no formatting. The source includes `<cstdint>`; the tests add `<catch2/...>`, `<cstddef>` and `<vector>`.

## State

`DiagLog` holds `entries[64]`, `count` (valid entries), `next` (the slot written next) and `dropped` (entries overwritten so far, saturating at `UINT64_MAX`). Valid state: `count <= 64` and `next < 64`. Anything else is invalid and is treated as empty.

The window is the `count` most recent slots ending at `next - 1` with wrap-around, that is slots `(next - count + i) mod 64` for `i = 0..count-1`, oldest first. A hand-built object with `count < 64` and `next != count` reads exactly those slots. Slots outside the window are never read.

## log_clear

Returns false when `log` is null. Otherwise `*log = DiagLog{}` (all slots zero, `count`, `next`, `dropped` 0), also for an invalid object, and returns true.

## log_push

Returns false and changes nothing when `log` is null. Otherwise, in this order:

1. An invalid object is repaired: `count = 0`, `next = 0`. `dropped` and the slots are left as they are, and the repair itself counts no drop.
2. The entry is written at `next` (a non-full valid object honors its own `next`, even when it differs from `count`).
3. `next` advances by one and wraps from 63 to 0.
4. If `count < 64`, `count += 1`. Otherwise the oldest entry was overwritten and `dropped += 1`, saturating at `UINT64_MAX`.

Returns true.

## log_read

Returns false and leaves `*out` untouched when `log` or `out` is null, the state is invalid, or `index >= count` (checked in that order). Otherwise `*out` is the entry at slot `(next + 64 - count + index) mod 64`: index 0 is the oldest, `count - 1` the newest.

## log_size

`count` for a valid log, 0 for a null or invalid one.

## log_digest

FNV-1a 64, byte-wise: for each byte, xor it into the hash and then multiply by the prime `0x100000001b3`. The initial value is the offset basis `0xcbf29ce484222325`, which is also the digest of a null log (no bytes hashed). The input is a stream of 32-bit words, each fed least significant byte first. For each entry of the window from the oldest to the newest: timestamp low 32 bits, timestamp high 32 bits, `code` (zero extended), `a`, `b` (as `uint32_t`, two's complement). Then `dropped` low 32 bits and `dropped` high 32 bits. An invalid log hashes as an empty log: no entries, but the supplied `dropped` is still hashed (it is not reset). Entries are read field by field, so struct padding is never hashed.

## Bounds

| Item | Bound |
| --- | --- |
| capacity | 64 entries |
| `count`, `next` on input | any 32-bit value; out of range is invalid |
| `timestamp_ns` | any value |
| `code` | 0 to 65535 |
| `a`, `b` | any `int32_t` |
| `dropped` | saturates at `UINT64_MAX` |
| digest | 5 words per entry, 2 words for `dropped`: at most 322 words |

## Tests

`tools/diag_log_ref.py` is an independent reference in Python (the window is walked backward from `next - 1`, the digest is built from a flat list of words). It prints a generated block with the grids and FNV-1a 64 digests (the whole object, return values and read results after every call) that the test recomputes.

```
python3 tools/diag_log_ref.py                                          # print the block
python3 tools/diag_log_ref.py --check tests/diag_log/diag_log_tests.cpp
```

`tests/diag_log/diag_log_tests.cpp` has 15 test cases and 1,607 assertions. Entries are compared field by field. Written cases: constants and defaults, `log_clear` from a full, an invalid and a null object, push through the first entry, every slot, the 65th entry and 70 more (drop count), full-range field values, saturation at `UINT64_MAX - 2` through `UINT64_MAX` and the 32-bit carry of `dropped`, repair of eight invalid states (`count` 65, 64 with `next` 64, `UINT32_MAX`, and so on) with stale slots and `dropped` kept, a hand-built non-full object with `next != count`, `log_read` for the oldest, the newest and a middle entry, a hand-built wrapping window (slots 59 to 63 then 0 to 4), every failure with `*out` untouched, `log_size` for valid, invalid and null objects, known digest values (null, empty, one and two entries, `dropped` above 32 bits, a log of 66 pushes) taken from the reference, an invalid log hashing as an empty log with `dropped` as given, and a check that flipping any word of any entry or `dropped` changes the digest.

Sweeps use an oracle that shares no code with the implementation (a list model that shifts on overflow, a backward walk of the window, a flat word list for the digest) and digests from the reference:

- 4 scripted sequences of 400 operations (a clear every 61 operations on average, one in five timestamps, one in three codes and values from the edge lists): after every operation the whole ring is read back against the model and the digest against the oracle (1,600 cases);
- 1,080 hand-built objects (2 fill patterns, 10 `count` values, 9 `next` values, 6 `dropped` values up to `UINT64_MAX`, including every invalid combination): size, digest, 9 read indexes, one push (the whole object hashed) and the digest after it;
- 15 saturation steps from `dropped` = `UINT64_MAX - 3`, `UINT64_MAX - 1` and `UINT64_MAX`.

## Mutation check

91 hand-written source mutants of `diag_log.cpp` (the FNV constants and byte loop, every `state_ok` comparison, each null check, the repair of an invalid object, every field of the pushed entry and its slot, the wrap, count growth, the drop rule and its saturation, the window formula and index check in `log_read`, `log_size`, and every word, order and invalid-state rule of `log_digest`) were compiled against the unchanged tests. 90 were killed by a plain build and 1 survived. Ten kills lost the plain test report to a crash or a hang (a null check removed, a weakened `state_ok`, the repair removed, the read index check weakened).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented, the test object unchanged), the same 90 are killed and the same 1 survives; no mutant was hidden from the plain build. Run directly under the sanitizers, the ten report as follows:

- null `log` or `out` check removed: `reference binding to null pointer of type 'const struct DiagLog'` (push, read, size, digest), `store to null pointer of type 'struct DiagLog'` (clear) and `store to null pointer of type 'struct LogEntry'` (read).
- `state_ok` with `||`: `index 4294967295 out of bounds for type 'LogEntry [64]'`.
- `state_ok` without the `count` check: the digest loops about four billion times; the run is stopped by the harness timeout.
- repair removed and `log_read` rejecting the newest entry: failed `CHECK`s (`diag_log_tests.cpp` lines 252 and 292 of the file at this commit) before the later crash that lost the report in the plain build.

The unmutated source passes all tests under the same flags.

Survivor in both builds, argued:

- the `log_read` slot written as `(next + index + 64 - count) % 64` instead of `(next + 64 - count + index) % 64`: the same sum in a different order, with the same value in unsigned 32-bit arithmetic for every input (including wrap). Equivalent, not a behavior change.

Not run: Windows (CI only), Android, the editor, `tools/ci/test.py`, fuzzing, a full local build.
