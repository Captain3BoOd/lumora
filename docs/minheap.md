# minheap

`core/minheap/minheap.h` and `minheap.cpp` (`lumora::minheap`) are a binary min-heap of (u32 key, u32 value) pairs in caller arrays, ordered lexicographically (key first, then value): `heap_push`, `heap_pop_min` and `heap_peek_min`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no libm, no allocation, no recursion, no global state. The source includes `<cstdint>` only. The functions keep no state outside the caller's arrays and `*size`, so there is nothing to share between threads and no thread tests were written.

## Algorithm

The usual array layout: the children of index `i` are `2i+1` and `2i+2`. Push sifts up with a hole (each moved pair is written once, the new pair is written last). Pop saves the top pair, takes the last pair out, sifts the hole down along the smaller child (the first child wins a tie) and writes the last pair into the hole. The layout inside the heap is not specified by the header and is not tested. Equal pairs are identical, so the popped sequence is fully determined by the pairs pushed.

## Check order

Push, pop and peek: any null pointer argument (for pop and peek also `out_key` and `out_value`): NullArgument. Then `capacity > kMaxCapacity` (2^26): TooLarge. Then `*size > capacity`: BadSize. Then the operation: push at `*size == capacity`: Full; pop or peek at `*size == 0`: Empty. Capacity 0 is allowed (every push is Full, every pop and peek is Empty when `*size` is 0; `*size` above 0 is BadSize). On any failure nothing is written and no array element is accessed; `*size` and, for pop and peek, the out values are untouched. A null `*size` pointer is never dereferenced.

Write rules (the only layout facts tested): a successful push writes only at indexes below the new size; a successful pop writes only at indexes below the old size; a successful peek writes nothing. Elements at or above those bounds keep their values. The contents of the vacated slot after a pop are unspecified and are not tested. Elements at or above `*size` are never read in a way that changes a result.

| Item | Bound |
| --- | --- |
| `capacity` | 0 to `kMaxCapacity` (2^26) |
| `*size` | 0 to `capacity` |
| order | lexicographic: key, then value, both unsigned 32-bit |
| overlap of `keys`, `values`, `size` | not supported, not tested |

## Tests

`tools/minheap_ref.py` is an independent reference in Python, written from the header text with Python integers. Its model is a sorted list of tuples (insertion by bisect, pop from the front): no heap, no array layout. It prints a generated block with 64-bit digests that the tests recompute (one fold over every result, popped or peeked pair and final size of each scenario). It runs in about 2 seconds.

```
python3 tools/minheap_ref.py                                           # print the block
python3 tools/minheap_ref.py --check tests/minheap/minheap_tests.cpp
```

The tests also carry a naive oracle that shares no code with the implementation: an unsorted vector scanned for its minimum with a hand-written comparison. Every operation is compared with it (result code, popped or peeked pair, size). Arrays are heap buffers of exactly `capacity` elements, so AddressSanitizer sees any access past the end; the write-rule runs use buffers filled with canaries, padded by 3 elements, and compare every element with a snapshot taken before the call.

`tests/minheap/minheap_tests.cpp` has 12 test cases and 160 assertions (sweeps count mismatches and assert once per sweep, plus the case count, the operation count and the digest).

| Scenario | Cases | Operations | Against |
| --- | --- | --- | --- |
| every sequence of 0 to 6 operations over 6 operations (push of one of 4 pairs, pop, peek), capacity 0 to 4 | 279,935 | 1,623,630 | oracle, reference digest |
| the same sequences with canary padding (write rules) | 279,935 | same | oracle and snapshots |
| every permutation of the first n of 7 distinct pairs (n = 0 to 7): push all, pop n + 1 times | 5,914 | 86,552 | oracle, reference digest |
| every push sequence of length 0 to 7 over 3 pairs (duplicates), then pop len + 1 times | 3,280 | 45,928 | oracle, reference digest |
| pseudo-random: 200 seeds x 2,000 operations, capacity 64 (half the seeds with padding) | 200 | 400,000 | oracle, reference digest |
| deep heaps: 20 seeds x 6,000 operations, capacity 1,000, push-heavy and pop-heavy phases of 500 | 20 | 120,000 | oracle, reference digest |

The first four rows are exhaustive over their stated sets. The pseudo-random and deep scenarios use a fixed 64-bit LCG (seeds 1 to 200 and 1 to 20) and are NOT exhaustive. The 4 pairs of the first scenario are (0,0), (0,FFFFFFFF), (1,0) and (FFFFFFFF,0), so key-first order, value tie-break and the unsigned extremes are all hit. Other cases: lexicographic order table (key equal and value different, value and key at FFFFFFFF), argument checks for every null combination and for the order Null, TooLarge, BadSize, Full or Empty (including `*size` null with a huge capacity, size above capacity at the same time as a capacity above the maximum, and capacity 0), untouched out values and arrays on failures, peek leaves arrays and size unchanged, and header anchors.

