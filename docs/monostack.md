# monostack

`core/monostack/monostack.h` and `monostack.cpp` (`lumora::monostack`) answer three questions about an array `a[0 .. n)` of `u32` values with a monotonic stack or deque: `monostack_bounds` (the nearest strictly smaller value to the left and the nearest smaller-or-equal value to the right of every position), `monostack_max_rect` (the largest rectangle in a histogram) and `monostack_window_min` (the index of the leftmost minimum of every window of `k` values). Limit: at most `1048576` values.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `de9a8f1`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers. The header "Decisions" block was treated as data.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no large automatic arrays. The source includes `monostack.h` only. All working memory is the caller's scratch.

## Check order and writes

- `monostack_scratch_words`: a null `out_words` is NullArgument; then `n > 1048576` is TooLarge (nothing written). Otherwise `*out_words = n` (0 for `n = 0`).
- `monostack_bounds`: NullArgument for a null `a`, `scratch`, `left` or `right`, only while `n > 0`; then `n > 1048576` is TooLarge (the condition uses the given `n`); then, only while `n > 0`, `scratch_words < n` is ScratchTooSmall. On success it writes `left[0 .. n)` and `right[0 .. n)`. `n = 0` writes nothing and accepts null arrays. `kNone` is `0xFFFFFFFF`.
- `monostack_max_rect`: NullArgument for a null `out_area`, `out_l` or `out_r` always, and for a null `a` or `scratch` only while `n > 0`; then TooLarge; then ScratchTooSmall (`n > 0` and `scratch_words < n`). On success it writes the three outputs. `n = 0` gives area 0, `l = 0`, `r = 0`.
- `monostack_window_min`: NullArgument for a null `a`, `scratch` or `out`, only while `n > 0`; then TooLarge; then BadWindow (`k == 0` or `k > n`, only while `n > 0`; `k = n + 1` is BadWindow; `n = 0` accepts any `k`); then ScratchTooSmall. On success it writes `out[0 .. n - k + 1)` and nothing else.
- On every failure every output stays untouched. Exactly `n` scratch words, or more, succeed. Scratch contents after a call are unspecified; the tests never assert them, and overlapping arrays are not tested.

## Method

- `bounds`: one pass with a stack of indices in `scratch`. The stack holds indices with strictly increasing values. Element `i` pops every index whose value is `>= a[i]` and sets that index's `right` to `i`; after the pops the top (the nearest earlier index with a strictly smaller value) is `left[i]`, or `kNone` if the stack is empty. Then `i` is pushed. A final drain sets `right = kNone` for every index still on the stack.
- `max_rect`: the same stack, with the loop running to `i = n` where every remaining bar is popped. A bar popped at `i` has the range `(index below it on the stack) + 1 .. i - 1` (0 if nothing is below it): the bar below is strictly smaller, and `a[i]` is smaller or equal (or `i = n`). Of several equal bars the last popped one has the widest range. The area is `(r - l + 1) * a[bar]` in `u64` (at most `2^20 * (2^32 - 1)`, no overflow). A candidate replaces the best when its area is larger, or equal with a smaller `l`, or equal with the same `l` and a smaller `r`. The start is `(0, 0, 0)`, so an all-zero array gives `(0, 0)`.
- `window_min`: the deque is `scratch[head .. tail)` (indices, values strictly increasing from the front). A new element removes from the back every index with a strictly larger value (equal values stay, so the leftmost minimum stays in front), is pushed, and the front is dropped when `i - front >= k`. Once `i + 1 >= k` the front is the answer for the window starting at `i + 1 - k`. Only one front removal per iteration is needed: after iteration `i - 1` the front index is at least `i - k`, so only an index equal to `i - k` can leave at `i`.

## Bounds

Every element is pushed once and popped at most once, so the work is linear. The counts of the ACTUAL code:

- `bounds`: `n` pushes; at most `n` pops in total (main loop plus drain); `while` condition evaluations: `n + (pops in the loop)` in the main loop and `(pops in the drain) + 1` in the drain, in total `2n + 1`. Outer iterations `n`, drain iterations at most `n`.
- `max_rect`: `n + 1` outer iterations; `n` pushes; exactly `n` pops (the pass at `i = n` empties the stack); `2n + 1` `while` condition evaluations; at most `n` area comparisons.
- `window_min`: `n` outer iterations; `n` pushes; back pops plus front removals at most `n - 1` (the deque is never empty after a push, so one index always stays); back-pop condition evaluations `n + (back pops)`; front removals at most one per iteration.

No recursion, no allocation, no loop depends on `n` beyond these counts.

### One-time scaling table (observation, not asserted)

Single run, best of 3, the three functions only (`window_min` with `k = 1000`), machine class Intel Xeon Platinum 8375C 2.90 GHz, 1 vCPU, Linux, g++. These are observations, not guarantees; the test suite asserts no timing. Times in seconds.

