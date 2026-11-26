// Tests for core/nodetree. Golden digests come from an independent Python reference (own generator, own recursive solver, own FNV).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "nodetree.h"

using namespace lumora::nodetree;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

namespace {

struct Graph {
  std::vector<u32> offsets, children;
  u32 n() const { return static_cast<u32>(offsets.size() - 1); }
};

Graph from_kids(const std::vector<std::vector<u32>>& kids) {
  Graph g;
  g.offsets.push_back(0);
  for (const auto& k : kids) {
    for (u32 c : k) g.children.push_back(c);
    g.offsets.push_back(static_cast<u32>(g.children.size()));
  }
  if (g.children.empty()) g.children.push_back(0);  // a valid pointer; the extent is zero words
  return g;
}

struct Result {
  Status st;
  std::vector<u32> parent, depth, order, size, scratch;
};

Result run(const Graph& g) {
  Result r;
  const u32 n = g.n();
  r.parent.assign(n, 7);
  r.depth.assign(n, 7);
  r.order.assign(n, 7);
  r.size.assign(n, 7);
  r.scratch.assign(n, 7);
  r.st = build(g.offsets.data(), g.children.data(), n, r.parent.data(), r.depth.data(), r.order.data(), r.size.data(),
               r.scratch.data());
  return r;
}

u64 digest_all(const Result& r) {
  std::vector<u32> w;
  for (const auto* v : {&r.parent, &r.depth, &r.order, &r.size}) w.insert(w.end(), v->begin(), v->end());
  return digest_words(w.data(), w.size());
}

struct Lcg {
  u64 s;
  explicit Lcg(u64 seed) : s(seed) {}
  u64 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return s >> 33;
  }
};

std::vector<std::vector<u32>> forest(u32 n, u64 seed) {
  Lcg g(seed);
  std::vector<std::vector<u32>> kids(n);
  std::vector<u32> perm(n);
  for (u32 i = 0; i < n; ++i) perm[i] = i;
  for (u32 i = n - 1; i > 0; --i) {
    const u32 j = static_cast<u32>(g.next() % (i + 1));
    std::swap(perm[i], perm[j]);
  }
  for (u32 i = 0; i < n; ++i) {
    const u32 r = static_cast<u32>(g.next() % (i + 1));
    if (r == i) continue;
    kids[perm[r]].push_back(perm[i]);
  }
  return kids;
}

}  // namespace

TEST_CASE("status text", "[nodetree]") {
  CHECK(std::string(status_text(Status::Ok)) == "ok");
  CHECK(std::string(status_text(Status::NullBuffer)) == "null buffer");
  CHECK(std::string(status_text(Status::BadCount)) == "bad count");
  CHECK(std::string(status_text(Status::BadOffsets)) == "bad offsets");
  CHECK(std::string(status_text(Status::Overlap)) == "overlap");
  CHECK(std::string(status_text(Status::BadChild)) == "bad child");
  CHECK(std::string(status_text(Status::MultipleParents)) == "multiple parents");
  CHECK(std::string(status_text(Status::Cycle)) == "cycle");
  CHECK(std::string(status_text(Status::TooDeep)) == "too deep");
  CHECK(std::string(status_text(static_cast<Status>(99))) == "unknown");
}

TEST_CASE("digest_words vectors", "[nodetree]") {
  CHECK(digest_words(nullptr, 5) == 0xcbf29ce484222325ull);
  const u32 zero = 0;
  CHECK(digest_words(&zero, 0) == 0xcbf29ce484222325ull);
  // FNV-1a 64 of the four bytes 01 00 00 00 (computed by hand with the reference in the test header comment script)
  const u32 one = 1;
  u64 h = 0xcbf29ce484222325ull;
  const unsigned char bytes[4] = {1, 0, 0, 0};
  for (unsigned char b : bytes) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  CHECK(digest_words(&one, 1) == h);
}

TEST_CASE("explicit small forest", "[nodetree]") {
  // 0: 3 1 ; 1: 4 ; 2: (root, no children) ; 3: ; 4: 5 ; 5:
  const Graph g = from_kids({{3, 1}, {4}, {}, {}, {5}, {}});
  const Result r = run(g);
  REQUIRE(r.st == Status::Ok);
  CHECK(r.parent == std::vector<u32>{kNoParent, 0, kNoParent, 0, 1, 4});
  CHECK(r.depth == std::vector<u32>{0, 1, 0, 1, 2, 3});
  CHECK(r.order == std::vector<u32>{0, 3, 1, 4, 5, 2});
  CHECK(r.size == std::vector<u32>{5, 3, 1, 1, 2, 1});
}

