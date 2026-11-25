# Testing strategy (Section 16)

> Normative document of the Lumora Master Plan. Section numbers are stable IDs shared by all files and are not contiguous inside one file. Read `LUMORA_MASTER.md` first.

## 16. Testing strategy (D22)

### 16.1 Agent-side (automated, run continuously during development)

- **Unit tests** (Catch2): scene format round-trip, math, package writer/reader, settings binding, Lua sandbox escapes (attempted escapes must fail), checksum verification. The sandbox escape suite explicitly includes: binary bytecode chunk rejection (`load` with precompiled chunks), `string.dump` absence, `debug`-library absence and hook-tampering attempts (`debug.sethook` must not exist, and the engine's own instruction hook must be unremovable from inside a scene), and `_ENV` whitelist probes. The escape suite also covers the native standard-library cases (long `string.rep`, pathological patterns, `table.sort` and `table.concat` abuse), allowlist probes for `dofile`, `loadfile`, `require`, `package`, `io`, `os`, `print` and `collectgarbage`, frozen shared tables, hostile `__gc`/`__close`/`__index`, and coroutine-wrapped loops.
- **Fuzzing / malformed-input tests:** package reader/writer, ZIP metadata and extraction, manifest/JSON, scene deserializer, glTF, image/KTX2, Lua API bindings, shader validation, settings schema - against malformed, truncated, oversized, deeply nested, and adversarial inputs. **Runs in CI from M0:** libFuzzer or AFL++ targets for the ZIP reader, JSON, glTF, KTX2, Ogg/Opus/Vorbis and the Lua API bindings, built with ASan and UBSan, a committed seed corpus, a short run per pull request (about 60 s per target) and a long scheduled run with the corpus persisted and crash reproducers uploaded; TSan jobs on Linux. OSS-Fuzz only if the repository is public.
- **Golden-image tests:** reference scenes rendered offscreen in CI (Vulkan on lavapipe built from a pinned source and on SwiftShader; GLES via SwiftShader/ANGLE; **D3D12 on WARP if the M1 spike succeeds**, otherwise D3D12 has no golden images before G5 - see 6.4) and compared against blessed images. **Policy:** the software Vulkan path is the reference; the comparison metric (per-channel delta or SSIM) and per-scene thresholds are documented, because lavapipe's JIT output depends on CPU features and WARP output will not equal lavapipe output, so baselines are per renderer; color space and image format are pinned; blessing is an explicit reviewed step through `bless-goldens.yml` (Section 14.2). Renderer versions are pinned in the manifest. A **mediump-emulation test** runs the corpus with RelaxedPrecision stripped versus kept to surface fp16-class bugs, with the honest note that real Mali `mediump` behavior is owner-device-only. GLES-via-ANGLE-on-SwiftShader reuses the same Vulkan rasterizer, so it validates the translation layer, not GLES drivers. Golden images complement API validation and hardware tests; they never replace them.
- **Shader corpus translation gate:** the fixed shader corpus is compiled and translated to all three targets (SPIR-V, GLSL ES, HLSL/DXIL) in the test suite; SPIRV-Cross HLSL output is a known weak point for compute and some resource types, so the corpus deliberately covers compute and every resource type the contract allows, and a translation failure blocks export rather than shipping a broken artifact.
- **GPU timing method:** GPU frame time is measured with Vulkan timestamp queries (checking `timestampValidBits`); GLES timer queries (`EXT_disjoint_timer_query`) are often absent on Mali, so on the GLES path GPU time is reported as unavailable and present-to-present plus CPU-submit timings are used as proxies, labeled as such.
- **Packaging smoke tests:** the `.deb` and AppImage install-and-run in containers on clean Ubuntu 24.04, 26.04 and 26.10 (22.04 only if it is still a build baseline), AppImage tested with `--appimage-extract-and-run` and separately with FUSE absent and present (containers have no FUSE or real session by default, so the FUSE path needs a VM-type runner); the Windows zip is unzipped on a clean runner and the packer and `editor --version` run. Each release records SHA-256 files and build provenance. GNOME/mutter behavior is checked in the nested-shell job or owner-side.
- **Graphics lifecycle tests:** resource lifetime, resize/surface recreation, device loss and recovery, synchronization correctness, in-flight frame limits, pipeline cache behavior, texture upload/staging, descriptor/binding lifetime, repeated create/destroy cycles, and GPU leak detection - automated on the software backends, and scripted for owner hardware in the final gate run.
- **D3D12 and Windows evidence labels:** test reports separate **CI-verified (Windows, no GPU)** build, link, DXIL and CPU-test results; **WARP software evidence** (if the spike works) for D3D12 device, clear/draw and goldens; and **owner-hardware-verified (RX 570)** for real driver behavior, which exists only from G5. Compile/link and WARP success are never reported as evidence of real-GPU D3D12 behavior.
- **Test report identity:** every test report records backend, device/profile, GPU and driver version, OS version, renderer path (native GLES / native Vulkan / translation / software), and the four compute capability flags. A result without its environment is not evidence.
- **Format/tooling tests:** the packer CLI round-trips every example scene; shader toolchain compiles the shader corpus to all three targets with zero errors.
- **Android:** x86_64 emulator runs (emulator/test-only; the sole shipping ABI is `arm64-v8a`, D25) on **API 29 (minimum, every pull request), API 34 and API 36 (nightly, required)**, plus an Android 15 16 KB page-size image run, covering display size/density variants, permission denial and revocation, background-execution restrictions, process death and service recreation, and SAF / share-sheet import. Release artifacts are built and inspected for `arm64-v8a` only; CI fails the release if any `armeabi-v7a` library is present, if any `.so` is not 16 KB aligned, or if `x86_64` appears outside emulator artifacts. The `play` and `full` flavors are both built. Wallpaper lifecycle is tested on the emulator (install, set wallpaper, lock/unlock, screen off/on) with log assertions; the per-screen scene path is tested on API 34+. The emulator is never evidence for GPU driver behavior, battery, thermal, OEM lock-screen behavior or hardware video decoding.
- **Linux:** the Linux editor and desktop runtime are built and tested in CI and locally: unit, fuzz and packer tests; software-Vulkan golden images (**agent-verified, software renderer**); Xvfb and wlroots-style headless tests for window-type, layer and stacking logic; a nested or headless `gnome-shell` job (Ubuntu 26.04 container or VM; feasibility unverified, and hosted availability of a 26.04 image should be re-checked before relying on it) running the extension spike and checking placement, stacking, input-through, enable/disable and screenshots; extension lint plus the extensions.gnome.org review checklist; lifecycle tests (monitor hot-plug simulation, surface loss, pause/resume). Real-desktop checks are owner-side only (Section 4.4).
- **Editor:** logic unit tests; ImGui UI tests with `imgui_test_engine` and screenshots from an offscreen RHI target, uploaded as CI artifacts; IME, Wayland typing and fractional scaling are owner-hardware checks.
- **Determinism and supply-chain jobs:** a CI job exports every example scene twice on the same image and compares hashes of inputs and tool versions (cross-host byte identity is not promised because encoders and shader tools are not guaranteed byte-identical across hosts; semantic identity is the minimum); a dependency-audit job (license scan, dependency review, Dependabot, CodeQL, SBOM) enforces the Section 14.1 rules.
- **Flake policy:** required checks are listed in `docs/ci.md`; at most one automatic retry, emulator jobs only; flaky tests are quarantined by name; every job has `timeout-minutes`.
- **Benchmark scene suite:** minimal 2D parallax, typical 2D with text and particles, basic 3D PBR, skinned glTF, shadow-heavy, overdraw-heavy, physics, audio-reactive, video-texture, and a combined stress scene - each with documented budgets and device profiles, replacing a single "reference scene".
- **Per-milestone artifacts:** every milestone produces verifiable artifacts: build logs, unit-test reports, golden-image diff reports, per-backend shader compile reports, package validation reports, logcat captures, and perf/memory measurements where applicable. Each result is labeled **agent-verified**, **emulator-verified**, or **owner-hardware-verified**; a hardware-dependent claim is never reported as fully passed on software evidence alone.

### 16.2 Owner-side (prepared scripts, final verification)

The owner runs three numbered test scripts at the end (D22, D23; CI now covers everything that does not need real hardware, so the scripts shrink to real-device checks):

1. **Phone script (Realme 6):** install APK (the `full` flavor, and the `play` flavor once) -> import the bundled sample scenes -> set as home+lock wallpaper through the system chooser -> record what the lock screen actually does on this firmware (a separate lock-screen scene is not expected on API 29-33; the step records behavior instead of asserting two engines) -> allow restricted settings for the Notification Listener -> verify 60/90 fps overlay -> touch/gyro reactions -> screen off/on pause behavior -> reboot and check pre-unlock behavior -> probe `EXT_multisampled_render_to_texture`, `EXT_color_buffer_float` and `EXT_disjoint_timer_query` -> three-engine memory soak where the firmware allows it -> battery drain sanity (overnight) -> per-scene settings changes -> a friend-scene import.
2. **Windows script (RX 570 PC), cut down:** record versions, launch the CI-built editor zip on real hardware, open the sample project, edit, play, export `.lumora`, transfer to phone, run it; then run the D3D12 and Vulkan real-GPU checks (G5). A scripted MSVC/DXC build-and-report on the owner's machine is not needed, because the Windows CI job does it on every push.
3. **Linux script:** record `lsb_release -a`, `$XDG_CURRENT_DESKTOP`, `$XDG_SESSION_TYPE`, `vulkaninfo --summary`; install the CI-built `.deb`; first-run extension onboarding (enable extension, log out and in); launch the editor on a real session, open the sample project, edit, play, export `.lumora`; then run the wallpaper runtime: set a scene, multi-monitor if applicable, fullscreen-app pause, lock/unlock, suspend/resume, pointer reaction, audio-reactive scene, autostart on/off, type Arabic through IME in the editor on Wayland. Results are recorded per distribution and desktop actually tested.

Each script is a checklist with expected results and "if X fails, send me Y" diagnostics (the app and editor have a built-in diagnostics/log export button).

### 16.3 Performance, memory, and battery budgets

**Frame-time budgets** (targets mean sustained frame pacing, not average FPS; headroom is reserved for the OS/compositor):

| Target | Frame interval | CPU budget | GPU budget |
|---|---:|---:|---:|
| 30 fps | 33.33 ms | <= 12 ms | <= 24 ms |
| 60 fps | 16.67 ms | <= 6 ms | <= 12 ms |
| 90 fps | 11.11 ms | <= 4 ms | <= 8 ms |
| 120 fps | 8.33 ms | <= 3 ms | <= 6 ms |

The 120 fps row is unverifiable on owned hardware (no 120 Hz device) and stays a design bound, not a validated target.

Tracked metrics: CPU and GPU frame time are tracked **separately** - they overlap and are never summed into one number - plus missed present deadlines, frame-time percentiles (including 1% low and worst-spike capture), thermal state, render scale, draw calls/state changes, and shader-compile and streaming stalls. Average FPS alone is never reported as a passing result. A scene becomes a shippable sample only if the benchmark suite passes on the Realme 6 profile: 60 fps sustained at 1.0 render scale, memory within budget, cold load under 2 seconds.

**Memory budgets:** tracked separately as PSS (primary), RSS, native heap, Kotlin/Java heap, estimated GPU texture/buffer memory, decoder buffers, Lua heap, and peak loading/transcode memory. The ~350 MB figure is the **typical-scene PSS budget including the app process** on the 4 GB Realme 6 variant; idle, stress-scene, and peak-loading budgets are defined per tier in `docs/performance-budgets.md` (M4). The ~350 MB figure predates the multi-engine design. Per-engine and aggregate PSS and GPU budgets are defined per tier in `docs/performance-budgets.md` (M4) for up to three engines (API 34+) or two engines otherwise on the 4 GB variant, including a low-RAM behavior (the lock engine gets a reduced tier or a static fallback) and a three-engine soak in the phone script.

**Battery methodology:** measurements happen under controlled conditions, not estimated: fixed brightness, fixed 60 Hz mode, airplane radios, charged-then-discharging device, repeated timed runs of the reference scene against a static-wallpaper baseline, reported separately for visible, hidden, screen-off, and lock-screen states, using measured current where available rather than the battery-percentage gauge alone. The procedure ships inside the phone test script. Targets are **per workload**, not one global number: idle 2D, animated 2D, typical 3D, heavy 3D, audio-reactive, and video workloads each get their own budget, and every measurement records device, brightness, refresh rate, screen state, duration, and method. CI cannot validate any battery, thermal or GPU-time number; hosted jobs run only functional and CPU-side regression proxies.

### 16.4 Acceptance criteria

1. RHI: backend-neutral tests pass where supported; unsupported capabilities are explicitly reported and fall back correctly.
2. Shader pipeline: invalid shaders produce useful diagnostics; the correct artifact is selected per backend.
3. Package security: adversarial packages are rejected without path escape, uncontrolled resource use, or unsafe partial extraction.
4. Lua sandbox: script and native API budgets enforced; expensive work cancellable safely.
5. Wallpaper lifecycle: repeated surface and visibility transitions leak no resources and corrupt no runtime state.
6. Video: frame acquisition, synchronization, color conversion, and release tested per backend.
7. RTL: mixed Arabic/English editing works in real editor controls (not only in a rendering demo), with the scope chosen in open question OQ1; the GLSL/Lua code editor is left-to-right only and is excluded from this item.
8. Recovery: crashes, invalid scenes, missing assets, and backend failures never cause unrecoverable crash loops.
9. Performance: frame-time percentiles, missed deadlines, memory, and workload-specific battery results recorded.
10. Android build: CI and release artifacts validate exactly the shipping ABI set `{ arm64-v8a }` (D25); the APK/package inspection step fails the release if any `armeabi-v7a` native library is present, and `x86_64` appears only in emulator test artifacts, never in a release artifact.
11. CI: every push builds and tests Linux, Windows and Android on pinned runners; Windows build, link, DXIL and CPU tests, the 16 KB alignment check, the ABI check and the fuzz smoke are required checks; evidence labels follow Section 16.1.


### 16.5 Added test requirements

- Adversarial and runaway shaders in the fuzz corpus (Section 15.1).
- Crash-loop breaker test: a scene that crashes the engine N times ends in `CrashDisabled` with fallback and a user message (Section 10.7).
- Microphone capture pause and resume on the emulator lanes and in the phone script; if a level refuses resume, the Stopped state is entered cleanly (Section 9.3 item 5).
- Flash-rate lint test with known flashing and non-flashing scenes (Section 9.5).
- Documentation consistency check runs in required CI (`tools/check_docs.py`).

### 16.6 Running the Null, Vulkan and GLES test subsets

All RHI tests are Catch2 test cases in one executable, `lumora_tests`. `python3 tools/ci/test.py` (the entry point CI uses) runs the docs check, configures, builds, and runs `ctest`, which runs that one executable. Add `--sanitize` for ASan and UBSan, and `--build-dir DIR` or `--config Debug` to change the build. To run only part of the suite, build once and then call the executable directly with a Catch2 test-name filter. On a single-config generator (Linux, with `--build-dir build`) the executable is `build/tests/lumora_tests`; on Windows with Visual Studio it is under `build/tests/Release/`.

| Subset | Test names | Filter | Needs |
|---|---|---|---|
| Null backend | names starting with `null ` | `build/tests/lumora_tests "null *"` | Nothing. No GPU, no driver. Always built. |
| Vulkan | names starting with `vulkan` | `build/tests/lumora_tests "vulkan*"` | The Vulkan headers and loader at configure time, plus a Vulkan driver at run time (see below). |
| GLES | names starting with `gles` | `build/tests/lumora_tests "gles*"` | EGL and GLESv2 with the GLES 3.2 headers at configure time (Linux), plus a software GLES 3.2 context at run time. |

Use `build/tests/lumora_tests --list-tests` to see which test cases exist in your build. A subset whose backend was not found at configure time is not compiled, so its filter matches nothing and Catch2 prints "No tests ran". CMake prints one line per backend at configure time: `Lumora: Vulkan backend enabled` or `Lumora: Vulkan not found, backend disabled`, and the same for GLES. Check those lines before reading "No tests ran" as a pass.

#### Null backend

The Null device validates descriptions and tracks resource lifetime, with no GPU work. Its tests need no driver, so they run wherever the suite builds, including the Linux and Windows CI jobs.

#### Vulkan (software driver)

CI installs `libvulkan-dev`, `mesa-vulkan-drivers`, `vulkan-validationlayers` and `glslang-tools` on Ubuntu 24.04. The Vulkan tests then run on Mesa lavapipe, a software Vulkan driver, so no GPU is needed.

- The device test expects the Khronos validation layer to be active. If you have no validation layer, set `LUMORA_ALLOW_NO_VALIDATION=1` to skip that one check.
- The triangle render test is compiled only when its shaders are available: `glslangValidator` found at configure time, or `-DLUMORA_PRECOMPILED_SPV_DIR=<dir>` pointing at `triangle_vert_spv.h` and `triangle_frag_spv.h`.
- The render test compares the image with the baseline under `tests/golden/lavapipe/`. See `docs/golden-images.md` for tolerance and blessing. Do not overwrite a baseline to make a diff pass.
- On Windows, the `windows-vulkan-spike` CI job runs the same Vulkan tests on lavapipe from a pinned Mesa build. It is a spike, marked `continue-on-error`, and does not gate the result.

#### GLES (software context)

CI runs the GLES triangle test on Mesa llvmpipe through a headless EGL context, in the same Linux job. Its baseline is `tests/golden/llvmpipe-gles/`.

#### What the results mean

Results from lavapipe, llvmpipe and WARP are software evidence. They show that the RHI code runs and renders correctly on a software renderer. They are not evidence about any real GPU, phone driver or device. Label them as CI-verified software evidence, as in `CONTRIBUTING.md`, and never as owner-hardware-verified.

#### Environment variables used by the tests and CI

- `LUMORA_ALLOW_NO_VALIDATION=1`: skip the check that the Vulkan validation layer is active.
- `LUMORA_BLESS_GOLDEN=1`: write the actual image as the new baseline. Review the image by eye before committing it.
- `SDL_VIDEODRIVER=dummy` and `ASAN_OPTIONS=detect_leaks=0`: set by the Linux CI job. The second one is there because the software Vulkan driver leaves small allocations.
