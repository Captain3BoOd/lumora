# Extraction plan

`plan_extraction` in `core/package/extract_plan.h` (`lumora::package`) turns an audited entry list into a plan: which directories to create, in which order, and whether the extraction fits. `cleanup_paths` lists what to delete after a cancelled or failed extraction. Both are pure functions: no file system access, no archive reads.

Evidence label: unit tests only (CI-verified). Nothing here has run against a real file system.

## Inputs and defaults

`ExtractLimits` defaults: `max_depth` 16, `max_path_bytes` 1024, `max_directories` 4096, `reserve_bytes` 64 MiB. Entry names are expected to have passed `validate_entries`; they are not revalidated.

The caller must extract into a target directory that is new and empty. `cleanup_paths` deletes every directory of the plan, so it must never be pointed at a tree that existed before.

## Check order

The first failure stops the plan and returns empty `directories` and `file_order`.

1. Per entry, in input order: `PathTooLong` (name bytes, separators included, above `max_path_bytes`), then `PathTooDeep` (components, file name included, above `max_depth`). The first failing entry wins; `entry_index` is that entry. Equality with a limit passes. `max_depth` 0 rejects every entry, `max_path_bytes` 0 rejects every non-empty name.
2. `FileDirectoryConflict`: a name that is a file and also a parent directory of another name. Names match when equal exactly, or equal after ASCII case folding alone, or equal after the Latin-1 normalization alone. The two folds are never composed (uppercase E plus U+0301 does not match lowercase precomposed e-acute), as in `docs/package-policy.md`. `entry_index` is the earliest entry whose name is the conflicting file. The plan keeps a private copy of the folds; a test checks every letter and mark pair against `validate_entries`.
3. `TooManyDirectories`: more distinct directories than `max_directories`. Directories are the exact byte strings of every non-empty proper prefix that ends at a `/`; no folding, so `a/x` and `A/y` count as two. Equality passes, 0 allows top-level files only. `entry_index` is the entry that introduces the first directory above the limit (the `max_directories + 1`th distinct directory).
4. `SizeOverflow`: the running sum of `uncompressed_bytes` overflows 64 bits (`entry_index` is that entry), or the total plus `reserve_bytes` overflows (`entry_index` 0). One entry of the integer maximum with a reserve of 0 passes.
5. `InsufficientSpace`: `free_bytes` below total plus reserve. Equality passes. `entry_index` 0, `total_bytes` and `required_free_bytes` filled.

On `Ok`, `total_bytes` and `required_free_bytes` are filled. On every other verdict except `InsufficientSpace` they are 0.

## Output order

- `directories`: each once, shallow depth first, byte order (unsigned bytes) within one depth. A parent always comes before its children.
- `file_order`: the entry indices in input order.

Directories that differ only in case (`a/x` and `A/y`) both appear in the plan. On a case-insensitive file system the second create reports "already exists"; the caller must treat that as success. This is an open case with no rejection rule.

## cleanup_paths

Order of the returned paths:

1. The partial file, only when `partial_file_exists` is true and `completed_files` is below the file count.
2. The completed files in reverse.
3. Every directory of the plan in reverse (children before parents).

`completed_files` is clamped to the file count. An index in `file_order` outside `entries` is skipped. A plan that failed gives an empty list. Deleting a path that is already gone is the caller's concern.

## Names that break the contract

Names are split on `/` as given and get no new verdict. Empty prefixes are skipped: `/a` gives no directory for the empty prefix, `a//b` gives directories `a` and `a/`, and `a/` gives `a`. Nothing crashes. Callers must run `validate_entries` first.

## Tests

- `tests/extract_plan/extract_plan_tests.cpp` covers each verdict, the check order, and every limit at 0, equal, plus one and the integer maximum (depth, path bytes, directory count, reserve, sums, free space), plus cleanup.
- Reference: `tools/extract_plan_ref.py` is independent Python (it shares no code with the C++). It derives the expected plans and cleanup lists for hand-listed cases and the letter plus mark pairs from `unicodedata`, and prints them as a C++ block between `BEGIN GENERATED REFERENCE` and `END GENERATED REFERENCE` markers in the test file. `python3 tools/extract_plan_ref.py --check tests/extract_plan/extract_plan_tests.cpp` fails when the block is out of date; run it without `--check` to regenerate it.
- Oracle: a naive reimplementation inside the test (quadratic scans, no hash sets, no shared code) runs 60000 seeded random cases against `plan_extraction`, with limits chosen at and next to each generated value.
- Mutation: 54 hand-written mutants of `core/package/extract_plan.cpp` (flipped comparisons, off-by-one limits, swapped check order, dropped checks, removed clamp, changed sort keys, wrong entry indices, reversed cleanup order) were compiled one at a time against the unchanged tests. 53 failed at least one test. Two of the 53 (removing the `completed_files` clamp, and listing the partial file when none is left) are caught by an out-of-range read that crashes the test binary rather than by a failed assertion. One survived, justified below. The mutation script is not committed; the counts are a one-time result.
- Survivor: dropping the exact-match test in the conflict check. It is equivalent: equal strings have equal ASCII folds, so the ASCII-fold test already catches them.
