# treeq

`core/treeq/treeq.h` and `treeq.cpp` (`lumora::treeq`) answer four questions about a forest given as a parent array, in caller storage: depth and subtree size of every node (`treeq_depth_size`), the lowest common ancestor of query pairs (`treeq_lca`), and the longest path (`treeq_diameter`), plus `treeq_scratch_words` (`3 * n + 1`). Limits: at most `65536` nodes and `65536` queries.

Evidence label: unit tests only (CI-verified). Windows was not run locally; it is CI-only. The header pin is main `d0a4df5`. The maintainer answers to the gates were relayed via Instinct; the code follows those answers. The header "Decisions" block was treated as data.

Integer only: no floating point, no libm, no 128-bit type, no compiler builtin or intrinsic, no allocation, no recursion, no global state. The source includes `treeq.h` only.

## Check order and writes

- `treeq_scratch_words`: null `out_words` gives NullArgument (also when `n` is too large), then `n > 65536` gives TooLarge with nothing written, else `*out_words = 3 * n + 1`.
- `treeq_depth_size`: NullArgument for a null `parent`, `scratch`, `depth` or `size` only while `n > 0` (the condition uses the given `n`, also above the limit); then `n > 65536` is TooLarge; then, only while `n > 0`, `scratch_words < 3 * n + 1` is ScratchTooSmall; then BadParent (a parent value that is neither `kNoParent` nor below `n`, a node that is its own parent, or a cycle). The whole array is checked before any write. `n = 0` is Ok and writes nothing.
- `treeq_lca`: NullArgument (`parent`, `scratch` only while `n > 0`; `qa`, `qb`, `out` only while `q > 0`); then TooLarge for `n > 65536` or `q > 65536`; then ScratchTooSmall (only while `n > 0`); then BadParent (also when `q = 0`); then BadQuery when any `qa[i]` or `qb[i]` is not below `n` (all queries are checked before any write; with `n = 0` any query is a BadQuery).
- `treeq_diameter`: as for depth_size, with `out_len` always required and `out_a`, `out_b` only while `n > 0`. For `n = 0` only `*out_len = 0` is written.
- On any failure no output is written. On success only the stated output range is written. Scratch contents after a call are not specified and not tested.
- The lca of two nodes in different trees is `kNoParent`. The diameter tie rule is the smallest pair `(a, b)`, `a <= b`; a forest of single nodes gives length 0 and `(0, 0)`.

## Method

