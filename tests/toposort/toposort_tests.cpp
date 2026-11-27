// Tests for core/toposort. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/toposort_ref.py, an independent Python implementation (set-based scan,
// cross-checked there against a brute force over permutations). Regenerate it with: python3 tools/toposort_ref.py
// The tests also carry a naive oracle that shares no code with core/toposort/toposort.cpp: a predecessor matrix and a repeated scan for the
// smallest unplaced node whose predecessors are all placed (no heap, no adjacency lists, no counters).
// Buffers are heap buffers of exactly the stated size (AddressSanitizer sees any access past the end): from_to has 2 * edges words, scratch has
// exactly scratch_words words (prefilled with garbage), order has nodes words plus canaries. A failure must leave order unchanged; on success
// only order[0 .. nodes) may change. Scratch contents after a call are unspecified and never asserted.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "toposort.h"

using namespace lumora::toposort;

namespace {

// BEGIN GENERATED REFERENCE (tools/toposort_ref.py)
const std::uint32_t kRefExhMaxN = 4, kRefRandSeeds = 400;
const std::uint64_t kRefDigest_exh = 0x78264EDED5FC77DFull;
const std::size_t kRefCases_exh = 66066, kRefOps_exh = 198198;
const std::uint64_t kRefDigest_rand = 0xFEE3E4E1EC41FE69ull;
const std::size_t kRefCases_rand = 400, kRefOps_rand = 1200;
// END GENERATED REFERENCE

constexpr std::uint32_t kSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kPad = 3;
constexpr std::uint32_t kGarbage = 0xA5A5A5A5u;

using Vec = std::vector<std::uint32_t>;

std::uint32_t code(TopoResult r) { return static_cast<std::uint32_t>(r); }

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  std::size_t ops = 0;
  void add(std::uint64_t v) { h = (h ^ v) * 0x100000001B3ull; }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

// ---- oracle ----
// Expected result code and order for the given arguments.
TopoResult o_expect(std::uint32_t n, const Vec& flat, std::uint32_t scratch_words, Vec* order) {
  order->clear();
  const std::size_t e = flat.size() / 2;
  for (std::uint32_t x : flat) {
    if (x >= n) return TopoResult::BadEdge;
  }
  if (n == 0u) return TopoResult::Ok;
  if (scratch_words < 3u * n + static_cast<std::uint32_t>(e) + 1u) return TopoResult::ScratchTooSmall;
  std::vector<std::vector<char>> pred(n, std::vector<char>(n, 0));
  for (std::size_t k = 0; k < e; ++k) pred[flat[2 * k + 1]][flat[2 * k]] = 1;
  std::vector<char> done(n, 0);
  for (std::uint32_t step = 0; step < n; ++step) {
    bool found = false;
    for (std::uint32_t v = 0; v < n && !found; ++v) {
      if (done[v] != 0) continue;
      bool ready = true;
      for (std::uint32_t u = 0; u < n; ++u) {
        if (pred[v][u] != 0 && done[u] == 0) ready = false;
      }
      if (ready) {
        done[v] = 1;
        order->push_back(v);
        found = true;
      }
    }
    if (!found) {
      order->clear();
      return TopoResult::Cycle;
    }
  }
  return TopoResult::Ok;
}

// ---- harness ----
// Runs toposort_u32 with exact buffers and checks it against the oracle. Returns the result code; appends to the fold when f is not null.
void do_topo(Fold* f, std::size_t* bad, std::uint32_t n, const Vec& flat, std::uint32_t scratch_words) {
  const std::uint32_t e = static_cast<std::uint32_t>(flat.size() / 2);
  std::unique_ptr<std::uint32_t[]> ft(new std::uint32_t[flat.empty() ? 1 : flat.size()]);
  for (std::size_t i = 0; i < flat.size(); ++i) ft[i] = flat[i];
  std::unique_ptr<std::uint32_t[]> sc(new std::uint32_t[scratch_words == 0u ? 1u : scratch_words]);
  for (std::uint32_t i = 0; i < scratch_words; ++i) sc[i] = kGarbage + i;
  const std::uint32_t total = n + kPad;
  std::unique_ptr<std::uint32_t[]> ord(new std::uint32_t[total]);
  for (std::uint32_t i = 0; i < total; ++i) ord[i] = kSentinel - i;
  Vec want;
  const TopoResult wr = o_expect(n, flat, scratch_words, &want);
  const TopoResult r = toposort_u32(n, ft.get(), e, ord.get(), sc.get(), scratch_words);
  if (r != wr) ++*bad;
  if (r == TopoResult::Ok && wr == TopoResult::Ok) {
    for (std::uint32_t i = 0; i < n; ++i) {
      if (ord[i] != want[i]) ++*bad;
    }
    for (std::uint32_t i = n; i < total; ++i) {
      if (ord[i] != kSentinel - i) ++*bad;
    }
  } else {
    for (std::uint32_t i = 0; i < total; ++i) {
      if (ord[i] != kSentinel - i) ++*bad;  // a failure leaves order untouched
    }
  }
  for (std::size_t i = 0; i < flat.size(); ++i) {
    if (ft[i] != flat[i]) ++*bad;  // from_to is never modified
  }
  if (f != nullptr) {
    ++f->ops;
    f->add(static_cast<std::uint64_t>(code(r)) << 32);
    if (r == TopoResult::Ok) {
      for (std::uint32_t i = 0; i < n; ++i) f->add(ord[i]);
    }
  }
}

void do_graph(Fold* f, std::size_t* bad, std::uint32_t n, const Vec& flat) {
  const std::uint32_t need = 3u * n + static_cast<std::uint32_t>(flat.size() / 2) + 1u;
  do_topo(f, bad, n, flat, need);
  do_topo(f, bad, n, flat, need - 1u);
  do_topo(f, bad, n, flat, need + 5u);
  if (f != nullptr) ++f->cases;
}

}  // namespace

