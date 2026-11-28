# collatz: Collatz trajectories over u64

Code: `core/collatz/collatz.cpp` (contract in `core/collatz/collatz.h`). Tests: `tests/collatz/collatz_tests.cpp`. Reference: `tools/collatz_ref.py`. Evidence label: unit tests only.

## Rule

- The map is x -> x / 2 for even x and x -> 3 x + 1 for odd x, run from n until x is 1. `collatz_steps` gives the number of steps (0 for n = 1), `collatz_peak` the largest value met (n itself included).
- Check order for both functions: a null `out` is NullArgument; then n = 0 is BadArgument; then Overflow when the walk meets an odd x with 3 x + 1 above 2^64 - 1. Nothing is written to `*out` on a failure. Both functions use one shared walk, so their results and Overflow conditions are the same.
- Overflow is decided at the first such odd x, even if the trajectory would come back down later.
- Pure integer. No heap, arrays, tables, recursion, mutable statics, 128-bit types, builtins or intrinsics, and no iteration cap, in the implementation.

## Method

- `walk` keeps x, a step counter s and the peak p (start n). While x is not 1: an even x is halved; an odd x is first compared with the constant kMaxX = 0x5555555555555554 = 6148914691236517204 and gives Overflow when x > kMaxX, otherwise x = 3 x + 1 and p is raised when x is above it. Each loop pass counts one step. `*steps` and `*peak` of the walk are only used on Ok, and the public functions write `*out` only after Ok.
- Overflow test. For an odd x, 3 x + 1 <= 2^64 - 1 holds exactly when x <= (2^64 - 2) / 3 = 6148914691236517204.67, that is x <= 6148914691236517203 (0x5555555555555553, the largest odd x whose update fits). The next odd number, T = 0x5555555555555555 = 6148914691236517205, gives 3 T + 1 = 2^64. The comparison x > kMaxX with the even constant kMaxX = T - 1 therefore means x >= T for an odd x. It is an exact comparison made before any multiplication: no wrapped value is ever formed, and nothing is compared after the fact.
- No intermediate leaves u64: x starts at n at most 2^64 - 1, halving only lowers it, and 3 x + 1 is formed only when it is at most 2^64 - 2.

## Termination and the external verification claims

The loop has no counter and this change does not prove that it ends: that every start reaches 1 is the open Collatz conjecture. What can be said is this. The call ends with Overflow as soon as the trajectory passes an odd x above kMaxX, and it ends with Ok when x reaches 1. The only way to run forever inside u64 would be a trajectory that never reaches 1 and never overflows, so a cycle other than the one through 1. Published computations (external claims, not checked here, read on 2026-11-28) report that every start up to a bound far above 2^64 reaches 1:

- Barina, "Convergence verification of the Collatz problem", J. Supercomput. (2020 online, doi 10.1007/s11227-020-03368-x), https://www.fit.vut.cz/research/result-file/c168171/280316/Barina2020_Article_ConvergenceVerificationOfTheCo.pdf. The text says: "From September 2019 to May 2020, the project managed to verify this conjecture for all numbers below 2^68" (the PDF prints the exponent inline), where the conjecture is that the iteration always converges to the cycle through 1.
- Barina, "Improved verification limit for the convergence of the Collatz conjecture", J. Supercomput. 81, 810 (published 2 May 2025), https://link.springer.com/article/10.1007/s11227-025-07337-0. Its abstract says the new result "pushes the limit for which the conjecture is verified up to 2^71", and its related-work section defines convergence as all tested numbers eventually reaching the value 1.
- Project page, https://pcbarina.fit.vut.cz/. It says that the convergence of all numbers below 2075 x 2^60 (about 2^71.02) has been verified, and its log has the entry "2025-01-15 the convergence of all numbers below 2^71 is verified". This page is a live progress claim and can change.

If those claims hold, every u64 start reaches 1 on the unbounded trajectory, so the library loop ends for every input. The tests report the largest step count they saw (see Tests); that is a measurement, not a bound.

## Proofs

