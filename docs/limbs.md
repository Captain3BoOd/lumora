# limbs

`core/limbs/limbs.h` and `limbs.cpp` (`lumora::limbs`) are arbitrary-length unsigned integers as little-endian arrays of 32-bit limbs: `limbs_cmp`, `limbs_add`, `limbs_sub`, `limbs_mul` and `limbs_divmod_u32`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no libm, no 128-bit types, no compiler builtins, no allocation, no recursion, no global state. The source includes `<cstdint>` only and uses 64-bit intermediates.

## Number and normalization notes

A number is `x[0 .. n)`, limb 0 is the least significant. Inputs may carry zero limbs at the high end and `n` may be 0. Outputs are normalized: the value 0 has length 0 and writes nothing, and the output length is a function of the value only. Inputs are normalized first (`norm_len`), so leading zero limbs never raise the needed capacity. Output and input overlap is forbidden and not tested. Two input arrays may be the same array (a and b aliased); that is tested for cmp, add, sub and mul.

Multiplication is a column (comba) product. A column sum can reach 2^80, so it is kept in two `uint64_t` words. Division by a 32-bit value is limb-by-limb long division with a 64-bit remainder step.

## Check order and failure atomicity

Every function: a null pointer gives NullArgument (an input array counts only while its length is above 0, an output array only while its capacity is above 0, every out pointer always, including `q_len` and `rem`); then an input length above `kMaxLimbs` (2^16) gives TooLarge; then Underflow (sub) or DivideByZero (divmod); then BufferTooSmall when the capacity is below the normalized result length. Null beats TooLarge. A capacity is never TooLarge. `dst == nullptr` with `dst_cap == 0` is fine only when the result is zero; `dst_cap > 0` with a null `dst` is NullArgument even for a zero result.

Failure atomicity is strict (maintainer answers to the gates, relayed via Instinct): on any failure nothing is written, neither `dst` nor an out pointer (`*out_len`, `*q_len`, `*rem`). No scratch is written to `dst` before every failure condition is ruled out, including the normalized multiplication length. There is no allocation, so each operation does read-only passes first and writes only after success: add finds the carry limb, sub finds the borrow chain (after the Underflow compare), mul finds the normalized length by a read-only column pass, divmod reads the top limb. Multiplication writes straight into `dst` in one pass only when `dst_cap >= la + lb` (no failure is possible then).

Success tail: the maintainer answer is that slots from `out_len` up to the capacity are UNSPECIFIED (may be preserved or written). The tests do not assert either. They assert only the normalized prefix `dst[0 .. out_len)`, that no slot at index `dst_cap` or above is written (canaries right after the capacity), that a zero result writes nothing, and that a failure writes nothing at all.

Divmod: the quotient length is `la`, or `la - 1` when the top limb is below `d`. A zero quotient needs no storage and `*rem` is still set. `*q_len` and `*rem` are set only on success.

## Bounds

| Item | Bound |
| --- | --- |
| each input length | 0 to `kMaxLimbs` (65536 limbs) |
| capacity | any `uint32_t` |
| required capacity | the normalized result length (0 for a zero result) |
| divisor | any `uint32_t`, 0 gives DivideByZero |
| output and input overlap | forbidden, not tested |
| a and b the same array | allowed, tested |

## Tests

`tools/limbs_ref.py` is an independent reference in Python on Python integers (add, subtract, multiply, divmod, compare, then conversion to normalized limbs). It prints a block of constants and digests that the tests recompute:

```
python3 tools/limbs_ref.py                                           # print the block
python3 tools/limbs_ref.py --check tests/limbs/limbs_tests.cpp
```

The tests also carry a naive oracle that shares no code or representation with the implementation: numbers as 16-bit digits (base 65536), schoolbook add, subtract and multiply with 32-bit intermediates, and long division with a 64-bit remainder, converted back to limbs.