TEST_CASE("header anchors", "[toposort]") {
  CHECK(kMaxNodes == (1u << 16));
  CHECK(kMaxEdges == (1u << 20));
  CHECK(code(TopoResult::Ok) == 0u);
  CHECK(code(TopoResult::NullArgument) == 1u);
  CHECK(code(TopoResult::TooLarge) == 2u);
  CHECK(code(TopoResult::BadEdge) == 3u);
  CHECK(code(TopoResult::ScratchTooSmall) == 4u);
  CHECK(code(TopoResult::Cycle) == 5u);
}

TEST_CASE("every edge set (self-loops included) on 1 to 4 nodes x scratch need, need - 1, need + 5", "[sweep][toposort]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t n = 1; n <= kRefExhMaxN; ++n) {
    for (std::uint32_t mask = 0; mask < (1u << (n * n)); ++mask) {
      Vec flat;
      for (std::uint32_t k = 0; k < n * n; ++k) {
        if ((mask >> k & 1u) != 0u) {
          flat.push_back(k / n);
          flat.push_back(k % n);
        }
      }
      do_graph(&f, &bad, n, flat);
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_exh);
  CHECK(f.ops == kRefOps_exh);
  CHECK(f.h == kRefDigest_exh);
}

TEST_CASE("pseudo-random graphs of 0 to 40 nodes (not exhaustive)", "[sweep][toposort]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t seed = 1; seed <= kRefRandSeeds; ++seed) {
    Lcg g(seed);
    const std::uint32_t n = g.next() % 41u;
    const std::uint32_t e = g.next() % (2u * n + 3u);
    const std::uint32_t mode = seed % 5u;
    std::vector<std::uint32_t> perm(n);
    for (std::uint32_t i = 0; i < n; ++i) perm[i] = i;
    for (std::uint32_t i = n; i-- > 1u;) {
      const std::uint32_t j = g.next() % (i + 1u);
      const std::uint32_t t = perm[i];
      perm[i] = perm[j];
      perm[j] = t;
    }
    std::vector<std::uint32_t> ea(e), eb(e);
    for (std::uint32_t k = 0; k < e; ++k) {
      std::uint32_t a = g.next();
      std::uint32_t b = g.next();
      const std::uint32_t m = n > 0u ? n : 4u;
      a %= m;
      b %= m;
      if (mode != 0u && n >= 2u) {
        if (a == b) b = (a + 1u) % n;
        const std::uint32_t lo = a < b ? a : b;
        const std::uint32_t hi = a < b ? b : a;
        a = perm[lo];
        b = perm[hi];
      }
      ea[k] = a;
      eb[k] = b;
    }
    if (mode == 2u) {
      for (std::uint32_t k = 0; k < e; ++k) {
        const std::uint32_t r = g.next();
        if (r % 4u == 0u && k > 0u) {
          const std::uint32_t src = (r >> 8) % k;
          ea[k] = ea[src];
          eb[k] = eb[src];
        }
      }
    }
    Vec flat;
    for (std::uint32_t k = 0; k < e; ++k) {
      flat.push_back(ea[k]);
      flat.push_back(eb[k]);
    }
    if (mode == 3u && e > 0u) {
      const std::uint32_t pos = g.next() % (2u * e);
      flat[pos] = n + g.next() % 3u;
    }
    if (mode == 4u && e > 0u && n > 0u) {
      const std::uint32_t pos = g.next() % e;
      const std::uint32_t v = g.next() % n;
      flat[2u * pos] = v;
      flat[2u * pos + 1u] = v;
    }
    do_graph(&f, &bad, n, flat);
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_rand);
  CHECK(f.ops == kRefOps_rand);
  CHECK(f.h == kRefDigest_rand);
}

