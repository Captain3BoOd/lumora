// Fuzz target: nodetree. Decodes bytes into a child-list graph (including out-of-range children) and calls build. A success must give a
// consistent forest: parents match the lists, the preorder is a permutation with parents before children, depths and subtree sizes
// are consistent. Anything else aborts, so the fuzzer reports it as a crash.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "nodetree.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::nodetree;
  if (size < 1) return 0;
  const std::uint32_t n = 1 + data[0] % 48;
  std::size_t pos = 1;
  std::vector<std::uint32_t> offsets(n + 1, 0), children;
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint32_t c = pos < size ? data[pos++] % 4 : 0;
    for (std::uint32_t k = 0; k < c; ++k) children.push_back(pos < size ? data[pos++] % (n + 1) : 0);
    offsets[i + 1] = static_cast<std::uint32_t>(children.size());
  }
  if (children.empty()) children.push_back(0);
  std::vector<std::uint32_t> parent(n), depth(n), order(n), sz(n), scratch(n);
  const Status st = build(offsets.data(), children.data(), n, parent.data(), depth.data(), order.data(), sz.data(), scratch.data());
  if (st != Status::Ok) return 0;
  std::vector<std::uint32_t> pos_of(n, 0xFFFFFFFFu);
  for (std::uint32_t i = 0; i < n; ++i) {
    if (order[i] >= n || pos_of[order[i]] != 0xFFFFFFFFu) std::abort();
    pos_of[order[i]] = i;
  }
  std::vector<std::uint32_t> kids(n, 0);
  for (std::uint32_t v = 0; v < n; ++v)
    for (std::uint32_t e = offsets[v]; e < offsets[v + 1]; ++e) {
      const std::uint32_t c = children[e];
      if (c >= n || parent[c] != v || depth[c] != depth[v] + 1 || pos_of[v] >= pos_of[c]) std::abort();
      ++kids[v];
    }
  std::vector<std::uint32_t> sum(n, 1);
  for (std::uint32_t k = n; k > 0; --k) {
    const std::uint32_t v = order[k - 1];
    if (sum[v] != sz[v]) std::abort();
    if (parent[v] == kNoParent) {
      if (depth[v] != 0) std::abort();
    } else {
      sum[parent[v]] += sum[v];
    }
  }
  return 0;
}
