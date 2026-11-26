# Capabilities

`core/package/capabilities.h` (`lumora::package`) holds the capability state machine of "Runtime capability enforcement" in `docs/security-model.md`: a manifest declaration never grants anything by itself, the user grants or denies per scene, every capability request gets an explicit status, and a grant is reset when a scene update declares more. Pure functions on small values: no I/O, no platform calls, no persistence. The caller stores the `GrantRecord` for each installed scene and reloads it.

Evidence label: unit tests only (CI-verified).

## Capabilities and names

| Value | Capability | Manifest name |
| --- | --- | --- |
| 0 | `Microphone` | `microphone` |
| 1 | `Notifications` | `notifications` |
| 2 | `ActivityRecognition` | `activity_recognition` |
| 3 | `CoarseLocation` | `coarse_location` |
| 4 | `Count_` | not a capability |

Name matching is exact: lowercase, the full bytes of the name. Wrong case, leading or trailing whitespace, an empty name and names with an embedded NUL byte are all unknown (no C-string truncation; `"microphone\0x"` does not match). `capability_name` returns an empty string for `Count_` and for any value above it. `capability_from_name` leaves `*out` untouched when it returns false, and a null `out` is allowed: the result still says whether the name matches.

Sets are bit masks (`CapabilitySet`, bit `i` for capability `i`). Every function checks the capability value before computing a bit, so values of 32 or more never shift.

## parse_declared

`parse_declared(names, max_names = 32)` turns the manifest `uses` list into a set.

1. `TooManyNames` when `names.size() > max_names`, with `index == max_names`. This comes first, so it wins even when an earlier name is unknown.
2. Then per name in input order: `UnknownCapability`, then `DuplicateCapability` (the capability was already named; `index` is the second occurrence).

The first failure wins and `declared` is 0 on every failure. An empty list is `Ok` with an empty set. `max_names` 0 allows only the empty list. There is no implicit four-name limit: with a larger `max_names`, a list of five names fails as unknown or duplicate. `SIZE_MAX` works.

## Grants

`GrantRecord` has `declared` (from the installed manifest) and `granted`.

- `grant`: returns false and changes nothing when the capability is not declared, is `Count_` or above, or the record pointer is null. Granting twice succeeds and changes nothing.
- `revoke`: clears the bit and returns true, also when the capability was not granted. For `Count_` or above it returns true and changes nothing. A null record returns false.
- Each touches only its own bit. Neither repairs stray `granted` bits outside `declared`; only `reconcile_update` does.
- Revocation shows in the next `query`, also while the scene runs. There are no callbacks or events.

## reconcile_update

`reconcile_update(old_record, new_declared)` returns the record for the new package.

1. Old `declared`, old `granted` and `new_declared` are masked to the four valid bits. High bits never trigger a reset and never appear in the output.
2. Old `granted` is repaired: `granted & declared`.
3. If the new set has any capability the old declared set did not have, every grant is reset: `granted` is 0.
4. Otherwise (a subset, including equal sets and the empty set) the grants that are still declared are kept: `granted = repaired & new declared`.

The output `declared` is the masked new set. An empty old set with a non-empty new set resets (grants stay 0).

## query

`query(record, capability, platform)` returns the first match in this order, without changing anything:

1. `NotDeclared`: not in `declared`, or `Count_` and above. A granted bit outside `declared` does not help.
2. `PermissionDenied`: declared but not granted.
3. `Unsupported`: not in `platform.supported`. Active without supported is still `Unsupported`.
4. `TemporarilyUnavailable`: not in `platform.active`.
5. `Available`.

## to_string

`to_string` returns the identifier spelling of the verdict or status, and `"Unknown"` for a value outside the enum.

## Tests

- `tests/capabilities/capabilities_tests.cpp` has hand-written tests for every rule above, including revocation sequences and invalid capability values (4, 5, 31, 32, 255).
- Reference: `tools/capabilities_ref.py` is an independent Python implementation (sets of names, no bit masks). It prints the name table, enum values and FNV-1a 64 digests over exhaustive enumerations, between `BEGIN GENERATED REFERENCE` and `END GENERATED REFERENCE` markers in the test file; the tests recompute each digest from the C++ functions. `python3 tools/capabilities_ref.py --check tests/capabilities/capabilities_tests.cpp` fails when the block is stale; without arguments it prints the block.
- Digest coverage: `query` over all 16 declared x 16 granted x 16 supported x 16 active sets x 5 capability values (327680 cases); `query` with stray high bits and invalid capability values (18432 calls); `reconcile_update` over 16 x 16 x 16 sets with 4 high-bit patterns on each side (65536 cases); `grant` and `revoke` over 16 declared x 32 granted x 9 capability values (4608 pairs); `parse_declared` over every list of up to 5 names from a pool of 10 (valid names, wrong case, trailing space, empty, near miss, embedded NUL) with 9 limits (999999 cases).
- Oracle: a second implementation inside the test using arrays of booleans (no bit tricks, no shared code) is compared with the functions over all `query` inputs including capability 5 (393216 cases), all `reconcile_update` inputs (4096), all `grant`/`revoke` inputs (3584 pairs) and every `parse_declared` list of up to 4 names (99999 cases).
- Mutation: 80 hand-written mutants of `core/package/capabilities.cpp` (flipped masks and comparisons, swapped check order, dropped checks, wrong indices, changed reset rule, stray-bit repair added or removed, name table edits) were compiled one at a time against the unchanged tests. 77 failed at least one test. Two of the 77 (dropping a null-pointer check) are caught by a crash rather than a failed assertion. Three survived, all equivalent:
  - Not masking old `declared` to the valid bits: the new set is masked, so `new & ~old` has no high bits either way.
  - Not masking old `granted` to the valid bits: it is intersected with `declared` (masked) and then with the new set.
  - Not repairing old `granted` (`granted & declared`): the result is intersected with the new declared set, which in the keep branch is a subset of the old declared set, and the reset branch gives 0. The repair is unobservable in the output; the header requires it, so it stays.
  The mutation script is not committed; the counts are a one-time result.
