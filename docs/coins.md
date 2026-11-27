# coins

`core/coins/coins.h` and `coins.cpp` (`lumora::coins`) fill three tables over a list of `m <= 64` coin values (each any `u32` except 0) for the amounts `0 .. n` with `n <= 1048576` (2^20): `coins_min_count` (the fewest coins that sum to each amount, unlimited use of every kind, `kUnreachable` if impossible), `coins_ways` (the number of multisets over the coin KINDS, modulo 998244353; equal values are separate kinds) and `coins_subset_ways` (the number of subsets of the list POSITIONS, modulo 998244353).

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `004dd00`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers. The header "Decisions" block was treated as data.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no scratch. The output table is the only working memory. The source includes `coins.h` only.

## Check order and writes

All three functions: a null `out` (always) or a null `coins` (only while `m > 0`) is NullArgument; then `m > 64` is TooManyCoins; then `n > 1048576` is TooLarge; then any coin equal to 0 is BadCoin. Every one of the `m` coins is scanned, whatever `n` is, also when `n = 0` or the coins are above `n`. The checks use the given `m` and `n`, so `m = 0xFFFFFFFF` (never read past the first check) and `n = 0xFFFFFFFF` are plain failures. On every failure nothing is written (`out` stays untouched). On success exactly `out[0 .. n]` is written (`n + 1` entries) and nothing else.

- `m = 0` is legal and `coins` may be null: `out[0] = 0` / `1` / `1` for the three tables, and `out[v] = kUnreachable` / `0` / `0` for `v > 0`. `n = 0` writes only `out[0]`.
- A coin above `n` is legal and never used. Equal values are separate kinds: `{1, 1}` gives `ways = 1, 2, 3, 4, ...` (`n + 1` entries 1 .. n+1) and `subset_ways = 1, 2, 1, 0, ...`. Order of the coins does not change any table.
- `out[0]` is 0 (min) and 1 (ways, subsets) always. Overlapping `coins` and `out` are not tested (the header leaves them open).

## Method

One pass per coin over `out`, in place, in coin order:
- `min_count`: ascending `v = c .. n`: `out[v] = min(out[v], out[v - c] + 1)` when `out[v - c]` is reachable. Ascending order lets the same kind be used again (unlimited coins).
- `ways`: ascending `v = c .. n`: `out[v] += out[v - c]` modulo 998244353. The entry `out[v - c]` already includes this kind, so any number of uses of the kind is counted once per multiset.
- `subset_ways`: DESCENDING `v = n .. c`: `out[v] += out[v - c]`; the entry `out[v - c]` is still the value before this coin, so each position is used at most once.

Bounds of the arithmetic (no products anywhere, no widening needed):
- A modular sum adds two residues, each below `kMod = 998244353`. The sum is below `2 * kMod = 1996488706 < 2^32 = 4294967296`, so it fits a `u32`, and one subtraction of `kMod` (when the sum is at least `kMod`) brings it into `0 .. kMod - 1`.
- A stored fewest count is at most `n <= 2^20` because every coin is at least 1 (using only coins of value 1 would give exactly `v`). The `+ 1` is only done on a reachable entry (never on `kUnreachable`), so it is at most `2^20 + 1` and never reaches `kUnreachable = 0xFFFFFFFF`.
- Coin values compare as `u32` (`v >= c`, loop start `c`), no sum of a coin value and an amount is formed, so coins up to `0xFFFFFFFF` cannot overflow.

## Bounds

The ACTUAL code per call: the check does at most `m` coin tests; the initialisation writes `n` entries (the entries `1 .. n`; `out[0]` is one more write). For every coin `c <= n` the pass has exactly `n - c + 1` inner iterations, each with one read of `out[v - c]`, one comparison or one addition, and at most one write; a coin `c > n` costs nothing. In total at most `sum over the coins of (n - c + 1) <= m * n` inner iterations (for `m` coins of value 1 exactly `m * n`), so at most `64 * 2^20 = 67108864` inner iterations, plus `n + 1` initial writes and at most `m` coin tests. No recursion, no allocation, no loop depends on anything else.

### One-time scaling table (observation, not asserted)

Single run, best of 3, machine class Intel Xeon Platinum 8375C 2.90 GHz, 1 vCPU, Linux, g++ (these are observations, not guarantees; the test suite asserts no timing). Times in seconds per call; `n` is the largest amount.

