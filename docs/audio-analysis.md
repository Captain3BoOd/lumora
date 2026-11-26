# Audio analysis

`core/audio/analysis.h` and `analysis.cpp` (`lumora::audio`) give the audio-reactive features of `docs/physics-audio.md` ("Audio reactivity"): block loudness, an envelope follower, and an integer FFT power spectrum with band sums, all on 16-bit PCM blocks. Pure functions on caller-owned buffers: no I/O, no platform calls, no allocation beyond small local arrays. The microphone capture and its capability gate are elsewhere (`docs/capabilities.md`).

Evidence label: unit tests only (CI-verified).

The analysis path uses no floating point and no libm. Every result is bit-exact on every compiler and platform, and the tests compare exact values with no tolerances. Floating point appears only in `tools/audio_analysis_ref.py`, which builds the tables with `math.cos` and `math.sin`.

## Level

`measure_level(samples, n, out)` returns the peak and the sum of squares of a block.

- `peak` is the largest absolute value. -32768 counts as 32768.
- `sum_squares` is the exact sum of `s*s` in unsigned 64 bits. Each term is at most 2^30, so the sum is exact for up to 2^34-1 samples. Beyond that it wraps modulo 2^64: no saturation and no error.
- `n == 0` gives zeros and returns true (`samples` may be null). A null `out`, or null `samples` with `n > 0`, returns false and writes nothing.

`isqrt64(v)` is the floor of the square root, exact for every 64-bit value (bitwise method, no float).

## Envelope follower

`envelope_step(env, x, attack_q16, release_q16)`: coefficients are Q16 (65536 is 1.0) and a larger value is clamped to 65536. The coefficient is `attack` if `x > env`, else `release`. With `delta = x - env` as a signed 64-bit value, `step = (delta * coef) >> 16` (arithmetic, flooring shift) and the result is `env + step`. The result always lies between `env` and `x` inclusive.

The floor makes the follower asymmetric on purpose: a falling delta of -5 with coefficient 1 steps by -1, while a small rising delta steps by 0. A zero coefficient holds, and 65536 reaches `x` exactly.

## FFT power spectrum

`fft_power(samples, n, out_power)` writes `n/2` bins. The Nyquist bin is not reported. Bin k is the frequency k/n of the sample rate.

| Item | Bound |
| --- | --- |
| `n` | a power of two from 64 to 1024 (64, 128, 256, 512, 1024) |
| bins written | `n/2` (32 to 512) |
| window | `int32`: `x[i] = (s[i] * h[i*(1024/n)]) >> 15` |
| twiddle products | `int64`: `(b.re*wr - b.im*wi + 16384) >> 15`, floor shift |
| butterfly sums | `int32`, floor halving `>> 1` per component |
| power | `int64` squared sum cast to `uint32`, no saturation |

Any other `n`, or a null pointer, returns false and writes nothing.

Tables (embedded in `analysis.cpp`, 1024 entries each, Q15):

- `c[j] = clamp(round(32768*cos(2*pi*j/1024)), -32767, 32767)`
- `h[j] = min(32767, round(32768*(0.5-0.5*cos(2*pi*j/1024))))`
- `round` is half away from zero.

Steps:

1. Window with the Hann table: `x[i] = (s[i] * h[i*(1024/n)]) >> 15`.
2. Bit-reverse the windowed samples. The imaginary parts start at zero.
3. Run spans 1, 2, ..., n/2. For a stage with block `m = 2*span` and position `j`, `t = j*(1024/m)`, `wr = c[t]`, `wi = -c[(t+768) mod 1024]`.
4. Butterfly: `t.re = (b.re*wr - b.im*wi + 16384) >> 15`, `t.im = (b.re*wi + b.im*wr + 16384) >> 15`, `a' = (a + t) >> 1`, `b' = (a - t) >> 1`. Every stage halves, so the output is scaled by 1/n.
5. `out_power[k] = re*re + im*im` in 64 bits, cast to `uint32`.