TEST_CASE("single node and all roots", "[nodetree]") {
  const Result a = run(from_kids({{}}));
  REQUIRE(a.st == Status::Ok);
  CHECK(a.order == std::vector<u32>{0});
  CHECK(a.size == std::vector<u32>{1});
  const Result b = run(from_kids({{}, {}, {}}));
  REQUIRE(b.st == Status::Ok);
  CHECK(b.order == std::vector<u32>{0, 1, 2});
  CHECK(b.depth == std::vector<u32>{0, 0, 0});
}

TEST_CASE("error statuses", "[nodetree]") {
  CHECK(run(from_kids({{1}, {1}})).st == Status::MultipleParents);
  CHECK(run(from_kids({{2}, {}})).st == Status::BadChild);
  CHECK(run(from_kids({{0}})).st == Status::Cycle);          // self loop
  CHECK(run(from_kids({{1}, {0}})).st == Status::Cycle);     // two-cycle, no root
  CHECK(run(from_kids({{}, {2}, {3}, {1}})).st == Status::Cycle);  // cycle next to a root
  CHECK(run(from_kids({{1, 1}, {}})).st == Status::MultipleParents);
  CHECK(run(from_kids({{0, 1}, {}})).st == Status::Cycle);            // node 0 is its own child, so it is not a root
  CHECK(run(from_kids({{0}, {0}})).st == Status::MultipleParents);    // listed under itself and under another node
}

TEST_CASE("argument checks and order", "[nodetree]") {
  const Graph g = from_kids({{1}, {}});
  std::vector<u32> a(2), b(2), c(2), d(2), e(2);
  CHECK(build(nullptr, g.children.data(), 2, a.data(), b.data(), c.data(), d.data(), e.data()) == Status::NullBuffer);
  CHECK(build(g.offsets.data(), nullptr, 2, a.data(), b.data(), c.data(), d.data(), e.data()) == Status::NullBuffer);
  CHECK(build(g.offsets.data(), g.children.data(), 2, nullptr, b.data(), c.data(), d.data(), e.data()) == Status::NullBuffer);
  CHECK(build(g.offsets.data(), g.children.data(), 2, a.data(), nullptr, c.data(), d.data(), e.data()) == Status::NullBuffer);
  CHECK(build(g.offsets.data(), g.children.data(), 2, a.data(), b.data(), nullptr, d.data(), e.data()) == Status::NullBuffer);
  CHECK(build(g.offsets.data(), g.children.data(), 2, a.data(), b.data(), c.data(), nullptr, e.data()) == Status::NullBuffer);
  CHECK(build(g.offsets.data(), g.children.data(), 2, a.data(), b.data(), c.data(), d.data(), nullptr) == Status::NullBuffer);
  // NullBuffer before BadCount
  CHECK(build(nullptr, g.children.data(), 0, a.data(), b.data(), c.data(), d.data(), e.data()) == Status::NullBuffer);
  CHECK(build(g.offsets.data(), g.children.data(), 0, a.data(), b.data(), c.data(), d.data(), e.data()) == Status::BadCount);
  CHECK(build(g.offsets.data(), g.children.data(), kMaxNodes + 1, a.data(), b.data(), c.data(), d.data(), e.data()) ==
        Status::BadCount);
  // BadCount before BadOffsets
  std::vector<u32> bad = {1, 1, 1};
  CHECK(build(bad.data(), g.children.data(), 0, a.data(), b.data(), c.data(), d.data(), e.data()) == Status::BadCount);
  CHECK(build(bad.data(), g.children.data(), 2, a.data(), b.data(), c.data(), d.data(), e.data()) == Status::BadOffsets);
  bad = {0, 2, 1};
  CHECK(build(bad.data(), g.children.data(), 2, a.data(), b.data(), c.data(), d.data(), e.data()) == Status::BadOffsets);
  bad = {0, 0, kMaxNodes + 1};
  CHECK(build(bad.data(), g.children.data(), 2, a.data(), b.data(), c.data(), d.data(), e.data()) == Status::BadOffsets);
}

