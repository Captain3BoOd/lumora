# zfunc

`core/zfunc/zfunc.h` and `zfunc.cpp` (`lumora::zfunc`) give two arrays for a byte string `s[0 .. n)`: the Z array (`zfunc_z`: the longest common prefix of `s` and each suffix) and the border array, also called the prefix function (`zfunc_pi`: the longest proper border of each prefix). Limit: at most `262144` bytes. The smallest period of a string is `n - pi[n - 1]` for `n > 0`; it is not a separate function.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. This module was written by the maintainer's own branch under the same standards as the contributor modules: header contract, definition oracles, independent reference, exhaustive extents, mutants with survivor arguments.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no scratch. The source includes `zfunc.h` only.

## Check order and writes

- Both functions: NullArgument for a null `s` or a null output array only while `n > 0`; then `n > 262144` is TooLarge (the condition uses the given `n`, also above the limit, and a null pointer wins over TooLarge). On failure nothing is written. On success only `out_z[0 .. n)` or `out_pi[0 .. n)` is written (the tests put sentinel words before and after the array). `n = 0` is Ok with every pointer null and nothing written.
- Bytes are compared exactly as values 0 to 255. `z[0] = n`; `pi[0] = 0`. Overlap of the arrays with `s` is not specified and not tested.

## Method and bounds

`zfunc_z` keeps the window `[l, r)` that matches a prefix of `s` and ends furthest to the right. For `i < r` it starts from `min(z[i - l], r - i)` and compares forward only from there. The window moves right whenever `i + z[i] > r`.

`zfunc_pi` keeps the length `k` of the current border. For each `i` it falls back through earlier borders (`k = pi[k - 1]`) until `s[k] == s[i]` or `k = 0`, then extends by one if equal.

Step bounds, counted in byte comparisons:
- Z scan: every comparison that succeeds moves `r` right by one (a successful comparison happens at a position `>= r`, because inside the window the start value is already a lower bound that is exact unless it hits `r - i`), so there are at most `n` successful comparisons in total; every `i` has at most one failing comparison. At most `2n - 1` comparisons.
- Border scan: `k` grows by at most one per `i`, every fall-back step lowers `k` by at least one, so the fall-backs total at most `n - 1`. Comparisons: at most `2n - 2`.
- No counter wraps: all indices are at most `n <= 262144`.

## Tests

`tests/zfunc/zfunc_tests.cpp`, 6 test cases, 6717067 assertions. Expected values come from three labelled sources. (1) DEFINITION ORACLES in the test: the Z value by comparing the prefix with each suffix byte by byte, and the border by trying every candidate length from the longest down and comparing the two substrings (no window, no fall-back chain). (2) DIGESTS from `tools/zfunc_ref.py` (independent Python with the same definitions for small strings and a differently written linear version for large strings, border array rebuilt from the Z array); its `--check` verifies that the digest table in the test file is current and compares definitions against the linear versions on 30000 random strings (0 mismatches). A digest is FNV-1a 64 over the little-endian bytes of every `u32` of `z` then `pi`, string after string. (3) CLOSED FORMS for four large strings.

Extents, with their labels:
- EXHAUSTIVE by digest: every string of length 0 to 16 over `{0, 255}` (131071 strings); length 0 to 10 over `{0, 128, 255}` (88573); length 0 to 6 over `{0, 1, 127, 128, 254, 255}` (55987); the empty string, all 256 one-byte strings and all 65536 two-byte strings (65793). Bytes 0 and 255 are adjacent in the first group.
- EXHAUSTIVE against the in-test definition oracle: every `{0, 255}` string through length 13 and every `{0, 1, 2}` string through length 8, with the sentinel check on every call.
- SAMPLED: 3 x 600 fixed-seed strings of length 17 to 400 over 2, 3 and 256 symbols, half of the 256-symbol ones near-periodic (a short block repeated with one byte flipped near the end), against the oracle.
- LARGE, SAMPLED PATTERNS (8, each checked by its digest, by the Z-to-border certificate and by range checks `z[i] <= n - i`, `pi[i] <= i`): one repeated byte (262144), alternating `0/255` (262143), `0 ... 0 1`, `1 0 ... 0`, the Fibonacci word, random over 2 symbols, random over 256 symbols, and a random 1000-byte block repeated. Closed forms are asserted for the first four: repeated byte (`z[i] = n - i`, `pi[i] = i`), alternating of odd length (`z[i] = n - i` for even `i` and 0 for odd `i`, `pi[i] = i - 1`), `0 ... 0 1`, and `1 0 ... 0` (all zero after the first entry).
- Hand rows (8) with values worked out by hand from the definitions, and the check-order matrix including the largest accepted length.
- Not asserted: overlapping arrays, running time.

## Mutation testing

25 real source mutants of `zfunc.cpp` (2 of them changed nothing and are void, so 23 count), each compiled with the prebuilt Catch2 objects and run on the FINAL test file in the plain build (`-O1`), one at a time with a 20 s limit. 15 killed by failing assertions or by a crash (the crash is the kill: dropped null checks and an off-by-one in the output index end in SIGSEGV or SIGABRT), 2 timed out and are NOT counted as kills, 6 survived.

Timeouts: `window reuse disabled` (the Z scan always starts from 0) gives the right output but is quadratic, so the large cases time out; it is a time-only mutant, killed by no assertion. `fall-back reads pi[k] instead of pi[k - 1]` ends in a long loop; not proved non-terminating.

Survivors, with the argument:
- `i < r` to `i <= r` in the Z scan: equivalent. At `i = r` the clamp `r - i` is 0, so the start value is 0 either way.
- `i + z >= r` instead of `i + z > r`: equivalent in output. The window is moved to `[i, r)` with the same right end; it still matches a prefix, so every later start value is still a valid lower bound that the scan extends.
- `i + z > r + 1` instead of `i + z > r`: equivalent in output and still linear. The window is not moved when it would grow by exactly one, so a later `i` may compare one extra position past `r`; that is at most one extra comparison per `i`.
- `r = i + z - 1`: equivalent in output, same argument (the window is one shorter, which only costs at most one extra comparison per `i`).
- `k < i + 1` guard added before `++k`: equivalent, because `k <= i - 1` before the extension, so the guard is always true.
- `out_pi[i] = k + (k == i + 1 ? 0 : 0)`: a no-op written as a mutant, equivalent.

## Not run

- Windows: CI-only. MSan and other compilers were not run.
- The mutant runs used the plain build only; no sanitizer-instrumented mutants.
- No timing assertion; no scaling table.
- No exhaustive run beyond the alphabets and lengths listed above.