`tests/limbs/limbs_tests.cpp` has 13 test cases and 315 assertions (sweeps count mismatches and assert once per sweep, plus the case count, the operation count and the digest). Inputs are heap buffers of exactly the stated length, so a read past the end shows up under AddressSanitizer. The limb alphabet is {0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF}.

| Scenario | Cases | Operations | Against |
| --- | --- | --- | --- |
| cmp, add, sub, mul: every pair of arrays of length 0 to 3 over the alphabet (156 x 156), every capacity in {0, need-1, need, need+1, 7} (sub underflow: {0, 1, 7}); run again with 3 canaries after `dst` | 24,336 | 354,564 (and as many padded) | oracle, reference digest |
| divmod: every array of length 0 to 3 x 8 divisors (including 0, 1, 0x80000000 and 0xFFFFFFFF) x the same capacities, each also with canary padding | 1,248 | 5,645 (and as many padded) | oracle, reference digest |
| pseudo-random: 400 seeds, arrays of 0 to 64 limbs in four modes, cmp, add, sub, mul with the exact capacity and one less, divmod by four divisors with the exact capacity and one less | 400 | 5,731 | oracle, reference digest |
| special arrays 2^(32k) - 1 (k 1 to 24) and 2^(32k) (k 0 to 24): carry and borrow chains, every pair of the 49 | 2,401 | 9,604 | oracle, reference digest |

The first, second and fourth rows are exhaustive over their stated sets. The pseudo-random scenario uses a fixed 64-bit LCG (seeds 1 to 400) and is NOT exhaustive.

Further cases: header anchors (`kMaxLimbs`, the six result codes); null rules per function (including a null array with length 1); check order (Null before TooLarge, TooLarge before Underflow, DivideByZero and BufferTooSmall, Underflow before BufferTooSmall, DivideByZero before BufferTooSmall and after Null and TooLarge); TooLarge at `kMaxLimbs + 1` using 1-element buffers (nothing is read); aliasing of a and b; leading zero limbs and zero results needing capacity 0; and calls at exactly 65536 limbs: cmp variants, add (all-ones + 1 needs 65537 limbs, capacity 65536 gives BufferTooSmall with `dst` untouched, capacity 65537 succeeds), sub (all-ones - 1, 1 - all-ones is Underflow, capacity one short, all-ones - all-ones), mul of all-ones by a 1-limb value in both orders (exact capacity and one short), mul by a zero-valued 65536-limb number (length 0), and divmod of all-ones by 1, 3, 0x80000000 and 0xFFFFFFFF (exact capacity and, for 1, one short) and by 0.

## Mutation check

296 hand-written source mutants of `limbs.cpp` (normalization and limb access, the compare direction and length rule, add carry, bounds and carry limb, sub borrow, the Underflow test and its order, length tracking and write limit, every column bound, accumulator carry, shift and length rule in the multiplication, its one-pass branch and read-only pass, every null rule, the TooLarge, BufferTooSmall and DivideByZero tests and their order, the divmod length formula, remainder and quotient write guard, and the out-pointer writes). Each mutant replaces one unique text of `limbs.cpp` and is compiled against the prebuilt test object, then the whole test binary is run with a 25 second timeout.

Plain build: 248 kills by a failing assertion, 33 by a crash alone, 15 survivors, no timeouts. Run individually, the 33 crash-only kills gave: SIGSEGV (25 mutants: 27, 30, 31, 32, 35, 36, 37, 42, 43, 51, 52, 57, 65, 66, 71, 75, 79, 80, 165, 166, 167, 206, 209, 250 and 262; null or oversized-length reads), SIGABRT with no message printed (109, 161, 176, 190, 238 and 286: the mutants write past the capacity; the abort source was not investigated further), and SIGFPE (252 and 294: division by zero).