TEST_CASE("overlap of every pair", "[nodetree]") {
  const Graph g = from_kids({{1}, {}});
  std::vector<u32> buf(40, 0);
  std::vector<u32> offs = g.offsets, ch = g.children;
  // seven arrays in one pool: all disjoint works, then each pair forced to overlap by pointing one at the other
  u32* base = buf.data();
  u32 *o[5] = {base + 0, base + 2, base + 4, base + 6, base + 8};
  CHECK(build(offs.data(), ch.data(), 2, o[0], o[1], o[2], o[3], o[4]) == Status::Ok);
  CHECK(build(offs.data(), ch.data(), 2, o[0], o[0], o[2], o[3], o[4]) == Status::Overlap);
  CHECK(build(offs.data(), ch.data(), 2, o[0], o[1], o[1] + 1, o[3], o[4]) == Status::Overlap);
  CHECK(build(offs.data(), ch.data(), 2, o[0], o[1], o[2], o[3], o[3] + 1) == Status::Overlap);
  CHECK(build(offs.data(), ch.data(), 2, o[0], o[1], o[2], o[3], o[2]) == Status::Overlap);
  CHECK(build(offs.data(), ch.data(), 2, o[0], o[1], o[2], o[3], o[3]) == Status::Overlap);
  // input overlapping an output, and the offsets (n + 1 words) overlapping the children (1 word)
  std::vector<u32> pool(8, 0);
  pool[0] = 0; pool[1] = 1; pool[2] = 1;  // offsets for n = 2: 0 1 1
  pool[3] = 1;                            // children: [1]
  std::vector<u32> x(2), y(2), z(2), w(2);
  CHECK(build(pool.data(), pool.data() + 3, 2, x.data(), y.data(), z.data(), w.data(), pool.data() + 4) == Status::Ok);
  CHECK(build(pool.data(), pool.data() + 2, 2, x.data(), y.data(), z.data(), w.data(), pool.data() + 5) == Status::Overlap);
  CHECK(build(pool.data(), pool.data() + 3, 2, pool.data() + 2, y.data(), z.data(), w.data(), pool.data() + 5) == Status::Overlap);
  // edges == 0: the children extent is empty and overlaps nothing
  std::vector<u32> zero_offs = {0, 0, 0};
  CHECK(build(zero_offs.data(), zero_offs.data(), 2, x.data(), y.data(), z.data(), w.data(), pool.data()) == Status::Ok);
}

TEST_CASE("edge count boundary and empty children extent", "[nodetree]") {
  // edges == kMaxNodes is inside the offsets limit (a forest cannot have that many edges, so the graph fails later)
  std::vector<std::vector<u32>> star(kMaxNodes);
  for (u32 i = 1; i < kMaxNodes; ++i) star[0].push_back(i);
  star[0].push_back(1);
  CHECK(run(from_kids(star)).st == Status::MultipleParents);
  star[0].push_back(1);
  CHECK(run(from_kids(star)).st == Status::BadOffsets);
  // a zero-word children extent that lies inside an output array is not an overlap
  std::vector<u32> offs = {0, 0, 0}, x(2), y(2), z(2), w(4), sc(2);
  CHECK(build(offs.data(), w.data() + 1, 2, x.data(), y.data(), z.data(), w.data(), sc.data()) == Status::Ok);
}

TEST_CASE("depth limit", "[nodetree]") {
  for (u32 len : {kMaxDepth, kMaxDepth + 1, kMaxDepth + 2}) {
    std::vector<std::vector<u32>> kids(len);
    for (u32 i = 0; i + 1 < len; ++i) kids[i].push_back(i + 1);
    // a chain of len nodes: the last node has depth len - 1
    const Result r = run(from_kids(kids));
    if (len - 1 <= kMaxDepth) {
      REQUIRE(r.st == Status::Ok);
      CHECK(r.depth[len - 1] == len - 1);
      CHECK(r.size[0] == len);
    } else {
      CHECK(r.st == Status::TooDeep);
    }
  }
  // TooDeep wins over Cycle when both exist (the cycle is found last)
  std::vector<std::vector<u32>> kids(kMaxDepth + 4);
  for (u32 i = 0; i + 3 < kids.size(); ++i) kids[i].push_back(i + 1);
  kids[kMaxDepth + 2] = {kMaxDepth + 3};
  kids[kMaxDepth + 3] = {kMaxDepth + 2};  // a separate two-cycle; the chain 0 .. kMaxDepth + 1 is one node too deep
  CHECK(run(from_kids(kids)).st == Status::TooDeep);
}

