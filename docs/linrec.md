# linrec

`core/linrec/linrec.h` and `linrec.cpp` (`lumora::linrec`) work with linear recurrences over the field of residues modulo the prime `998244353`. `linrec_nth` gives the n-th term of a recurrence of order `d` (at most 256) for any `n` up to `2^64 - 1`; `linrec_bm` gives the shortest recurrence that generates a sequence of at most 1024 terms (the Berlekamp-Massey method). A recurrence of order `d` with coefficients `c[0 .. d)` means `a[k] = c[0] a[k-1] + ... + c[d-1] a[k-d]` for every `k >= d`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. This module was written on the maintainer's own branch under the same standards as the contributor modules: header contract, independent reference, exhaustive extents, mutants with survivor arguments.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state. All products are `u64` products of two residues below `2^30`, reduced at once. The source includes `linrec.h` only.

## Check order and writes

- `linrec_nth`: NullArgument for a null `out` (always) or a null `coef`, `init` or `scratch` (only while `d > 0`); then `d > 256` is TooLarge; then, only while `d > 0`, `scratch_words < 4 d` is ScratchTooSmall; then any entry of `coef` or `init` that is not below the modulus is ValueOutOfRange (the check covers every entry, also when `n < d`). `*out` is written only on Ok. `d = 0` is Ok with `*out = 0` and needs no other pointer.
- `linrec_bm`: NullArgument for a null `out_d` (always) or a null `seq`, `scratch` or `out_coef` (only while `len > 0`); then `len > 1024` is TooLarge; then, only while `len > 0`, `scratch_words < 3 len + 3` is ScratchTooSmall; then any entry not below the modulus is ValueOutOfRange; then `out_cap < L` is OutTooSmall (decided after the computation, nothing written, not even `*out_d`). On Ok `*out_d = L` and only `out_coef[0 .. L)` is written.
- The two sizing functions check a null output, then the limit.

## Methods and bounds

`linrec_nth`: the term is a linear combination of the initial terms, with the coefficients of `x^n` reduced modulo the characteristic polynomial `x^d - c[0] x^(d-1) - ... - c[d-1]`. The scratch holds the result polynomial, the base polynomial (`x` modulo the characteristic polynomial; for `d = 1` that is the constant `c[0]`) and a product buffer of `2 d` words, so `4 d` words. Binary exponentiation over the bits of `n`: at most 64 squarings and 64 multiplications, each a schoolbook product of `d^2` multiplications and a reduction of `d (d - 1)` multiplications. So at most `128 (2 d^2)` field multiplications. For `n < d` the term is read from `init` through the same polynomial path (equivalent, see survivors). The last squaring is skipped. The reduction folds the coefficients from degree `2 d - 2` down to `d`, each in `d` steps.

`linrec_bm`: the classical method with three polynomials `C` (the current connection polynomial, `C[0] = 1`), `B` (the one before the last change of length) and `T` (a copy), each of `len + 1` words, so `3 len + 3` words. For every position the discrepancy `disc = seq[i] + sum C[j] seq[i-j]` is computed; if it is 0 only the shift `m` grows; otherwise `C` is corrected by `disc / b x^m B`, and when `2 L <= i` the length becomes `i + 1 - L` and `B`, `b`, `m` are renewed. The order is the final `L`; the coefficients returned are `-C[j]` modulo the prime. Steps: `len` positions, each a dot product of at most `L <= len` terms, a correction of `len + 1` words and, on a length change, two copies of `len + 1` words, so at most `O(len^2)` = about 4.2 million multiplications for `len = 1024` plus the same number of copies. One modular inverse (by exponentiation, 30 squarings) per discrepancy that is not 0.

## Tests

`tests/linrec/linrec_tests.cpp`, 6 test cases, 223074 assertions. Expected values come from three labelled sources. (1) IN-TEST ORACLES: the recurrence iterated term by term; closed forms at the largest `n` (a geometric sequence `7 * 3^n` with the exponent reduced by Fermat, an arithmetic sequence `5 + 3 n`, a constant sequence, a zero coefficient, Fibonacci at `n = 10`, 90, `10^18` and `2^64 - 1`); and the generation property of every returned recurrence (each term from `L` on equals the recurrence). (2) DIGESTS from `tools/linrec_ref.py` (independent Python): matrix power for orders up to 16 and a full 64-bit `n`; term-by-term iteration for small `n` and for orders 64 and 256 at the boundary values of `n`; Gaussian elimination mod p (with a binary search on `L`, valid because a recurrence of order `L` is also one of order `L + 1`) for the shortest recurrence of all sets up to length 100. For orders 24 to 256 with a full 64-bit `n`, and for the shortest recurrence of the large sequences (length 200 to 1024), the reference uses polynomial exponentiation and Berlekamp-Massey written separately (the SAME METHODS as the C++ code, different code); its self-check compares them with iteration and elimination (for the order 256, length 512 sequence the elimination at `L` and `L - 1` runs in the self-check). A digest is FNV-1a 64 over the little-endian bytes of every `u32` emitted: the term for `linrec_nth`; for `linrec_bm` the order and, only when `2 L <= len` (the recurrence is then unique), the coefficients. `--check` verifies that the digest table in the test file is current and runs the self-check (about 20 s). (3) Exhaustive grids.

