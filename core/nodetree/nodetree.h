// Lumora node tree validator (written by the agent). Pure CPU, deterministic, integer only, no I/O, no threads, no heap allocation,
// no recursion, no static mutable state, no pointer values in any decision except the Overlap check.
// Purpose: turn a node graph given as child lists (compressed rows, as glTF scenes and UI trees provide them) into a validated forest
// with parent links, depths, a preorder and subtree sizes, in bounded time and space. Evidence label: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::nodetree {

constexpr std::uint32_t kMaxNodes = 1u << 16;   // node_count in 1 .. kMaxNodes
constexpr std::uint32_t kMaxDepth = 1024;       // a root has depth 0; deeper nodes are rejected
constexpr std::uint32_t kNoParent = 0xFFFFFFFFu;

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,        // a required pointer is null (all pointers are always required, also children when there are no edges)
  BadCount,          // node_count is 0 or above kMaxNodes
  BadOffsets,        // child_offsets[0] != 0, decreasing offsets, or child_offsets[node_count] above kMaxNodes
  Overlap,           // touched byte ranges intersect (modular address rule as in core/composite)
  BadChild,          // a child index is not below node_count
  MultipleParents,   // a node is listed as child more than once (also a node listed under itself and under another node)
  Cycle,             // some node is not reachable from a root (a cycle, including a node listed as its own child)
  TooDeep            // a node deeper than kMaxDepth
};
// Lower-case spaced names ("ok", "null buffer", "bad count", "bad offsets", "overlap", "bad child", "multiple parents", "cycle",
// "too deep"), "unknown" otherwise.
const char* status_text(Status s);

// Node i has the children children[child_offsets[i] .. child_offsets[i + 1]) in that order. A root is a node that is nobody's child.
// Outputs, each node_count words: parent[i] (kNoParent for roots), depth[i], order (preorder: roots in ascending index, children in
// listed order, every node once) and subtree_size[i] (the node and all its descendants). scratch is node_count words of working
// space. On any error the outputs and scratch are unspecified, and the status says why. Checks in this order: NullBuffer, BadCount,
// BadOffsets, Overlap (every pair of the seven arrays, exact byte ranges: child_offsets node_count + 1 words, children
// child_offsets[node_count] words skipped when zero, the five outputs node_count words), BadChild and MultipleParents (nodes in
// index order, children in listed order, the first violation wins), then TooDeep (preorder, first violation wins), then Cycle.
Status build(const std::uint32_t* child_offsets, const std::uint32_t* children, std::uint32_t node_count, std::uint32_t* parent,
             std::uint32_t* depth, std::uint32_t* order, std::uint32_t* subtree_size, std::uint32_t* scratch);

// FNV-1a 64 over 32-bit little-endian words (each word as 4 bytes). Null or zero count hashes zero bytes.
std::uint64_t digest_words(const std::uint32_t* words, std::size_t count);

}  // namespace lumora::nodetree
