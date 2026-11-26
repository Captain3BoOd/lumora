# glTF binary loader (core/glb)

`core/glb/glb.h` and `glb.cpp` load a `.glb` file (glTF 2.0 binary) from untrusted bytes into plain vectors. It is pure CPU code with
no file system and no GPU. Evidence label: software (CPU) only. It does not render anything and does not load materials, textures,
nodes, skins or animations yet; those are later M4 steps.

## Supported subset

Anything outside this list returns `Unsupported`, never a guess:

- Triangle primitives only (`mode` 4, the default).
- `POSITION` and `NORMAL` as FLOAT VEC3, `TEXCOORD_0` as FLOAT VEC2. Every float must be finite.
- Indices as UNSIGNED_BYTE, UNSIGNED_SHORT or UNSIGNED_INT scalars, or no indices (then `0, 1, 2, ...`; the vertex count must be a multiple of 3).
- One embedded BIN chunk as buffer 0. No external `uri`, no second buffer, no sparse accessors, no accessor without a buffer view.
- No entry in `extensionsRequired`.

## Container rules

- Header: magic `glTF`, version 2, total length equal to the byte count, at most `max_file_bytes`, multiple of 4.
- The first chunk is JSON (type `JSON`), then an optional BIN chunk (type `BIN`), then the end of the file. Chunk lengths are multiples of 4
  and must fit in the file. Trailing chunks are rejected.
- The JSON text is at most `max_json_bytes` (default 4 MiB) and is parsed by the project JSON parser with a depth cap of 16.
  The structural pre-scan in `core/package/json_limits.h` is not used here: it is not implemented on main yet.
- `asset.version` must start with `2.`.

## Bounds

All offsets, lengths and counts come from JSON numbers that must be integers in `[0, 2^31)`, so every sum below fits in `uint64`.

- A buffer view must satisfy `byteOffset + byteLength <= BIN length`. `byteStride` is 4 to 252 and a multiple of 4.
- An accessor must have `count >= 1`. Its last element ends at `byteOffset + (count - 1) * stride + element size`, which must be at most the
  view length. With `count < 2^31` and `stride <= 252` this is below `2^40`. The absolute start (`view offset + accessor offset`) and the stride
  must be multiples of the component size.
- `buffers[0].byteLength` must not exceed the BIN length and the BIN length may exceed it by at most 3 padding bytes.
- Normals and texture coordinates must have the same count as the positions. Every index must be below the vertex count; the index count must be
  a multiple of 3.
- Memory is reserved only after the accessor was checked against the view, and counts are checked against the limits first
  (`max_vertices`, `max_indices`, `max_total_vertices`, `max_meshes`, `max_primitives`).

## Status order

The checks run in this order and the first failure is returned: `TooSmall`, `BadMagic`, `BadVersion`, `BadLength`, `BadChunk`, size limit,
`BadJson`, `BadSchema` and `Unsupported` for the document root and buffers, buffer views, accessors, then meshes in order. On any failure the model is empty.

## Evidence

`tests/glb/glb_tests.cpp` builds files byte by byte and checks exact values, every status, the limits, the three index types, interleaved
views, the implicit index case, name cutting, every truncation of a valid file and every single bit change (no crash, no out of range index; run
under the sanitizers in CI). The golden digest `0x0a3d1eac2b13743d` of the sample model comes from an independent Python script that builds the
canonical byte stream from the intended values, not from the file. A fuzz target (`tools/fuzz/fuzz_glb.cpp`) with a seed corpus runs as a replay
test on every compiler.
