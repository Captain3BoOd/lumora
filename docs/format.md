# Scene package format (Section 8)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 8. Scene package format: `.lumora`

A `.lumora` file is a **single ZIP64 container** (confirmed: one compressed file). ZIP is chosen because it is openly specified, streamable, debuggable with any unzip tool, and supports per-file compression. The format is versioned and documented in `docs/format.md` from day one; it is an **open format** by decision (D7-adjacent owner intent:).

```
scene.lumora (ZIP64)
 manifest.json # format version, engine min version, scene name/author, entry points
 scene.json # scene graph: nodes, transforms, components, materials, lights, cameras
 settings.json # author-exposed, user-tweakable parameters (sliders/colors/toggles)
 scripts/*.lua # behavior
 shaders/src/*.glsl # optional, kept for openness/debugging
 shaders/bin/ # compiled at EXPORT time:
 *.spv # SPIR-V (Vulkan)
 *.gles # GLSL ES 3.2 source (GLES fallback path)
 *.dxil # DXIL (D3D12 / future Windows runtime)
 models/*.glb # glTF 2.0 binaries
 textures/*.ktx2 # Basis Universal / ASTC / ETC2 compressed
 images/* # png/webp for UI-ish sprites
 audio/* # ogg/opus
 video/* # mp4 (h.264), optional
 thumbnail.png # library preview
 checksums.txt # per-file SHA-256, verified at load
```

Export-time shader compilation (D8) is performed by the editor's export pipeline (and the `packer` CLI): GLSL → glslang → SPIR-V → SPIRV-Cross → GLSL ES + HLSL → DXC → DXIL, on both editor hosts (the pinned Linux DXC produces DXIL identically). The phone never runs a shader compiler. **Scene cardinality:** one scene per package in v1 (a project's `scenes/*.scene.json` are exported one package each); multi-scene packages are a post-v1 candidate with manifest semantics to be defined then.

`settings.json` deserves emphasis: the scene author declares user-facing knobs ("Sky color", "Particle density", "Speed") with types and ranges; the Android app auto-generates a settings screen for the scene from it. This is how "the user decides quality and speed" (D14) reaches each scene. The settings schema is formal: stable setting IDs (never display labels as identity), localization keys, min/max/step/default validation, typed colors with color space, enums, conditional-visibility rules, persistence scope, reset behavior, and version migration. Lua writes to settings are validated against the schema.

### 8.1 Versioning and migration

- The package carries four versions: package format version, scene schema version, Lua API version, and minimum engine version, plus declared feature requirements.
- Every node, asset, and setting has a stable UUID; names and array positions are never identity.
- Load policy: older packages are migrated in memory on load (the original file is never overwritten); newer-than-supported packages are rejected with a clear message; unknown fields are ignored and preserved on re-export where practical. Migration rules and backward-compatibility policy live in `docs/format.md` from M3.

### 8.2 Asset dependency model

- The scene graph is a strict tree of nodes; assets (models, textures, audio, video, scripts, shaders) are shared resources referenced by UUID from a per-package asset table, deduplicated on pack.
- Cyclic references are rejected at validation; external references (paths outside the package) are prohibited.
- Missing assets are load errors listed by the validator, not silent gaps. Large scenes stream: the scene loads a minimal set first and streams the rest on worker threads. **Streaming policy:** priority-ordered load queues (visible-first), cancellable loads, defined placeholder behavior, LRU eviction within per-tier memory budgets, mip-level streaming for large textures, and Android `onTrimMemory` handling that sheds caches and lowers budgets under pressure. An asset that misses its deadline keeps its placeholder and the frame renders anyway; a persistent stream failure degrades the scene (placeholder plus diagnostics entry), never hangs the loader.

### 8.3 Texture strategy

- Source images are compressed at export to **KTX2 with Basis Universal** (UASTC for quality-critical, ETC1S for the rest): one universal representation per texture, transcoded on the device at load into the best supported GPU format (ASTC on Mali-G76, ETC2 as the universal GLES 3.x fallback, BC7/BC on desktop (Windows and Linux), RGBA32 as the last resort). No per-GPU duplicate variants inside the package; the transcode cache survives between loads.
- Mipmaps are generated at export; alpha modes and color space are preserved in KTX2 metadata. Texture memory budgets per quality tier are estimated and enforced by the exporter, which warns before packing. Source images stay in the editor project; only processed textures ship in the package. Video files are not transcoded at export (h.264/mp4 as authored, with a validator warning when they exceed device decode limits). KTX2/Basis transcode cost on the device is measured at M4 against the 2-second cold-load budget (2 fast A76 + 6 slow A55 cores); where it does not fit, the exporter offers a per-scene pre-transcoded ASTC profile in the export profiles (8.5) so the device skips transcoding, and transcoded output is cached between loads.

### 8.4 Reproducible export

- "Reproducible" means **byte-identical output for unchanged input** as a goal, guaranteed as semantic identity as a minimum. The packer uses stable ZIP entry ordering and fixed timestamps, stable JSON key ordering and float formatting, pinned shader/texture tool versions (recorded in the manifest), and generated IDs derived from stable source UUIDs rather than randomness. What determinism is and is not guaranteed is documented in `docs/format.md`.

### 8.5 Backend-specific export profiles

