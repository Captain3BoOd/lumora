# Fuzzing

Lumora fuzzes every parser that reads untrusted input. The targets live in `tools/fuzz/`, the seed inputs in
`tools/fuzz/corpus/<target>/`.

| Target | Entry point under test | Extra checks |
| --- | --- | --- |
| `fuzz_json` | `parse_json`, `scan_json` | none |
| `fuzz_manifest` | `parse_manifest` | `manifest_supports_backend` on accepted manifests |
| `fuzz_scene` | `parse_scene` (default and depth 4) | none |
| `fuzz_settings` | `parse_settings_schema` | accepted schemas go through `resolve_values`, `validate_value`, `is_visible`, `reset_values`, `migrate_values` |
| `fuzz_zip` | `read_zip_store`, then `load_package` when parsing works | `write_zip` / `read_zip_store` round trip on a `FileMap` built from the input; `load_package` must never accept a file that differs from `checksums.txt` |
| `fuzz_inflate` | `inflate_raw` with a 64 KB output limit | `deflate_raw` / `inflate_raw` round trip at levels 1 and 6 |
| `fuzz_ppm` | `decode_ppm` with a 256 pixel dimension limit | a decoded image is valid and survives `encode_ppm` / `decode_ppm` |
| `fuzz_animclip` | `parse_clip_json` | accepted clips: every track passes `validate_track`, the clip duration is finite, and `sample_track` returns finite values at five times |
| `fuzz_atlas` | `pack_atlas` and `atlas_index_json` on a few tiny images built from the input bytes, with small limits | accepted atlases: valid image inside the limits, entries inside the image and not overlapping, non-empty index; failures give an empty index |

A failed invariant calls `std::abort()`, so it shows up as a crash.

## Replay tests (every compiler)

Each target is also built as `fuzz_<name>_replay`. It runs every file under `corpus/<name>/` once, plus the empty
input, and is registered with ctest:

    cmake -S . -B build -DLUMORA_BUILD_EDITOR=OFF
    cmake --build build
    ctest --test-dir build -R fuzz_

The replay runs are not fuzzing. They keep the corpus, and every past crash input, as regression tests.

## Fuzzing with Clang and libFuzzer

libFuzzer needs Clang. The fuzzer binaries are built with AddressSanitizer and UndefinedBehaviorSanitizer:

    cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
          -DLUMORA_BUILD_EDITOR=OFF -DLUMORA_BUILD_FUZZERS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build-fuzz

Run one target for a fixed time against a working copy of the corpus, so new inputs do not land in the source tree:

    mkdir -p /tmp/corpus_zip && cp tools/fuzz/corpus/zip/* /tmp/corpus_zip/
    build-fuzz/tools/fuzz/fuzz_zip -max_total_time=300 -max_len=65536 /tmp/corpus_zip \
        tools/fuzz/corpus/zip

The first directory receives new inputs. Pass the source corpus as a second directory to read it as well.

## When a target crashes

1. libFuzzer writes `crash-<hash>` into the working directory. Reproduce it with
   `build-fuzz/tools/fuzz/fuzz_zip crash-<hash>`.
2. Minimize it: `fuzz_zip -minimize_crash=1 -runs=10000 crash-<hash>`.
3. Check the minimized file into `tools/fuzz/corpus/<target>/` under a name that says what it is, such as
   `regress_<what>.bin`. The replay test then fails until the bug is fixed and keeps guarding afterwards.
4. Fix the bug in the project code in a separate change, with a unit test.

## Adding seeds

Keep each seed small (at most 64 KB) and the whole corpus under 2 MB. Seeds must be hand-made or generated, never
copied from third-party data. Cover valid inputs, each error path, and limit edges. Do not commit libFuzzer's
auto-generated corpus.

## Geometry targets (nodetree, outline, stroke)

`fuzz_nodetree`, `fuzz_outline` and `fuzz_stroke` decode raw bytes into structured inputs (a child-list graph, path commands with
a tolerance, a polyline with a style), including invalid values on purpose. They check the documented invariants on success (a
consistent forest, bounded contours without consecutive duplicates, counter-clockwise stroke contours) and that an error leaves
every output untouched. Their seeds under `tools/fuzz/corpus/` run as replay tests under ctest on every compiler. The libFuzzer step
in CI does not run these three yet; a local run is `fuzz_<name> corpus-dir` with a Clang build and `LUMORA_BUILD_FUZZERS=ON`.
Evidence label: software only.