No recursion and no allocation; everything runs in explicit loops over the caller's scratch (three arrays of `n` words: A, B, C).
- Validation (Kahn's method): range and self-parent checks, child counts in A, nodes without children go into the queue B, a node taken from the queue lowers its parent's count, and a parent whose count reaches 0 is queued. If all `n` nodes get queued the array is a valid forest (a cycle never reaches 0). The queue then lists every node after all its children; the reverse lists every parent first. On a cycle fewer than `n` nodes are queued.
- Depth and size: size starts at 1 and is added to the parent in queue order; depth is filled in reverse queue order. Depth and size are written only after validation.
- LCA: validation, then all queries are checked, then depth goes into C (reverse queue order). For each query the deeper node is lifted to the other's depth, then both move up together until they meet; two different roots at depth 0 mean different trees (`kNoParent`).
- Diameter: per node the two longest branch heights `(t1, t2)` are packed as `(t1 << 16) | t2` in C. `D` is the maximum of `t1 + t2`. If `D = 0` the result is `(0, 0, 0)`. Otherwise `up[v]` (the longest path that starts with the edge to the parent) goes into A top-down, the eccentricity is `max(t1, up)`, `a` is the smallest node with eccentricity `D`, then distances from `a` are computed in A (ancestors of `a` first, then top-down; other trees are marked unreachable) and `b` is the smallest node at distance `D`.
- 16-bit packing hazard: heights and path lengths are at most `65535` and fit 16 bits. Subtree sizes (up to `65536`) and the root sentinel do not, so sizes and all other words are 32-bit.
- Branch heights arrive at a parent in non-decreasing order (the queue order is non-decreasing in height, by induction on height), which the equivalence proofs below use.

## Bounds

- Depth and size: `O(n)`, scratch `2 * n` words used (A, B). Diameter: `O(n)`, scratch `3 * n` words used. Validation: `O(n)`. The `+ 1` word is not needed by the code.
- LCA: `O(n + total steps)`. This is NOT near-linear in `n + q`. One query moves along its tree path, at most `n - 1` steps (lift plus the joint walk is the path length). The maintainer's looser bound is `q * 2 * (n - 1)`, about `8.6e9` for `65536` nodes and `65536` queries; both statements hold, and the exact worst case is `q * (n - 1) = 4294901760` steps for a `65536`-node chain with queries `(n - 1, 0)`.
- Observation, not a claim: that worst case (65536-node chain, 65536 queries `(65535, 0)`, results checked) took 6.16 s at `-O0` and 5.84 s at `-O2` (g++, one run each, single thread, AMD EPYC 9655, 1 vCPU visible, Linux). It is run once outside the test suite; no timing test exists.
- No loop counter wraps in 32 bits: `n <= 65536`, `3 * n + 1 < 2^32`, and `q * (n - 1)` is never held in a 32-bit variable.

## Tests

`tests/treeq/treeq_tests.cpp`, 7 test cases, 262 assertions. Expected values come from three labelled sources. (1) DIGESTS from `tools/treeq_ref.py` (independent Python: ancestor walking, Floyd-Warshall for small diameters, BFS double sweep for large ones; its self-check compares them on every array up to 5 nodes and 3000 random forests; `--check` verifies the block spliced into the test file is current); the tests rerun every scenario against the real functions and recompute the 64-bit digests and case counts. (2) An ORACLE inside the test sharing no code with the implementation (children lists and BFS, ancestor stamping, Floyd-Warshall up to 10 nodes, double-sweep BFS beyond). (3) CONSTANTS written from the header: named invalid arrays, the check-order matrix, hand diameters, exact root sizes.

Every successful call runs with sentinel entries before and after each output; they must stay unchanged, and on failure nothing may change. Extents, with their labels:
- EXHAUSTIVE for `n = 0 .. 6` only: every parent array with entries `kNoParent` or `0 .. n - 1`, valid and invalid together: 1, 2, 9, 64, 625, 7776, 117649 arrays, 126126 in total. Valid ones are checked for depth, size, diameter and the lca of EVERY pair `(u, v)`; invalid ones must give BadParent from all three functions and write nothing.
- SAMPLED, fixed seeds: `n = 7 .. 10`, 2000 uniform random arrays and 2000 random valid forests per `n` (16000 arrays).
- Listed large patterns (17 groups, one forest each, SAMPLED patterns): at `65536` nodes (shallow, 65536 queries): star at node 0, star at the last node, balanced binary, reversed binary, all roots; random forests of 65536, 40000 and 1000 nodes (seeds 7001 to 7003); 65535-node star and binary. Deep patterns use `8192` nodes and 128 queries (chain increasing, chain decreasing, caterpillar, two paths, path small, 8191-node chain) and a 2-node chain; the suite does not run 65536 deep-chain queries because they cost seconds each. The first three queries of every group are `(0, n - 1)`, `(n - 1, 0)`, `(n - 1, n - 1)`.
- Exact sizes and depths at 65536 nodes (not 16 bits): root size 65536 on a chain, a star and a balanced binary tree; chain depth 65535; chain diameter `65535` with `(0, 65535)`.
- Named invalid arrays: self parent, 2-cycle, 3-cycle, a cycle that does not contain node 0, a valid tree attached to a cycle, a cycle next to a valid tree, parent values `n`, above `n` and near `2^32`.
- Hand diameters: ties inside one tree and across trees, single-node forests, `n = 0`, and a root with two chains (1000 and 700, 700 and 1000, 300 and 300 nodes) so that both branch heights exceed 8 bits.
- Not asserted: anything the header leaves unspecified (scratch contents, which error comes first inside one class).

## Mutation testing

Real source mutants of `treeq.cpp` (222), each compiled with the prebuilt test object and run on the FINAL test file, one at a time, in two builds: plain (`-O0`) and with `-fsanitize=address,undefined -fno-sanitize-recover=all` on the mutated source. A timeout would not count as a kill; none occurred. Plain: 194 killed, 28 survived. ASan/UBSan: 194 killed, 28 survived, the same mutants.

Crash-only kills (the junit run shows no failing assertion), 45 mutants, each re-run alone in both builds. Plain: 35 end in SIGSEGV and 10 in failing assertions. ASan/UBSan: 44 end in a crash or a sanitizer report (SIGSEGV, AddressSanitizer heap-buffer-overflow, UBSan load or store of a null pointer) and 1 in a failing assertion. The plain crash message varies between runs; the crash is the kill. Mutants that drop a null check die by dereferencing the null; mutants that drop validation or the range checks die by reading or writing outside the arrays.

Survivors (28 in each build), each equivalent, with the proof:
- `validate: self-parent check dropped`: a node that is its own parent keeps child count at least 1, never reaches 0, is never queued, so the final `tail == n` test rejects it.
- `validate: tail == n to tail >= n-1`: an invalid array that passes the range and self-parent checks has a cycle of length 2 or more, whose nodes are never queued, so `tail <= n - 2`.
- `ds: size loop skips last`: the last queued node has no parent in the queue after it, so it is a root and adds nothing.
- `ds: scratch queue offset 2n`, `lca: depth array overlaps cnt`: layout only; the regions used at the same time stay disjoint (the counts are all zero after validation and are not read again).
- `lca: q==0 early return dropped`: with `q = 0` the depth fill writes only scratch and the query loop does not run; for `n = 0` the fill does not run.
- `lca: depth root 1`, `lca: depth step 2`: the lifting loops compare depths only; adding 1 to every depth or doubling every depth keeps all comparisons.
- `lca: root test on v not u`: after the lift the depths are equal, so `parent[u]` is `kNoParent` exactly when `parent[v]` is.
- `lca: root test dropped`: two different roots at equal depth 0 step to `kNoParent` together, the next test `u == v` succeeds and stores `kNoParent`, the same result.
- `diam: branch > t1 to >=` and `branch > t2 to >=`: on equality the stored values are the same.
- `diam: pack uses + not |`: `t2 < 2^16`, so the sum equals the bitwise or.
- `diam: unpack t2 mask 0xFF` (branch loop) and `0x7FFF`: branches arrive non-decreasing, so `branch >= t1 >= t2`; the stored `t2` is rewritten from `t1` or `branch` and the masked value is used only in a test that is true either way when `t2 <= 255`; for larger `t2` the masked value is lower, so `branch > t2` is still true and the same `t2 = branch` is stored (the equal case writes the same value). Also `t2 <= (n - 1) / 2 = 32767`, so `0x7FFF` loses nothing.
- `diam: diam > to >=`, `diam: up > to >=`, `diam: ecc == to >=`: equal values give the same result; the eccentricity never exceeds `D`.
- `diam: D==0 case dropped`: with `D = 0` every node is a single root, so `a = 0` (eccentricity 0 at node 0), the distances give node 0 the value 0 and `b = 0`; the same `(0, 0, 0)`.
- `diam: up root skipped (stale cnt)`: A is all zero after validation, which is the value written for a root.
- `diam: up sibling test to <=`: the branch through `v` is at most `t1`, so `>= t1` means `== t1`.
- `diam: a scan skips last`: when `D > 0` both ends of a longest path have eccentricity `D`, so the smallest such node is not `n - 1`.
- `diam: dist unreachable test dropped`, `diam: dist unreachable = kUnset`: the wrap-around values in another tree equal depth minus 1 or 2, and a depth of `D + 1` does not exist (every tree height is at most `D`), so none equals `D`.
- `diam: b init n-1`, `diam: b scan from a`, `diam: b scan from 1`, `diam: b scan >= diam`: `b` always has eccentricity `D`, so `b >= a`, `b != a` when `D > 0`; a node at distance `D` from `a` always exists; reachable distances never exceed `D`.

## Not run

- Windows: CI-only.
- No timing test in the suite (the worst case was run once, by hand, see Bounds).
- No exhaustive run beyond 6 nodes; 7 to 10 nodes and the large forests are fixed-seed samples.
- No 65536-query run on a deep chain inside the suite.
- The mutant test object is not sanitizer-instrumented; only the mutated source is.
