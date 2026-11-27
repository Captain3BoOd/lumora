# palin

`core/palin/palin.h` and `palin.cpp` (`lumora::palin`) answer questions about the palindromes of a byte string `s[0 .. n)`: the palindrome radius at every center (`palin_radii`), the first longest palindromic substring (`palin_longest`) and the number of palindromic substrings (`palin_count`), plus `palin_scratch_words` (`2 * n`). Limit: at most `262144` bytes.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `880ecb4`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers. The header "Decisions" block was treated as data.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no large automatic arrays. The source includes `palin.h` only.

## Check order and writes

- `palin_scratch_words`: null `out_words` gives NullArgument (also when `n` is too large), then `n > 262144` gives TooLarge with nothing written, else `*out_words = 2 * n`.
- `palin_radii`: NullArgument for a null `s`, `odd` or `even` only while `n > 0` (the condition uses the given `n`, also above the limit); then `n > 262144` is TooLarge. It needs no scratch. `n = 0` is Ok and writes nothing.
- `palin_longest`: NullArgument for a null `s` or `scratch` only while `n > 0`, and for a null `out_start` or `out_len` always; then TooLarge; then, only while `n > 0`, `scratch_words < 2 * n` is ScratchTooSmall. For `n = 0` no scratch is needed and the result is start 0, length 0.
- `palin_count`: the same order, with `out_count` always required. For `n = 0` the count is 0.
- On any failure no output is written. On success only the stated output range is written. Scratch contents after a call are not specified and not tested. The arrays must not overlap; no test uses overlapping arrays.
- Definitions (from the header): `odd[i] >= 1` is the number of odd palindromes centered at byte `i`; `even[i]` is the number of even palindromes centered between bytes `i - 1` and `i`, with `even[0] = 0`. Bytes are compared as unsigned values, exactly. The longest palindrome with the smallest start wins ties.

## Method

Manacher's algorithm, run once for odd lengths and once for even lengths, writing the radii straight into the output arrays (`palin_radii`) or into the two halves of the caller's scratch (`palin_longest`: `odd = scratch[0 .. n)`, `even = scratch[n .. 2n)`; the same for `palin_count`).

Each pass keeps the rightmost palindrome found so far as the half-open range `[lo, hi)`. For a center `i < hi` the radius starts at `min(mirror, hi - i)`, where `mirror` is the radius already computed at the mirrored center (odd: index `lo + hi - 1 - i`; even: index `lo + hi - i`). Otherwise it starts at 1 (odd) or 0 (even). The extension loop then compares `s[i + k]` with `s[i - k]` (odd) or `s[i - k - 1]` (even) while both indices are in range. After the center, if `i + k > hi` the range becomes the palindrome just found.

`palin_longest` scans the centers in increasing order, odd then even, and keeps the longest length; on equal length the smaller start wins (start of an odd palindrome is `i + 1 - k`, of an even one `i - k`). `palin_count` adds all radii in `u64`.

## Bounds

- Time: linear. Proof for one pass. The right end `hi` never decreases (it is replaced only when `i + k > hi`) and is at most `n`. Look at one center and its extension steps (successful comparisons). If `i < hi` and the radius starts below `hi - i`, the first comparison fails (the mirror already ends the palindrome there) and there are no steps. If it starts at `hi - i` or the center is `>= hi`, the start value of `i + k` is at least `hi` (odd, center `>= hi`: `i + 1 > hi`; even, center `>= hi`: `i >= hi`), and every step adds 1 to `i + k`. So after `s` steps `i + k >= hi + s`, the update fires, and the new `hi` is at least the old `hi` plus `s`. The steps over a whole pass therefore sum to at most the final `hi <= n`. In addition each center makes at most one failing comparison. So a pass makes at most `n` successful and `n` failing comparisons, at most `2 * n` in all, plus `n` mirror reads; the two passes make at most `4 * n` comparisons. `palin_longest` and `palin_count` add one scan of `2 * n` words.
- Memory: `palin_radii` uses only the caller's two output arrays. `palin_longest` and `palin_count` use `2 * n` caller words (`2 * 262144 = 524288` words); the method itself needs nothing else, and the code has no array on the stack.
- Count: at most `n * (n + 1) / 2`, which is `34359869440` for `n = 262144` (one repeated byte), held in `u64`. The radii and their sum are at most `n` and `2 * n`, so the 32-bit radii never wrap. No loop counter wraps in 32 bits: `i + k <= n <= 262144`, `2 * n < 2^32`.
- Observation, not a claim: a one-time local scaling run (below). A gross quadratic method (expand around every center without reusing a mirror) would be expected to need `n * n / 2` comparisons for one repeated byte, about `3.4e10` at `262144`, and so to exceed the CI time limit; that is an expectation, not an observed timeout. The suite asserts no timing.

