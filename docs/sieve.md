# sieve

`core/sieve/sieve.h` and `sieve.cpp` (`lumora::sieve`) fill three tables over `0 .. limit` in caller storage, for a limit up to `2^20`: `spf_table` (smallest prime factor), `phi_table` (Euler's totient) and `mu_table` (Moebius, with -1 stored as 255). Every table has exactly `limit + 1` entries; `out[0] = 0` and `out[1] = 1` in all three.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `925ab78`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state, no scratch. The source includes `sieve.h` only.

## Check order and writes

- A null `out` gives NullArgument, also when the limit is above the maximum. Then `limit > 2^20` gives TooLarge. On failure nothing is written at all.
- On success exactly `out[0 .. limit]` is written and nothing else, at no time (not even temporarily). Temporary values live only inside `out[0 .. limit]` during the call. For `mu_table` every entry from 0 to `limit` is finalized to exactly 0, 1 or 255; no working flag is left behind.

## Method

A linear sieve needs a stored list of primes, which is scratch, so all three use Eratosthenes-style sieves over the caller's array.
- spf: `out[n] = n`; for each `p` with `p <= limit / p` (so `p * p <= limit` without any wrap) and `out[p] == p` (a prime: a composite was claimed by a smaller prime), every multiple `m = p*p, p*p + p, ...` with `out[m] == m` gets `p`. Because primes are taken in increasing order, the first claim is the smallest prime factor. Entries that stay `n` are primes (and 0, 1).
- phi: `out[n] = n`; for each `p` from 2 to `limit` with `out[p] == p` (an entry nothing has lowered is a prime: a composite was lowered by its smallest prime already), every multiple `m = p, 2p, ...` gets `out[m] -= out[m] / p`. At that moment `out[m]` is `m` times `(q - 1) / q` over the primes `q` already processed, that is `(m / Q) * prod(q - 1)` where `Q` is the product of those primes; `p` still divides `m / Q`, so the division is exact, and the result is the totient formula `m * prod(1 - 1/p)`.
- mu: the output bytes hold working flags (bit 1 composite mark, bit 2 odd number of primes so far, bit 4 square factor) that start at 0. For each `p` from 2 to `limit` whose composite mark is not set (prime), every multiple flips the odd flag, every multiple above `p` gets the composite mark, and (when `p <= limit / p`) every multiple of `p * p` gets the square flag. A final pass sets `out[1] = 1` (when `limit >= 1`) and turns every entry from 2 on into 0 (square flag), else 255 (odd flag), else 1.
- No loop counter reaches `2^21`: `m <= limit + p <= 2^21`, and `p * p` is formed only when `p <= limit / p`, so it is at most `2^20`. Nothing wraps in 32 bits.

## Fixed bounds

Every loop bound depends on `limit` only; no loop reads data to decide its length except the primality test, which skips (it does not change the bounds).
- Initialization: `limit + 1` writes. mu's final pass: at most `limit - 1` steps.
- Prime-multiple steps: for `phi` and the odd-flag pass of `mu`, `sum over primes p <= limit of floor(limit / p)`; for `spf`, `sum over primes p <= sqrt(limit) of (floor((limit - p*p) / p) + 1)`; for the mu square flags, `sum over primes p <= sqrt(limit) of floor(limit / (p*p))`. Each is at most `limit * (1 + ln limit)` (a sum of `floor(limit / k)` over all `k` from 2 to `limit`), so the work is O(limit log limit). The outer loops run at most `limit` steps (spf: at most `floor(sqrt(limit))`).
- EXACT counts for `limit = 2^20` (computed from the formulas above): 82025 primes up to `2^20`, 172 of them up to `1024`. spf: 2228882 prime-multiple steps (and 1023 outer steps); phi: 2996044 prime-multiple steps; mu: 2996044 prime-multiple steps plus 474010 square steps. Each plus the `1048577` initialization writes.

## Tests

`tests/sieve/sieve_tests.cpp`, 10 test cases, 1047 assertions. Expected values come from three labelled sources. (1) DIGESTS from `tools/sieve_ref.py` (trial division only, no sieve; cross-checked against gcd counting and the Moebius definition for every `n <= 3000`; `--check` verifies that the block spliced into the test file is current); the tests re-run each scenario against the real functions and recompute the 64-bit digest and the entry counts. (2) An ORACLE inside the test that shares no code with the implementation: per-entry trial division. The full table for `limit = 2^20` is compared ENTRY BY ENTRY with it for all three functions, and every other table is compared with the matching prefix (the entries of a table do not depend on the limit); the oracle is itself checked against the reference digests. (3) CONSTANTS: the header values and 155 hand rows from the reference (prime powers of the first 30 primes, `1019^2`, `1021^2`, the primorials up to 510510, products of two large primes near `2^10` and near `2^19`, the largest primes below `2^20`, values near `2^20`).

Every successful call is run with 8 sentinel entries before `out[0]` and 16 after `out[limit]`, which must stay unchanged; pointers are naturally aligned. Exhaustive ONLY for the stated limits, not all limits through `2^20` (the test names say so):
- every limit `0 .. 700`, all three functions: 701 limits, 246051 entries per function.
- the 62 listed limits, all three functions, 11485335 entries per function: `1023, 1024, 1025, 4095, 4096, 65535, 65536, 65537, 2^20 - 1, 2^20`; `p^2 - 1, p^2, p^2 + 1` for `p = 1019` and `1021` (`1031^2` is above `2^20`); `2^k - 1, 2^k, 2^k + 1` for `k = 2 .. 19` (each limit once).
- the full table for `2^20`: 1048577 entries per function, entry by entry against the oracle.
- the `p * p` guard: limits `2^20 - 1`, `2^20` and the squares of 1019 and 1021 with their predecessors, all three functions.
- null `out` with limits 0, 1, 2, `2^20 - 1`, `2^20`, `2^20 + 1`, `2^32 - 1`; oversized limits `2^20 + 1`, `2^20 + 2`, `2^31 - 1`, `2^32 - 2`, `2^32 - 1` with a sentinel buffer that must stay untouched.
- Not asserted: anything the header leaves unspecified (the array outside `limit + 1`, beyond "not written").

## Mutation testing

Real source mutants of `sieve.cpp` (114), each compiled with the prebuilt test object and run on the FINAL test file, one at a time (one core), in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. Plain: 105 killed, 9 survived. Sanitizer: 105 killed, 9 survived. No timeout occurred.

Crash-only kills (the failure is the crash, with no failing assertion), each re-run alone. Plain: the three `null check dropped` mutants (SIGSEGV, in the null-output check); the six `limit > to > +1` and `TooLarge check dropped` mutants end in a crash from the write past the 64-entry buffer (SIGABRT or SIGSEGV, which varied between runs; the process prints no message). Sanitizer: the three null mutants ("store to null pointer") and the six oversize mutants (AddressSanitizer: heap-buffer-overflow in `spf_table`, `phi_table` or `mu_table`). One-build kill, `mu: init one too far`, was reported as crash-only in the full run; run alone it fails an assertion in the test (a guard entry changed), so it is an ordinary kill, and in the plain build too. The mutated source is instrumented; the test object is not.

Survivors (9 in each build), each equivalent, with the proof:
- `spf: p*p <= limit form`: `p` is below `2^11`, so `p * p` does not wrap, and `p * p <= limit` holds exactly when `p <= limit / p` (integer division).
- `spf: p <= limit / p + 1` and `mu: square guard loose`: the one extra `p` has `p * p > limit` (and `p <= 1025`, no wrap), so the inner loop starts above `limit` and runs zero times.
- `spf: composite p not skipped`: for a composite `p` with smallest prime `q < p`, every multiple `m >= p*p` of `p` is a multiple of `q` with `m >= q*q`; `q` was processed earlier, so `out[m] != m` and nothing is written.
- `spf: multiples start at 2p`: a multiple `m = k*p` with `2 <= k < p` has a prime factor `r <= k < p` with `r * r < m`; `r` was processed earlier and claimed `m`, so the extra steps write nothing; from `k = p` it is the original loop.
- `spf: multiples start at p`: `out[p] == p` for a prime `p` when it is reached, so the write stores `p` again; the rest is as above.
- `phi: multiply form` (`out[m] = out[m] / p * (p - 1)`): `p` divides `out[m]` at that moment (see the method), so `out[m] - out[m] / p = out[m] / p * (p - 1)` exactly.
- `mu: composite mark for m == p too`: the composite mark of `p` is read only at iteration `p`, before it can be set by `p` itself, and the final pass reads only the odd and square flags.
- `mu: final starts at 1`: `out[1]` is already 1 and has no flags, so the pass writes 1 again; for `limit = 0` the loop does not run.

## Not run

- Windows: CI-only.
- No timing test.
- No exhaustive run over all limits through `2^20` (only the stated limits).
- The mutant test object is not sanitizer-instrumented; only the mutated source is.
