// Lexicographically smallest topological order of a directed graph given as an edge list; see toposort.h for the contract and docs/toposort.md
// for the check order, the scratch layout and the bounds. Integer only, includes <cstdint> only, no allocation, no recursion, no global state.
//
// Scratch layout (3 * nodes + edges + 1 words): indegree [nodes), offsets [nodes + 1), adjacency [edges), heap [nodes). The adjacency is a
// compressed row list by source node. The ready nodes live in a binary min-heap at the front of the heap array and the placed nodes are
// stored from the back of the same array (a node is either unseen, in the heap or placed, so heap size + placed count never exceeds nodes).
// The order is copied into `order` only after the whole graph is placed, so a cycle leaves `order` untouched.
#include "toposort.h"

#include <cstdint>

namespace lumora::toposort {

namespace {

void heap_push(std::uint32_t* h, std::uint32_t* size, std::uint32_t v) {
  std::uint32_t i = *size;
  ++*size;
  while (i > 0u) {
    const std::uint32_t parent = (i - 1u) / 2u;
    if (h[parent] <= v) break;
    h[i] = h[parent];
    i = parent;
  }
  h[i] = v;
}

// Removes and returns the smallest element. The heap must not be empty.
std::uint32_t heap_pop(std::uint32_t* h, std::uint32_t* size) {
  const std::uint32_t top = h[0];
  --*size;
  const std::uint32_t n = *size;
  if (n > 0u) {
    const std::uint32_t last = h[n];
    std::uint32_t i = 0;
    for (;;) {
      std::uint32_t child = 2u * i + 1u;
      if (child >= n) break;
      if (child + 1u < n && h[child + 1u] < h[child]) ++child;
      if (last <= h[child]) break;
      h[i] = h[child];
      i = child;
    }
    h[i] = last;
  }
  return top;
}

}  // namespace

TopoResult toposort_scratch_words(std::uint32_t nodes, std::uint32_t edges, std::uint32_t* out_words) {
  if (out_words == nullptr) return TopoResult::NullArgument;
  if (nodes > kMaxNodes || edges > kMaxEdges) return TopoResult::TooLarge;
  *out_words = 3u * nodes + edges + 1u;
  return TopoResult::Ok;
}

TopoResult toposort_u32(std::uint32_t nodes, const std::uint32_t* from_to, std::uint32_t edges, std::uint32_t* order, std::uint32_t* scratch,
                        std::uint32_t scratch_words) {
  if ((from_to == nullptr && edges > 0u) || (order == nullptr && nodes > 0u) || (scratch == nullptr && nodes > 0u)) {
    return TopoResult::NullArgument;
  }
  if (nodes > kMaxNodes || edges > kMaxEdges) return TopoResult::TooLarge;
  const std::uint32_t words = 2u * edges;
  for (std::uint32_t k = 0; k < words; ++k) {
    if (from_to[k] >= nodes) return TopoResult::BadEdge;
  }
  if (nodes == 0u) return TopoResult::Ok;
  if (scratch_words < 3u * nodes + edges + 1u) return TopoResult::ScratchTooSmall;

  std::uint32_t* const indeg = scratch;
  std::uint32_t* const off = scratch + nodes;
  std::uint32_t* const adj = scratch + 2u * nodes + 1u;
  std::uint32_t* const heap = scratch + 2u * nodes + 1u + edges;

  for (std::uint32_t i = 0; i < nodes; ++i) indeg[i] = 0u;
  for (std::uint32_t i = 0; i <= nodes; ++i) off[i] = 0u;
  for (std::uint32_t k = 0; k < edges; ++k) {
    ++indeg[from_to[2u * k + 1u]];
    ++off[from_to[2u * k] + 1u];
  }
  for (std::uint32_t i = 0; i < nodes; ++i) off[i + 1u] += off[i];
  for (std::uint32_t i = 0; i < nodes; ++i) heap[i] = off[i];  // fill cursors, the heap array is free until the ordering starts
  for (std::uint32_t k = 0; k < edges; ++k) {
    adj[heap[from_to[2u * k]]++] = from_to[2u * k + 1u];
  }

  std::uint32_t size = 0;
  for (std::uint32_t i = 0; i < nodes; ++i) {
    if (indeg[i] == 0u) heap_push(heap, &size, i);
  }
  std::uint32_t placed = 0;
  while (size > 0u) {
    const std::uint32_t u = heap_pop(heap, &size);
    heap[nodes - 1u - placed] = u;
    ++placed;
    for (std::uint32_t p = off[u]; p < off[u + 1u]; ++p) {
      const std::uint32_t v = adj[p];
      --indeg[v];
      if (indeg[v] == 0u) heap_push(heap, &size, v);
    }
  }
  if (placed < nodes) return TopoResult::Cycle;
  for (std::uint32_t i = 0; i < nodes; ++i) order[i] = heap[nodes - 1u - i];
  return TopoResult::Ok;
}

}  // namespace lumora::toposort