TEST_CASE("golden digests from the Python reference", "[nodetree]") {
  struct Case { u32 n; u64 seed; u64 digest; };
  const Case cases[] = {{1, 1, 0x9a1a79517ab39c70ull},
                        {7, 2, 0x7030ece6ef71aa5cull},
                        {64, 3, 0xef99c75421305ed1ull},
                        {1000, 4, 0x315e979bc4053267ull}};
  for (const Case& c : cases) {
    const Result r = run(from_kids(forest(c.n, c.seed)));
    REQUIRE(r.st == Status::Ok);
    CHECK(digest_all(r) == c.digest);
  }
}

namespace {
// Reference with a different method: parent walk for depth, cycle detection by walking at most n steps, sizes by counting
// ancestors. Returns the expected status (only Ok, BadChild, MultipleParents, Cycle, TooDeep).
Status reference_status(const std::vector<std::vector<u32>>& kids) {
  const u32 n = static_cast<u32>(kids.size());
  std::vector<int> par(n, -1);
  for (u32 i = 0; i < n; ++i)
    for (u32 c : kids[i]) {
      if (c >= n) return Status::BadChild;
      if (par[c] != -1) return Status::MultipleParents;
      par[c] = static_cast<int>(i);
    }
  for (u32 v = 0; v < n; ++v) {
    u32 steps = 0;
    int x = static_cast<int>(v);
    while (par[x] != -1 && steps <= n) {
      x = par[x];
      ++steps;
    }
    if (steps > n) return Status::Cycle;
  }
  return Status::Ok;
}
}  // namespace

TEST_CASE("exhaustive graphs up to 4 nodes agree with the reference", "[nodetree]") {
  // every assignment of "parent of node c" in {none, 0..n-1} with the children of a node ordered by child index, plus both orders
  // for two children of one node via the reversed variant
  for (u32 n = 1; n <= 4; ++n) {
    u32 combos = 1;
    for (u32 i = 0; i < n; ++i) combos *= (n + 1);
    for (u32 code = 0; code < combos; ++code) {
      std::vector<std::vector<u32>> kids(n);
      u32 x = code;
      for (u32 c = 0; c < n; ++c) {
        const u32 p = x % (n + 1);
        x /= (n + 1);
        if (p < n) kids[p].push_back(c);
      }
      const Result r = run(from_kids(kids));
      const Status want = reference_status(kids);
      REQUIRE(r.st == want);
      if (want == Status::Ok) {
        // preorder contains every node once and parents come before children
        std::vector<int> pos(n, -1);
        for (u32 i = 0; i < n; ++i) pos[r.order[i]] = static_cast<int>(i);
        for (u32 v = 0; v < n; ++v) {
          REQUIRE(pos[v] >= 0);
          if (r.parent[v] != kNoParent) REQUIRE(pos[r.parent[v]] < pos[v]);
        }
      }
    }
  }
}

TEST_CASE("random forests: sizes, depths and preorder invariants", "[nodetree]") {
  for (u64 seed = 1; seed <= 200; ++seed) {
    const u32 n = 1 + static_cast<u32>(seed * 37 % 300);
    const auto kids = forest(n, seed);
    const Result r = run(from_kids(kids));
    REQUIRE(r.st == Status::Ok);
    u32 root_total = 0;
    for (u32 v = 0; v < n; ++v) {
      if (r.parent[v] == kNoParent) {
        REQUIRE(r.depth[v] == 0);
        root_total += r.size[v];
      } else {
        REQUIRE(r.depth[v] == r.depth[r.parent[v]] + 1);
      }
      // the subtree of v is the contiguous preorder block starting at v
      u32 start = 0;
      while (r.order[start] != v) ++start;
      for (u32 k = 1; k < r.size[v]; ++k) {
        u32 u = r.order[start + k];
        while (u != v) {
          REQUIRE(r.parent[u] != kNoParent);
          u = r.parent[u];
        }
      }
    }
    REQUIRE(root_total == n);
  }
}

TEST_CASE("largest tree performance", "[nodetree]") {
  const auto kids = forest(kMaxNodes, 99);
  const Result r = run(from_kids(kids));
  REQUIRE(r.st == Status::Ok);
  // a star of kMaxNodes - 1 children
  std::vector<std::vector<u32>> star(kMaxNodes);
  for (u32 i = 1; i < kMaxNodes; ++i) star[0].push_back(i);
  const Result s = run(from_kids(star));
  REQUIRE(s.st == Status::Ok);
  CHECK(s.size[0] == kMaxNodes);
  CHECK(s.order[1] == 1);
  CHECK(s.order[kMaxNodes - 1] == kMaxNodes - 1);
}
