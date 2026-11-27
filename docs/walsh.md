# walsh

`core/walsh/walsh.h` and `walsh.cpp` (`lumora::walsh`) transform an array of `2^k` residues modulo `kMod = 998244353` (`k <= 20`, so at most 1048576 entries): the xor Walsh-Hadamard transform (`walsh_xor`) and its inverse (`walsh_xor_inverse`), the subset zeta transform (`subset_zeta`) and its inverse (`subset_mobius`), and the xor convolution of two arrays (`xor_convolve`). Every transform takes exactly `k * 2^(k-1)` butterflies, so O(k * 2^k); the convolution takes three transforms and two linear passes.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `12943d0` (the header amendment: a failed `xor_convolve` leaves `out` AND `scratch` unchanged). The maintainer answers to the gates were relayed via Instinct; the code follows those answers.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no large automatic arrays. The working memory is the array itself; `xor_convolve` uses `out` for one transform and the caller's `scratch` (exactly `2^k` words are enough, more are accepted) for the other.

## Check order and writes

The four transforms: a null `a` is NullArgument; then `k > 20` is TooLarge; then any entry of `a` at `kMod` or more is ValueOutOfRange (every entry is scanned). On any failure the array is unchanged (all checks come before the first write).

`xor_convolve`: a null among `a`, `b`, `out`, `scratch` is NullArgument (all four are required for every `k`, also `k = 0` and a `k` that is too large; null wins over every other failure, also with `scratch_words = 0`); then `k > 20` is TooLarge; then `scratch_words < 2^k` is ScratchTooSmall (compared as `u32`; `scratch_words` above `2^k` is fine; `k = 0` needs one word); then any entry of `a`, then of `b`, at `kMod` or more is ValueOutOfRange. Nothing is written on any failure: `out` and `scratch` keep their contents (the tests compare whole buffers with sentinels around them). Nothing observable tells whether `a` or `b` was scanned first. `a` and `b` are never modified. `a`, `b`, `out` and `scratch` must not overlap (outside the contract, not tested; `a == b` as the same pointer is covered by that rule).

`k = 0` is legal: one entry. All four transforms leave it unchanged (the inverse scales by `2^0 = 1`); the convolution is the product modulo `kMod`.

## Method

Each transform is an iterative pass over block lengths `len = 1, 2, 4, .., 2^(k-1)`; for every pair `(j, j + len)` with the bit `len` clear in `j`, in place:
- `walsh_xor`: `(x, y) -> (x + y, x - y)` modulo `kMod`.
- `subset_zeta`: `a[j + len] += a[j]` (the superset gets its subset). `subset_mobius`: `a[j + len] -= a[j]`, the inverse pass.
- `walsh_xor_inverse`: the `walsh_xor` pass, then every entry times `2^-k`. The factor is `(kMod + 1) / 2` (which is `2^-1`, `kMod` is odd) multiplied `k` times.
- `xor_convolve`: copy `a` into `out` and `b` into `scratch`; `walsh_xor` pass on both; `out[i] = out[i] * scratch[i]`; one more pass on `out`; scale by `2^-k`. Only after every check; only `out[0 .. 2^k)` and `scratch[0 .. 2^k)` are written.

Bounds of the arithmetic:
- A modular sum adds two residues, each below `kMod`. The sum is below `2 * kMod = 1996488706 < 2^32`, so it fits a `u32` and one subtraction of `kMod` (when the sum is at least `kMod`) brings it back below `kMod`. A modular difference is `x - y` when `x >= y`, else `x + kMod - y`, which is below `kMod` (here `x < y`) and above zero, and never exceeds `2 * kMod`.
- The only products are the pointwise product and the scale. Both are taken as `u64` (`< kMod^2 < 2^60`) and reduced with one `%`. No 128-bit type.
- Every value that enters a helper is a residue: the inputs are checked first, and every helper returns a residue.