Power bound: after the 1/n scaling every component stays within about 2^15, so the squared sum stays below 2^32 (about 2^31 at the very most). The tests cover full-scale and -32768 blocks.

Only entries 0..511 and 768..1023 are read (`wr` uses `t < 512`, `wi` uses `(t+768) mod 1024`), so entries 512..767 are never read by the transform. They are kept so the table is the plain 1024-entry cosine.

## Band energies

`band_energies(power, bins, edges, band_count, out_energy)` sums bins into bands. Band `b` covers `[edges[b], edges[b+1])`.

- `edges` is required and has `band_count + 1` entries, even when `band_count` is 0.
- `edges` must be non-decreasing, with `edges[0] <= bins` and `edges[band_count] <= bins`.
- Bins outside `[edges[0], edges[band_count])` are ignored. An empty band gives 0. Sums are exact in 64 bits.
- `out_energy` is required when `band_count > 0`. `power` is required only if some band is non-empty.
- On any violation, or a null pointer where data is needed, it returns false and writes nothing.

## Regenerating and checking

The tables in `analysis.cpp` and the reference block in the tests are generated.

```
python3 tools/audio_analysis_ref.py --emit-tables              # table block for analysis.cpp
python3 tools/audio_analysis_ref.py --check-tables core/audio/analysis.cpp
python3 tools/audio_analysis_ref.py                            # reference block for the tests
python3 tools/audio_analysis_ref.py --check tests/audio_analysis/audio_analysis_tests.cpp
```

The reference builds both tables with `math.cos` and checks them at import against 50-digit decimal tables. It has its own constants and shares no code with the C++.

## Tests

`tests/audio_analysis/audio_analysis_tests.cpp` has exact checks only:

- silence, full scale, -32768 samples, tones, impulses, DC, and seeded random blocks (xorshift64*), for every size from 64 to 1024: 27 fixtures per size, 135 golden digests of the full power output;
- invalid sizes and null pointers, and that nothing is written on failure;
- an in-test naive oracle (an independent transform with its own arithmetic, sharing no code with the implementation) compared on every size;
- digests over 8100 envelope cases and 6000 band cases, and an `isqrt64` boundary sweep around perfect squares and at the 64-bit limits.

## Mutation check

82 hand-written source mutants of `analysis.cpp` (table entries, missing rounding term, dropped halving, shifts, sign of `wi`, stride, bit reversal, window, power width, `isqrt`, envelope clamp and flooring, band validation and null rules) were each compiled against the unchanged tests. 75 were killed, 7 survived. Seven of the kills were crashes only (null-pointer or size guards removed): they die on a null dereference or out-of-bounds access rather than on a failed assertion.

Survivors, each argued:

- `c[256]` plus 1: the entry is 0, and it is read only as `wr` at `t = 256` in the last stage. The change adds `b.re >> 15`, which is nonzero only when `|b.re| >= 16384`. The Hann window averages 1/2, so `|b.re|` stays at or under about 16383. No input found in 2248 targeted tries (every bin as a tone at four phases, plus random full-scale blocks).
- `c[700]` minus 1: entries 512..767 are never read (see FFT section). Equivalent.
- Envelope: `x == env` early return removed. `delta` is 0, so the step is 0. Equivalent.
- Twiddle products in 32 bits: the largest product sum is below 32768*sqrt(2)*32767, about 1.5e9, under 2^31. Equivalent for every reachable input.
- Power computed in `int32`: the sum is at most about 2^31 only at an impossible extreme. Equivalent for every reachable input.
- Power saturated at 2,000,000,000: same bound. Equivalent for every reachable input.
- `edges[0] > bins` check removed: implied by non-decreasing edges and `edges[band_count] <= bins`. Equivalent.

Not run: Windows, Android, Vulkan/GLES, the editor, `tools/ci/test.py`, fuzzing, a full local build. Windows is covered by CI only.