### One-time scaling table (observation, not asserted)

`palin_count`, best of 3 runs, one thread, in seconds, g++ `-std=c++23`, `-O0` / `-O2`. Machine class: Intel Xeon Platinum 8375C at 2.90 GHz, 1 vCPU visible, Linux 7.3.4. It was run once outside the test suite; no timing test exists. Doubling `n` roughly doubles the time on every pattern.

| pattern | 32768 | 65536 | 131072 | 262144 |
| --- | --- | --- | --- | --- |
| repeated byte | 0.0006 / 0.0002 | 0.0013 / 0.0005 | 0.0026 / 0.0009 | 0.0051 / 0.0018 |
| alternating | 0.0005 / 0.0002 | 0.0010 / 0.0003 | 0.0020 / 0.0007 | 0.0041 / 0.0013 |
| Fibonacci | 0.0006 / 0.0002 | 0.0013 / 0.0004 | 0.0026 / 0.0007 | 0.0052 / 0.0015 |
| random bytes | 0.0004 / 0.0001 | 0.0008 / 0.0002 | 0.0015 / 0.0005 | 0.0031 / 0.0009 |

## Tests

`tests/palin/palin_tests.cpp`, 10 test cases, 324 assertions. Expected values come from three labelled sources. (1) DIGESTS from `tools/palin_ref.py` (independent Python: every substring against its reverse, expansion around centers, a separator-string Manacher and a palindromic tree; its self-check compares them with each other; `--check` verifies the block spliced into the test file is current). The tests rerun every string against the real functions and recompute the 64-bit digests (`n`, `odd`, `even`, longest start and length, count) and the case counts. (2) An ORACLE inside the test sharing no code with the implementation: expansion around every center for the radii (strings up to 1000 bytes), a palindromic tree for the count and the longest palindrome, and every substring against its reverse for strings up to 24 bytes. For the large strings the radii go through the digests and through `sum(odd + even)` equal to the tree's count. (3) CONSTANTS written from the header: the hand rows, hand radii, the check-order matrix and the closed forms for one repeated byte (count `34359869440`).

Every successful call runs with sentinel entries before and after each output; they must stay unchanged, and on failure nothing may change. Extents, with their labels:
- EXHAUSTIVE: every string of length 0 to 16 over `{0, 255}` (131071 strings); length 0 to 10 over `{0, 128, 255}` (88573); length 0 to 6 over `{0, 1, 127, 128, 254, 255}` (55987); the empty string, all 256 one-byte strings and all 65536 two-byte strings (65793). Bytes 0 and 255 are adjacent in the first group and in the hand rows.
- SAMPLED, fixed seeds 3002, 3003 and 3256: 600 random strings each of length 17 to 400 over 2, 3 and 256 symbols (1800 strings), checked against the oracle.
- Listed large patterns (25 strings, SAMPLED patterns): one repeated byte (`0` at 262144, 262143, 262142 and 32768, `255` at 262144), alternating `0/255` (262144, 262143, 262142), alternating `a/b`, period 3 (262144, 262143, 262142), Fibonacci (262144, 262143, 262142), a long palindrome (262144, 262143), a palindrome with the middle byte flipped, a palindrome with the last byte flipped, palindromes touching both ends, random over 2 symbols (262144, 262143), random over 256 symbols (262144, 262143, 1000).
- Hand rows (24): several longest palindromes tied at the start, in the middle and at the end, palindromes touching both ends, bytes 0 and 255 next to each other.
- Check-order matrix: null arguments, TooLarge, ScratchTooSmall (`2 * n - 1` words), `n = 0` with null `s` and `scratch`, nothing written on failure.
- Not asserted: anything the header leaves unspecified (scratch contents, which error comes first inside one class, overlapping arrays, running time).