`capacity == kMaxCapacity` is accepted and `kMaxCapacity + 1` is TooLarge, tested with 1-element arrays: only the elements below `*size` and the slot a push writes are touched, so push, peek and pop succeed with `*size` 0 and 1, and a push at `*size == kMaxCapacity` is Full without touching the arrays. No array near 2^26 elements is allocated.

## Mutation check

145 hand-written source mutants of `minheap.cpp` (the comparison and each of its operators, every argument check, result code and the check order, the Full and Empty tests, the sift loops and their bounds, the parent and child index formulas, the child choice and the tie rule, every move and final write, the new size, and the out values) were compiled against the unchanged tests. Plain build: 134 killed, 11 survived. With `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented): 134 killed, the same 11 survive.

Plain build: 102 kills by a failing assertion, 30 by a crash alone and 2 by a timeout at 25 seconds (mutant 39, parent index `(i+1)/2`, and mutant 88, child index `2i`; the cause was not investigated, they presumably loop). The crash-only kills were run individually: 29 report SIGSEGV (Catch2 "Segmentation violation signal"): null checks removed or weakened in push, pop and peek (including `out_key`, `out_value` and the `*size` read), the status check removed, the Full check as `>`, the push loop `i >= 0`, the Empty check removed or `== 1`, the pop new size `size - 2`, and the pop child bound removed. One, mutant 34 (the Full check removed in push, which writes one element past the end), aborted with SIGABRT and no message in the individual run, and gave SIGSEGV in a standalone re-run; it is listed as an abnormal exit.

Sanitizer build: 101 kills by a failing assertion, 2 timeouts (the same), 31 by a sanitizer report or crash, run individually: load of a null pointer (17: null checks removed in push, pop and peek, the status check removed, `*size` read without a null test), store to a null pointer (4: `out_key` and `out_value` null checks removed in pop and peek), heap-buffer-overflow (6: Full check as `>`, Empty check removed, new size `size`, child bound removed, child bound against the old size, peek key index 1), an AddressSanitizer SEGV report (3: push loop `i >= 0`, Empty check `== 1`, new size `size - 2`) and one SIGABRT with no report (mutant 34, as above). Several of these are killed by an assertion or a crash in the plain build too; they are listed as the sanitizer saw them.

Survivors (11), each argued:

- Push full check `i >= capacity` (mutant 31): `*size <= capacity` holds after the BadSize check, so `i >= capacity` and `i == capacity` are the same condition. Equivalent.
- Push compare as `less(parent, new)` break (mutant 44) and pop break as `less(last, child)` (mutant 107): the changed test differs from the original only when the two pairs are equal, and equal pairs are identical. Moving an equal pair up or down writes the same bits into the slot (a pair equal to the one that is being placed), so the array after the call is bit-identical, also across later iterations: a pair that moves on an equal compare is itself equal to the pair being placed, and every pair below it is not smaller. Equivalent.
- `less` with `va <= vb` on equal keys (mutant 3): `less(a, a)` becomes true. In the push and pop break tests this has the effect of mutants 44 and 107 (bit-identical arrays). In the choice between the two children (mutant 103, `!less(first, second)` instead of `less(second, first)`) an equal pair of children picks the second child instead of the first. Both children are identical pairs, so the heap invariant (each parent is not larger than its children) still holds and the multiset of pairs is unchanged, so the popped sequence is unchanged; but the arrays can differ from the original after such a pop (which of two identical children moves up), so the equivalence is only under the contract, which leaves the layout unspecified and untested. Mutants 3 and 103 are not killed by any test, by design.
- Pop `n >= 0` and `if (true)` (mutants 82, 83): they add the case `n == 0` (a one-element heap). Then the last pair is `keys[0]`, `values[0]` (a valid read), the loop does not run (`child = 1 >= 0`) and the same pair is written back to index 0. Bit-identical. Equivalent.
- Pop child bound `child > n` (mutant 92): the only extra case is `child == n`. Index `n` still holds the last pair (every write so far is below `n`), so the loop compares the last pair with itself, `!less(last, last)` is true, and it breaks at the same `i`. Equivalent.
- Pop second child `child + 1 <= n`, the bound removed, and `child + 1 < n + 1` (mutants 96, 97, 99): the only extra case is `child + 1 == n`, the slot of the last pair. It is chosen only when `less(last, keys[child])`, and then the original breaks too (`child` is not smaller than `last`), and the mutant breaks because the chosen pair equals `last`. Both write the last pair at `i`. Equivalent.

Mutant 3 and mutant 103 are the only two whose equivalence is under the contract only (layout), not bit-identical. No survivor is left unargued.

## Not run

Heaps near 2^26 elements were NOT run (memory); only small buffers with the maximum capacity argument. The pseudo-random and deep scenarios are not exhaustive. Windows runs only in CI. Overlapping `keys`, `values` and `size`, threads and the layout inside the heap are untested.