namespace {

TopoResult run(std::uint32_t n, const Vec& flat, std::uint32_t sw, Vec* order_out) {
  const std::uint32_t e = static_cast<std::uint32_t>(flat.size() / 2);
  std::unique_ptr<std::uint32_t[]> ft(new std::uint32_t[flat.empty() ? 1 : flat.size()]);
  for (std::size_t i = 0; i < flat.size(); ++i) ft[i] = flat[i];
  std::unique_ptr<std::uint32_t[]> sc(new std::uint32_t[sw == 0u ? 1u : sw]);
  for (std::uint32_t i = 0; i < sw; ++i) sc[i] = kGarbage;
  std::unique_ptr<std::uint32_t[]> ord(new std::uint32_t[n == 0u ? 1u : n]);
  for (std::uint32_t i = 0; i < n; ++i) ord[i] = kSentinel;
  const TopoResult r = toposort_u32(n, ft.get(), e, ord.get(), sc.get(), sw);
  order_out->assign(ord.get(), ord.get() + n);
  return r;
}

Vec sentinel_order(std::uint32_t n) { return Vec(n, kSentinel); }

}  // namespace

TEST_CASE("scratch words: formula, limits, null and order of checks", "[args][toposort]") {
  std::uint32_t w = kSentinel;
  CHECK(toposort_scratch_words(0, 0, &w) == TopoResult::Ok);
  CHECK(w == 1u);
  CHECK(toposort_scratch_words(3, 2, &w) == TopoResult::Ok);
  CHECK(w == 12u);
  CHECK(toposort_scratch_words(1, 0, &w) == TopoResult::Ok);
  CHECK(w == 4u);
  CHECK(toposort_scratch_words(0, 7, &w) == TopoResult::Ok);
  CHECK(w == 8u);
  CHECK(toposort_scratch_words(kMaxNodes, kMaxEdges, &w) == TopoResult::Ok);
  CHECK(w == 3u * kMaxNodes + kMaxEdges + 1u);
  CHECK(w == 1245185u);
  w = kSentinel;
  CHECK(toposort_scratch_words(kMaxNodes + 1u, 0, &w) == TopoResult::TooLarge);
  CHECK(toposort_scratch_words(0, kMaxEdges + 1u, &w) == TopoResult::TooLarge);
  CHECK(toposort_scratch_words(0xFFFFFFFFu, 0xFFFFFFFFu, &w) == TopoResult::TooLarge);
  CHECK(w == kSentinel);
  CHECK(toposort_scratch_words(1, 1, nullptr) == TopoResult::NullArgument);
  CHECK(toposort_scratch_words(kMaxNodes + 1u, kMaxEdges + 1u, nullptr) == TopoResult::NullArgument);  // Null beats TooLarge
  CHECK(w == kSentinel);
}

