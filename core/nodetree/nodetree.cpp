// Lumora node tree validator. Integer only, no heap, no recursion, no mutable statics. See docs/nodetree.md.
#include "nodetree.h"

namespace lumora::nodetree {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

// Same modular address rule as core/composite and the contributor modules: clear the top byte, take the unsigned difference
// modulo 2^64, answer "no overlap" at |delta| >= 2^47 before other arithmetic, then compare the exact ranges.
bool ranges_overlap(const void* a, u64 a_len, const void* b, u64 b_len) {
  const u64 kTagMask = 0x00FFFFFFFFFFFFFFull;
  const u64 ua = static_cast<u64>(reinterpret_cast<std::uintptr_t>(a)) & kTagMask;
  const u64 ub = static_cast<u64>(reinterpret_cast<std::uintptr_t>(b)) & kTagMask;
  const u64 diff = ua - ub;
  const bool negative = diff >= (static_cast<u64>(1) << 63);
  const u64 absd = negative ? (static_cast<u64>(0) - diff) : diff;
  if (absd >= (static_cast<u64>(1) << 47)) return false;
  const i64 delta = negative ? -static_cast<i64>(absd) : static_cast<i64>(absd);
  return delta < static_cast<i64>(b_len) && delta + static_cast<i64>(a_len) > 0;
}

struct Extent {
  const void* p;
  u64 bytes;
};

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadCount: return "bad count";
    case Status::BadOffsets: return "bad offsets";
    case Status::Overlap: return "overlap";
    case Status::BadChild: return "bad child";
    case Status::MultipleParents: return "multiple parents";
    case Status::Cycle: return "cycle";
    case Status::TooDeep: return "too deep";
  }
  return "unknown";
}

Status build(const u32* child_offsets, const u32* children, u32 node_count, u32* parent, u32* depth, u32* order,
             u32* subtree_size, u32* scratch) {
  if (child_offsets == nullptr || children == nullptr || parent == nullptr || depth == nullptr || order == nullptr ||
      subtree_size == nullptr || scratch == nullptr)
    return Status::NullBuffer;
  if (node_count < 1 || node_count > kMaxNodes) return Status::BadCount;
  if (child_offsets[0] != 0) return Status::BadOffsets;
  for (u32 i = 0; i < node_count; ++i)
    if (child_offsets[i + 1] < child_offsets[i]) return Status::BadOffsets;
  const u32 edges = child_offsets[node_count];
  if (edges > kMaxNodes) return Status::BadOffsets;

  const u64 nb = static_cast<u64>(node_count) * sizeof(u32);
  const Extent ext[7] = {{child_offsets, nb + sizeof(u32)}, {children, static_cast<u64>(edges) * sizeof(u32)},
                         {parent, nb},                      {depth, nb},
                         {order, nb},                       {subtree_size, nb},
                         {scratch, nb}};
  for (int i = 0; i < 7; ++i) {
    if (ext[i].bytes == 0) continue;
    for (int j = i + 1; j < 7; ++j) {
      if (ext[j].bytes == 0) continue;
      if (ranges_overlap(ext[i].p, ext[i].bytes, ext[j].p, ext[j].bytes)) return Status::Overlap;
    }
  }

  for (u32 i = 0; i < node_count; ++i) parent[i] = kNoParent;
  for (u32 i = 0; i < node_count; ++i) {
    for (u32 e = child_offsets[i]; e < child_offsets[i + 1]; ++e) {
      const u32 c = children[e];
      if (c >= node_count) return Status::BadChild;
      if (parent[c] != kNoParent) return Status::MultipleParents;
      parent[c] = i;
    }
  }

  // Preorder with an explicit stack in scratch. Each node is pushed at most once (through its only parent, or as a root), so the
  // stack never holds more than node_count entries.
  u32 count = 0;
  for (u32 r = 0; r < node_count; ++r) {
    if (parent[r] != kNoParent) continue;
    u32 sp = 0;
    depth[r] = 0;
    scratch[sp++] = r;
    while (sp != 0) {
      const u32 v = scratch[--sp];
      if (depth[v] > kMaxDepth) return Status::TooDeep;
      order[count++] = v;
      for (u32 e = child_offsets[v + 1]; e > child_offsets[v]; --e) {
        const u32 c = children[e - 1];
        depth[c] = depth[v] + 1;
        scratch[sp++] = c;
      }
    }
  }
  if (count != node_count) return Status::Cycle;

  for (u32 i = 0; i < node_count; ++i) subtree_size[i] = 1;
  for (u32 k = node_count; k > 0; --k) {
    const u32 v = order[k - 1];
    if (parent[v] != kNoParent) subtree_size[parent[v]] += subtree_size[v];
  }
  return Status::Ok;
}

std::uint64_t digest_words(const u32* words, std::size_t count) {
  u64 h = 0xcbf29ce484222325ull;
  if (words == nullptr) return h;
  for (std::size_t i = 0; i < count; ++i) {
    u32 v = words[i];
    for (int b = 0; b < 4; ++b) {
      h ^= static_cast<u64>(v & 0xFFu);
      h *= 0x100000001b3ull;
      v >>= 8;
    }
  }
  return h;
}

}  // namespace lumora::nodetree
