# globmatch

`core/globmatch/globmatch.h` and `globmatch.cpp` (`lumora::globmatch`) match a byte string against a wildcard pattern: `glob_match(pattern, pattern_len, text, text_len, out_match)`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no 128-bit types, no compiler builtins, no allocation, no recursion, no global state, no scratch. The source includes `<cstdint>` only.

## Pattern language

Everything is raw bytes, compared as `unsigned char` (no locale, no case folding; NUL, 0x7F, 0x80 and 0xFF are ordinary bytes).

- `?` matches exactly one byte, any value (NUL and bytes 0x80 to 0xFF included).
- `*` matches any run of bytes, including the empty run. `**` is the same as `*`.
- `\` makes the NEXT pattern byte literal, whatever it is: `\*` is the byte `*`, `\?` the byte `?`, `\\` one backslash.
- Any other byte matches itself.

The whole text must match (no partial match, no search). Wildcards in the TEXT are ordinary bytes: the text `*` is matched by the pattern `*`, `?` or `\*`, but the pattern `a` does not match the text `?`. The empty pattern matches only the empty text; `*` matches everything.

## Check order

1. NullArgument: `pattern` null while `pattern_len > 0`, `text` null while `text_len > 0`, or `out_match` null (always). A null pointer with length 0 is valid (an empty pattern or text).
2. TooLarge: `pattern_len > kMaxLen` or `text_len > kMaxLen` (kMaxLen is 65,536). Neither buffer is read.
3. BadPattern: the WHOLE pattern is checked, before any matching, for a trailing lone backslash, that is an odd run of backslashes at the very end. The text does not matter: a pattern that would fail at its first byte, or against an empty or null text, is still BadPattern. The first byte of a trailing run is never itself escaped (the byte before the run is not a backslash), so the parity of the run decides; `\\` at the end (even run) is a literal backslash and is valid.
4. Match, then `*out_match` is written.

`*out_match` is untouched on every failure. On Ok it is always written.

## Time and memory bound

The bound is O(pattern_len * text_len) time and O(1) extra memory (a few `uint32_t` locals). The matcher reads the pattern as tokens (`*`, `?`, a literal byte, or a backslash pair that is one literal byte) and keeps ONE saved star: the pattern position just after the latest `*` and the text position that star has consumed up to. On a mismatch it gives the star one more text byte and restarts at the token after the star. Why one star is enough: if the pattern is split by its stars into pieces, a match of the remaining pieces that starts at an earlier star can always be moved to start at the latest star (the latest star can absorb everything the earlier piece boundary absorbed, and the pieces before it have already matched). So backtracking to an earlier star never finds a match that the latest star does not find, and the search needs no stack.

Cost: `star_text` never decreases (a new star is saved at the current text position, which is at least the old one; a backtrack increases it by one), and it is at most `text_len`, so there are at most `text_len` backtracks. Between two backtracks the matcher advances over at most `pattern_len` tokens. That is at most `pattern_len * text_len` token steps plus `text_len` backtracks. The pattern position is always at a token boundary (a star position is the byte after a `*`, and an escape pair is consumed whole), so an escape pair is never split.

Measured, not only argued: the main-loop iterations for the shape `*` + a^(n/2) + `b` against a^n are 4,198,401 at n = 4096 and 16,785,409 at n = 8192 (about n^2/4, so the quadratic bound is reached); the shapes `*aa`... + `b`, `*a`... + `b` and a^(n/2) `*` a^(n/2-2) `b` against a^n take about 2n iterations with this matcher (the saved-latest-star design makes them linear).

| Item | Bound |
| --- | --- |
| `pattern_len`, `text_len` | 0 to `kMaxLen` (65,536) each; above it TooLarge |
| time | O(pattern_len * text_len) |
| extra memory | O(1) |

## Tests

`tools/globmatch_ref.py` is an independent reference in Python: the pattern is parsed into tokens (a trailing lone backslash is BadPattern), matched by a dynamic programme, and every result is cross-checked, each time the script runs, against the pattern translated to a regular expression over bytes (`fullmatch`, DOTALL). It prints a block of digests that the tests recompute:

```
python3 tools/globmatch_ref.py                                              # print the block
python3 tools/globmatch_ref.py --check tests/globmatch/globmatch_tests.cpp
```

The tests also carry a naive oracle that shares no code with the implementation: the pattern is decoded into tokens and matched with a row-by-row dynamic programme over the text (a table; no saved star, no backtracking).

`tests/globmatch/globmatch_tests.cpp` has 11 test cases and 72 assertions (sweeps count mismatches and assert once per sweep, plus the case count, the operation count and the digest). Buffers are heap buffers of exactly the stated length, so a read past the end shows up under AddressSanitizer; `*out_match` is preset to the value a wrong result would keep.

| Scenario | Pairs | Against |
| --- | --- | --- |
| exhaustive: every pattern and every text of length 0 to 4 over `a b ? * \` (781 x 781) | 609,961 | oracle, reference digest |
| byte sweep: every pattern and every text of length 0 to 3 over 0x00 0x7F 0x80 0xFF `? * \` (400 x 400) | 160,000 | oracle, reference digest |
| 400 pseudo-random seeds (NOT exhaustive): a pattern of 0 to 30 symbols and a text that is random, built from the pattern, or built from the pattern with one byte changed (fixed 64-bit LCG) | 400 | oracle, reference digest |

The first two rows are exhaustive over their stated sets. The random row is NOT exhaustive and is kept separate. Further cases: 79 hand-checked pattern and text pairs (escapes, stars, text wildcards, trailing backslashes) checked against both the implementation and the oracle; 25 hand-checked byte cases (NUL, 0x7F, 0x80, 0xFF, `?` matching them, escaped NUL and 0xFF); null rules and check order (Null beats TooLarge, TooLarge beats BadPattern, a null empty pattern or text is valid, a null `out_match` is always NullArgument, nothing is written on failure); BadPattern independent of the text; 65,536-byte exact buffers; `kMaxLen + 1` on either side with a 1-byte buffer (nothing is read) gives TooLarge.

Exact-limit buffers of 65,536 bytes are run only for shapes whose cost is linear: `*` x 65536, `?` x 65536, equal literals (and one that differs at the first or the last byte), `*a` repeated against a^65536, `*b` against a^65536 and a^65535 `b`, a^65535 `*`, `*` a^65535, trailing backslash runs of one, two and three, and 65,536 backslashes against 32,768 and 32,767 backslashes. Only correctness is asserted, never time.

The adversarial shapes run on real buffers of 4096 bytes (also against the oracle) and 8192 bytes (against hand-written expectations): `*` a^(n/2) `b`, `*aa` repeated then `b`, `*a` repeated then `b`, a^(n/2) `*` a^(n/2-2) `b`, `a*` repeated, and `*?` repeated, each against a^n and, where it matters, a^(n-1) `b`.

## Mutation check

90 hand-written source mutants of `globmatch.cpp` (every null rule, the TooLarge tests and their order against the null rules, the trailing-backslash scan and its parity test, the loop bounds, the star detection, the saved star position and text position, `?`, the escape pair width and the escaped compare, the signed or unsigned byte compares, the match step, the backtrack, the tail star skip and the final compare, and the written result). Each mutant replaces one unique text of `globmatch.cpp` and is compiled against the prebuilt test object, then the whole test binary is run with a 25 second timeout.

Plain build: 66 kills by a failing assertion, 12 by a crash alone, 5 timeouts, 7 survivors. Sanitizer build (`-fsanitize=address,undefined -fno-sanitize-recover=all`): 56 kills by a failing assertion, 24 by a sanitizer report or crash, 3 timeouts, 7 survivors.

The timeouts are NOT claimed as kills and were not investigated: plain mutants 29 (`text loop <=`), 43 (the star's `pi` not advanced), 71 (no `++star_text`), 73 (`ti` not reset on a backtrack) and 77 (backtrack to pattern position 0); sanitizer 43, 71 and 73 (29 and 77 are killed there, by AddressSanitizer or an assertion).

The crash-only kills were run individually. Plain: SIGSEGV ("Segmentation violation signal", no other message) for all 12 (mutants 1, 3, 6, 8, 9, 10, 12, 13, 17, 28, 31, 80). Sanitizer: AddressSanitizer heap-buffer-overflow (19, 21, 22, 23, 24, 25, 29, 31, 36, 42, 48, 62, 80), AddressSanitizer SEGV on an unknown address (12, 13, 17, 28), UBSan load of a null pointer (1, 3, 6, 8, 10, 32) and UBSan store to a null pointer (9).

Survivors (7), all argued equivalent. Facts used: a saved star position is at least 1 and at most `pattern_len`, `pi` is always at most `pattern_len` (a validated pattern never ends inside an escape pair, so `pi + 2 <= pattern_len`), and `kNone` is only ever compared to the saved star position.
- 37, `star_next = pi - 1` (the index of the star itself), and 76, backtrack with `pi = star_next - 1`: the matcher resumes AT the star, reads `*`, sets `star_next` to the same value and `star_text = ti` (which is the `star_text` it just set), and continues. That is the original state one iteration later. Equivalent.
- 65, the "no saved star" test removed, and 67, tested against 0 instead of `kNone`: without a saved star the original returns false. The mutant takes a backtrack with `pi == kNone`, which is above `pattern_len`, so no token is read, every later iteration backtracks again (`star_text` grows by one each time), the loop ends when `ti` reaches `text_len`, and the tail gives `pi == pattern_len` false. The result is false in both. (A star position is never 0, so the test against 0 never fires: the same case as 65.) Equivalent, with extra linear work only.
- 85, `pi >= pattern_len` for `pi == pattern_len`: `pi` never exceeds `pattern_len`, so the two are the same. Equivalent.
- 87 and 88, `kNone` changed to 0xFFFFFFFE and to 0: the value only has to differ from every real star position (1 to 65,536); both do. Equivalent.

## Not run

Windows was not run locally. The full maximum-length quadratic worst case (pattern and text both 65,536 bytes, about 1e9 main-loop iterations for the shape `*` a^32768 `b` against a^65536, 65536^2/4) was NOT run: it is impractical under sanitizers and for 90 mutants. The quadratic shapes ran at 4096 and 8192 bytes; at 65,536 only linear-cost shapes ran, so the bound is argued and measured at 8192, not run at the maximum. Mutants 29 and 77 (plain) and 43, 71 and 73 (both builds) timed out and were not investigated.
