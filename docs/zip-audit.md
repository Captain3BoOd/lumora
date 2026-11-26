# ZIP container audit

Implements the "overlapping or shared local-header data", "central-directory versus local-header mismatches", "multiple end-of-central-directory records and prepended or appended payloads" and "ZIP64 field mismatches" rules of `docs/security-model.md` (Archive defenses) as `audit_zip` in `core/package/zip_audit.h` (`lumora::package`). It runs on a byte buffer before any entry is read: no inflate, no file system, no recursion, no dependencies. It checks the layout and the agreement between the central directory and the local headers, then returns the file entries. The caller then runs `validate_entries` from `core/package/entry_policy.h` on `to_entry_infos(result.entries)`. The header comment is the binding statement of the check order; this page repeats it with the exact bounds. Where this page and the tests differ, the tests in `tests/zip_audit/zip_audit_tests.cpp` are the reference. Evidence label: unit tests only (CI-verified, no real packages are read).

## Accepted layout

`[local header + data] x N` in central directory order with no gap and starting at offset 0; the central directory starting exactly where the last entry data ends; optionally the ZIP64 end record and locator; the end record, ending exactly at the buffer end after its comment. Everything else is rejected. The audit never repairs or guesses.

## Check order

The first failure wins. Every stage runs over all entries before the next stage starts. `offset` is the byte offset reported with the verdict, `index` the central directory index (0 when no entry is to blame). On any failure `entries` is empty and `directory_count` is 0.

| Step | Verdict | Rule | Offset |
|---|---|---|---|
| 1 | `TooLarge` | size above `max_archive_bytes` (checked before everything else) | 0 |
| 1 | `TooSmall` | size below 22 bytes (also a null pointer) | 0 |
| 2 | `NoEndRecord` | no end record signature in the scan window; or the one found has a comment that runs past the buffer; or its comment is longer than `max_comment_bytes` | 0, or the record |
| 2 | `MultipleEndRecords` | two end record signatures in the window, comment included | the second signature |
| 2 | `TrailingData` | the single record's comment ends before the buffer end | first byte after the comment |
| 2 | `MultiDisk` | disk or central directory disk not 0, or entries on this disk differ from the total | the record |
| 3 | `Zip64Mismatch`, `MultiDisk` | see ZIP64 below | locator, record or end record |
| 4 | `TooManyEntries` | count above `max_entries`, directories included, checked before the directory is read | the end record |
| 4 | `CentralDirectoryBounds` | declared start plus size (uint64) is not exactly where the ZIP64 record, or the end record, starts; the start is above that position; or walking the directory by the declared lengths does not give exactly the declared count and end | the end record |
| 5 | `BadCentralHeader` | bad signature; empty name; name above `max_name_bytes`, extra above `max_extra_bytes`, entry comment above `max_comment_bytes`; an extra block that runs past the extra field | the central header |
| 5 | `MultiDisk` | disk start number not 0 | the central header |
| 5 | `Encrypted` | flag bit 0, 6 or 13 | the central header |
| 5 | `UnsupportedFeature` | flag bit 3 (data descriptor) or any bit except 0, 1, 2, 6, 11, 13; bits 1 and 2 with a method other than 8 | the central header |
| 5 | `UnsupportedMethod` | method other than 0 and 8 | the central header |
| 5 | `SpecialFile` | version-made-by OS is 3 (Unix) and the mode type is not regular (0x8000), directory (0x4000) or unspecified (0) | the central header |
| 5 | `BadCentralHeader` | name ends with `/` and the mode says regular; or the mode says directory and the name has no `/` | the central header |
| 5 | `BadSizes` | a directory (name ends with `/`) that is not method 0 with both 32-bit size fields 0 | the central header |
| 5 | `Zip64Mismatch` | entry ZIP64 extra rules, see below | the central header |
| 6 | `BadLocalHeader` | offset past the buffer or too close to its end for a header; bad signature; name above `max_name_bytes` or extra above `max_extra_bytes`; the header does not fit the buffer; broken extra; local ZIP64 rules | the local header |
| 6 | `HeaderMismatch` | any difference in name, flags, method, CRC, either size (after ZIP64 resolution), modification time or date. Extras may differ | the local header |
| 7 | `BadSizes` | stored with different sizes; deflate with 0 compressed bytes; data end beyond the buffer; a wrapping sum | the local header |
| 8 | `PrependedData` | first local header offset not 0; or an empty archive whose directory starts above 0 | the first local header, or the directory start |
| 8 | `OutOfOrder` | offset not above the previous entry's offset | the local header |
| 8 | `Overlap` | offset below the previous data end; or data that ends after the directory start | the local header |
| 8 | `Gap` | offset above the previous data end; or last data end below the directory start | the first uncovered byte |