**The first-update boundary and what it means for the result.** T - 2 = 6148914691236517203 is the largest odd x whose first update fits (3 (T - 2) + 1 = 2^64 - 6). The trajectory then halves to 2^63 - 3, which is odd and above kMaxX, so the second odd update overflows: the whole call is Overflow. T itself overflows at the first update. The doubles 2 (T - 2) = 12297829382473034406 and 2 T = 12297829382473034410 reach those values after one halving and are Overflow too. All four are tested as Overflow with nothing written. The largest odd start below T that is Ok is T - 68 = 6148914691236517137 (559 steps, peak 18446744073709551412, which is 2^64 - 204).

**Which update overflows for the odd starts T - 66 to T** (6148914691236517139 to 6148914691236517205), all Overflow. The reference command `overflow_table` prints, for each of them, the odd update (counted from 1) that overflows, the steps taken before it and the odd x it happens at:

```
start n | T - n | overflowing odd update (1 = the first) | steps taken before it | odd x there
6148914691236517139 | 66 | 2 | 2 | 9223372036854775709
6148914691236517141 | 64 | 13 | 31 | 6232805962420322303
6148914691236517143 | 62 | 2 | 2 | 9223372036854775715
6148914691236517145 | 60 | 3 | 5 | 6917529027641081789
6148914691236517147 | 58 | 2 | 2 | 9223372036854775721
6148914691236517149 | 56 | 8 | 18 | 6566248256706183113
6148914691236517151 | 54 | 2 | 2 | 9223372036854775727
6148914691236517153 | 52 | 15 | 36 | 7011906707722862591
6148914691236517155 | 50 | 2 | 2 | 9223372036854775733
6148914691236517157 | 48 | 37 | 93 | 6404037772671962489
6148914691236517159 | 46 | 2 | 2 | 9223372036854775739
6148914691236517161 | 44 | 3 | 5 | 6917529027641081807
6148914691236517163 | 42 | 2 | 2 | 9223372036854775745
6148914691236517165 | 40 | 13 | 31 | 6232805962420322303
6148914691236517167 | 38 | 2 | 2 | 9223372036854775751
6148914691236517169 | 36 | 32 | 80 | 6746640616477458425
6148914691236517171 | 34 | 2 | 2 | 9223372036854775757
6148914691236517173 | 32 | 12 | 28 | 8310407949893763071
6148914691236517175 | 30 | 2 | 2 | 9223372036854775763
6148914691236517177 | 28 | 3 | 5 | 6917529027641081825
6148914691236517179 | 26 | 2 | 2 | 9223372036854775769
6148914691236517181 | 24 | 25 | 62 | 6317841784428822521
6148914691236517183 | 22 | 2 | 2 | 9223372036854775775
6148914691236517185 | 20 | 12 | 28 | 8310407949893763071
6148914691236517187 | 18 | 2 | 2 | 9223372036854775781
6148914691236517189 | 16 | 10 | 23 | 7387029288794456063
6148914691236517191 | 14 | 2 | 2 | 9223372036854775787
6148914691236517193 | 12 | 3 | 5 | 6917529027641081843
6148914691236517195 | 10 | 2 | 2 | 9223372036854775793
6148914691236517197 | 8 | 8 | 18 | 6566248256706183167
6148914691236517199 | 6 | 2 | 2 | 9223372036854775799
6148914691236517201 | 4 | 7 | 15 | 8754997675608244223
6148914691236517203 | 2 | 2 | 2 | 9223372036854775805
6148914691236517205 | 0 | 1 | 0 | 6148914691236517205
```

So no odd start between T - 66 and T overflows later than the 37th odd update, and T - 2 (second update) and T (first update) are the edge cases above. Because every odd value in T - 66 .. T overflows whenever it is met (as a start or part-way along a trajectory, the walk from it is the same), any threshold constant between 0x5555555555555511 and 0x5555555555555554 gives the same results for every input; the mutants in that range are listed as survivors below.

**Largest values.** The largest value ever stored by the walk is a peak of an Ok call, never above 2^64 - 1; the largest peak seen in the tests is 18446744073709551412.

## Tests

