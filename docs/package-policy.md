# Package entry policy

Implements the entry-name and entry-set rules of "Archive defenses" in `docs/security-model.md` as pure functions in `core/package/entry_policy.h` (`lumora::package`). There is no ZIP library, no file system access and no new dependency. The ZIP reader (a later milestone) calls these functions before it touches an entry. This page records the behavior as implemented and tested in `tests/package_policy_tests.cpp`; the tests are the reference when this page and the code differ.

Evidence label: unit tests only (CI-verified, no real archives are read).

## validate_name

Rules apply in this order and the first match is returned:

1. `Empty`: the name has zero bytes.
2. `TooLong`: more bytes than `Limits::max_name_bytes` (bytes, not characters).
3. `InvalidUtf8`: strict UTF-8. Overlong forms, surrogates (U+D800 to U+DFFF), values above U+10FFFF and stray continuation bytes are rejected.
4. `ControlCharacter`: any byte 0x00 to 0x1F, or 0x7F.
5. `Backslash`: any `\`. A leading `\` therefore reports `Backslash`, not `Absolute`.
6. `Absolute`: leading `/` (this includes `//server/share`), or an ASCII letter followed by `:` at the start (a drive letter, with or without a following slash).
7. `EmptyComponent`: any empty component when split on `/`, including a trailing slash. A directory entry such as `dir/` is rejected here; the reader is expected to handle directory entries before calling the policy.
8. `Traversal`: any component equal to `..`.
9. `TrailingDotOrSpace`: any component ending in `.` or a space. A single `.` component is rejected by this rule.
10. `WindowsReserved`: any component whose base name (the part before the first `.`, ignoring trailing spaces) equals CON, PRN, AUX, NUL, COM1 to COM9 or LPT1 to LPT9, in any letter case. `CON.txt` and `dir/NUL.tar.gz` are rejected; `CONS`, `COM0` and `COM10` are accepted.

A leading dot (`.hidden`) and dots inside a component (`a..b`) are accepted.

## validate_entries

Order of checks. The first failure stops the check and `first_index` is the offending entry.

1. Entry count: more entries than `max_entries` gives `TooManyEntries` with `first_index == max_entries`.
2. For each entry in order: name (`BadName`, with `name_verdict`), then size (`EntryTooLarge` when `uncompressed_bytes > max_entry_bytes`), then ratio (`RatioTooHigh`), then the running total: `SizeOverflow` if adding the entry would overflow 64 bits (checked before comparing), then `TotalTooLarge` if the total exceeds `max_total_uncompressed_bytes`.
3. Collisions, over the whole set in entry order: for each entry against the earlier ones, an exact duplicate (`DuplicateName`), then an ASCII case-fold collision (`CaseFoldCollision`), then a normalization collision (`NormalizationCollision`). `other_index` is the earliest matching entry.

Ratio rule: compressed 0 with uncompressed above 0 is `RatioTooHigh`; both 0 passes; otherwise the entry passes when `uncompressed <= max_compression_ratio * compressed`, computed without overflow. Equality passes.

Size limits at the uint64 maximum: one entry of the maximum size passes when all limits are the maximum, and a second non-empty entry gives `SizeOverflow`.

## Unicode handling that is supported

The normalization fold maps an ASCII letter followed by one of U+0300 (grave), U+0301 (acute), U+0302 (circumflex), U+0303 (tilde), U+0308 (diaeresis), U+030A (ring above) or U+0327 (cedilla) to the precomposed Latin-1 Supplement letter, where one exists (for example `e` + U+0301 and U+00E9). Two names that are equal after this fold collide. The fold is applied once per letter; a second mark after the first is left as is.

## Open cases (not handled, no rejection rule)

- Case folding is ASCII only. `U+00C9` and `U+00E9` do not collide, and an uppercase letter with a decomposed accent does not collide with the lowercase precomposed form.
- Normalization covers only the pairs above. Other scripts (for example Hangul), other marks, reordering of several marks, and compatibility forms are not folded.
- C1 control characters (U+0080 to U+009F) are valid UTF-8 and are accepted.
- Windows-invalid punctuation (`<`, `>`, `:` after the first position, `"`, `|`, `?`, `*`) is accepted. A name such as `a:b` is NTFS alternate-stream syntax on Windows.
- Bidirectional controls and zero-width characters are accepted.
- Reserved names with superscript digits (such as COM followed by U+00B9) are accepted.
- The sum of compressed sizes is not tracked.
