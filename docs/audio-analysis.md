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

Magnitude bounds (derived, then checked):

- Every butterfly output is a mean of windowed samples times unit-modulus twiddles, plus rounding. A sub-FFT of m >= 2 points taken at stride n/m covers whole periods of the Hann window, so the mean of its weights is 1/2 (for m = 2, `h[i] + h[i+n/2] = 1`). With `|s| <= 32768` every complex value at every stage therefore has modulus at most about 16384 plus a rounding drift under 8 (window floor, at most 1; each twiddle rounding at most 0.71, halved at each later stage).
- So components stay within about 2^14, power `re*re + im*im` stays below about 2^28 (under 2^29 with the drift), and the twiddle sums `b.re*wr - b.im*wi` stay below about 16392 * 32769, about 2^29, under 2^30. The largest `|wr, wi|` modulus in the table is 32768.56.
- Without the averaging argument, the cruder induction `R' = (R + R*|w|/32768 + 0.71)/2 + 0.5` from `R = 32767` gives a modulus under 32779 at every stage, so twiddle sums and power stay under 1.075e9, below 2^31. This is what the `int32` and 2,000,000,000 mutants rely on.
- Largest value seen over full-scale DC, impulses and a sweep of full-scale sine and square tones at 64, 256 and 1024 points: power 268402689 (a component of 16383), just under 2^28.

The tests cover full-scale, -32768 and tone blocks. `uint32` holds the power with a factor of at least 4 to spare.

## Band energies

`band_energies(power, bins, edges, band_count, out_energy)` sums bins into bands. Band `b` covers `[edges[b], edges[b+1])`.

- `edges` is required and has `band_count + 1` entries, even when `band_count` is 0.
- `edges` must be non-decreasing, with `edges[0] <= bins` and `edges[band_count] <= bins`.
- Bins outside `[edges[0], edges[band_count])` are ignored. An empty band gives 0. Sums are exact in 64 bits.
- `out_energy` is required when `band_count > 0`. `power` is required only if some band is non-empty.
- On any violation, or a null pointer where data is needed, it returns false and writes nothing.

## Table test hook

The two tables have external linkage in `lumora::audio` but are not declared in `analysis.h`. The test file declares them `extern` and compares all 1024 entries of each against values computed with `std::cos` (round half away from zero, clamped as above). The test also checks that no value sits within 1e-6 of a rounding tie, so double rounding cannot differ from exact arithmetic. This is the only reason for the linkage: do not use the tables from other code.

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
- all 1024 entries of each table against `std::cos`, and a zero-band call with `edges[0]` above `bins`;
- an in-test naive oracle (an independent transform with its own arithmetic, sharing no code with the implementation) compared on every size;
- digests over 8100 envelope cases and 6000 band cases, and an `isqrt64` boundary sweep around perfect squares and at the 64-bit limits.

## Mutation check

82 hand-written source mutants of `analysis.cpp` (table entries, missing rounding term, dropped halving, shifts, sign of `wi`, stride, bit reversal, window, power width, `isqrt`, envelope clamp and flooring, band validation and null rules) were each compiled against the unchanged tests. 77 were killed, 5 survived. Seven of the kills are crashes only (null-pointer or size guards removed): they die on a null dereference or out-of-bounds access rather than on a failed assertion. The single-entry table mutants (`c[256]`, `c[700]`, the last cosine entry, Hann entries) are killed by the full table comparison.

Survivors, each argued:

- Envelope `x == env` early return removed: `delta` is 0, so the step is 0. Equivalent.
- Twiddle products in 32 bits: the sums stay below about 2^30 (and below 2^31 even by the crude bound), see the magnitude bounds. Equivalent for every reachable input.
- Power computed in `int32`: power stays below about 2^28 (below 2^31 by the crude bound). Equivalent.
- Power saturated at 2,000,000,000: never reached, same bound. Equivalent.
- `edges[0] > bins` check removed: implied by non-decreasing edges and `edges[band_count] <= bins` (for zero bands, `edges[0]` is `edges[band_count]`). Equivalent. A test still pins the zero-band `edges[0] > bins` result.

Not run: Windows, Android, Vulkan/GLES, the editor, `tools/ci/test.py`, fuzzing, a full local build. Windows is covered by CI only.