TEST_CASE("toposort_u32: null rules", "[args][toposort]") {
  std::uint32_t ft[2] = {0u, 1u};
  std::uint32_t ord[2] = {kSentinel, kSentinel};
  std::uint32_t sc[16];
  for (std::uint32_t& x : sc) x = kGarbage;
  CHECK(toposort_u32(2, nullptr, 1, ord, sc, 16) == TopoResult::NullArgument);
  CHECK(toposort_u32(2, ft, 1, nullptr, sc, 16) == TopoResult::NullArgument);
  CHECK(toposort_u32(2, ft, 1, ord, nullptr, 16) == TopoResult::NullArgument);
  CHECK(toposort_u32(2, ft, 1, ord, nullptr, 0) == TopoResult::NullArgument);  // scratch_words is not looked at for the null rule
  CHECK(toposort_u32(2, ft, 1, ord, nullptr, 0xFFFFFFFFu) == TopoResult::NullArgument);
  CHECK(toposort_u32(1, nullptr, 1, nullptr, nullptr, 0) == TopoResult::NullArgument);
  CHECK(ord[0] == kSentinel);
  CHECK(ord[1] == kSentinel);
  // from_to counts only while edges > 0
  CHECK(toposort_u32(2, nullptr, 0, ord, sc, 7) == TopoResult::Ok);
  CHECK(ord[0] == 0u);
  CHECK(ord[1] == 1u);
  // order and scratch count only while nodes > 0
  ord[0] = kSentinel;
  CHECK(toposort_u32(0, nullptr, 0, nullptr, nullptr, 0) == TopoResult::Ok);
  CHECK(toposort_u32(0, ft, 0, nullptr, nullptr, 0) == TopoResult::Ok);
  CHECK(toposort_u32(0, nullptr, 0, ord, sc, 0) == TopoResult::Ok);
  CHECK(ord[0] == kSentinel);
  // nodes 0 with edges: from_to must be non-null, then every edge is a BadEdge
  CHECK(toposort_u32(0, nullptr, 1, nullptr, nullptr, 0) == TopoResult::NullArgument);
  CHECK(toposort_u32(0, ft, 1, nullptr, nullptr, 0) == TopoResult::BadEdge);
  CHECK(toposort_u32(0, ft, 1, ord, sc, 16) == TopoResult::BadEdge);
  CHECK(ord[0] == kSentinel);
}

TEST_CASE("toposort_u32: order of checks Null, TooLarge, BadEdge, ScratchTooSmall, Cycle", "[args][toposort]") {
  std::uint32_t one[1] = {0u};
  std::uint32_t ord[1] = {kSentinel};
  // Null beats TooLarge
  CHECK(toposort_u32(kMaxNodes + 1u, one, 0, nullptr, one, 1) == TopoResult::NullArgument);
  CHECK(toposort_u32(1, nullptr, kMaxEdges + 1u, ord, one, 1) == TopoResult::NullArgument);
  // TooLarge: nothing is read (1-word buffers), nodes or edges above the limit, TooLarge beats BadEdge and ScratchTooSmall
  CHECK(toposort_u32(kMaxNodes + 1u, one, 0, ord, one, 1) == TopoResult::TooLarge);
  CHECK(toposort_u32(1, one, kMaxEdges + 1u, ord, one, 1) == TopoResult::TooLarge);
  CHECK(toposort_u32(0, one, kMaxEdges + 1u, ord, one, 0) == TopoResult::TooLarge);
  CHECK(toposort_u32(0xFFFFFFFFu, one, 0xFFFFFFFFu, ord, one, 0) == TopoResult::TooLarge);
  CHECK(ord[0] == kSentinel);
  Vec o;
  // BadEdge: an endpoint equal to nodes, either end, any edge, the very last word; 0xFFFFFFFF too
  CHECK(run(3, {0, 1, 1, 2, 2, 3}, 100, &o) == TopoResult::BadEdge);
  CHECK(run(3, {0, 1, 1, 2, 3, 0}, 100, &o) == TopoResult::BadEdge);
  CHECK(run(3, {3, 0}, 100, &o) == TopoResult::BadEdge);
  CHECK(run(3, {0, 0xFFFFFFFFu}, 100, &o) == TopoResult::BadEdge);
  CHECK(run(3, {0, 1, 1, 2, 2, 2}, 100, &o) == TopoResult::Cycle);  // 2 is a valid endpoint (nodes - 1), the self-loop is a Cycle
  CHECK(o == sentinel_order(3));
  // BadEdge beats ScratchTooSmall and Cycle
  CHECK(run(3, {0, 3}, 0, &o) == TopoResult::BadEdge);
  CHECK(run(3, {1, 1, 0, 9}, 100, &o) == TopoResult::BadEdge);
  CHECK(o == sentinel_order(3));
  // ScratchTooSmall beats Cycle: 3n + e + 1 = 5 for one node with a self-loop
  CHECK(run(1, {0, 0}, 4, &o) == TopoResult::ScratchTooSmall);
  CHECK(run(1, {0, 0}, 5, &o) == TopoResult::Cycle);
  CHECK(run(1, {0, 0}, 0, &o) == TopoResult::ScratchTooSmall);
  CHECK(o == sentinel_order(1));
  // nodes 0 and edges 0 need no scratch
  CHECK(run(0, {}, 0, &o) == TopoResult::Ok);
  // the exact need is enough, need - 1 is not
  CHECK(run(4, {0, 1, 1, 2}, 15, &o) == TopoResult::Ok);
  CHECK(run(4, {0, 1, 1, 2}, 14, &o) == TopoResult::ScratchTooSmall);
  CHECK(run(2, {}, 7, &o) == TopoResult::Ok);
  CHECK(run(2, {}, 6, &o) == TopoResult::ScratchTooSmall);
}