Step counts (exact, for the real code): a transform pass is `k * 2^(k-1)` butterflies: `walsh_xor` that many adds and as many subtracts, `subset_zeta` that many adds, `subset_mobius` that many subtracts. The range scan is `2^k` comparisons. `walsh_xor_inverse` adds `k + 2^k` multiplications. `xor_convolve` is `3 * k * 2^(k-1)` adds, as many subtracts, `2^k + k + 2^k` multiplications and `2 * 2^k` comparisons plus `2 * 2^k` copies. These were checked for every `k = 0 .. 20` and the five functions with a counting copy of the real source (a text substitution adds a counter to `add_mod`, `sub_mod`, `mul_mod` and to the range compare; nothing else changes): all counts match, for example `k = 20`: `walsh_xor` 10485760 adds, 10485760 subtracts, 1048576 compares; `walsh_xor_inverse` 1048596 multiplications; `xor_convolve` 31457280 adds, 31457280 subtracts, 2097172 multiplications, 2097152 compares. The counting copy and its driver are not part of the change (the real source has no counters).

### One-time scaling table (observation, not asserted)

Best of 3, milliseconds, a pseudo-random array, this machine, single thread. There is no timing assertion in the tests.

| k | -O0 xor / inverse / zeta / mobius / convolve | -O2 xor / inverse / zeta / mobius / convolve |
|---|---|---|
| 16 | 2.0 / 2.1 / 1.3 / 1.2 / 6.2 | 0.5 / 0.6 / 0.3 / 0.3 / 1.7 |
| 18 | 8.6 / 8.9 / 5.4 / 5.5 / 27.2 | 2.3 / 2.5 / 1.3 / 1.4 / 7.5 |
| 20 | 38.4 / 39.7 / 24.7 / 24.7 / 121.1 | 11.4 / 12.1 / 7.1 / 7.2 / 36.4 |

## Tests

`tests/walsh/walsh_tests.cpp` has 12 test cases (49 assertions; most checks are accumulated and reported once per group, with the first failing case in the message). Expected values come from three places:
1. DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from `tools/walsh_ref.py`, a standard-library Python program with three formulations compared with each other by the script (the definitions as sums, a recursive-halving formulation with the inverse built from `pow(2^k, -1, p)`, and sums over the non-zero entries of sparse arrays). The tests recompute each digest (FNV-1a over 64-bit words: `k`, the inputs, the outputs) and the case count from the real functions. Groups (the file documents the exact fold order):

| group | cases | digest |
|---|---|---|
| ex3 | 6654 | `0x40907D64C95C99B0` |
| ex4 | 276 | `0x80C98B9C54593554` |
| cv3 | 6651 | `0xA8174D93B3C33D68` |
| cv4 | 272 | `0x3C8C2D215DB50681` |
| cvb | 65536 | `0x032BF01C21D490E5` |
| unit | 2047 | `0x9D508B01A747DCED` |
| upair | 5461 | `0x8C0C332D5C1A6858` |
| rnd | 33 | `0x91158E7507083D72` |
| dense | 6 | `0x5B74DCCC372ED37D` |
| sp20 | 1 | `0x369EA838C8FAD314` |
| cd20 | 1 | `0xB779F1401E31E80F` |

2. ORACLE inside the test, sharing no code with the implementation: the definitions as sums over `(s, t)` with a bit-counting loop, the inverse scaled by `(2^k)^(p-2)` (Fermat), the convolution as the double loop, in `u64`.
3. CLOSED FORMS and identities: the transforms of unit vectors, of the all-ones array, of the parity pattern `(-1)^popcount(t)` and of the upper-half indicator; the convolution of two unit vectors; inverse after transform, Mobius after zeta, zeta after Mobius, the transform applied twice is `2^k` times the array, the convolution theorem, commutativity, `e_0` as the identity.

