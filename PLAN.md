# Lumora project plan and status

This file is the live view of the plan: what is done, what is in progress and what is missing. It is updated at every milestone and whenever the plan changes. The full specification is [LUMORA_MASTER.md](LUMORA_MASTER.md) and the topic documents under [docs/](docs/); where this file and a normative document differ, the normative document wins. Session-level detail is in [docs/PROJECT_STATE.md](docs/PROJECT_STATE.md).

Last updated: 2026-11-25. Current milestone: **M3, Scene format and packer (next).** M0 and M1 exit criteria as written in docs/milestones.md are met (CI-verified); carry-over items remain, listed in sections 4 and 5.

## 1. Vision

Lumora is a live wallpaper engine. A user authors a fully custom 2D or 3D scene (glTF models, GLSL shaders, Lua scripts, animation, audio, physics) in a desktop editor, exports one `.lumora` package, and the runtime renders it as the live wallpaper on Android (home and lock screen) and on the Linux desktop (GNOME on Ubuntu). Two programs, one shared C++ core, one renderer, one scene format, one scripting API.

## 2. Key decisions (summary)

Full decision log D1-D31 is in `LUMORA_MASTER.md`.

- Editor: Windows and Linux, C++ with Dear ImGui. Runtime: Android (minSdk 29, 64-bit `arm64-v8a` only) and a Linux desktop runtime. iOS is out of scope.
- Scripting: Lua 5.4, sandboxed. 3D assets: glTF 2.0. Shaders: GLSL, cross-compiled at export to SPIR-V, GLSL ES and HLSL.
- Rendering: one new in-house RHI with Vulkan, OpenGL ES 3.2 and Direct3D 12 backends.
- Performance: 60 fps baseline, 90 and 120 fps where the display allows; per-scene quality and frame cap for the end user.
- Interface languages: Arabic and English with full RTL support.
- Open source under Apache-2.0. Repository and CI exist from M0.
- Evidence honesty: CI-verified, WARP software evidence and owner-hardware-verified are different labels. Nothing is claimed verified on real Windows, Android or GNOME hardware without a real test.

## 3. Milestone status

Status values: not started, in progress, done (exit criteria met in CI, with the run linked in PROJECT_STATE.md).

