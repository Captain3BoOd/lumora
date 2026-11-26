# Load budget

`check_load_budget` in `core/package/load_budget.h` (`lumora::package`) checks the resource numbers of a scene package against limits before anything is decoded or allocated, as in "Resource limits" of `docs/security-model.md`. Pure numbers in, verdict out: no decoding, no file system, no allocation beyond the result. A package over any budget is rejected with a verdict and the offending item, never partly loaded.

Evidence label: unit tests only (CI-verified).

The inputs must come from validated headers of the real files. This module trusts nothing else about them, and it does not check that the numbers match the file contents.

## Limits

`LoadBudgetLimits` defaults:

| Field | Default |
| --- | --- |
| `max_textures` | 1024 |
| `max_texture_dimension` (width and height each) | 8192 |
| `max_texture_bytes_each` (all mips and layers) | 128 MiB |
| `max_texture_bytes_total` | 512 MiB |
| `max_script_bytes_each` | 1 MiB |
| `max_script_bytes_total` | 4 MiB |
| `max_nodes` | 100000 |
| `max_audio_bytes_each` | 32 MiB |
| `max_audio_bytes_total` | 128 MiB |
| `max_audio_ms_each` | 600000 (10 minutes) |

Every limit accepts the value itself and rejects the value plus one. A limit of 0 accepts 0 and rejects any positive value: `max_textures` 0 rejects any texture, `max_texture_dimension` 0 rejects every otherwise valid texture. Empty lists, zero-byte scripts and clips, zero duration and zero nodes pass.

## Check order

The first failure wins. The stages run in this order, so an audio problem never hides a texture problem, and within a stage items run in input order.

1. Textures. First `TooManyTextures` (more textures than `max_textures`, `item_index` 0, before any single texture). Then per texture, in order:
   1. `BadTexture`: zero width, height or layers, `mip_levels` 0, `mip_levels` above the full chain (`floor(log2(max(width, height))) + 1`), or a format value outside the enum. This comes before any dimension check, so a 9000 x 9000 texture with 15 mips is `BadTexture` and with 14 mips is `TextureDimension`.
   2. `TextureDimension`: width or height above `max_texture_dimension`.
   3. `SizeOverflow`: the texture's byte count does not fit 64 bits (any level, the chain sum, or the layers multiplier). Checked before the size limit, even when the limit is smaller.
   4. `TextureTooLarge`: bytes above `max_texture_bytes_each`.
   5. `SizeOverflow` if the running total plus this texture does not fit 64 bits, then `TextureBudget` if it is above `max_texture_bytes_total`.
2. Scripts: `ScriptTooLarge` (one above `max_script_bytes_each`), then `SizeOverflow` for the running total, then `ScriptBudget` (total above `max_script_bytes_total`). There is no count limit for scripts.
3. `TooManyNodes`: `node_count` above `max_nodes`, `item_index` 0.
4. Audio: `AudioTooLarge` (bytes above the each limit), `AudioTooLong` (duration above `max_audio_ms_each`), then `SizeOverflow` for the running total, then `AudioBudget`. There is no count limit for audio.

## Result fields

`item_index` is the offending texture, script or clip, and 0 for `TooManyTextures` and `TooManyNodes`. The three totals count only items accepted before the offending one; the rejected item is never added. A later stage's total stays 0 when an earlier stage fails. A script failure keeps the final texture total. A node or audio failure keeps the final texture and script totals. `Ok` carries all three final totals.

## Texture size

`texture_bytes` returns the bytes of one texture. Level `l` below `mip_levels` has `max(1, width >> l)` by `max(1, height >> l)` texels. For `Bc1` and `Bc7` each level is `ceil(w / 4) * ceil(h / 4)` blocks, at least one. The unit is 1 byte (`R8`), 2 (`Rg8`), 4 (`Rgba8`), 8 per block (`Bc1`) or 16 per block (`Bc7`). Levels are summed and the sum is multiplied by `layers`. Tight packing: no row, level or layer padding or alignment. It returns false for an invalid texture or an overflow and leaves `*out` untouched. `check_load_budget` runs its own validation first, so false at the size step means `SizeOverflow`.

| Texture | Bytes |
| --- | --- |
| 1 x 1 Rgba8, 1 mip | 4 |
| 256 x 256 Rgba8, 1 mip | 262144 |
| 256 x 256 Rgba8, 9 mips (full chain) | 349524 |
| 256 x 256 Rgba8, 1 mip, 6 layers | 1572864 |
| 1024 x 1024 Rgba8, 11 mips | 5592404 |
| 1024 x 1024 Bc1, 11 mips | 699064 |
| 1024 x 1024 Bc7, 11 mips | 1398128 |
| 5 x 5 Bc1, 3 mips | 48 |
| 8192 x 8192 Rgba8, 1 mip | 268435456 (above the 128 MiB default, `TextureTooLarge`) |
| 8192 x 8192 Bc7, 14 mips | 89478512 |

Overflow is reachable only when `max_texture_dimension` is raised: at the default, the largest texture is far below 2^64 bytes.

## Tests

- `tests/load_budget/load_budget_tests.cpp` has hand-written sizes, check order tests, `to_string` tests, and scenes from the reference script.
- Reference: `tools/load_budget_ref.py` is an independent Python implementation (Python integers, explicit 2^64 - 1 comparisons, no shared code). It builds scenes at 0, limit, limit plus one and the integer maximum for every limit, plus texture sizes and overflow cases, and prints the expected results as a C++ block between `BEGIN GENERATED REFERENCE` and `END GENERATED REFERENCE`. `python3 tools/load_budget_ref.py --check tests/load_budget/load_budget_tests.cpp` fails when the block is stale; without arguments it prints the block.
- Oracle: a naive implementation inside the test, using 128-bit values built from two uint64 halves (no compiler extensions), runs 60000 seeded random scenes with limits set at and next to values in the scene. The test requires every verdict to occur at least 20 times.
- Mutation: 87 hand-written mutants of `core/package/load_budget.cpp` (flipped comparisons, off-by-one limits, swapped or dropped checks, dropped overflow guards, wrong format sizes, wrong chain length, changed indices and totals) were compiled one at a time against the unchanged tests. 85 failed at least one test. Two survived, both equivalent:
  - Replacing the `a != 0` guard in the multiply helper by a division by 1 when `a` is 0: with `a` 0 the product is 0 and fits either way.
  - Dropping the overflow check on `width * height` of one level: both are at most 2^32 - 1 after block rounding, so the product always fits in 64 bits. The check is a guard that cannot trigger.
  The mutation script is not committed; the counts are a one-time result.