Extents (labelled):
- EXHAUSTIVE, all four transforms against the oracle and the digest: every array over `{0, 1, p - 1}` for `k = 0 .. 3` (6654 arrays) and over `{0, 1, 2, p - 1}` for `k = 0 .. 2` (276 arrays).
- EXHAUSTIVE, every unit vector for `k = 0 .. 12`, all four transforms, every output entry against the closed form (digest through `k = 10`; for `k = 11` and `12` the closed form is the only check). The closed forms are themselves compared with the oracle for every unit vector with `k <= 6`.
- EXHAUSTIVE convolution against the oracle and the digest: every ordered pair over `{0, 1, p - 1}` for `k = 0 .. 2` (6651 pairs), over `{0, 1, 2, p - 1}` for `k = 0 .. 1` (272 pairs), over `{0, 1}` for `k = 3` (65536 pairs); every ordered pair of unit vectors for `k = 0 .. 6` (5461 pairs, the result must be `e_(i xor j)`).
- SAMPLED unit vectors for `k = 13 .. 20`, all four transforms, closed forms: for `k = 13 .. 16` the index list 0, 1, every power of two, `2^j - 1` for `j = 2 .. k` and four seeded random indices; for `k = 17 .. 20` a thinner list (0, 1, `2^j` for `j` in 1, 2, `k/2 - 1`, `k/2`, `k/2 + 1`, `k - 2`, `k - 1`, `2^j - 1` for `j` in 2, `k/2`, `k - 1`, `k`, two seeded random). No full unit sweep was done for `k = 13 .. 20` (a full sweep at `k = 20` is 2^20 transforms of 2^20 entries, not feasible in a test).
- SAMPLED random arrays (fixed LCG seeds, entries biased to `0`, `1`, `p - 1`, `p - 2`): `k = 0 .. 10` three each, `k = 11 .. 16` one each; the oracle runs for `k <= 8`, the identities for all.
- SAMPLED at `k = 20`: a sparse array with three entries (three transforms against the definition summed over the non-zero entries, the convolution of a sparse pair), a dense random array convolved with a sparse array against `out[s] = sum b[u] * a[s xor u]`, a random pair through every identity and the convolution theorem, and three dense structured arrays (all ones, parity pattern, upper-half indicator) against closed forms; an array of `p - 1` everywhere round trips through every transform pair.
- Failure matrix: a null `a` (each transform) and each of the four null pointers (convolution) for `k = 0, 1, 20, 21, UINT32_MAX` and `scratch_words` 0, large and `UINT32_MAX`; `k = 21` and `UINT32_MAX` with bad entries and `scratch_words` 0 (TooLarge beats the rest); `scratch_words` `0` and `2^k - 1` for `k = 0 .. 4` and at `k = 20`, with and without bad entries (ScratchTooSmall beats ValueOutOfRange); entries `kMod`, `kMod + 1`, `UINT32_MAX` at the first, middle and last position, in `a` only, `b` only and both, and a bad last entry at `k = 20`; scratch above `2^k` accepted. Every failure leaves `a`, `b`, `out` and `scratch` (the whole buffer including the sentinel words) unchanged.
- Boundary rows (hand-written, from the definitions): sums exactly equal to `kMod` (`[1, p - 1]`), `kMod - 1` (`[p - 2, 1]`) and `2 * kMod - 2` (`[p - 1, p - 1]`); differences exactly 0 (`[5, 5]`) and wrap cases; products of `p - 1` residues.

## Boundary values of the modular add and subtract

No search was needed: the exhaustive arrays over `{0, 1, p - 1}` and the hand rows reach every boundary of the one conditional correction by construction. Sum exactly `kMod`: `walsh_xor([1, p - 1])`; sum `kMod - 1`: `walsh_xor([p - 2, 1])`; difference exactly 0: `walsh_xor([5, 5])`; the largest sum: `[p - 1, p - 1]`. The equality variants of the add (`>` instead of `>=`, thresholds `kMod - 1` and `kMod + 1`, subtracting `kMod - 1`) and of the subtract (`>` instead of `>=`, `x >= y + 1`, wrapping with `kMod - 1` or `kMod + 1`, the reversed compare) were each run against the hand-row test case alone and all fail there (rc 42, an assertion failure), and are killed in the full run.