The exporter supports optional package profiles so a scene does not have to carry every backend's artifacts: **Android Vulkan-only**, **Android GLES-only**, **Windows D3D12-only**, **Desktop Vulkan (Windows + Linux)**, and **full multi-platform** (the default). The manifest declares which backends and artifacts are included, and the loader reports a clear "this package has no artifacts for your backend" error instead of failing mysteriously. The full multi-platform package stays the default so a scene shared with anyone just works; profiles exist to shrink size for known-target sharing.


### 8.6 Initial numeric limits and glTF extension list

Validators and fuzzers need numbers as targets, so every cap that other sections describe as "defined in docs" is pinned with a number at M3 in `docs/format.md` and `docs/security-model.md`: package size, entry count, per-entry size, texture memory, node count, script size and count, audio and video size and duration. This plan does not invent the values before the M3 measurements on the reference phone.

Supported glTF extensions are an allowlist, finalized at M3. Initial candidate list (from general knowledge, **unverified** against the current Khronos registry): `KHR_texture_basisu`, `KHR_mesh_quantization`, `KHR_materials_unlit`, `KHR_lights_punctual`, `KHR_texture_transform`, `EXT_meshopt_compression`. Anything not on the list is rejected at import with a clear message.

Skinning limits (maximum joints per skin, maximum influences per vertex) are pinned at M3 against the measured uniform-buffer limits of the Mali-G76 reference phone, not assumed. Skins above the limit are rejected at import, or split by the importer if a tested splitting rule exists.

### 8.7 manifest.json fields (M3 first slice)

Implemented in `core/scene/manifest.h` and covered by `tests/scene/scene_tests.cpp`. These names are the first concrete draft of the manifest; the rest of section 8 stays normative.

| Field | Type | Rule |
|---|---|---|
| `format_version` | whole number | required, at least 1; larger than the engine supports is rejected |
| `scene_schema_version` | whole number | required, same rule |
| `lua_api_version` | whole number | required, same rule |
| `min_engine_version` | string `major.minor.patch` | required; a newer engine than this build is rejected |
| `name` | string | required, 1 to 128 bytes |
| `author` | string | optional, at most 128 bytes |
| `entry.scene` | package-relative path | required |
| `entry.settings` | package-relative path | optional |
| `entry.script` | package-relative path | optional; one Lua file of at most 64 KB defining `on_parallax` (docs/scripting-slice.md); the loader checks that it exists and is within the size cap |
| `backends` | array of `vulkan`, `gles`, `d3d12` | optional; absent means a full multi-platform package; an unknown name is rejected |

Unknown fields are ignored (section 8.1). Entry paths must not be empty, absolute, contain a drive prefix, backslashes, control characters, or `.`, `..` or empty components. The full archive entry policy is `core/package/entry_policy.h`.

### 8.8 Checksums, scene.json draft and the loader order

Implemented in `core/scene/checksums.h` and `core/scene/scene.h`, covered by `tests/scene/`. This is the first concrete draft; the rest of section 8 stays normative.

- `checksums.txt`: one line per file, `<64 lowercase hex SHA-256>`, two spaces, the package-relative path. Duplicate paths, uppercase hex and empty paths are errors.
- Load order: read `checksums.txt`, verify every other file against it (unlisted files, missing files and mismatches are rejected), and only then parse `manifest.json` and the scene file. No content is parsed before it is verified.
- Load-time budgets: after the scene parses, the loader checks node count, script size and the pixel size of each texture asset against `core/package/load_budget.h` (default limits), using only the PNG or PPM header. A package over a budget is rejected with `LoadError::ResourceBudget` and the verdict name. A texture whose header cannot be read is not budgeted here; the decoder rejects it on its own. Evidence: unit tests only.
- `scene.json` draft: an `assets` array (`id`, `kind` = `texture`, `path`) and a `root` node tree. A node has `id` (unique), optional `name`, `transform` (`position`, `scale`, `rotation_z_degrees`), optional `layer` (`asset`, `parallax_depth` in 0..1) and `children`. Unknown fields are ignored, references to unknown assets are errors.
- Signature block: the reserved entry `signature.lumora` is rejected by this loader (`SignatureUnsupported`), because a loader that cannot verify a signature must not ignore it. Writing and verifying signatures stays a post-v1 candidate as in docs/security-model.md.
- ZIP container (`core/scene/zip_store.h`): the writer and reader handle STORED and DEFLATE (method 8) entries, using the dependency-free codec in `core/package/deflate.h` (contributor module, `docs/deflate.md`). No ZIP64, encryption, data descriptors or multi-disk. The reader bounds-checks every offset, checks CRC-32, local and central name agreement, duplicates, entry-count and total-size caps, inflates with the declared size as the output cap, and refuses entries whose declared size exceeds 200 times the compressed size plus 1 KB (compression bomb guard). The writer deflates an entry only when it gets smaller and stays inside that ratio, so every package it writes also loads. Tests include truncation and byte-flip runs under ASan and UBSan locally; they are not a fuzzing campaign.
- Packer: `lumora-packer pack <dir> <out.lumora>` reads a source directory (symlinks and a pre-existing `checksums.txt` are refused), generates `checksums.txt`, runs the full loader on the result and only then writes the ZIP (deflate level 6). `lumora-packer verify <file>` reads an archive and runs the loader. The sample scene is `samples/aurora` (placeholder PPM textures; KTX2 comes with M4).
- Contributor interfaces on main: `core/package/deflate.h` (raw DEFLATE inflate and deflate, stub implementation) and `core/render2d/cpu_renderer.h` (CPU reference renderer for 2D parallax scenes, stub implementation, not yet assigned). Their headers are fixed; implementations land by pull request.