Expected values come from the header's examples, from FNV-1a 64-bit digests made by `tools/collatz_ref.py` (Python standard library only, no code shared with the library), and from two test-only oracles of a different method than the library. Oracle 1 is the shortcut map T(x) = (3 x + 1) / 2 for odd x (two steps), with its own step count, the peak taken as 2 times the shortcut value, and the overflow limit made by a division, (2^64 - 2) / 3. Oracle 2 is the inverse tree: from 1 upwards through the predecessors 2 m and (m - 1) / 3 (when m = 4 mod 6), which assigns steps and peak to every value whose whole trajectory stays at or below 2^24 without iterating the map forwards; the test checks both directions for every n up to 2^24 (a value is a tree node exactly when the library's peak is at most 2^24, and then the depth and peak agree). The node count is 6648354 at bound 2^24 and 1661341 at bound 2^22 (the number of nodes whose library peak is at most 2^22); both are asserted against constants made by the reference's table method.

**The reference** runs every trajectory with unbounded Python integers and declares Overflow when an odd x has 3 x + 1 above 2^64 - 1 (decided on the big-integer value, not with the library's constant). For the two exhaustive ranges it uses the table method (follow the trajectory until it drops below n, then read the stored result for the smaller value), which is a different method again; `check` cross-checks it against the plain trajectory for every n up to 2^16 and 20000 seeded values up to 2^22, against its own inverse tree for every value up to 2^20 (415921 nodes), and for the node count up to 2^22 (1661341), and checks the boundary facts above and the header examples. Digests fold n, the result code (0 Ok, 3 Overflow) and, for Ok, steps and peak.

Sets and counts (the constants are in the test file; Ok and Overflow counts are from the reference):

| set | values | Ok | Overflow | largest step count among Ok |
| --- | --- | --- | --- | --- |
| every n from 1 to 2^22 (digest checked at n = 2^22) | 4194304 | 4194304 | 0 | 596 (first at 3732423) |
| every n from 1 to 2^24 | 16777216 | 16777216 | 0 | 704 (first at 15733191) |
| edge window T - 4096 .. T + 4096 | 8193 | 2441 | 5752 | 856 |
| doubled window 2 m for m in the same window | 8193 | 2441 | 5752 | 857 |
| shifted values a 2^k and neighbours (a in 1, 3, 5, 7, 9, 15, 17, 27, 31, 63) plus 2^64 - 1, 2^64 - 2 | 1837 | 1587 | 250 | 759 |
| seeded values, 3000 for each bit length 1 to 64 (stratified set, an extra) | 192000 | 185930 | 6070 | 939 |
| 200000 seeded uniform u64 (splitmix at positions 2000000 + i) | 200000 | 79334 | 120666 | 1085 |
| 200000 seeded values below 2^40 (splitmix at positions 3000000 + i, shifted right by 24, 0 replaced by 1) | 200000 | 200000 | 0 | 892 |
| first-overflow-step sample (see below) | 2069 | 0 | 2069 | none (all Overflow) |

The largest step count seen in any test is 1862 (the header example 942488749153153); the instrumented measurement below confirms 1862 loop passes as the maximum for one walk in the whole suite. This is a measurement, not a bound.

**The first-overflow-step sample.** Scan every integer from T - 2^18 to T + 2^18 upwards; for each the index j of the odd update at which the call overflows (1 = the first odd update; made with the shortcut map in the test and with unbounded integers in the reference) is found, and the first 64 starts for each j are kept. The result is 2069 starts with j from 1 to 102 over 42 distinct values of j: j = 1, 2, 3, 5, 7, 8, 10, 12, 13, 15, 17, 19, 20, 22, 24, 25, 27, 29, 31, 32, 34, 36, 37, 39, 41, 43, 49 and 54 have 64 starts each, and j = 44 has 53, 46 has 30, 48 has 57, 51 has 37, 53 has 27, 56 has 2, 58 has 7, 60 has 1, 61 has 18, 65 has 2, 66 has 15, 68 has 1, 75 has 4 and 102 has 23. The test checks the per-j counts against the reference, then the digest, with every value an Overflow checked against the shortcut oracle.

Local runtimes (gcc 11.4.0, `-O1`, ASan + UBSan, one core; base 8023657; 11 test cases and 26 assertions; whole binary about 9.4 s; every case far under 30 s). Cases are declared cheap first:

- `collatz header examples`, 0.000 s: every example in `collatz.h` (22 Ok values with steps and peak, the 5 listed Overflow inputs) and 2^k for k = 0..63 (k steps, peak 2^k).
- `collatz check order and untouched cells`, 0.000 s: both functions with a null `out`, n = 0, Overflow inputs (T - 2, T, 2^64 - 1) and Ok inputs: null first, then n = 0, then Overflow; a guard cell is untouched on every failure and written on Ok.
- `collatz overflow boundary`, 0.001 s: the four boundary inputs 6148914691236517203, 6148914691236517205, 12297829382473034406 and 12297829382473034410 as Overflow with nothing written; the constants and the identity 3 T + 1 = 2^64; the Ok edge T - 68 (559 steps, peak 18446744073709551412); every odd start from T - 66 to T + 4095 as Overflow; every start from T - 4096 to T - 67 against oracle 1.
- `collatz digest set: edge window` 0.002 s, `doubled window` 0.002 s, `shifted values` 0.001 s, `seeded bit-length classes` 0.214 s, `200000 seeded uniform u64` 0.200 s, `200000 seeded values below 2^40` 0.289 s, `varied first-overflow step` 0.055 s: digest, Ok and Overflow counts and largest step count against the reference, each value also against oracle 1 and the cheap invariants (peak >= n, steps >= floor(log2 n)), with steps and peak returning the same code.
- `collatz every n up to 2^24 against both oracles and the digests`, 8.607 s: the inverse tree is built for bound 2^24, then for every n up to 2^24 the library is checked against oracle 1 and the inverse tree (both directions, steps and peak, node counts), with the digest and largest step count at n = 2^22 and at n = 2^24 from the reference (the ruling required the exhaustive range at 2^22 and 2^24 if it fits; it fits).

The result checks are collected in bools, so a loop over millions of values is one assertion. There are no timing assertions.

The reference commands, all run: `header` (under 1 s), `check` (about 12 s), `emit` (about 30 s) and `overflow_table`.

Measurement of the library (a local build outside the repository: the walk with a trip counter and every 3 x + 1 formed with an overflow-checked multiply and add that aborts; the whole test binary under ASan + UBSan): no abort, so no intermediate above 2^64 - 1 ever formed; the largest x seen in the loop is 2^64 - 1 (the input), and at most 1862 loop passes in one call.

## Mutation testing

252 real source mutants of `collatz.cpp` (threshold constants and comparison, parity test, halving, odd update, peak update and initial value, step counting, loop condition, counter and value types, check order and codes, the writes on failure, cost mutants), each run on the whole final suite (test md5 and library md5 below). The box has one core, so the first pass ran one mutant at a time, with a 14 s limit per run (the whole suite takes about 9.4 s). Runs are not concurrent. md5 of `tests/collatz/collatz_tests.cpp` 83ca3ff0bd6e744c84d51b333c68ab63, of `core/collatz/collatz.cpp` 6835c0b82e76fd8371ca70e60280f1ac, of `tools/collatz_ref.py` 81a30a8f0927843c7c123c58d5c4ca49. This is the second full run: the first run (on the first version of the test, which lacked the uniform, below-2^40 and first-overflow sets and ran the inverse tree to 2^22 only) gave the same split, 163 killed, 3 abnormal ends, 41 timeouts and the same 45 survivors; it was redone in full on the final test as the revision requires.

- 163 killed by a failed assertion, 3 by an abnormal end (a null write, a signed overflow report).
- 41 hit the 14 s limit. A timeout is not a kill. Each was rerun alone with a trip counter added to the walk loop (abort at 10^7 passes of one walk): 35 stopped at the counter (step-limited, non-termination not proven; the wrapped or mis-stepped trajectory did not reach 1 in 10^7 steps) and 6 did not: those are cost mutants.
- 45 survived; each also survived alone (the same suite, run again on its own). All have an argument below.
- Cost mutants (their own class, exact mutation and observed alone runtime of the whole suite): `cm_walk_rep_s` and `cm_walk_rep_p` (the walk repeated 12 times in `collatz_steps`, respectively `collatz_peak`) passed in 20.5 s and 20.4 s; `cm_halve_slow` (40 extra dummy additions per halving) passed in 42.1 s; `cm_halve_bits` (halving by a 64-pass bit loop) passed in 98.5 s (just under the 100 s limit I gave it); `cm_halve_sub` (halving by subtracting 2 until x is at most 1) did not finish in 30 s, and for n = 2^63 the first halving needs 2^62 subtractions, so the suite cannot finish (an arithmetic argument, not a measured hang); `cm_x_copy` (3 million dummy assignments per walk) did not finish in 50 s (3 million per walk over about 40 million walks is far beyond any limit). Two further mutants written as cost mutants, `cm_double_walk` (12 passes of `s += 0 * x`) and `cm_mul_loop` (3 x + 1 as 1 + x + x + x), cost nothing measurable (the first is removed by the optimizer, the second is the same arithmetic) and are survivors below.

Survivors and why each is equivalent (or what it needs to be killed):

- Threshold constant 0x5555555555555553 and `x > kMaxX - 1`: kMaxX is even, so the two comparisons differ only at the even value 0x5555555555555554, which is never odd. `x >= kMaxX` and `x + 1 > kMaxX` behave the same way.
- Thresholds 0x5555555555555552, ...51, ...50, ...40, ...20, ...12, ...11 and the comparisons `x > kMaxX - 2`, `x + 2 > kMaxX`: they flag odd values in T - 66 .. T - 2 at the first update instead of letting the walk reach the overflowing update later. The table above shows each of those odd values (as a start, and so also part-way along any trajectory) ends in Overflow, and the test checks every odd start from T - 66 to T + 4095 as Overflow, so the result of every call is the same. The threshold 0x5555555555555510 and anything that flags T - 68 are killed.
- `(x & 1) <= 0` is the same as `== 0` for an unsigned value; `&& x != 1` in the parity test is never false inside the loop (x is not 1 there).
- `x -= x >> 1`, `x = (x + 1) >> 1` and `x = x >> 1 | (x & 1)` for the halving: x is even there, so each gives x / 2 exactly (x + 1 does not wrap for an even x).
- `x + 2 x + 1` and `(3 x + 1) / 2 * 2` for the odd update: the same value (3 x + 1 is even and at most 2^64 - 2 when formed). `3 x + 1 - (x >> 63)` and `3 x + 1 + (x >> 63)`: x is below 2^63 whenever the update is made (x <= kMaxX), so the shift is 0.
- Peak update `x >= p`, `x > p + 1`, `x > p && x != n`, `x > p && x > 2`, `x > p && (x & 1) == 0`: `>=` stores the same value; after an update x is even and at least 4, p is either n (before the first odd step) or an earlier even update value, so x = p + 1 cannot happen (x even and p even, or p = n odd and x = 3 n + 1 differs from n + 1), x = n cannot be above p, and an even x is the only kind stored. `pk_even` and `pk_after` raise p at an even step or after the step: the value is never above p there (a halved value is below the previous x, which is at most p; an odd-step value was already folded in). `u64 p = x` at the start is the same as `u64 p = n` (x was just set to n). `(void) s` and `(void) n` are no-ops.
- `while (x > 1)`, `while (x >= 2)`, `while (x < 1 || x > 1)`: x is never 0 (it starts at n >= 1, halving an even x gives at least 1, and an odd update is at least 4), so these equal `x != 1`.
- `p` held as a signed 64-bit value: the comparison and the final store convert back with the same bits on gcc (two's complement), so every result is the same; this is an implementation-defined conversion before C++20, which is why it is listed.
- Writes into the walk's locals on Overflow (`*steps = s; *peak = p` and zeros): the public functions return before copying anything on a non-Ok result, so nothing observable changes.
- Capacity survivors, not claimed equivalent: counting steps in 32 or 16 bits, or stopping the loop at 5000 or 2000 passes. They differ from the library only for a walk of more than 65535, 2^32, 5000 or 2000 steps, and the tests never reach more than 1862 steps (the maintainer accepted these as a disclosed capacity limit, not equivalent). They would be killed by an input with more steps than that; this suite has none and does not claim that none exists (the header says "at most a few thousand steps"). The 10^7-pass trip counter used above is only a probe in a local copy.

The mutation script is a local, resumable program outside the repository (the mutation text is as in this list).

## Not run

Other builds than the local gcc 11.4 ASan + UBSan, MSVC, Clang, Windows, Android, any timing assertion, and a mutant run on shuffled case order. The exhaustive ranges end at 2^24; above it the checks are the digest sets (seeded, shifted, window and first-overflow-step samples) and the shortcut oracle on those values only; the inverse tree is checked to 2^24. The external verification claims above were read, not reproduced.