## Mutation testing

139 real source mutants of `core/walsh/walsh.cpp` (one text change each: the modular add, subtract and multiply and their equality variants, the range scan, every pass bound and step, the scale, every check and its position, the check codes, the convolution copies, passes, product and scale), each run on the FINAL test file (no staging) in two builds, serially, one mutant at a time, each as a complete run of the whole suite: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. The test object is the same plain object in both builds: it is NOT sanitizer-instrumented; only the mutated source is. Plain: 128 killed, 11 survived, 0 timed out. Sanitizer build: 128 killed, 11 survived, 0 timed out, the same sets. No mutant is killed only by the sanitizer build. A timeout would not count as a kill (none occurred). Catch2 runs the test cases in a random order, so which test case fails first differs between runs; the kill count does not depend on it.

Crash-only kills (the batch run shows no failing assertion): 19 mutants (45, 48, 49, 58, 63, 65, 70, 80, 82, 83, 85, 99, 100, 101, 102, 103, 104, 106, 117; the union of both builds), each re-run alone. Plain, alone: SIGSEGV (`Segmentation violation signal`) for 49, 58, 80, 82, 83, 85, 99 to 104 and 106; SIGABRT for 65 and 70; abort (rc 134) for 45 and 63 after failing assertions were printed; 117 fails an assertion (rc 42); 48 printed a failing assertion at the `k = 20` test and then ran past the 100 s limit alone (its batch run ended with an abort, rc -6); it counts as killed by the failing assertion, not by the timeout. Sanitizer build, alone: AddressSanitizer `heap-buffer-overflow` for 48, 49, 58, 63, 65, 70, 82, 83, 85 and 106; UBSan `load of null pointer` for 80, 99, 100 and 103; UBSan `store to null pointer` for 101, 102 and 104; UBSan `shift exponent 4294967295 is too large` for 117; 45 exits with rc 1 and failing assertions.

Survivors (11), all argued equivalent:
- 10 `s % kMod` for the single subtraction: `s < 2 * kMod`, so `s % kMod` is `s - kMod` when `s >= kMod` and `s` otherwise.
- 11 `s + 0u`: the same value.
- 22 `(x + kMod - y) % kMod` on the wrap branch: there `x < y`, so `0 < x + kMod - y < kMod` and `%` does nothing.
- 30 `x % kMod` before the product: `x` is always a residue (inputs are checked, every helper returns a residue), so it does nothing. The tests cannot reach a non-residue `x` because out-of-range input is rejected before any arithmetic.
- 42 the range scan returning at the first bad entry: the function has no side effect and the callers write nothing on failure, so scanning on is not observable.
- 92 scaling by `2^-k` before the `walsh_xor` pass: the pass is linear over the integers modulo `kMod`, so the scale commutes with it; same function.
- 98 `return r` instead of `Ok` at the end of `walsh_xor`: `r` is Ok there.
- 135 `k = 0` special case computing `a[0] * b[0]` directly: the general path at `k = 0` has no passes, the product, and a scale by 1; same result.
- 136 `2u << (k - 1)` for `k > 0` equals `1u << k`; `k = 0` keeps 1.
- 137 `j < (i | len)` for `j < i + len`: `i` is a multiple of `2 * len`, so the bit `len` is clear and `i | len = i + len`.
- 138 `(kMod >> 1) + 1` for `(kMod + 1) / 2`: `kMod` is odd, both are 499122177.

## Not run

Windows (CI only); any timing assertion; overlapping `a`, `b`, `out`, `scratch` and `a == b` as one pointer (the header leaves them open); a full sweep of the unit vectors for `k = 13 .. 20`; a digest for the unit vectors at `k = 11` and `12` (closed forms only); `k = 20` random dense arrays against an oracle (identities and the digest of a dense-with-sparse convolution only). The mutant test object is not sanitizer instrumented. `python3 tools/walsh_ref.py --check tests/walsh/walsh_tests.cpp` takes about 24 s here (`NAME...` checks only the named groups).