| coins | n | min_count -O0 | min_count -O2 | ways -O0 | ways -O2 | subset_ways -O0 | subset_ways -O2 |
|---|---|---|---|---|---|---|---|
| 64 x value 1 | 65536 | 0.0095 | 0.0030 | 0.0380 | 0.0109 | 0.0160 | 0.0034 |
| 64 x value 1 | 262144 | 0.0383 | 0.0125 | 0.1530 | 0.0440 | 0.0642 | 0.0135 |
| 64 x value 1 | 1048576 | 0.1539 | 0.0502 | 0.6148 | 0.1758 | 0.2607 | 0.0550 |
| 64 coins 1..64 | 65536 | 0.0125 | 0.0030 | 0.0424 | 0.0038 | 0.0161 | 0.0034 |
| 64 coins 1..64 | 262144 | 0.0501 | 0.0120 | 0.1701 | 0.0152 | 0.0644 | 0.0135 |
| 64 coins 1..64 | 1048576 | 0.2013 | 0.0482 | 0.6857 | 0.0609 | 0.2595 | 0.0547 |
| 64 coins 1..3000 random | 65536 | 0.0110 | 0.0043 | 0.0293 | 0.0035 | 0.0176 | 0.0033 |
| 64 coins 1..3000 random | 262144 | 0.0410 | 0.0134 | 0.1480 | 0.0144 | 0.0662 | 0.0135 |
| 64 coins 1..3000 random | 1048576 | 0.1622 | 0.0504 | 0.6453 | 0.0582 | 0.2642 | 0.0549 |
| 8 coins | 65536 | 0.0017 | 0.0007 | 0.0021 | 0.0004 | 0.0021 | 0.0004 |
| 8 coins | 262144 | 0.0063 | 0.0018 | 0.0086 | 0.0018 | 0.0086 | 0.0017 |
| 8 coins | 1048576 | 0.0247 | 0.0064 | 0.0355 | 0.0074 | 0.0346 | 0.0069 |

## Tests

`tests/coins/coins_tests.cpp`, 11 test cases, 1987 assertions. Expected values come from three labelled sources. (1) DIGESTS from `tools/coins_ref.py` (independent Python: the definitions by breadth-first search, by enumeration of multiplicity vectors and by enumeration of bit masks, with exact integers; and a second formulation by residue classes of the coin value with prefix sums and prefix minima, compared with the definitions by the script; the generated block is spliced into the test, `python3 tools/coins_ref.py --check tests/coins/coins_tests.cpp` verifies it, and the test recomputes each digest from the C++ results; each case folds `m`, the coins, `n` and the three tables). (2) An ORACLE inside the test that shares no code with `coins.cpp`: amount by amount minimum over the coins, and two-dimensional tables over (coin kinds used so far, amount) for multisets and subsets, in `u64` with a reduction after every sum. (3) Closed forms and constants.

Every call runs with 8 sentinel entries before and 16 after the `n + 1` output entries; on failure the whole buffer must be unchanged. Extents, with their labels:
- EXHAUSTIVE: every ordered coin list of length 0 to 5 over `{1, 2, 3, 4}` (1365 lists), of length 6 over `{1, 2, 3}` (729) and of length 7 over `{1, 2}` (128), each for every `n = 0 .. 30` (42315, 22599 and 3968 digest cases); for each list the tables at every `n` are checked against the prefix of the table at `n = 30`, and the table at `n = 30` against the oracle.
- EXHAUSTIVE: every ordered list of one and of two coins over `1 .. 64` (4160 lists) at `n = 130` (digest and oracle) and at every `n = 0 .. 130` (the table must be a prefix of the `n = 130` table; the digest covers only `n = 130`).
- SAMPLED, fixed seeds 8001, 8002 and 8003: 200 random lists each of 3 to 64 coins with `n` from 0 to 5000, coin values `1 .. 6`, `1 .. 200` and the full `u32` range (a draw of 0 becomes 1), checked against the oracle and the digests.
- Large cases at `n = 2^20` (SAMPLED lists): 64 coins of value 1 (closed forms: fewest `= v`, ways `= C(v + 63, 63)` by modular inverses, subsets `= C(64, v)`), the 20 powers of two `2^0 .. 2^19` (fewest `= popcount(v)` below `2^20` and 2 at `2^20`; every sum below `2^20` is one subset, `2^20` none; digest), a 64-coin list with values `1 .. 3000`, a coin above `n`, a coin equal to `n`, a duplicate and a repeated value (digest), and the lists `{1, 1}`, `{}`, `{0xFFFFFFFF}`, `{2^20}`, `{2^20 + 1}` (closed forms and digest).
- Counts above the modulus before reduction: 64 coins of value 1 (`C(64, 32) = 1832624140942590534`, compared with an exact `u64` Pascal triangle), and the coins `1 .. 64` at `n = 4000` (the oracle reduces many sums; the exact unreduced tables, computed in Python, have 3886 ways entries and 1647 subset entries at or above 998244353, constants in the generated block).
- Sums exactly at the modulus, found by a search over random lists: a list (`n = 65536`) where one addition in the ways table gives exactly 998244353, one where it gives exactly 998244352, and a list of 64 coins at `n = 2^20` whose final ways entry at the amount 33777 is 998244352 (checked directly and by digest).
- Hand rows (20): the header example, no coins, duplicates, greedy-failing sets, `n = 0`, large values; expected values come from the Python definitions.
- Failures: `n = 2^20 + 1` and `n = 0xFFFFFFFF`, `m = 65` and `m = 0xFFFFFFFF`, a zero coin at every position for `m = 1`, 2 and 64 (with `n` = 0, 1, 10), a zero among coins above `n`, a zero at the end with `n = 2^20`, null `out`, null `coins` with `m = 1`, 3, 65 and `0xFFFFFFFF`, every pair in the check order (null before TooManyCoins before TooLarge before BadCoin) and nothing written on failure; a zero beyond `m` is not scanned; `m = 0` with null `coins`; `n = 0`.
- Not asserted: scratch (none), overlapping arrays, running time.