Extents, with their labels:
- EXHAUSTIVE by comparison with iteration: orders 1, 2 and 3 with every coefficient and every initial term over the four residues `{0, 1, 2, p - 1}` and every `n` from 0 to 20 (`21 * (16 + 256 + 4096)` cases); order 4 over `{0, 1}` for coefficients and initial terms, every `n` from 0 to 12 (`256 * 13`).
- EXHAUSTIVE by digest for the shortest recurrence: every sequence over `{0, 1, 2}` of length 0 to 8 (9841) and over `{0, 1, 2, p - 1}` of length 0 to 6 (5461), each with the generation property and the capacity checks (exact capacity `L` gives Ok, capacity `L - 1` gives OutTooSmall with nothing written and sentinels intact).
- SAMPLED (SplitMix64, seeds in the reference): 780 `linrec_nth` cases (orders 0 to 12, `n` below 4000, also compared with the in-test iteration); 96 with orders up to 16 and a full 64-bit `n`; 21 with orders 24 to 256 and a full 64-bit `n`; 18 at orders 64 and 256 with the `n` values 0, 63, 64, 255, 256, 257, 300, 1000 and 3000 (also compared with the in-test iteration); 155 sequences from known recurrences of order 0 to 30 at lengths `2 L`, `2 L + 1`, `2 L + 5`, `3 L + 3` and `L`; 60 random sequences of length below 25; 8 large sequences (order 256 at length 512, 513 and 1024; order 255 at 511; order 100 at 200 and 1000; orders 1 and 2 at 1024) and one random sequence of the largest length 1024.
- By construction: all-zero sequences (order 0) up to length 1024, a lone one after zeros (order equals the length), a constant sequence, powers of 2, Fibonacci over 300 terms, a lone one at the front (order 1, coefficient 0).
- Odd-word scratch offsets everywhere (the scratch passed starts one word into a larger array). The check-order matrix covers every failure and the exact scratch size.
- Not asserted: overlapping arrays, scratch contents, running time.

## Mutation testing

57 real source mutants of `linrec.cpp`, each compiled with the prebuilt Catch2 objects and run on the FINAL test file in the plain build (`-O1`), one at a time with a 20 s limit (the final test file adds three assertions to the mutant runs 1 to 20, which were run before the check on a coefficient equal to the modulus; that mutant is killed by it). 42 killed by failing assertions or by a crash (dropped null checks end in SIGSEGV; the crash is the kill). No timeouts. 15 survivors, each with an argument:
- `x > y` instead of `x >= y` in the field subtraction: equivalent, for `x == y` the result is the modulus instead of 0, and the modulus is a valid (unreduced) zero for every later use (a product is reduced again, a sum with it reduces to the other summand, the output of the coefficient maps the modulus to 0).
- the product buffer cleared only up to `2 d - 2` words: equivalent, the highest word is never written or read.
- an added `0 * j`, an added `+ 0` and `+ 0u` (three no-op mutants written as mutants, equivalent).
- the reduction does not clear the folded words: equivalent, only the first `d` words are copied out.
- `n < d - 1` instead of `n < d` and the early return removed: equivalent, for `n = d - 1` and below the polynomial path gives the initial term too (the polynomial `x^n` is its own reduction).
- the last squaring not skipped: equivalent, the squared base is not used after the last bit.
- the initial `B[0]` or `b` set to 2 and the copy of `T[0]` into `B[0]` dropped: equivalent, at the first nonzero discrepancy `L = 0` so `2 L <= i` holds, the old `B` is overwritten with `T` before it is ever used, and `C[0] = 1` always.
- `2 L <= i + 1` instead of `2 L <= i`: equivalent up to the unspecified choice. At `2 L = i + 1` the new length `i + 1 - L` equals `L`; `B` becomes the old `C`, which has exactly the discrepancy `disc` at position `i`, so every later correction is still valid. The order is the same and the coefficients are the same whenever they are unique (`2 L <= len`); for non-unique cases only the generation property is asserted.

## Not run

- Windows: CI-only. MSan and other compilers were not run.
- The mutant runs used the plain build only; no sanitizer-instrumented mutants.
- No timing assertion; no scaling table.
- The reference uses the same methods (separately written) for the largest `linrec_nth` and `linrec_bm` cases; the matrix method and elimination cover the smaller ones.
