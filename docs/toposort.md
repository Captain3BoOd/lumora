# toposort

`core/toposort/toposort.h` and `toposort.cpp` (`lumora::toposort`) compute the lexicographically smallest topological order of a directed graph given as an edge list, detect cycles, and use caller scratch: `toposort_scratch_words` and `toposort_u32`.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only.

Integer only: no floating point, no 128-bit types, no compiler builtins, no allocation, no recursion, no global state. The source includes `<cstdint>` only.

## Algorithm and scratch

Nodes are 0 to `nodes - 1`; `from_to` holds `edges` pairs (from, to). Duplicate edges and self-loops are allowed (a self-loop is a cycle). The order is the smallest node among the ready nodes (indegree 0) taken at each step, from a binary min-heap.

Scratch is `3 * nodes + edges + 1` words (`toposort_scratch_words`): indegree `[nodes)`, offsets `[nodes + 1)`, adjacency `[edges)`, heap `[nodes)`. The ready nodes live in the heap at the front of the heap array and placed nodes are stored from the back of the same array (a node is unseen, ready or placed, so the two never meet). The order is copied to `order` only after every node is placed.

## Check order and failure atomicity

1. NullArgument: `from_to` null with `edges > 0`, `order` or `scratch` null with `nodes > 0` (`toposort_scratch_words`: `out_words` null).
2. TooLarge: `nodes > kMaxNodes` (65,536) or `edges > kMaxEdges` (1,048,576).
3. BadEdge: any of the 2 * edges words is `>= nodes` (the whole list is checked, wherever the bad word is).
4. `nodes == 0` is Ok (nothing is written, null `order` and `scratch` are allowed).
5. ScratchTooSmall: `scratch_words < 3 * nodes + edges + 1`.
6. Cycle.

`order` is untouched on every failure, Cycle included. On success `order[0 .. nodes)` is written.

## Scratch contents (maintainer answer relayed via Instinct)

Formula-sized scratch must suffice; the implementation uses exactly that. The header promises nothing about scratch words past the need prefix when more is supplied, so the tests do not assert them untouched. Scratch contents after any call are unspecified and never asserted. Nothing is written past `scratch_words` (tests use exact-need allocations, so an overrun shows under AddressSanitizer). With need - 1 words the result is ScratchTooSmall and `order` is preserved; with need and need + 5 words the output is identical; garbage prefills do not change the result.

| Item | Bound |
| --- | --- |
| nodes | 0 to 65,536 |
| edges | 0 to 1,048,576 |
| scratch words | need = 3 * nodes + edges + 1, at most 1,310,721 |

## Tests

`tools/toposort_ref.py` is an independent Python reference (a linear scan for the smallest ready node), with a brute-force cross-check over permutations (the first valid permutation in lexicographic order): every graph on up to 4 nodes in the exhaustive scenario, and 300 random 5-node graphs. It prints the digests the tests recompute:

```
python3 tools/toposort_ref.py
python3 tools/toposort_ref.py --check tests/toposort/toposort_tests.cpp
```

The tests also carry a naive C++ oracle that shares no code with the implementation.

`tests/toposort/toposort_tests.cpp` has 10 test cases and 221 assertions (sweeps count mismatches and assert once per sweep).

| Scenario | Cases | Operations | Against |
| --- | --- | --- | --- |
| exhaustive: every edge set (self-loops included) for n = 1 to 4, each with scratch need, need - 1 and need + 5 | 66,066 | 198,198 | oracle, reference digest |
| 400 pseudo-random graphs of 0 to 40 nodes (NOT exhaustive) | 400 | 1,200 | oracle, reference digest |

The first row is exhaustive over its set. The random row is NOT exhaustive and is kept separate. Further cases: header anchors; the scratch-words function (formula, limits, null, order); null rules; check order; hand-checked orders, duplicates, shuffled edge lists and garbage prefills; real 65,536-node buffers (no edges, chain, reverse chain, all edges to node 0, a cycle, scratch short by 1); and one real 65,536-node, 1,048,576-edge test (identity order, cycle, ScratchTooSmall, a bad endpoint in the last word). Canaries after `order[nodes - 1]` are asserted on success only.

## Mutation check

146 hand-written source mutants of `toposort.cpp` (the heap compares and bounds, the null rules, the limits, the bad-edge scan, the scratch test, the layout, the offsets and cursors, the initial and later pushes, the placement index and the copy). Each replaces one unique text and is compiled against the prebuilt test object, then the whole binary runs with a timeout.

Plain: 112 kills by assertion, 26 by crash alone, 1 timeout (mutant 22, "pop child 2i", not investigated, not claimed as a kill), 7 survivors. Sanitizer (`-fsanitize=address,undefined -fno-sanitize-recover=all`): 108 by assertion, 30 by sanitizer report or crash, 1 timeout (22), 7 survivors. One test (null order or scratch with 1 node) was added at the end of the test file after the first run, to kill mutants 61 and 64.

Crash-only kills were run individually. Plain: SIGSEGV with the message "Segmentation violation signal" for mutants 14, 17, 40, 58, 61, 63, 64, 74, 79, 80, 82, 95, 99, 110, 111, 112, 115, 116, 130, 132, 141; SIGABRT (return code -6, no message printed) for 87, 88, 90, 92, 119. Sanitizer: ASan heap-buffer-overflow (17, 26, 80, 87, 88, 100, 132, 139), ASan SEGV on an unknown address (14, 74, 79, 99, 105, 110, 111, 112, 114, 115, 116, 130, 141) and UBSan null-pointer load or store (40, 57, 58, 60, 61, 63, 64, 66, 75).

Survivors (7), all argued equivalent. Heap values are distinct (a node is pushed once: at the start with indegree 0, or when its indegree reaches 0, which happens once), and `h[n]` still holds `last` during the sift.
- 3, push `<` for `<=`: `h[parent] == v` cannot happen (distinct), equivalent.
- 30, sibling compare `<=` for `<`: two equal siblings cannot exist, equivalent.
- 33, stop compare `<` for `<=`: `last == h[child]` cannot happen, equivalent.
- 24, `child > n` for `child >= n`: only `child == n` differs; then `h[child]` is the saved `last`, `last <= last` breaks and `h[i] = last` as in the original, equivalent.
- 27 and 28, sibling bound `<=` and removed: only `child + 1 == n` differs, so `h[child + 1]` is `last`. If `last < h[child]` the mutant takes child = n, then `last <= h[n]` breaks and `h[i] = last`; the original sees `last <= h[child]`, breaks, `h[i] = last`. If `last >= h[child]` the mutant does not advance, as the original. Equivalent.
- 108, offset prefix loop from 1: it skips `off[1] += off[0]`, and `off[0]` is 0 (only indices `from + 1 >= 1` are counted), equivalent.

## Not run

Windows was not run locally. The largest allocations are the header limits (65,536 nodes, 1,048,576 edges). Mutant 22 was not investigated. Scratch contents after a call are unspecified and were not asserted.