TEST_CASE("toposort_u32: hand-checked orders, duplicates, edge list order, scratch garbage", "[toposort]") {
  Vec o;
  CHECK(run(3, {}, 10, &o) == TopoResult::Ok);
  CHECK(o == Vec({0, 1, 2}));
  CHECK(run(3, {2, 1, 1, 0}, 20, &o) == TopoResult::Ok);
  CHECK(o == Vec({2, 1, 0}));
  CHECK(run(4, {3, 0}, 20, &o) == TopoResult::Ok);
  CHECK(o == Vec({1, 2, 3, 0}));
  CHECK(run(4, {1, 0, 3, 2}, 20, &o) == TopoResult::Ok);  // 0 waits for 1, 2 waits for 3: 1, 0, 3, 2
  CHECK(o == Vec({1, 0, 3, 2}));
  CHECK(run(5, {4, 0, 4, 1, 3, 1}, 30, &o) == TopoResult::Ok);
  CHECK(o == Vec({2, 3, 4, 0, 1}));
  // a duplicate edge counts once for ordering
  Vec a, b;
  CHECK(run(3, {0, 2}, 20, &a) == TopoResult::Ok);
  CHECK(run(3, {0, 2, 0, 2, 0, 2}, 30, &b) == TopoResult::Ok);
  CHECK(a == b);
  CHECK(a == Vec({0, 1, 2}));
  // self-loop and a longer cycle
  CHECK(run(3, {1, 1}, 20, &o) == TopoResult::Cycle);
  CHECK(run(4, {0, 1, 1, 2, 2, 0}, 30, &o) == TopoResult::Cycle);
  CHECK(o == sentinel_order(4));
  // a cycle that only appears after a valid prefix
  CHECK(run(5, {0, 1, 2, 3, 3, 4, 4, 2}, 40, &o) == TopoResult::Cycle);
  CHECK(o == sentinel_order(5));
  // the edge list order does not change the result: shuffles of a 30-node graph with duplicates
  Lcg g(99);
  Vec edges;
  for (std::uint32_t k = 0; k < 80; ++k) {
    const std::uint32_t x = g.next() % 30u;
    const std::uint32_t y = g.next() % 30u;
    if (x == y) continue;
    edges.push_back(x < y ? y : x);  // edges point from the larger number to the smaller one: a DAG, reversed numbering
    edges.push_back(x < y ? x : y);
  }
  Vec first;
  CHECK(run(30, edges, 400, &first) == TopoResult::Ok);
  for (std::uint32_t round = 0; round < 40; ++round) {
    const std::size_t m = edges.size() / 2;
    for (std::size_t i = m; i-- > 1;) {
      const std::size_t j = g.next() % (i + 1);
      const std::uint32_t ta = edges[2 * i], tb = edges[2 * i + 1];
      edges[2 * i] = edges[2 * j];
      edges[2 * i + 1] = edges[2 * j + 1];
      edges[2 * j] = ta;
      edges[2 * j + 1] = tb;
    }
    if (round % 8u == 0u) {
      edges.push_back(edges[0]);
      edges.push_back(edges[1]);
    }
    Vec again;
    CHECK(run(30, edges, 400, &again) == TopoResult::Ok);
    CHECK(again == first);
  }
  // prefilled scratch with zeros, ones and the first sentinel gives the same order
  const Vec flat = {5, 0, 4, 0, 5, 2, 1, 3};
  Vec want;
  CHECK(run(6, flat, 40, &want) == TopoResult::Ok);
  for (std::uint32_t fill : {0u, 0xFFFFFFFFu, 1u, 6u}) {
    std::unique_ptr<std::uint32_t[]> sc(new std::uint32_t[3u * 6u + 4u + 1u]);
    for (std::uint32_t i = 0; i < 23u; ++i) sc[i] = fill;
    std::uint32_t ord[6];
    CHECK(toposort_u32(6, flat.data(), 4, ord, sc.get(), 23) == TopoResult::Ok);
    CHECK(Vec(ord, ord + 6) == want);
  }
}