Sanitizer build (`-fsanitize=address,undefined -fno-sanitize-recover=all`): 217 kills by a failing assertion, 50 by a sanitizer report or crash, 29 timeouts at 25 seconds. The 29 timeouts are not investigated and no kill is claimed from them: 15 are the survivors below and 14 are killed in the plain build (mutants 28, 30, 33, 43, 51, 52, 54, 65, 67, 78, 80, 81, 249 and 250). The 50 crash-only kills were run individually: ASan heap-buffer-overflow (26 mutants: 3, 4, 5, 94, 95, 105, 109, 110, 111, 112, 163, 186, 187, 190, 200 to 203, 205, 207 to 210, 236, 281 and 286), UBSan load or store of a null pointer (27, 42, 47, 57, 71, 75 and 247), ASan SEGV on an unknown address (31, 32, 35, 36, 37, 56, 66, 70, 79, 165, 166, 167, 206, 245 and 262), and UBSan division by zero (252 and 294).

A mutant first survived the plain run for lack of a test: `limbs_cmp` with a null array and length 1 (mutant 27, the null test `na > 1u`). The test was added at the end of the file and mutant 27 was re-run alone: killed by SIGSEGV (plain) and by a UBSan null load (sanitizer). The other mutants were run with the final test object (sanitizer) or with the object before that one test was added (plain); a test added later cannot turn a kill into a survivor.

Survivors (15), each argued. All are equivalent:

- Compare digit `<=` instead of `<` (mutant 14): the test runs only when `a[i] != b[i]`, so `<=` and `<` agree.
- Add carry `(s >> 32) + (s >> 33)` (mutant 90): `s` is at most `2 * (2^32 - 1) + 1 = 2^33 - 1`, so `s >> 33` is 0.
- Add second pass with a and b swapped (mutant 119): the limb sum with carry is symmetric in a and b, so the written limbs and the length are the same.
- Sub write without the null check on `dst` (mutant 148): the first pass has limit 0, so `i < limit` is false and nothing is written; the second pass has `dst` non-null whenever `len > 0`, because `dst_cap >= len > 0` and a null `dst` with a positive capacity is NullArgument.
- Mul one-pass threshold `la + lb + 1` (mutant 171), `>` (173), never (175): the result is the same by either path (one pass or a read-only pass followed by a write pass), and the failure cases are the same because the one-pass branch is taken only when no failure is possible. Only the path differs.
- Mul one-pass write count the capacity (179) or unlimited (180): in the one-pass branch `dst_cap >= la + lb`, and the loop only runs `k < la + lb`, so `k < write_count` is true for every `k` in all three forms.
- Mul columns `la + lb + 1` (mutant 198): the extra column has an empty inner loop (`i_first = la + 1 > i_last = la - 1`) and a zero sum (the product is below `2^(32 (la + lb))`), so its limb is 0, it does not change the length, and it is never written (`write_count` is at most `la + lb`).
- Mul shift `+` instead of `|` (mutant 227): `lo >> 32` is below 2^32 and `hi << 32` has its low 32 bits clear, so `+` and `|` agree.
- Mul `hi = 0` (229) and `hi >>= 31` (230) instead of `hi >>= 32`: a column has at most 65536 products below 2^64, so the sum is below 2^80 plus a carry below 2^48; `hi` is below 2^17, and `hi >> 31` and `hi >> 32` are 0.
- Mul write without the null check on `dst` (mutant 239): the first pass has `write_count` 0, the one-pass branch has `dst_cap >= la + lb >= 2` (so non-null), and the second pass has `len > 0` and `dst_cap >= len` (so non-null).
- Divmod shift with `+` (mutant 273): `r < d < 2^32`, so `r << 32` has its low 32 bits clear and `+` and `|` agree.

## Not run

General multiplication of two numbers of `kMaxLimbs` (65536) limbs each was NOT run: it is about 4.3e9 limb products. A multiplication of two such numbers that goes through the read-only length pass (a capacity below `la + lb`) was NOT run for the same reason, and neither was any other failure prepass that costs that much at this size. Multiplication at 65536 limbs is tested only with a 1-limb factor or a zero-valued factor. Add, sub, cmp and divmod at 65536 limbs are cheap and were run. Windows was not run locally. The success tail was not asserted (unspecified by the maintainer).