## Mutation testing

Real source mutants of `palin.cpp` (172), each compiled with the prebuilt test object and run on the FINAL test file, one at a time, in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. Plain: 137 killed, 27 survived, 8 timed out. ASan/UBSan: 137 killed, 27 survived, 8 timed out, the same mutants.

Timeouts (30 s each) are NOT kills: odd pass skips the mirror, odd boundary update with `<`, odd `hi` never updated, even pass skips the mirror, even boundary update inverted, even `hi` never updated, and the two quadratic mutants (full expansion at every center). Each is a slow but correct method. Observation: run once more with the large-string test cases excluded, all 8 pass the remaining tests (the output is right; only the time is wrong). The suite asserts no timing, so these 8 are not caught by it; the quadratic cost is shown by the large patterns exceeding the time limit, which is not an assertion. They stay unkilled.

Crash-only kills (the run shows no failing assertion), 23 mutants in the plain run and 28 in the ASan/UBSan run, each re-run alone. Plain: all 23 end in SIGSEGV (the message varies between runs and some print only the Catch signal line; the crash is the kill). ASan/UBSan: SIGSEGV, AddressSanitizer heap-buffer-overflow (extension bounds dropped or widened, a stale range between passes, the `TooLarge` check dropped) and UBSan stores through a null pointer (dropped null checks on `odd`, `even`, `scratch`, `out_start`, `out_len`, `out_count`, `out_words`).

Survivors (27 in each build), each equivalent in output, with the proof:
- `odd/even: compare as signed char`: converting both bytes to `signed char` is a bijection on 8 bits, so two bytes are equal exactly when their converted values are.
- `longest/count: scratch check not guarded by n>0`: for `n = 0` the test `0 < 0` is false.
- `longest/count: n==0 guard dropped`: for `n = 0` the loops do not run and pointer plus 0 is valid; the outputs are the empty answer.
- `longest: odd/even swapped roles in scratch`: layout only; the two halves are disjoint either way and are read back with the same roles.
- `count: odd + even in 32 bits then widened`: each sum is at most `262144 + 262144`, no wrap.
- `longest: tie rule start < to <=`: equal length and equal start is the same palindrome; an odd and an even candidate cannot have the same length (parity).
- `longest: tie rule dropped`: candidates of the same parity and the same length arrive at increasing centers, so their starts increase; the first one already has the smallest start; odd and even cannot tie.
- `odd: k starts at 0`: the first comparison `s[i] == s[i]` is true, which gives `k = 1`.
- `odd: i < hi to i + 1 < hi`, `even: i < hi to i + 1 < hi`, `even: i < hi to i <= hi`: the cases that change give `min(mirror, room)` of 1 (odd) or 0, the same as the start value.
- `odd/even: room is hi - i - 1`, `odd/even: min with <=`: the start value is still a lower bound of the true radius (a smaller value, or the same on a tie) and the extension loop re-verifies from it, so the radius is the same; the cost is at most one extra comparison per center.
- `odd/even: boundary update >=` and `+ 1`: the new range has the same `hi` (or one more is not reached because the comparison fails) and is a real palindrome around its center, so later mirror caps stay valid; the output is the same.
- `odd/even: boundary update always (condition dropped)`: the range is then replaced even when `hi` would move back to a smaller palindrome of the current center. Any palindrome range at a center before `i` gives a valid lower bound and the extension finishes the radius, so the OUTPUT is the same. The step bound is NOT proven for this mutant (`hi` can move back), so it is output-equivalent but not step-bound-equivalent. The suite asserts no timing by maintainer decision, so it cannot show the difference.
- `even: only hi reset between passes` (`lo` not reset): `lo` is read only when `i < hi`, and then `hi` was set in this pass together with `lo`.

## Not run

- Windows: CI-only.
- No timing assertion in the suite (maintainer decision); the scaling table is a one-time observation.
- No exhaustive run beyond the alphabets and lengths listed above; longer strings are fixed-seed samples and listed patterns.
- Overlapping arrays and the contents of scratch after a call are not tested (unspecified).
- The mutant test object is not sanitizer-instrumented; only the mutated source is.
- The 8 timed-out mutants were not killed.