TEST_CASE("sizes: 65536 nodes, real buffers", "[max][toposort]") {
  const std::uint32_t n = kMaxNodes;
  const std::uint32_t sw = 3u * n + 1u;
  std::unique_ptr<std::uint32_t[]> sc(new std::uint32_t[sw]);
  std::unique_ptr<std::uint32_t[]> ord(new std::uint32_t[n + kPad]);
  auto reset = [&]() {
    for (std::uint32_t i = 0; i < n + kPad; ++i) ord[i] = kSentinel;
  };
  auto is_untouched = [&]() {
    for (std::uint32_t i = 0; i < n + kPad; ++i) {
      if (ord[i] != kSentinel) return false;
    }
    return true;
  };
  // no edges: 0 .. n - 1; one word short of the scratch is ScratchTooSmall
  reset();
  CHECK(toposort_u32(n, nullptr, 0, ord.get(), sc.get(), sw - 1u) == TopoResult::ScratchTooSmall);
  CHECK(is_untouched());
  CHECK(toposort_u32(n, nullptr, 0, ord.get(), sc.get(), sw) == TopoResult::Ok);
  bool ok = true;
  for (std::uint32_t i = 0; i < n; ++i) ok = ok && ord[i] == i;
  CHECK(ok);
  for (std::uint32_t i = n; i < n + kPad; ++i) CHECK(ord[i] == kSentinel);
  // chain i -> i + 1 and the reversed chain i + 1 -> i, and everything pointing to node 0
  {
    const std::uint32_t e = n - 1u;
    std::vector<std::uint32_t> ft(2u * e);
    std::unique_ptr<std::uint32_t[]> sc2(new std::uint32_t[3u * n + e + 1u]);
    for (std::uint32_t k = 0; k < e; ++k) {
      ft[2u * k] = k;
      ft[2u * k + 1u] = k + 1u;
    }
    reset();
    CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc2.get(), 3u * n + e + 1u) == TopoResult::Ok);
    ok = true;
    for (std::uint32_t i = 0; i < n; ++i) ok = ok && ord[i] == i;
    CHECK(ok);
    for (std::uint32_t k = 0; k < e; ++k) {
      ft[2u * k] = k + 1u;
      ft[2u * k + 1u] = k;
    }
    reset();
    CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc2.get(), 3u * n + e + 1u) == TopoResult::Ok);
    ok = true;
    for (std::uint32_t i = 0; i < n; ++i) ok = ok && ord[i] == n - 1u - i;
    CHECK(ok);
    for (std::uint32_t k = 0; k < e; ++k) {
      ft[2u * k] = k + 1u;
      ft[2u * k + 1u] = 0u;
    }
    reset();
    CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc2.get(), 3u * n + e + 1u) == TopoResult::Ok);
    ok = ord[n - 1u] == 0u;
    for (std::uint32_t i = 0; i + 1u < n; ++i) ok = ok && ord[i] == i + 1u;
    CHECK(ok);
    // the chain closed into a cycle: Cycle, order untouched; and with one word less scratch: ScratchTooSmall
    for (std::uint32_t k = 0; k < e; ++k) {
      ft[2u * k] = k;
      ft[2u * k + 1u] = k + 1u;
    }
    ft[0] = n - 1u;
    ft[1] = 0u;  // edge 0 is now (n - 1) -> 0
    reset();
    CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc2.get(), 3u * n + e + 1u) == TopoResult::Ok);  // 1 -> 2 -> ... -> n - 1 -> 0 is a path, not a cycle
    ft[0] = n - 1u;
    ft[1] = 1u;  // (n - 1) -> 1 closes 1 -> ... -> n - 1 -> 1
    reset();
    CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc2.get(), 3u * n + e + 1u) == TopoResult::Cycle);
    CHECK(is_untouched());
    CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc2.get(), 3u * n + e) == TopoResult::ScratchTooSmall);
    CHECK(is_untouched());
  }
}

