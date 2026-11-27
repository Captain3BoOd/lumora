// Depth, subtree size, lowest common ancestor and diameter of a forest given as a parent array. See treeq.h for the contract and docs/treeq.md for
// the method, the check order and the bounds.
//
// Method. No recursion and no allocation: everything runs in explicit loops over the caller's scratch (three arrays of n words: A, B, C). A
// validation pass peels leaves (Kahn's method): it counts the children of every node, queues the nodes without children, and removes a node from
// its parent's count when the node is taken from the queue. If all n nodes get queued the array is a valid forest (a cycle never reaches count 0);
// the queue is then an order in which every node comes after all of its children, and its reverse an order in which every parent comes first.
// Outputs are written only after the whole array (and, for lca, every query) has been checked.
#include "treeq.h"

namespace lumora::treeq {

namespace {

constexpr std::uint32_t kUnset = 0xFFFFFFFFu;      // diameter: distance not computed yet
constexpr std::uint32_t kUnreachable = 0xFFFFFFFEu;  // diameter: the node is in another tree

// Checks the whole parent array. On Ok, queue[0 .. n) holds every node, children before parents; cnt[] is all zero. Scratch is used freely.
TreeResult validate(std::uint32_t n, const std::uint32_t* parent, std::uint32_t* cnt, std::uint32_t* queue) {
  for (std::uint32_t v = 0; v < n; ++v) {
    const std::uint32_t p = parent[v];
    if (p != kNoParent && (p >= n || p == v)) return TreeResult::BadParent;
  }
  for (std::uint32_t v = 0; v < n; ++v) cnt[v] = 0;
  for (std::uint32_t v = 0; v < n; ++v) {
    if (parent[v] != kNoParent) ++cnt[parent[v]];
  }
  std::uint32_t tail = 0;
  for (std::uint32_t v = 0; v < n; ++v) {
    if (cnt[v] == 0) queue[tail++] = v;
  }
  for (std::uint32_t head = 0; head < tail; ++head) {
    const std::uint32_t p = parent[queue[head]];
    if (p != kNoParent) {
      --cnt[p];
      if (cnt[p] == 0) queue[tail++] = p;
    }
  }
  return tail == n ? TreeResult::Ok : TreeResult::BadParent;
}

}  // namespace

TreeResult treeq_scratch_words(std::uint32_t n, std::uint32_t* out_words) {
  if (out_words == nullptr) return TreeResult::NullArgument;
  if (n > kMaxNodes) return TreeResult::TooLarge;
  *out_words = 3 * n + 1;
  return TreeResult::Ok;
}

TreeResult treeq_depth_size(std::uint32_t n, const std::uint32_t* parent, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* depth,
                            std::uint32_t* size) {
  if (n > 0 && (parent == nullptr || scratch == nullptr || depth == nullptr || size == nullptr)) return TreeResult::NullArgument;
  if (n > kMaxNodes) return TreeResult::TooLarge;
  if (n == 0) return TreeResult::Ok;
  if (scratch_words < 3 * n + 1) return TreeResult::ScratchTooSmall;
  std::uint32_t* const queue = scratch + n;
  const TreeResult r = validate(n, parent, scratch, queue);
  if (r != TreeResult::Ok) return r;
  for (std::uint32_t v = 0; v < n; ++v) size[v] = 1;
  for (std::uint32_t i = 0; i < n; ++i) {  // children before parents
    const std::uint32_t v = queue[i];
    if (parent[v] != kNoParent) size[parent[v]] += size[v];
  }
  for (std::uint32_t i = n; i > 0; --i) {  // parents before children
    const std::uint32_t v = queue[i - 1];
    depth[v] = parent[v] == kNoParent ? 0 : depth[parent[v]] + 1;
  }
  return TreeResult::Ok;
}

TreeResult treeq_lca(std::uint32_t n, const std::uint32_t* parent, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t q,
                     const std::uint32_t* qa, const std::uint32_t* qb, std::uint32_t* out) {
  if ((n > 0 && (parent == nullptr || scratch == nullptr)) || (q > 0 && (qa == nullptr || qb == nullptr || out == nullptr))) return TreeResult::NullArgument;
  if (n > kMaxNodes || q > kMaxQueries) return TreeResult::TooLarge;
  if (n > 0 && scratch_words < 3 * n + 1) return TreeResult::ScratchTooSmall;
  std::uint32_t* const queue = scratch + n;
  std::uint32_t* const depth = scratch + 2 * n;
  if (n > 0) {
    const TreeResult r = validate(n, parent, scratch, queue);
    if (r != TreeResult::Ok) return r;
  }
  for (std::uint32_t i = 0; i < q; ++i) {
    if (qa[i] >= n || qb[i] >= n) return TreeResult::BadQuery;
  }
  if (q == 0) return TreeResult::Ok;
  for (std::uint32_t i = n; i > 0; --i) {
    const std::uint32_t v = queue[i - 1];
    depth[v] = parent[v] == kNoParent ? 0 : depth[parent[v]] + 1;
  }
  for (std::uint32_t i = 0; i < q; ++i) {
    std::uint32_t u = qa[i], v = qb[i];
    while (depth[u] > depth[v]) u = parent[u];
    while (depth[v] > depth[u]) v = parent[v];
    std::uint32_t result = 0;
    for (;;) {
      if (u == v) {
        result = u;
        break;
      }
      if (parent[u] == kNoParent) {  // equal depth 0 and different nodes: two roots, different trees
        result = kNoParent;
        break;
      }
      u = parent[u];
      v = parent[v];
    }
    out[i] = result;
  }
  return TreeResult::Ok;
}

TreeResult treeq_diameter(std::uint32_t n, const std::uint32_t* parent, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_len,
                          std::uint32_t* out_a, std::uint32_t* out_b) {
  if (out_len == nullptr || (n > 0 && (parent == nullptr || scratch == nullptr || out_a == nullptr || out_b == nullptr))) return TreeResult::NullArgument;
  if (n > kMaxNodes) return TreeResult::TooLarge;
  if (n == 0) {
    *out_len = 0;
    return TreeResult::Ok;
  }
  if (scratch_words < 3 * n + 1) return TreeResult::ScratchTooSmall;
  std::uint32_t* const a_arr = scratch;      // child counts, then the longest path that starts upward, then distances from a
  std::uint32_t* const queue = scratch + n;  // children before parents
  std::uint32_t* const top = scratch + 2 * n;  // per node: (longest branch << 16) | second longest branch, in edges (at most 65535)
  const TreeResult r = validate(n, parent, a_arr, queue);
  if (r != TreeResult::Ok) return r;
  for (std::uint32_t v = 0; v < n; ++v) top[v] = 0;
  for (std::uint32_t i = 0; i < n; ++i) {  // branch heights, children first
    const std::uint32_t v = queue[i];
    const std::uint32_t p = parent[v];
    if (p == kNoParent) continue;
    const std::uint32_t branch = (top[v] >> 16) + 1;
    std::uint32_t t1 = top[p] >> 16, t2 = top[p] & 0xFFFFu;
    if (branch > t1) {
      t2 = t1;
      t1 = branch;
    } else if (branch > t2) {
      t2 = branch;
    }
    top[p] = (t1 << 16) | t2;
  }
  std::uint32_t diam = 0;  // the longest path has its top node v: the two longest branches of v
  for (std::uint32_t v = 0; v < n; ++v) {
    const std::uint32_t len = (top[v] >> 16) + (top[v] & 0xFFFFu);
    if (len > diam) diam = len;
  }
  if (diam == 0) {  // every tree is a single node
    *out_len = 0;
    *out_a = 0;
    *out_b = 0;
    return TreeResult::Ok;
  }
  for (std::uint32_t i = n; i > 0; --i) {  // up[v] = the longest path from v that starts with the edge to its parent
    const std::uint32_t v = queue[i - 1];
    const std::uint32_t p = parent[v];
    if (p == kNoParent) {
      a_arr[v] = 0;
      continue;
    }
    const std::uint32_t t1 = top[p] >> 16, t2 = top[p] & 0xFFFFu;
    const std::uint32_t sibling = ((top[v] >> 16) + 1 == t1) ? t2 : t1;  // the best branch of p that does not go through v
    a_arr[v] = 1 + (a_arr[p] > sibling ? a_arr[p] : sibling);
  }
  std::uint32_t a = 0;  // the smallest node whose longest path is the diameter
  for (std::uint32_t v = 0; v < n; ++v) {
    const std::uint32_t ecc = (top[v] >> 16) > a_arr[v] ? (top[v] >> 16) : a_arr[v];
    if (ecc == diam) {
      a = v;
      break;
    }
  }
  for (std::uint32_t v = 0; v < n; ++v) a_arr[v] = kUnset;
  std::uint32_t k = 0;  // the ancestors of a (a included) at their distance from a
  for (std::uint32_t x = a;; x = parent[x], ++k) {
    a_arr[x] = k;
    if (parent[x] == kNoParent) break;
  }
  for (std::uint32_t i = n; i > 0; --i) {  // distance from a to every node of a's tree, parents first
    const std::uint32_t v = queue[i - 1];
    if (a_arr[v] != kUnset) continue;
    const std::uint32_t p = parent[v];
    a_arr[v] = (p == kNoParent || a_arr[p] == kUnreachable) ? kUnreachable : a_arr[p] + 1;
  }
  std::uint32_t b = a;
  for (std::uint32_t v = 0; v < n; ++v) {
    if (a_arr[v] == diam) {
      b = v;
      break;
    }
  }
  *out_len = diam;
  *out_a = a;
  *out_b = b;
  return TreeResult::Ok;
}

}  // namespace lumora::treeq