A reachable positive amount whose count is 0 modulo 998244353 was NOT found, so there is no such test case. Search evidence: lists of 40 to 64 coins with random values, tables over 65536 amounts (4000 lists, ways and subset tables: about 5.2 * 10^8 entries) and 64-coin lists at `n = 2^20` (620 lists, ways table: about 6.5 * 10^8 entries); a count of 0 modulo the prime would be expected about once per 10^9 random entries. The search found no reachable entry equal to 0 and one equal to 998244352 (the case above). The search program is not part of the repository.

The reference `--check` run takes about 107 s on the machine above (`python3 tools/coins_ref.py --check tests/coins/coins_tests.cpp NAME...` checks only the named groups).

## Mutation testing

Real source mutants of `coins.cpp` (106: checks and their order, the modular sum, and the loop bounds, directions and conditions of the three passes), each compiled with the prebuilt test object and run on the FINAL test file (the whole suite, no staging), one at a time, in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. Plain: 102 killed, 4 survived, 0 timed out. Sanitizer build: 102 killed, 4 survived (the same 4), 0 timed out. No mutant was killed only by the sanitizer build, and none timed out.

Crash-only kills (the batch run shows no failing assertion): 10 mutants (the union of both builds), each re-run alone. Plain: `check: out null dropped`, `check: coins null dropped`, `check: coins null m > 1`, `check: out null and coins null swapped to &&`, `check: TooLarge limit + 1` and `check: TooLarge dropped` end in SIGSEGV or SIGABRT (Catch2's "Segmentation violation signal" message for most; `TooLarge limit + 1` ended with SIGSEGV and no assertion text), `min: v starts at 1 (reads before start)` ends in SIGSEGV, and `check: TooMany limit + 1`, `check: TooMany dropped` and `check: order BadCoin before TooMany` end in failing assertions. Sanitizer build: the 4 null mutants report `runtime error: load of / store to null pointer`, 5 report `AddressSanitizer: heap-buffer-overflow` (`TooMany` limit and dropped, `TooLarge` limit and dropped, `order BadCoin before TooMany`), and `min: v starts at 1` ends in SIGSEGV.

Survivors, with the argument (the same 4 in both builds):
- `min: compare <=` (`prev + 1 <= out[v]` instead of `<`): equivalent, the extra case assigns the value that is already there.
- `add: modulo operator instead` (`s % kMod`): equivalent, `s < 2 * kMod`, so `s % kMod` equals the single subtraction.
- `add: > instead of >=` and `add: threshold kMod + 1`: NOT equivalent, and NOT killed. They differ only when a sum is exactly `kMod`: the entry then keeps the value 998244353 instead of 0. A later addition with a nonzero entry gives a sum above `kMod`, which both versions reduce to the same residue; a later addition with a zero entry keeps the value 998244353 (equivalent to 0 in every later sum). So the difference is visible in the final table only if a final entry that is reachable and positive-amount equals 0 modulo the prime. The test has lists where a sum equals exactly `kMod` (`hit_p`), and the mutants survive them for this reason. The case that would kill them is a reachable amount with a count of 0 modulo 998244353; none was found (see the search above), so this is an honest limitation, not a proof. The sibling mutant `add: threshold kMod - 1` is killed by the final-entry case (998244352).

## Not run

- Windows: CI-only.
- No timing assertion in the suite; the scaling table is a one-time observation.
- No exhaustive run beyond the lists and amounts above; longer lists and larger amounts are fixed-seed samples and listed cases. The tables for `p64` are digest-checked at `n = 130` only (other `n` by the prefix property and the oracle).
- Overlapping arrays are not tested (unspecified).
- The mutant test object is not sanitizer-instrumented; only the mutated source is.
- Two survivors (`add: > instead of >=`, `add: threshold kMod + 1`) are not killed and have no equivalence proof; no reachable amount with a count of 0 modulo the prime was found.