TEST_CASE("sizes: 65536 nodes and 1,048,576 edges, real buffers", "[max][toposort]") {
  const std::uint32_t n = kMaxNodes;
  const std::uint32_t e = kMaxEdges;
  std::uint32_t need = 0;
  REQUIRE(toposort_scratch_words(n, e, &need) == TopoResult::Ok);
  REQUIRE(need == 3u * n + e + 1u);
  std::vector<std::uint32_t> ft(2u * e);
  // the chain i -> i + 1 first, then pseudo-random forward edges a -> b with a < b: the order is 0 .. n - 1
  for (std::uint32_t k = 0; k + 1u < n; ++k) {
    ft[2u * k] = k;
    ft[2u * k + 1u] = k + 1u;
  }
  Lcg g(2024);
  for (std::uint32_t k = n - 1u; k < e; ++k) {
    const std::uint32_t a = g.next() % (n - 1u);
    const std::uint32_t b = a + 1u + g.next() % (n - 1u - a);
    ft[2u * k] = a;
    ft[2u * k + 1u] = b;
  }
  std::unique_ptr<std::uint32_t[]> sc(new std::uint32_t[need]);
  std::unique_ptr<std::uint32_t[]> ord(new std::uint32_t[n + kPad]);
  for (std::uint32_t i = 0; i < n + kPad; ++i) ord[i] = kSentinel;
  CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc.get(), need) == TopoResult::Ok);
  bool ok = true;
  for (std::uint32_t i = 0; i < n; ++i) ok = ok && ord[i] == i;
  CHECK(ok);
  for (std::uint32_t i = n; i < n + kPad; ++i) CHECK(ord[i] == kSentinel);
  // the last edge replaced by (n - 1) -> 0, which closes the chain into a cycle
  for (std::uint32_t i = 0; i < n + kPad; ++i) ord[i] = kSentinel;
  ft[2u * (e - 1u)] = n - 1u;
  ft[2u * (e - 1u) + 1u] = 0u;
  CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc.get(), need) == TopoResult::Cycle);
  bool untouched = true;
  for (std::uint32_t i = 0; i < n + kPad; ++i) untouched = untouched && ord[i] == kSentinel;
  CHECK(untouched);
  // one word less scratch
  CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc.get(), need - 1u) == TopoResult::ScratchTooSmall);
  // a bad endpoint in the very last word
  ft[2u * e - 1u] = n;
  CHECK(toposort_u32(n, ft.data(), e, ord.get(), sc.get(), need) == TopoResult::BadEdge);
  untouched = true;
  for (std::uint32_t i = 0; i < n + kPad; ++i) untouched = untouched && ord[i] == kSentinel;
  CHECK(untouched);
}

TEST_CASE("toposort_u32: a null order or scratch with 1 node is a NullArgument", "[args][toposort]") {
  std::uint32_t ord[1] = {kSentinel};
  std::uint32_t sc[4] = {kGarbage, kGarbage, kGarbage, kGarbage};
  CHECK(toposort_u32(1, nullptr, 0, nullptr, sc, 4) == TopoResult::NullArgument);
  CHECK(toposort_u32(1, nullptr, 0, ord, nullptr, 4) == TopoResult::NullArgument);
  CHECK(toposort_u32(1, nullptr, 0, ord, sc, 4) == TopoResult::Ok);
  CHECK(ord[0] == 0u);
}