| input | n | bounds -O0 | bounds -O2 | max_rect -O0 | max_rect -O2 | window_min k=1000 -O0 | window_min k=1000 -O2 |
|---|---|---|---|---|---|---|---|
| increasing | 65536 | 0.0004 | 0.0001 | 0.0005 | 0.0002 | 0.0003 | 0.0003 |
| increasing | 262144 | 0.0017 | 0.0004 | 0.0020 | 0.0008 | 0.0014 | 0.0012 |
| increasing | 1048576 | 0.0070 | 0.0022 | 0.0082 | 0.0033 | 0.0055 | 0.0049 |
| decreasing | 65536 | 0.0003 | 0.0001 | 0.0004 | 0.0001 | 0.0003 | 0.0002 |
| decreasing | 262144 | 0.0014 | 0.0004 | 0.0017 | 0.0005 | 0.0014 | 0.0007 |
| decreasing | 1048576 | 0.0054 | 0.0018 | 0.0068 | 0.0022 | 0.0059 | 0.0028 |
| constant | 65536 | 0.0003 | 0.0001 | 0.0004 | 0.0001 | 0.0003 | 0.0002 |
| constant | 262144 | 0.0013 | 0.0004 | 0.0017 | 0.0005 | 0.0013 | 0.0007 |
| constant | 1048576 | 0.0054 | 0.0018 | 0.0070 | 0.0020 | 0.0052 | 0.0029 |
| random | 65536 | 0.0011 | 0.0005 | 0.0012 | 0.0007 | 0.0011 | 0.0006 |
| random | 262144 | 0.0044 | 0.0022 | 0.0048 | 0.0028 | 0.0042 | 0.0024 |
| random | 1048576 | 0.0177 | 0.0088 | 0.0193 | 0.0111 | 0.0172 | 0.0098 |
| sawtooth 1000 | 65536 | 0.0004 | 0.0001 | 0.0006 | 0.0002 | 0.0005 | 0.0002 |
| sawtooth 1000 | 262144 | 0.0017 | 0.0005 | 0.0022 | 0.0008 | 0.0019 | 0.0008 |
| sawtooth 1000 | 1048576 | 0.0072 | 0.0023 | 0.0090 | 0.0033 | 0.0077 | 0.0035 |

## Tests

`tests/monostack/monostack_tests.cpp`, 12 test cases, 6808 assertions. Expected values come from three labelled sources. (1) DIGESTS from `tools/monostack_ref.py` (independent Python: the definitions by scanning, and a stack and deque version compared with each other by a self-check; the generated block is spliced into the test, `python3 tools/monostack_ref.py --check tests/monostack/monostack_tests.cpp` verifies it, and the test recomputes each digest from the C++ results; each fold covers `n`, `left[]`, `right[]`, the area with `l` and `r`, and for a set of window sizes `k` the `out[]`). (2) An ORACLE inside the test that shares no code with `monostack.cpp`: the definitions (scanning) up to 400 values, pointer chains and a block method (prefix and suffix minima per block) above; a separate test checks the chain and block versions against the definitions on 400 random arrays of up to 150 values. (3) Closed forms at 2^20.

Window sizes per array: for `n <= 12` every `k` from 1 to `n`; for `n <= 400` the values 1, 2, 3, 5, 8, `n/4`, `n/2`, `n-2`, `n-1`, `n`; above that 1, 2, 3, 1000, `n/2`, `n-1`, `n` (each kept only when in `1 .. n`). So window sizes are exhaustive only up to `n = 12`; above that they are a fixed list.

Every successful call runs with 8 sentinel entries before and 16 after each output array. On failure all outputs must be untouched. Extents, with their labels (array extents EXHAUSTIVE; window sizes as above):
- EXHAUSTIVE: every array of length 0 to 11 over `{0, 1, 2}` (265720); length 0 to 9 over `{0, 1, 2, 3}` (349525); length 0 to 8 over `{0, 1, 0x80000000, 0xFFFFFFFF}` (87381); every permutation of `0 .. n - 1` for `n` = 0 to 9 (409114); the empty array, all 256 one-value arrays and all 65536 two-value arrays over 0..255 (65793).
- SAMPLED, fixed seeds 7003, 7016 and 7032: 600 random arrays each of length 10 to 400 over 3, 16 and 2^32 values (1800 arrays).
- Large patterns (12, SAMPLED patterns, length 1048576 or 1048575): increasing, decreasing, constant 0, constant 0xFFFFFFFF (1048576 and 1048575), alternating extremes, sawtooth 1000, organ pipe, random over 2 values (seed 9011), random over 2^32 values (9013, and 9014 with length 1048575) and a random permutation (9015).
- Closed forms at 1048576: constant `0xFFFFFFFF` gives area `2^20 * (2^32 - 1) = 4503599626321920`, `l = 0`, `r = n - 1`; constant 0 gives `(0, 0, 0)`; increasing gives area `2^38` with `l = n / 2`, `r = n - 1`; decreasing gives area `2^38` with `l = 0`, `r = n / 2 - 1`.
- Hand rows (24): ties, plateaus, zeros, extremes, equal minima. The expected values come from the Python brute force, not from the C++ code.
- Scratch rules: `monostack_scratch_words` gives `n` and 0; `n - 1` words give ScratchTooSmall with every output untouched; `n` and larger sizes succeed; `n = 0` with null arrays and any `scratch_words`.
- Check-order matrix: every null argument, null before TooLarge, TooLarge before BadWindow and ScratchTooSmall, BadWindow before ScratchTooSmall, `k = 0` and `k = n + 1`, nothing written on failure.
- Not asserted: scratch contents after a call, overlapping arrays, running time.