Notes:

- The end record scan covers the last `min(size, 22 + 65535)` bytes. A signature counts when 22 bytes remain from it to the buffer end, so a signature in the last 21 bytes does not. A stored file whose content holds an end record signature inside that window is rejected as `MultipleEndRecords`; this is deliberate.
- ZIP64 record and locator signatures inside a comment do not count.
- The empty archive (zero entries, directory empty, starting at offset 0 and ending at the end record) is `Ok`.
- Truncated central headers: the walk in step 4 uses the three length fields only and does not look at signatures. A header that is cut short, or a variable part that runs past the directory, is therefore `CentralDirectoryBounds`, not `BadCentralHeader`.

## ZIP64

ZIP64 is optional, and all or nothing.

- A sentinel is a 0xFFFF count or a 0xFFFFFFFF directory size or offset in the end record. With any sentinel, the locator must sit immediately before the end record and the ZIP64 record immediately before the locator, else `Zip64Mismatch` (at the end record).
- Locator: disk with the record 0 and total disks 1, else `MultiDisk`; the record offset field must equal the real position, else `Zip64Mismatch`.
- Record: signature, size field exactly 44 (no extensible data), disks 0 and equal counts (`MultiDisk`), else `Zip64Mismatch`. Version fields are not checked.
- Each of count, directory size and offset: if the ordinary field is a sentinel the 64-bit value must be at or above it (canonical form), else it must be equal to it. Any other disagreement is `Zip64Mismatch`.
- A locator with no sentinel in the end record is `Zip64Mismatch` at the locator.
- Entry extra: only fields whose header field is 0xFFFFFFFF, in the order uncompressed size, compressed size, local header offset, exact payload length, each value at or above the sentinel. A sentinel without a block, a block without a sentinel, a second block, a wrong length or a small value is `Zip64Mismatch`.
- Local header: both 32-bit size fields are sentinels and a ZIP64 block with exactly the two sizes (uncompressed, compressed) follows, or neither. Any other combination is `BadLocalHeader`.
- Consequence for tests: a valid ZIP64 entry needs 4 GiB of data, so entry level ZIP64 is tested through failures only (sizes of 2^32, 2^63, uint64 max, offsets at the sentinel and at uint64 max). A valid ZIP64 end record is tested with 65535 entries.
- The sums of compressed and uncompressed sizes are checked for uint64 wrap, but no archive that passes step 6 can reach it (a ZIP64 uncompressed size forces a ZIP64 compressed size, which cannot fit the buffer). The check is a guard.

## Limits

`ZipAuditLimits` defaults: archive 4096 MiB, 10000 entries, name 255 bytes, extra 1024 bytes (central and local each, per entry), comment 1024 bytes (archive comment and each entry comment). Each limit accepts the value itself and rejects the value plus one; the tests cover that for each, and the uint64 maximum as a limit.

## Memory and time

One pass over the end record window, one walk of the directory, and one internal record per entry (about 100 bytes). The entry count is bounded by the directory size, which lies inside the buffer, so memory is bounded by the archive size. No recursion.

## Tests

- `tests/zip_audit/zip_audit_tests.cpp` builds archives from code with per-field control and covers each verdict, the limit, limit + 1 and uint64 maximum boundaries, and hostile cases: overlap bombs, second end record in a comment, prepended and appended data, mismatched headers, ZIP64 mismatches.
- Reference archives: `tools/zip_audit_ref.py` builds a set of archives with `struct` and `zlib` only, opens the valid ones with Python's `zipfile`, and prints them as hex with the expected verdict, offset and index written by hand. The block between `BEGIN GENERATED REFERENCE` and `END GENERATED REFERENCE` in the test file is that output. Regenerate: `python3 tools/zip_audit_ref.py` and replace the block; verify: `python3 tools/zip_audit_ref.py --check tests/zip_audit/zip_audit_tests.cpp`. The script is not run in CI.
- Mutation: for a reference archive, every bit of every structural byte is flipped in turn and the audit must not return `Ok`. Truncating the archive anywhere, or adding a byte, must not return `Ok` either. Exempt fields, whose flips may still give `Ok`: file data, entry and archive comment text, extra field content (central and local), version needed and version made by, internal attributes, external attributes (permission bits, DOS bits and the mode, which only matters on Unix and is tested on its own). Timestamps, CRC values and names are not exempt when changed in one header only; changed identically in both they are free, since they are not read.