| ID | Milestone | Status |
|---|---|---|
| M0 | Foundations and CI | done (exit criteria CI-verified, carry-over items open) |
| M0-M1 | GNOME wallpaper-method spike (agent-side evidence only) | source review done (docs/linux-wallpaper-matrix.md); nested-shell run still to do |
| M1 | RHI core: Vulkan, GLES 3.2, D3D12 build and WARP spike | done: exit criteria met and CI-verified, with first-slice backends and a carry-over list (section 5) |
| M2 | Runtime MVP: WallpaperService, pause rules, own process, crash breaker | done: exit criteria met and CI-verified (software evidence on API 29, 34, 36 emulators), with caveats in section 5b |
| M3 | Scene format and packer | in progress: JSON model, manifest, SHA-256 checksums, scene.json draft, in-memory loader, ZIP with deflate (contributor DEFLATE module), packer CLI, sample scene and round-trip tests are done; the settings.json parser and validator (contributor module, merged), the CPU reference renderer (contributor module) renders the packed sample in tests and, through a JNI entry, on the Android emulator CPU in the instrumented tests (CI-verified, software evidence); full-image goldens of the packed sample (rest and two parallax offsets) join the contributor's CPU golden harness; the package entry policy (345b22a), the JSON structural limits (3e55fdb) and the ZIP container audit (16d8447) are merged; the ZIP reader runs the container audit and the entry policy before reading entries (cecbb48, CI-verified); the extraction plan is merged (d77e61e); the JSON pre-scan runs before parsing in the manifest, scene and settings parsers (31bcb92, CI-verified; huge arrays are now rejected as NotJson); the load budget check runs in the package loader (header sizes only, 2f2db44, CI-verified); the importer wiring of the extraction plan remains |
| M3S | Vertical slice: 2D parallax scene end to end in CI | in progress: packed sample loads and renders on host CPU and the emulator CPU with pixel checks and full-image goldens (CI-verified, software evidence); the Lua host (M5 work pulled forward minimally, CI-verified) runs the aurora sample script `on_parallax`, driven by the `parallax_strength` setting, with a golden for strength 0.5 (host CPU, software evidence); the PNG codec (decoder with limits, encoder, docs/png.md, fuzz target and seeds) is merged and CI-verified on the CPU; the Android build of Lua, and the in-app settings UI are carry-overs; the CPU texture loader now decodes PNG (chosen by signature) as well as PPM, covered by a host test |
| M4 | Renderer features (glTF, PBR, 2D, text; shadows, post-FX, particles, video) | not started |
| M5 | Scripting: Lua sandbox, cancellation and recovery | partly pulled forward: Lua 5.4.9 host with sandbox allowlist, instruction and memory limits, one `on_parallax` callback, 15 tests incl. escape and runaway cases (CI-verified on Linux and Windows); the full API, cancellation and recovery are not started |
| M6 | Interaction layer (non-audio), flavors | not started |
| MT | Lumora text engine (Arabic shaping and bidi) | not started |
| M7a | Editor MVP | not started |
| M8 | Physics and audio | not started |
| M7b | Editor full | not started |
| M9 | Android app polish | not started |
| ML | Linux editor and Linux desktop runtime | not started |
| MW | Windows desktop runtime: scene behind desktop icons, multi-monitor, pause for fullscreen apps, D3D12 first (added at the owner's request) | not started |
| M10 | Release prep and owner hardware gates G1-G7 | not started |

Effort estimate: 85-144 weeks serial, about 20-33 months (MW adds 5-9 weeks), re-estimated at the M2 and M4 boundaries (`docs/milestones.md`).

## 4. M0 detail

CI evidence: https://github.com/Captain3BoOd/lumora/actions/runs/45568499611 (commit 8b14281, all five jobs green; evidence label CI-verified, software graphics only, no GPU, no real device).

Exit criteria (from `docs/milestones.md`): CI green on Linux and Windows and builds the Android app; the Windows editor empty-window smoke test passes on the Windows runner (CI-verified, no GPU); an API 29 emulator boots in CI and runs one instrumented test.

| Item | Status |
|---|---|
| Repository, Apache-2.0 license, README.md, PLAN.md | done |
| Plan documents, `tools/check_docs.py`, PROJECT_STATE.md | done |
| CMake superbuild skeleton (core, tests, editor) | done (CI-verified) |
| Dependency pins (Catch2, SDL3, Dear ImGui) and manifest | done (CI-verified) |
| Coding standards and C++ standard docs | done |
| Shared test entry point (`tools/ci/test.py`) | done |
| CI: docs check, Linux build and tests with ASan and UBSan | done (CI-verified) |
| CI: Windows build and editor empty-window smoke test (no GPU) | done (CI-verified) |
| CI: Android Gradle release build, ABI set and 16 KB alignment checks | done (CI-verified) |
| CI: API 29 emulator boot and one instrumented test, KVM answer recorded | done (CI-verified; KVM works on ubuntu-24.04 hosted runners) |
| Secret scan in CI, Dependabot | done (CI-verified) |
| CodeQL | done: workflow for C and C++ runs green on push, pull request and weekly. Result upload to the Security tab needs code scanning enabled on the repository, an owner setting that is not changed here |
| Branch protection requiring green CI (owner repository setting) | not started, repository settings are not changed by the agent |
| NDK choice for 16 KB alignment | done (NDK 27.2.12479018 with flexible page sizes flag; alignment checked in CI) |
| DXC and glslang version pins | partly: SPIRV-Cross and DXC pins recorded in docs/dependency-manifest.md, not yet consumed by CI; glslang observed 15.1.0 |
| WARP and software-Vulkan-on-Windows spike | done: WARP D3D12 works (see above). Software Vulkan on Windows works: the Vulkan backend builds against the system loader (spike option, pinned Vulkan headers) and the Vulkan tests pass on the Windows runner with a pinned Mesa lavapipe ICD registered in the registry (the loader ignores environment overrides when elevated). Label: CI-verified software evidence, no validation layer on Windows, never RX 570 evidence. Run: https://github.com/Captain3BoOd/lumora/actions/runs/45634033800 |
| GNOME spike start | done: source review of the reference extension recorded in docs/linux-wallpaper-matrix.md |
| Feature-test header check for `docs/cpp-standard.md` | not started |
| Null-backend RHI unit tests (33 test cases, external contribution by RashaBoByte, reviewed and merged after green CI) | done (CI-verified) |

## 5. M1 detail (exit criteria met)

| Item | Status |
|---|---|
| RHI interface (backend-agnostic types, `Device`, capability flags, backend support states) and Null backend with unit tests | done (CI-verified) |
| WARP D3D12 spike on the Windows runner | done: a D3D12 device at feature level 12_0 on the WARP adapter, a render-target clear, copy and CPU readback all worked. Evidence label: WARP software evidence, CI-verified, never RX 570 evidence. Run: https://github.com/Captain3BoOd/lumora/actions/runs/45570530990. D3D12 backend state stays not implemented until the backend exists. |
| Software Vulkan on Windows spike | done (see section 4) |
| GNOME wallpaper-method spike (source review, nested shell) | source review done and written in docs/linux-wallpaper-matrix.md (unverified on real hardware, GPL source read only, nothing copied); nested-shell run in CI still to do |
| Shader toolchain pins (glslang, SPIRV-Cross, DXC) | partly: glslang 15.1.0, validation layers and Mesa versions recorded as observed (Ubuntu packages), not hard-pinned; SPIRV-Cross vulkan-sdk-1.4.357.0 and DXC v1.9.2609 recorded as pins, not yet fetched by CI |
| Vulkan backend: instance, device, validation layer (logged active and asserted in the test), named buffers and textures, command recording, one render pass (clear and store), graphics pipeline from SPIR-V, submit and fence wait, texture readback | done as a first slice (CI-verified on the lavapipe software driver, run https://github.com/Captain3BoOd/lumora/actions/runs/45587423739). A test renders a clear plus a triangle and checks pixels, with zero validation messages. Missing: swapchain, descriptor sets and bindings, vertex buffers, depth, blending, sync beyond a fence, pipeline cache, other formats. |
| GLES 3.2 backend | done (first slice): headless EGL surfaceless context, buffers, render-target textures, program from GLSL ES source, clear, draw, readback. CI-verified on Mesa llvmpipe (OpenGL ES 3.2, software evidence, never phone-GPU evidence). The triangle golden image is byte-identical to the Vulkan lavapipe one. Still to do: SPIR-V to GLSL ES through SPIRV-Cross, Android EGL surface path |
| D3D12 backend build, link and DXIL on Windows CI | done (first slice): WARP-only device, buffers, textures, pipeline from DXIL (DXC from the Windows SDK, shader model 6.0), command recording, fence wait, readback. A triangle golden image matches its baseline in Windows CI. Evidence label: WARP software evidence, CI-verified, never RX 570 evidence. |
| Golden-image harness (lavapipe baseline, tolerance compare, diff artifacts, see `docs/golden-images.md`) | done (CI-verified, run https://github.com/Captain3BoOd/lumora/actions/runs/45590514447) |
| Emulator hello triangle through the RHI | done: an instrumented test on the API 29 x86_64 emulator renders a triangle through the RHI GLES backend and checks pixels (CI-verified, run https://github.com/Captain3BoOd/lumora/actions/runs/45639527288). Observed limits: the emulator context is OpenGL ES 3.0 on SwiftShader, not 3.2, and it draws nothing when a constant array is indexed by gl_VertexID, so the emulator test uses an if-chain vertex shader. The GLES backend falls back to an ES 3.x context and a 1x1 pbuffer when EGL 1.5 attributes or surfaceless make-current are not available. Not phone-GPU evidence. |
| CodeQL | done: workflow for C and C++ runs green on push, pull request and weekly. Result upload to the Security tab needs code scanning enabled on the repository, an owner setting that is not changed here |

## 5b. M2 detail (exit criteria met)

| Item | Status |
|---|---|
| WallpaperService in its own process, protected by BIND_WALLPAPER, wallpaper metadata | done as a first slice (declared and checked by an instrumented test on the API 29 emulator). The service now draws through the RHI, with the Canvas animation as fallback |
| Frame pacing with a cap, visibility, surface and screen-off pause rules, fps overlay | done as pure logic with device tests (CI-verified on the API 29 emulator). Not yet verified on a real display or at the lock screen |
| Crash-loop breaker with safe static mode | CI-verified software evidence: four breaker tests passed on the API 29 emulator job (run 45642599643) |
| Engine renders through the RHI (window surface) | CI-verified software evidence (run 45652540037): the GLES backend renders into an Android surface through an EGL window surface on API 29, 34 and 36 emulators (SwiftShader), pixels checked via ImageReader. The wallpaper service draws through it and keeps the Canvas path as fallback and for safe mode. The service wiring itself is not run end to end on an emulator, only the native path; the fps overlay is a log line in the RHI path, no on-screen text yet. D3D12 and Vulkan window surfaces are not started |
| 16 KB, flavors, Direct Boot rule, AAB path checklist items | CI-verified (software evidence): 16 KB alignment check on both flavors, play and full flavors build, Direct Boot rule and process split asserted by an emulator test (run 45648170176). the play AAB builds on every push (run 45650323262, android job green) |
| Service end to end on the emulator | CI-verified software evidence (run 45658378405): the real wallpaper service is set as the device wallpaper on API 29, 34 and 36 emulators, starts in its own :engine process, draws through the RHI GLES window surface and logs about 30 frames per second (the frame cap). Pause when hidden is verified only as pure logic, not by hiding the wallpaper on a device. Emulator graphics are SwiftShader, not a phone GPU |
| Emulator lanes API 29, 34, 36 | CI-verified (run 45658378405): full flavor on 29, 34 and 36, play flavor on 29 and 36 (docs/ci.md). Only API 29 full gates; 34, 36 and play lanes are non-gating until they have been stable longer. The earlier end-to-end test skip is resolved |

## 6. What is missing overall

The product does not render a scene yet: no scene format, no Lua, no Android wallpaper service, no Linux wallpaper runtime, no editor beyond an empty window.

M1 carry-over (not exit criteria as written, still to do; SPIR-V to GLSL ES through SPIRV-Cross is done and CI-verified on GLES llvmpipe, software evidence): Vulkan swapchain, descriptor sets, vertex buffers, depth, blending and sync beyond a fence; GLES parity beyond the triangle and an ES 3.2 context check; DXC pin consumption; nested-shell GNOME spike run; branch protection (owner setting); real-GPU confirmation stays with gate G5.

## 6a. Tooling and module work merged outside the milestones

| Item | Status |
| --- | --- |
| Texture atlas packer (core/image/atlas, lumora-atlas CLI, docs/atlas.md) | merged and CI-verified on Linux, Windows and Android emulators (CPU only, software evidence) |
| Fuzz corpus tool (tools/fuzz-tools, docs/fuzz-tools.md) | merged; its unit tests run in CI |
| Animation clips and sampler (core/anim, M4 groundwork) | merged and CI-verified (CPU only, software evidence): tracks, Step, Linear and CubicSpline sampling, loop modes, draft clip JSON parser, docs/animation.md. Known cost: sample_track validates the whole track on each call. The clip player that binds tracks to scene nodes (docs/animation-player.md) and the easing curves, CSS cubic Bezier and tween helper (docs/easing.md) are merged and CI-verified on the CPU |
| CPU particle emitter (core/particles, M4 groundwork) | merged (8d8c3c0), CI-verified on Linux and Windows with an identical golden digest; software evidence only |
| Color and gradient module (core/color, M4 groundwork) | merged (0861e16), CI-verified on Linux and Windows with identical golden digests; software evidence only |
| 2D path and spline module (core/spline, M4 groundwork) | merged (f5950e6), CI-verified on Linux and Windows with identical golden digests; software evidence only |
| 2D polygon geometry module (core/geom, M4 groundwork) | merged (ae08058), exact orientation, CI-verified on Linux and Windows with identical golden digests; software evidence only |
| 2D transform and camera module (core/xform, M4 groundwork) | merged, CI-verified on Linux and Windows with identical golden digests; software evidence only |
| 2D sprite batcher (core/batch, M4 groundwork) | merged (02e0990), CI-verified on Linux and Windows with identical golden digests; software evidence only |
| 2D spatial grid broadphase (core/grid, groundwork) | merged (dd105a5), CI-verified on Linux and Windows with identical golden digests; software evidence only |
| Grid pathfinding (core/pathfind, groundwork) | merged (f3144ff), CI-verified on Linux and Windows with identical golden digests; software evidence only |
| Bitmap text layout (core/textlayout, M4 text groundwork) | merged (ea02ca1), CI-verified on Linux and Windows with golden digests asserted as constants; exact 64-bit layout decisions, saturating stores, 2^30 byte input cut (the cut itself is verified by a one-off local run, not in CI); software evidence only |
| CPU polygon rasterizer (core/raster, M4 groundwork) | merged (b5a4951), CI-verified on Linux and Windows with golden digests asserted as constants; edge snapping and doubled-area rule documented; software evidence only |
| CPU compositing and blur (core/composite, M4 post-FX groundwork) | merged, CI-verified on Linux and Windows with golden digests asserted as constants; premultiplied RGBA8 blend modes, masked fill, box blur with no allocation; the 2^47 address cutoff in the overlap check is argued in docs/composite.md but not exercised by a test; software evidence only |
| CPU integer noise (core/noise, M4 procedural groundwork) | merged (488e785), CI-verified on Linux and Windows with golden digests asserted as constants; exact multiword fade, 2D and 3D value and gradient noise, 3 x 3 cellular (not always the true nearest point), fBm; software evidence only |
| CPU image resampling and mips (core/resample, M4 texture groundwork) | merged (f3ac9dd), CI-verified on Linux and Windows with golden digests; Nearest, Bilinear, exact Box, Catmull-Rom, mip chain; the 2^47 address cutoff in the overlap check is argued in docs/resample.md, not tested; software evidence only |
| CPU signed distance fields (core/sdf, M4 text and UI groundwork) | merged (d83d5ab), CI-verified on Linux and Windows with golden digests; exact two-pass transform equal to brute force, SDF encoding, edge coverage; software evidence only |
| CPU spatial index (core/spatial, hit testing and culling groundwork) | merged (ff982b1), CI-verified on Linux and Windows with identical golden digests (including 2^20 items); software evidence only |
| CPU polygon tessellation (core/tess, shape and glyph fill groundwork) | merged (4200e18), CI-verified on Linux and Windows with identical golden digests; exact simple-polygon test, area, point location, ear clipping with collinear drops; unreachable defensive branches argued in docs/tess.md and backed by an exhaustive 3x3 grid enumeration, the fallback code itself is not executed by tests; software evidence only |
| CPU rectangle clipping (core/clip, scissor groundwork) | merged (9cef902), CI-verified on Linux and Windows with identical golden digests; exact rational clipping, proven polygon and scratch bounds, shared overlap rule, mutation checks with equivalence arguments for the two surviving mutants; the 2^47 overlap early out is argued and not exercised by tests; software evidence only |
| CPU polyline stroking (core/stroke, line and outline groundwork) | merged (789d9c6), CI-verified on Linux and Windows with identical golden digests; convex counter-clockwise contours, rounded normals, convexity-checked miters with bevel fallback, square caps, proven bounds; the 2^47 overlap early out is argued and not exercised by tests; software evidence only |
| CPU Bezier flattening (core/flatten, curve groundwork for stroke and tess) | merged (9bd7ad2), CI-verified on Linux and Windows with identical golden digests; exact Bernstein samples, overflow-safe segment counts, deviation bounds proved for exact samples and tolerance plus sqrt(2)/2 after rounding; the 2^47 overlap early out is argued and not exercised; software evidence only |
| CPU outline builder (core/outline, path to contours for stroke, tess and raster) | merged (d55d3ee), CI-verified on Linux and Windows with identical golden digests; path state machine, dedup and closing rules, curves through core/flatten, proven bounds; success leaves points beyond the reported count unspecified; the 2^47 overlap early out is argued and not exercised; software evidence only |
| CPU vector path drawing (core/vecdraw, fill and stroke of a path into a coverage mask) | merged (6777d29, scope Z); CI-verified on Linux and Windows with identical golden digests; the 2^47 cutoff is argued and not exercised; limited to 64 commands per path, so real glyphs need a later bump; software evidence only |
| CPU TrueType reader (core/ttf, glyf outlines, cmap format 4, hmtx to outline commands; text groundwork) | merged 27a7511 (contributor scope AA, CI-verified unit tests, independent Python reference, mutation check; no font file committed, test fonts are built in code); not yet used by a text renderer; software evidence only |
| Text run (core/textrun: strict UTF-8 to glyph ids and cumulative 24.8 pen positions over core/ttf, no kerning or shaping; scope AB) | merged 3e34878 (contributor scope AB, CI-verified unit tests, independent Python reference, mutation check); not yet used by a renderer; software evidence only |
| Wallpaper offset filter (core/input/offset_filter: Q16 dead zone and smoothing for the launcher offset and tilt input) | merged 7df8b6e (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet wired to the parallax input; software evidence only |
| Diagnostics log ring (core/diag/diag_log: 64 most recent events with an overwritten counter, for the in-app diagnostics screen) | merged a9c3ecd (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet wired to the diagnostics screen; software evidence only |
| Span set (core/spans: up to 16 disjoint integer intervals with merge, split and capacity failure, for dirty and free ranges) | merged b89b925 (own module, CI-verified unit tests, independent Python point-set model, mutation check: 50 mutants, 42 killed, 8 survivors argued equivalent or unreachable); not yet used by a renderer |
| Ordered dither (core/dither: 8x8 Bayer threshold and 16-bit to 8-bit quantization of rows, for gradient banding) | merged 3dfa4a9 (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet used by a renderer; software evidence only |
| Glyph cache index (core/glyphcache: 128-entry LRU map from font, glyph, size and quarter-pixel bin to a caller value) | merged d9795a2 (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet used by a renderer; software evidence only |
| 16.16 fixed-point math (core/fixed: saturating multiply, divide, lerp and an exact integer square root) | merged fe0fef8 (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet used by a renderer; software evidence only |
| Glyph shelf allocator (core/shelfpack: shelf placement of glyph rectangles in a fixed atlas, no pixels) | merged 581ccc5 (contributor module AD, CI-verified unit tests, independent Python reference, mutation check); not yet used by a renderer; software evidence only |
| LEB128 varint and zigzag (core/varint: canonical encode and strict decode of 64-bit values for compact binary records) | merged 7eb4eb5 (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet used by a record format; software evidence only |
| Glyph blit (core/glyphblit: copy of an 8-bit coverage bitmap into an atlas buffer with the padding cells cleared, over core/shelfpack positions) | merged e11d94c (contributor module AE, CI-verified unit tests, independent Python reference, mutation check); not yet used by a renderer; software evidence only |
| PCG32 generator (core/prng: seeding, next, unbiased bounded draw and O(64) jump-ahead, for deterministic effects) | merged f4de0a4 (contributor module, CI-verified unit tests, independent Python reference, mutation check); two cap mutants (32 and 128) are not killed and not proved equivalent, disclosed in docs/prng.md; not yet used by effects; software evidence only |
| Line breaking (core/linebreak: greedy breaks over glyph advances with space and newline items, for text layout) | merged 9e4f459 (contributor module AF, CI-verified unit tests, independent Python reference, mutation check); not yet used by a renderer; software evidence only |
| Integer rectangles (core/rect: validity, intersection, bounding union, point test, area, for damage regions and clip boxes) | merged 0d1f731 (contributor module, CI-verified unit tests, two independent Python oracles, mutation check with six argued equivalent survivors); not yet used by a renderer; software evidence only |
| Box-filter downscale (core/boxfilter: whole-factor downscale of 8-bit coverage images) | merged 687c4fe (contributor module AG, CI-verified unit tests, independent Python reference, mutation check; one survivor is equivalent only if the size outputs do not alias image buffers, see docs/box_filter.md); not yet used by a renderer; software evidence only |
| Damage set (core/damage: up to 8 disjoint dirty rectangles with overlap merging and a collapse to one bounding box, over core/rect, for partial redraw) | merged 85aecc3 (contributor module AH, CI-verified unit tests, independent Python reference, mutation check; two survivors argued equivalent, first-versus-last overlap hit also checked in an independent model); not yet used by a renderer; software evidence only |
| Byte FIFO (core/ring: 4096-byte ring with partial write, read, peek and skip, for staging audio or log bytes) | merged 75901eb (contributor module, CI-verified unit tests, independent Python reference, mutation check; 14 survivors argued equivalent; mutant 30 is killed only by AddressSanitizer with instrumented tests and by no assertion, and is not claimed equivalent); not yet used by a renderer; software evidence only |
| Histogram and auto levels (core/histogram: 256-bin histogram of an 8-bit image, percentile, mean and a contrast-stretch table) | merged 2d1e22c (contributor module AI, CI-verified unit tests, independent Python reference, mutation check; seven survivors argued equivalent); not yet used by a renderer; software evidence only |
| Generational slot map (core/slotmap: 256 slots, lowest-free allocation, 24-bit generations, stale-handle detection) | merged f4989f2 (contributor module, CI-verified unit tests, independent Python reference, mutation check; three survivors argued equivalent; full 24-bit generation loop tested); not yet used by a renderer; software evidence only |
| Run-length coding (core/rle: strict canonical byte run-length encode and decode with size checks, for masks and simple records) | merged a2c3554 (contributor module AJ, CI-verified unit tests, independent Python reference, mutation check; six survivors argued equivalent); not yet used by a renderer; software evidence only |
| xxHash32 (core/xxh32: one-shot and streaming 32-bit hash for cache keys and content ids, not for security) | merged 8f034a8 (contributor module, CI-verified unit tests, independent Python reference written from the header, mutation check; six survivors argued equivalent on the header-defined state, unused buffer bytes are not asserted; one undefined-behavior mutant is killed only by UBSan; only the two empty-input values are externally anchored); not yet used; software evidence only |
| Strict Base64 (core/base64: RFC 4648 standard alphabet with padding, canonical strict decode with explicit error order, for small embedded blobs) | merged 1b0261e (contributor module AK, CI-verified unit tests, independent Python reference, mutation check: 123 mutants, 117 killed, 6 survivors argued equivalent); not yet used by a renderer |
| Bit I/O (core/bitio: LSB-first bit field write and read over a caller buffer with 64-bit bounds, for compact binary records) | merged 50c5f21 (contributor module AL, CI-verified unit tests, independent Python bit-array model, mutation check: 127 mutants, 120 killed, 7 survivors argued equivalent; the success case ending exactly at 2^31 bits was not run); not yet used by a renderer |
| Strict hex text (core/hex: encode in either case, strict decode of mixed case with explicit error order, for digests and ids) | merged 5fb5339 (contributor module AM, CI-verified unit tests, independent Python reference, mutation check: 95 mutants, 90 killed, 5 survivors; four argued equivalent, one NOT equivalent and not killed: decode limit comparison differs only at exactly 268435456 characters, which needs a 256 MiB buffer and was not run); not yet used by a renderer |
| Checksums (core/checksum: CRC-32 IEEE and Adler-32 with incremental update, split-independent) | merged e76f3c7 (contributor module AN, CI-verified unit tests, independent Python reference without zlib, mutation check: 78 mutants, 72 killed, 6 survivors argued equivalent; inputs near 2^32 bytes not run, longest 70000); not yet used by a renderer |
| Bit sets (core/bitset: fixed-size bit sets over caller 64-bit words with set, test, fill, popcount, find next; no intrinsics) | merged c02f85f (contributor module AO, CI-verified unit tests, independent Python reference, mutation check: 96 mutants, 93 killed, 3 survivors; two argued equivalent, one NOT equivalent and not killed: the size limit comparison differs only at exactly 2^30 bits, which needs a 128 MiB array and was not run); not yet used by a renderer |
| Strict UTF-8 (core/utf8: well-formed validation with first bad offset, single code point decode and encode) | merged 217deb0 (contributor module AP, CI-verified unit tests, independent Python reference, mutation check: 167 mutants, 156 killed, 11 survivors argued equivalent; successful validation at exactly 2^28 bytes not run; the limit equality mutant is killed by a 1-byte buffer whose first byte is ill-formed, which relies on the scan stopping at the first ill-formed sequence); not yet used by a renderer |
| Exact integer helpers (core/intmath: isqrt, gcd, lcm, checked int64 add, sub and mul, a * b / d with an exact 128-bit intermediate and three rounding modes, no __int128) | merged 7e7ac2b (contributor module AQ, CI-verified unit tests, exact Python-integer reference, mutation check: 119 mutants, 112 killed, 7 survivors argued equivalent; isqrt not exhaustive over 2^64); not yet used by a renderer |
| Radix sort (core/sortu: stable radix sort of 32-bit keys with optional 32-bit values, caller scratch, sortedness check) | interface and stubs on main; implementation not started (contributor scope); not CI-verified yet |
| Strict UTF-16 (core/utf16: validation with first bad offset, code point count, single code point decode and encode) | interface and stubs on main; implementation not started (contributor scope); not CI-verified yet |
| Fenwick tree (core/fenwick: prefix and range sums with wrapping int64 arithmetic and a first-prefix search, over a caller array) | in review on branch own-fenwick (CI-verified unit tests, independent Python list model, mutation check: 67 mutants, 61 killed, 6 survivors: one not equivalent at exactly 2^24 elements, one UB-path caveat, four equivalent); not yet used by a renderer |
| Node tree validator (core/nodetree, scene graph groundwork, written by the agent) | merged (f839b21), CI-verified on Linux and Windows (13 of 13 checks); forest from child lists with parents, depths, preorder and subtree sizes, bounds proofs, exhaustive small-graph tests, golden digests from an independent Python reference; the 2^47 overlap early out is argued and not exercised; software evidence only |
| Fuzz targets for nodetree, outline and stroke (written by the agent) | merged (d7ad66f), CI-verified on Linux and Windows (13 of 13 checks): three structured-input targets with seed corpora and ctest replay; 300000 random inputs per target ran clean locally under ASan and UBSan; the libFuzzer CI step does not run them yet; software evidence only |
| Fuzz targets for atlas and animation parsing | done: fuzz_atlas and fuzz_animclip exist with corpora and replay tests (this row was stale) |
| Package entry policy (core/package/entry_policy, names and entry-set limits, contributor) | merged (345b22a), CI-verified on Linux and Windows (13 of 13 checks); pure functions, not yet called by the ZIP reader; software evidence only |
| JSON structural limits (core/package/json_limits, scan_json: size, depth, string, key, item, value limits and duplicate keys, contributor) | merged (3e55fdb), CI-verified on Linux and Windows (13 of 13 checks); pre-scan only, number and UTF-8 syntax are left to the later parser; software evidence only |
| ZIP container audit (core/package/zip_audit, layout, header agreement, overlap, ZIP64 consistency, contributor) | merged (16d8447), CI-verified on Linux and Windows (13 of 13 checks); independent Python reference archives asserted as constants plus per-byte mutation tests; valid entry-level ZIP64 is exercised only through failures (needs 4 GiB); not yet called by the ZIP reader; software evidence only |
| Extraction plan (core/package/extract_plan, directories, file-versus-directory conflicts, free-space check, cleanup order, contributor) | merged (d77e61e), CI-verified on Linux and Windows (13 of 13 checks); 54 source mutants, 53 killed (two by crash, which is the only possible outcome for those undefined-behaviour mutants), one equivalent survivor argued; not yet called by an importer; software evidence only |
| Load-time budgets (core/package/load_budget, texture memory, scripts, nodes, audio) | merged (09d9746); CI-verified on Linux and Windows; 87 source mutants with 85 killed and 2 argued equivalent; software evidence only |
| Capability model (core/package/capabilities, declared and granted permissions) | merged (a2f70e0); CI-verified on Linux and Windows; 80 source mutants with 77 killed and 3 argued equivalent; software evidence only |
| Audio analysis core (core/audio/analysis: level, envelope follower, integer FFT power spectrum, band energies) | merged (5e7abd2); CI-verified on Linux and Windows; 82 source mutants with 77 killed (7 by crash only) and 5 argued equivalent from documented magnitude bounds; software evidence only |
| Fixed-timestep accumulator (core/timing/fixed_step: clamped deltas, catch-up cap, interpolation factor) | merged (fcbde4a); CI-verified on Linux and Windows; 63 source mutants with 58 killed (5 by crash only) and 5 argued equivalent; software evidence only |
| Crash-loop breaker (core/lifecycle/crash_breaker: per scene and backend crash logs, window policy, Vulkan to GLES then disable) | merged (5eb8b2a); CI-verified on Linux and Windows; 65 source mutants with 62 killed (5 by crash only) and 3 argued equivalent; software evidence only; the 3 crashes in 24 hours defaults stay provisional until M2 |
| Microphone capture state machine (core/lifecycle/mic_state: Inactive, Active, Paused, Stopped transition table) | merged (fb0b3a4); CI-verified on Linux and Windows; 59 source mutants with 56 killed and 3 argued equivalent; software evidence only; the table does not gate capability or permission, a separate gate does |
| Frame pacing and render scale controller (core/lifecycle/pacing: frame interval from cap, vsync and battery saver; overrun and thermal notches) | merged 1bf0aaa (contributor module, CI-verified unit tests, independent Python reference, mutation check); recovery rule provisional; not yet wired into the render loop; software evidence only |
| Frame statistics and fps overlay text (core/lifecycle/frame_stats: 120-frame window, average, p99, worst, overlay line formatter) | merged c99cdce (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet wired to a renderer overlay; software evidence only |
| Vsync period estimator (core/lifecycle/vsync: median of the last 16 frame deltas with pause-gap reset) | merged c5a4a31 (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet wired to the pacing loop; software evidence only |
| Render gate (core/lifecycle/render_gate: surface, visibility and screen-off events to start and stop edges of the frame loop) | merged 210a8b8 (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet wired to the wallpaper service; software evidence only |
| Power policy (core/lifecycle/power_policy: render-scale percent, quality preset and scaled sizes from battery saver and notch) | merged a3d7b8a (contributor module, CI-verified unit tests, independent Python reference, mutation check); not yet wired to the render loop; software evidence only |
| glTF binary loader (core/glb, M4 asset groundwork, written by the agent) | merged (98e2aaf), CI-verified on Linux and Windows (13 of 13 checks); container, JSON, buffer views, accessors, triangle meshes, limits, fuzz target with corpus, golden digest from an independent Python reference; software evidence only |

## 6b. MW: Windows desktop runtime (planned)

Added at the owner's request, as its own milestone beside the Android runtime (M2) and the Linux runtime (ML). Full text is in `docs/milestones.md`. It does not change the M2 order.

Scope:
- A Windows desktop process that renders a packaged scene behind the desktop icons, one render surface per monitor.
- D3D12 backend first, through the RHI, with the Vulkan backend as a later option (question MW-Q2).
- Pause or throttle when a fullscreen app or game is running, on battery saver, on session lock, and when the display is off.
- Multi-monitor: hot-plug, different resolutions and DPI, per-monitor or spanned scene (question MW-Q1).
- Start with Windows, tray or control app, and a CLI. Packaging route follows OQ5.

Dependencies: M1 D3D12 backend (first slice exists on WARP), M3 loader and M3S vertical slice, M5 scripting for interactive scenes, M6 input sources (pointer). Steps run after M6, in parallel order with ML steps 4 to 6.

Evidence labels for MW:
- CI-verified (Windows, no GPU): build, link, unit tests of the pure policy code (monitor layout, pause rules), process and window creation on the hosted runner.
- WARP software evidence: D3D12 frames on the hosted runner. Not RX 570 evidence.
- owner-hardware-verified: only the real-desktop behavior in gate G8 (confirmed by the owner, timing later). Nothing about wallpaper placement behind icons, multi-monitor or fullscreen detection is claimed verified until then, because hosted runners have no real interactive desktop session.

Design candidates, all unverified until a spike runs:
- Placing the render window behind the icons by asking the shell to create the desktop worker window and parenting to it. This is an undocumented technique, so it can break on a Windows update. Reported but not verified: the window layout changed on newer Windows 11 builds. The spike must record the exact Windows build it ran on.
- Fullscreen detection through the shell's user-notification-state query plus a foreground-window and monitor-rectangle check, with an allow and deny list. Games that use borderless windows are the known hard case.
- Per-monitor DPI awareness and display-change messages for hot-plug.

Risks:
- Undocumented shell behavior breaks on updates (mitigation: a capability probe at start, a visible fallback to a normal borderless window below all others, and a documented supported-build list).
- Hosted CI cannot show a real desktop, so the main behavior is owner-side only (mitigation: a headless test seam around all policy code, a real-session script for G8).
- GPU contention with games (mitigation: pause rules first, frame cap, lower priority queue).
- Antivirus and SmartScreen reactions to a process that parents into shell windows (tied to OQ5).

Open questions for the owner (proposals, not decisions):
- MW-Q1: one scene per monitor, the same scene on every monitor, or one scene spanned across all monitors?
- MW-Q2: is a Vulkan fallback needed on Windows, for machines where D3D12 fails, in v1?
- MW-Q3: pause policy defaults: pause for any fullscreen app, or only for a game list; battery saver behavior.

## 7. Open owner questions

OQ1 to OQ10 in `LUMORA_MASTER.md` are open. None blocks M0. Reviewer recommendations in that file are recommendations, not decisions.

## 8. Owner hardware gates (end of project)

G1 first render on the physical phone, G2 a Windows-exported package on the phone, G3 lock-screen and lifecycle on real firmware, G4 thermal and battery baseline, G5 D3D12 and Vulkan on the real GPU, G6 cross-backend fallback validation, G7 Linux real-session validation. G8 is the MW gate: Windows desktop runtime real-session validation (behind icons, multi-monitor, fullscreen pause) on the owner's Windows machine. The owner confirmed he can run it, later and not now. Until these run, no claim of hardware verification is made.

## 9. Risks

Top risks are in `docs/risks.md`: scope for one executor, Arabic RTL in Dear ImGui, GNOME wallpaper method unproven without real hardware, unknown phone driver behavior, hostile shared scenes, slow CI-only feedback on Windows, owner availability at the end.

## 10. Change log of this file

- 2026-11-25: created at M0 start.
- 2026-11-25: M0 exit criteria met and recorded.
- 2026-11-25: M1 started: RHI interface and Null backend, WARP spike result.
- 2026-11-25: Vulkan backend first slice on lavapipe in Linux CI.
- 2026-11-25: Golden-image harness and first lavapipe baseline.
- 2026-11-25: Vulkan command recording, render pass, pipeline and pixel-checked triangle in CI. Leak detection is off in the ASan job because of third-party driver allocations (see ci.yml).
- 2026-11-25: M1 exit criteria met (Vulkan on lavapipe in Linux and Windows CI, GLES on llvmpipe and on the API 29 emulator, D3D12 on WARP, golden images, CodeQL). Carry-over listed in section 6. M2 started.
- 2026-11-25: MW (Windows desktop runtime) added to the plan at the owner's request. Estimate rises by 5-9 weeks. M2 order unchanged.
- 2026-11-25: Gate G8 (Windows desktop runtime, owner machine) confirmed by the owner, timing later. Play AAB path CI-verified.
- 2026-11-25: M2 engine path: RHI GLES window surface and wallpaper service drawing through it, CI-verified on emulator lanes 29, 34 and 36 (software evidence).
- 2026-11-25: M2 exit criteria met and tagged m2-done. Service runs end to end on the API 29, 34 and 36 emulators in CI (software evidence). Caveats: pause when hidden is logic-tested only; fps overlay is a log line in the RHI path; real display, lock screen and phone GPU are untested (gates G1 and G3).
- 2026-11-25: M2 boundary re-estimate: no change to the 85-144 week range yet. M0 to M2 were completed far faster in elapsed time than the one-developer weeks in the table, but that does not carry over to the later milestones, which depend on real-device gates and owner availability. The next re-estimate stays at the M4 boundary. Scope cuts remain the owner's call.
- 2026-11-25: M3 started: dependency-free JSON model and manifest.json parsing with version policy (docs/format.md section 8.7). Package entry policy and JSON limits modules are with a contributor.
