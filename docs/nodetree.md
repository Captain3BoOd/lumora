# core/nodetree: node tree validator

Evidence label: software (CPU) only, CI-verified on Linux and Windows once merged. Integer only, no heap, no recursion, no mutable statics, no floating point.

## What it does

`build` takes a node graph as child lists in compressed rows (`child_offsets`, `children`) and produces, for `node_count` nodes: the parent of each node, its depth, a preorder of all nodes and the subtree size of each node. Roots are the nodes that are nobody's child, taken in ascending index order; the children of a node keep their listed order. The input is untrusted: every index, offset and count is checked before use.

## Limits and bounds

- `node_count` is 1 to 65536 (2^16). `child_offsets[node_count]` (the edge count) is at most 65536; a forest has fewer than `node_count` edges, so larger counts are rejected as `BadOffsets` before any child is read.
- Depth is at most 1024. A root has depth 0.
- Working space is exactly `node_count` words (`scratch`). The preorder stack never holds more than `node_count` entries: a node is pushed once, either as a root or through its only parent, and `MultipleParents` is rejected before the walk.
- Time is O(node_count + edges): the parent pass visits each edge once, the preorder visits each node and edge once, the size pass visits each node once.

## Check order

NullBuffer, BadCount, BadOffsets, Overlap, then in node and child order the first BadChild or MultipleParents, then in preorder the first TooDeep, then Cycle (fewer nodes visited than `node_count`). A node listed as its own child is not a root and is never reached, so it is a Cycle unless another node also lists it (then MultipleParents). On error the outputs and scratch are unspecified.

Overlap uses the shared rule of the other modules: top byte cleared, unsigned difference modulo 2^64, answer "no overlap" at a difference of 2^47 or more before any other arithmetic, then the exact byte ranges. A zero-length range (children with no edges) is skipped. The 2^47 early out cannot be reached with real buffers in one process; it is argued here and not exercised by the tests.

## Tests and evidence

- Golden digests (FNV-1a 64 over parent, depth, order and size words) come from an independent Python script with its own generator, recursive solver and FNV.
- Exhaustive: every parent assignment for 1 to 4 nodes is checked against a reference that uses a different method (parent walks), for the status and for the preorder property.
- Random forests: depths, subtree blocks and root totals are checked against the definitions.
- Statuses, check order, depth boundary (1024 allowed, 1025 rejected), edge-count boundary, overlap pairs, the largest tree and a star of 65535 children, under ASan and UBSan.
- Mutation checks: 14 deliberate faults (boundaries, removed checks, wrong order, wrong sizes) were applied to the implementation; 13 are caught. The survivor lowers the 2^47 overlap cutoff and is argued, not exercised, as above.