The reference `--check` run takes about 105 s (the self-check about 2 s, the rest regenerating the block) on the machine above; it exceeds a two-minute tool call on a slower machine, so run it where such a limit does not apply.

## Mutation testing

Real source mutants of `monostack.cpp` (145: null checks, TooLarge, ScratchTooSmall variants, and variants of the stack, rectangle and deque logic), each compiled with the prebuilt test object, one at a time, in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source.

STAGED run (the maintainer allowed it, because the full suite takes about 25 s). Stage 1 ran every test case except the three big exhaustive groups and the large-array group (the cases `exhaustive: every array of length*`, `exhaustive: every permutation*` and `large arrays*`; about 1.3 s). Stage 2 ran the FULL suite for the mutants that survived stage 1 (7 mutants). Both stages ran on the final test file, in both builds. The 138 mutants killed in stage 1 were never run against the large groups; the large groups killed nothing that is claimed here.

Result (the same in both builds): 138 killed, 6 survived the full suite, 1 timed out. No mutant was killed only by the sanitizer build.

Timeout (a timeout is NOT a kill): Mutant `rect: bar from top` (`bar = scratch[top - 1]` instead of `scratch[--top]`) never pops, so the `while` loop never ends. It timed out at 60 s in stage 1 (where the whole run otherwise takes about 1.3 s) and at 100 s in stage 2, in both builds. It is a hang, not a kill.

Crash-only kills (the run shows no failing assertion in the batch run): 21 mutants (the union of both builds), each re-run alone with the console reporter. Plain: 18 end in SIGSEGV with Catch2's "Segmentation violation signal" message and no assertion (null-argument and null-output mutants; the other 3 of the 21 are killed by an ordinary assertion in the plain build). Sanitizer build, all 21: 15 report `runtime error: load of / store to null pointer`, 3 end in SIGSEGV (no sanitizer report), 2 report `AddressSanitizer: heap-buffer-overflow`, and 1 ends in failing assertions when run alone. The alone-run output files are not kept in the repository.

Survivors, with the argument (the same 6 in both builds):
- `bounds: push before left` (the new index is pushed, then `left = scratch[top - 2]`): equivalent, `scratch[top - 2]` after the push is the old top.
- `rect: outer i <= n + 1`: equivalent, at `i = n + 1` the stack is empty after the pass at `i = n`, so the `while` does not run, and nothing is read from `a[n + 1]`.
- `rect: pop test > instead of >=`: equivalent in output. Equal bars then stay on the stack; the first popped of a run of equal bars gets a narrower range, and the bottom one of the run is popped last with the same `r` and the leftmost `l` of the real rectangle. A narrower candidate of the same height has a strictly smaller area (for height 0 the area 0 never beats the start `(0, 0, 0)` because the tie needs `l < 0` or `r < 0`), so the best triple is the same.
- `rect: tie r < to r <=`: equivalent, the same `(l, r, area)` triple would be written again.
- `rect: tie rule only l`: equivalent in output. Pops happen in nondecreasing order of `r` (the `i - 1` of the pop), so of two candidates with the same area and the same `l` the one seen first has the smaller or equal `r`, which is what the full rule picks.
- `window: front expiry as while`: equivalent, at most one index leaves per iteration (see Method), so a loop removes the same one.

## Not run

- Windows: CI-only.
- No timing assertion in the suite; the scaling table is a one-time observation.
- No exhaustive run beyond the alphabets and lengths listed above; longer arrays are fixed-seed samples and listed patterns. Window sizes above `n = 12` are a fixed list.
- Scratch contents after a call and overlapping arrays are not tested (unspecified).
- The mutant test object is not sanitizer-instrumented; only the mutated source is.
- Staged mutation run: the big exhaustive and large groups ran only against the 7 stage-1 survivors.
- The 1 timed-out mutant was not killed; there are no unproven survivors.
