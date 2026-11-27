// Tests for core/treeq. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/treeq_ref.py, an independent Python implementation (ancestor
//      walking, all-pairs distances by Floyd-Warshall for small forests, adjacency and BFS for large ones; the script cross-checks the two). The tests
//      re-run every scenario against the real functions and recompute the 64-bit digest and the case counts.
//  (2) ORACLE inside the test, sharing no code with core/treeq/treeq.cpp: children lists and BFS for depth and size, ancestor stamping for the lca,
//      all-pairs distances (Floyd-Warshall) for the diameter of forests up to 10 nodes and a double-sweep BFS for the large ones. Every forest
//      is compared value by value with it, and the oracle is checked against the digests too.
//  (3) CONSTANTS: the named invalid arrays, the check-order matrix, the hand diameters and the exact sizes of the roots are written from the header.
// Output buffers carry 8 sentinel entries before and 16 after; on success nothing outside the stated range may change, on failure nothing may
// change at all. Scratch is 3 * n + 1 words and its contents are never examined.
// Extents: every parent array on up to 6 nodes is EXHAUSTIVE (valid and invalid together); the arrays on 7..10 nodes are fixed-seed SAMPLES; the
// large forests are the listed patterns and fixed-seed random forests (SAMPLED). The lca worst case is not run in the suite (see docs/treeq.md).
// What is NOT done: no timing test; Windows is CI-only; nothing asserted about scratch contents or about which error comes first inside one class.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "treeq.h"

using lumora::treeq::TreeResult;
using lumora::treeq::kMaxNodes;
using lumora::treeq::kMaxQueries;
using lumora::treeq::kNoParent;
using lumora::treeq::treeq_depth_size;
using lumora::treeq::treeq_diameter;
using lumora::treeq::treeq_lca;
using lumora::treeq::treeq_scratch_words;

namespace {

using Par = std::vector<std::uint32_t>;
constexpr std::uint32_t kSent = 0xC3C3C3C3u;
constexpr std::size_t kGb = 8, kGa = 16;

// BEGIN GENERATED REFERENCE (tools/treeq_ref.py)
const std::uint64_t kRefDigest_all_0 = 0x08328807B4EB6FEDULL;
const std::size_t kRefCases_all_0 = 1;
const std::uint64_t kRefDigest_all_1 = 0x202DF8E37F0DA6DEULL;
const std::size_t kRefCases_all_1 = 2;
const std::uint64_t kRefDigest_all_2 = 0x6D4EACE8D178BEDCULL;
const std::size_t kRefCases_all_2 = 9;
const std::uint64_t kRefDigest_all_3 = 0x3D5B87AEDE3878A3ULL;
const std::size_t kRefCases_all_3 = 64;
const std::uint64_t kRefDigest_all_4 = 0xA9335A5FC98522DFULL;
const std::size_t kRefCases_all_4 = 625;
const std::uint64_t kRefDigest_all_5 = 0x0AF026078B541C35ULL;
const std::size_t kRefCases_all_5 = 7776;
const std::uint64_t kRefDigest_all_6 = 0x379B4D65C98574B2ULL;
const std::size_t kRefCases_all_6 = 117649;
const std::uint64_t kRefDigest_big_all_roots = 0x304F1A05EF556668ULL;
const std::size_t kRefCases_big_all_roots = 1;
const std::uint64_t kRefDigest_big_binary = 0x5B266063B58F7480ULL;
const std::size_t kRefCases_big_binary = 1;
const std::uint64_t kRefDigest_big_binary_65535 = 0x3D6B4CAE460B06C5ULL;
const std::size_t kRefCases_big_binary_65535 = 1;
const std::uint64_t kRefDigest_big_binary_rev = 0x0200A90765E27CA0ULL;
const std::size_t kRefCases_big_binary_rev = 1;
const std::uint64_t kRefDigest_big_caterpillar = 0xC6F06CFBD7550678ULL;
const std::size_t kRefCases_big_caterpillar = 1;
const std::uint64_t kRefDigest_big_path_dec = 0x24BDFBC3B04FF8D0ULL;
const std::size_t kRefCases_big_path_dec = 1;
const std::uint64_t kRefDigest_big_path_inc = 0xB9AADC72D39A7F40ULL;
const std::size_t kRefCases_big_path_inc = 1;
const std::uint64_t kRefDigest_big_path_inc_2 = 0x161A3A491BD55BA6ULL;
const std::size_t kRefCases_big_path_inc_2 = 1;
const std::uint64_t kRefDigest_big_path_inc_8191 = 0x6EE9BCFA6523FBA1ULL;
const std::size_t kRefCases_big_path_inc_8191 = 1;
const std::uint64_t kRefDigest_big_path_small = 0xA2293C3D0B61BCD2ULL;
const std::size_t kRefCases_big_path_small = 1;
const std::uint64_t kRefDigest_big_random_1000 = 0x7D5628482B97BFF1ULL;
const std::size_t kRefCases_big_random_1000 = 1;
const std::uint64_t kRefDigest_big_random_40000 = 0xE1ADEFAB280FB193ULL;
const std::size_t kRefCases_big_random_40000 = 1;
const std::uint64_t kRefDigest_big_random_65536 = 0x18E882F46924E0B2ULL;
const std::size_t kRefCases_big_random_65536 = 1;
const std::uint64_t kRefDigest_big_star0 = 0x7F1B6526C89C7753ULL;
const std::size_t kRefCases_big_star0 = 1;
const std::uint64_t kRefDigest_big_star0_65535 = 0xA5543D48B2B30382ULL;
const std::size_t kRefCases_big_star0_65535 = 1;
const std::uint64_t kRefDigest_big_star_last = 0x2694649CC321EE19ULL;
const std::size_t kRefCases_big_star_last = 1;
const std::uint64_t kRefDigest_big_two_paths = 0x068965CBF32C40C7ULL;
const std::size_t kRefCases_big_two_paths = 1;
const std::uint64_t kRefDigest_rf_10 = 0x36EDA94F0CF34940ULL;
const std::size_t kRefCases_rf_10 = 2000;
const std::uint64_t kRefDigest_rf_7 = 0xFDF7C91B1E4DA7F8ULL;
const std::size_t kRefCases_rf_7 = 2000;
const std::uint64_t kRefDigest_rf_8 = 0x5BA5F08B9F7A8179ULL;
const std::size_t kRefCases_rf_8 = 2000;
const std::uint64_t kRefDigest_rf_9 = 0xEF1CD10A4F72A950ULL;
const std::size_t kRefCases_rf_9 = 2000;
const std::uint64_t kRefDigest_rnd_10 = 0x2E01AC71E1CCC5B4ULL;
const std::size_t kRefCases_rnd_10 = 2000;
const std::uint64_t kRefDigest_rnd_7 = 0xCB6C3C5950D484A3ULL;
const std::size_t kRefCases_rnd_7 = 2000;
const std::uint64_t kRefDigest_rnd_8 = 0xFD0E51644C77553DULL;
const std::size_t kRefCases_rnd_8 = 2000;
const std::uint64_t kRefDigest_rnd_9 = 0x2633EAB460CD064FULL;
const std::size_t kRefCases_rnd_9 = 2000;
// END GENERATED REFERENCE

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  void add(std::uint64_t v) { h = (h ^ v) * 0x100000001B3ull; }
};

struct Tally {
  std::size_t bad = 0;
  std::string first;
  void fail(const std::string& s) {
    if (bad == 0) first = s;
    ++bad;
  }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

// ---------------------------------------------------------------- the oracle
bool oracle_valid(const Par& par) {
  const std::size_t n = par.size();
  for (std::size_t v = 0; v < n; ++v)
    if (par[v] != kNoParent && (par[v] >= n || par[v] == v)) return false;
  for (std::size_t v = 0; v < n; ++v) {
    std::size_t x = v, steps = 0;
    while (par[x] != kNoParent) {
      x = par[x];
      if (++steps > n) return false;
    }
  }
  return true;
}

struct Truth {
  Par depth, size, lca;
  std::uint32_t len = 0, a = 0, b = 0;
};

// BFS from s over the undirected adjacency. d must hold kNoParent everywhere on entry; afterwards d is set for the nodes in queue and the caller resets
// it with bfs_reset (so a forest of many small trees does not pay for an n-sized array per tree).
void bfs_dist(const std::vector<std::uint32_t>& start, const std::vector<std::uint32_t>& list, std::uint32_t s, std::vector<std::uint32_t>& queue,
              std::vector<std::uint32_t>& d) {
  queue.clear();
  d[s] = 0;
  queue.push_back(s);
  for (std::size_t i = 0; i < queue.size(); ++i) {
    const std::uint32_t x = queue[i];
    for (std::uint32_t k = start[x]; k < start[x + 1]; ++k) {
      const std::uint32_t y = list[k];
      if (d[y] == kNoParent) {
        d[y] = d[x] + 1;
        queue.push_back(y);
      }
    }
  }
}

void bfs_reset(const std::vector<std::uint32_t>& queue, std::vector<std::uint32_t>& d) {
  for (std::uint32_t x : queue) d[x] = kNoParent;
}

// qa/qb: the queries. Diameter: Floyd-Warshall up to 10 nodes, double-sweep BFS beyond.
Truth truth(const Par& par, const Par& qa, const Par& qb) {
  const std::size_t n = par.size();
  Truth t;
  t.depth.assign(n, 0);
  t.size.assign(n, 1);
  std::vector<std::uint32_t> kstart(n + 2, 0), klist(n, 0);
  for (std::size_t v = 0; v < n; ++v)
    if (par[v] != kNoParent) ++kstart[par[v] + 1];
  for (std::size_t i = 0; i < n; ++i) kstart[i + 1] += kstart[i];
  {
    std::vector<std::uint32_t> fill(kstart.begin(), kstart.begin() + static_cast<std::ptrdiff_t>(n));
    for (std::size_t v = 0; v < n; ++v)
      if (par[v] != kNoParent) klist[fill[par[v]]++] = static_cast<std::uint32_t>(v);
  }
  std::vector<std::uint32_t> order;
  for (std::size_t v = 0; v < n; ++v)
    if (par[v] == kNoParent) order.push_back(static_cast<std::uint32_t>(v));
  for (std::size_t i = 0; i < order.size(); ++i) {
    const std::uint32_t x = order[i];
    for (std::uint32_t k = kstart[x]; k < kstart[x + 1]; ++k) {
      t.depth[klist[k]] = t.depth[x] + 1;
      order.push_back(klist[k]);
    }
  }
  for (std::size_t i = order.size(); i > 0; --i) {
    const std::uint32_t x = order[i - 1];
    if (par[x] != kNoParent) t.size[par[x]] += t.size[x];
  }
  std::vector<std::uint32_t> stamp(n, kNoParent);
  for (std::size_t i = 0; i < qa.size(); ++i) {
    for (std::uint32_t x = qa[i];; x = par[x]) {
      stamp[x] = static_cast<std::uint32_t>(i);
      if (par[x] == kNoParent) break;
    }
    std::uint32_t r = kNoParent;
    for (std::uint32_t y = qb[i];; y = par[y]) {
      if (stamp[y] == i) {
        r = y;
        break;
      }
      if (par[y] == kNoParent) break;
    }
    t.lca.push_back(r);
  }
  t.len = t.a = t.b = 0;
  if (n == 0) return t;
  if (n <= 10) {
    const std::uint32_t inf = 1u << 20;
    std::vector<std::uint32_t> d(n * n, inf);
    for (std::size_t v = 0; v < n; ++v) {
      d[v * n + v] = 0;
      if (par[v] != kNoParent) d[v * n + par[v]] = d[par[v] * n + v] = 1;
    }
    for (std::size_t k = 0; k < n; ++k)
      for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j)
          if (d[i * n + k] + d[k * n + j] < d[i * n + j]) d[i * n + j] = d[i * n + k] + d[k * n + j];
    for (std::size_t a = 0; a < n; ++a)
      for (std::size_t b = a; b < n; ++b)
        if (d[a * n + b] < inf && d[a * n + b] > t.len) {
          t.len = d[a * n + b];
          t.a = static_cast<std::uint32_t>(a);
          t.b = static_cast<std::uint32_t>(b);
        }
    return t;
  }
  // undirected adjacency
  std::vector<std::uint32_t> astart(n + 1, 0), alist(2 * n, 0), fill2;
  for (std::size_t v = 0; v < n; ++v)
    if (par[v] != kNoParent) {
      ++astart[v + 1];
      ++astart[par[v] + 1];
    }
  for (std::size_t i = 0; i < n; ++i) astart[i + 1] += astart[i];
  fill2.assign(astart.begin(), astart.begin() + static_cast<std::ptrdiff_t>(n));
  for (std::size_t v = 0; v < n; ++v)
    if (par[v] != kNoParent) {
      alist[fill2[v]++] = par[v];
      alist[fill2[par[v]]++] = static_cast<std::uint32_t>(v);
    }
  std::vector<std::uint32_t> ecc(n, 0), q, d0(n, kNoParent), dx(n, kNoParent), dy(n, kNoParent);
  std::vector<bool> done(n, false);
  for (std::size_t r = 0; r < n; ++r) {
    if (done[r]) continue;
    bfs_dist(astart, alist, static_cast<std::uint32_t>(r), q, d0);
    const std::vector<std::uint32_t> members = q;
    std::uint32_t x = static_cast<std::uint32_t>(r);
    for (std::uint32_t v : members) {
      done[v] = true;
      if (d0[v] > d0[x] || (d0[v] == d0[x] && v < x)) x = v;
    }
    bfs_dist(astart, alist, x, q, dx);
    std::uint32_t y = x;
    for (std::uint32_t v : members)
      if (dx[v] > dx[y] || (dx[v] == dx[y] && v < y)) y = v;
    bfs_dist(astart, alist, y, q, dy);
    for (std::uint32_t v : members) ecc[v] = dx[v] > dy[v] ? dx[v] : dy[v];
    bfs_reset(members, d0);
    bfs_reset(members, dx);
    bfs_reset(members, dy);
  }
  std::uint32_t big = 0;
  for (std::size_t v = 0; v < n; ++v)
    if (ecc[v] > big) big = ecc[v];
  t.len = big;
  if (big == 0) return t;
  for (std::size_t v = 0; v < n; ++v)
    if (ecc[v] == big) {
      t.a = static_cast<std::uint32_t>(v);
      break;
    }
  bfs_dist(astart, alist, t.a, q, d0);
  for (std::size_t v = 0; v < n; ++v)
    if (d0[v] == big) {
      t.b = static_cast<std::uint32_t>(v);
      break;
    }
  return t;
}

// ------------------------------------------------------------- running the implementation
struct Guarded {
  std::vector<std::uint32_t> buf;
  Guarded(std::size_t n) : buf(kGb + n + kGa, kSent) {}
  std::uint32_t* p() { return buf.data() + kGb; }
  bool guards_ok(std::size_t n) const {
    for (std::size_t i = 0; i < kGb; ++i)
      if (buf[i] != kSent) return false;
    for (std::size_t i = 0; i < kGa; ++i)
      if (buf[kGb + n + i] != kSent) return false;
    return true;
  }
  bool untouched() const {
    for (std::uint32_t v : buf)
      if (v != kSent) return false;
    return true;
  }
};

// Runs all three functions on one forest with the given queries; checks against the oracle and folds like the reference.
void check_forest(const Par& par, const Par& qa, const Par& qb, Fold& f, Tally& t) {
  const std::size_t n = par.size();
  const std::string tag = "n=" + std::to_string(n);
  Par scratch(3 * n + 1, 0xDEADBEEFu);
  Guarded dep(n), siz(n), lc(qa.size());
  std::uint32_t len = kSent, a = kSent, b = kSent;
  const std::uint32_t* pp = n ? par.data() : nullptr;
  const std::uint32_t* qap = qa.empty() ? nullptr : qa.data();
  const std::uint32_t* qbp = qb.empty() ? nullptr : qb.data();
  const std::uint32_t sw = static_cast<std::uint32_t>(3 * n + 1);
  const TreeResult r1 = treeq_depth_size(static_cast<std::uint32_t>(n), pp, scratch.data(), sw, dep.p(), siz.p());
  if (!oracle_valid(par)) {
    f.add(4);
    ++f.cases;
    const TreeResult r2 = treeq_diameter(static_cast<std::uint32_t>(n), pp, scratch.data(), sw, &len, &a, &b);
    const TreeResult r3 = treeq_lca(static_cast<std::uint32_t>(n), pp, scratch.data(), sw, static_cast<std::uint32_t>(qa.size()), qap, qbp, lc.p());
    if (r1 != TreeResult::BadParent || r2 != TreeResult::BadParent || r3 != TreeResult::BadParent) t.fail(tag + ": invalid array not BadParent");
    if (!dep.untouched() || !siz.untouched() || !lc.untouched() || len != kSent || a != kSent || b != kSent) t.fail(tag + ": output written on failure");
    return;
  }
  const Truth tr = truth(par, qa, qb);
  const TreeResult r2 = treeq_diameter(static_cast<std::uint32_t>(n), pp, scratch.data(), sw, &len, &a, &b);
  const TreeResult r3 = treeq_lca(static_cast<std::uint32_t>(n), pp, scratch.data(), sw, static_cast<std::uint32_t>(qa.size()), qap, qbp, lc.p());
  if (r1 != TreeResult::Ok || r2 != TreeResult::Ok || r3 != TreeResult::Ok) {
    t.fail(tag + ": valid forest not Ok");
    return;
  }
  if (!dep.guards_ok(n) || !siz.guards_ok(n) || !lc.guards_ok(qa.size())) t.fail(tag + ": guard entry written");
  f.add(0);
  for (std::size_t v = 0; v < n; ++v) {
    if (dep.p()[v] != tr.depth[v]) t.fail(tag + ": depth differs");
    f.add(dep.p()[v]);
  }
  for (std::size_t v = 0; v < n; ++v) {
    if (siz.p()[v] != tr.size[v]) t.fail(tag + ": size differs");
    f.add(siz.p()[v]);
  }
  if (len != tr.len || (n > 0 && (a != tr.a || b != tr.b))) t.fail(tag + ": diameter differs");
  if (n == 0 && (a != kSent || b != kSent)) t.fail(tag + ": out_a or out_b written for n = 0");
  f.add(len);
  if (n > 0) {
    f.add(a);
    f.add(b);
  }
  for (std::size_t i = 0; i < qa.size(); ++i) {
    if (lc.p()[i] != tr.lca[i]) t.fail(tag + ": lca differs");
    f.add(lc.p()[i]);
  }
  ++f.cases;
}

void all_pairs(std::size_t n, Par& qa, Par& qb) {
  qa.clear();
  qb.clear();
  for (std::uint32_t u = 0; u < n; ++u)
    for (std::uint32_t v = 0; v < n; ++v) {
      qa.push_back(u);
      qb.push_back(v);
    }
}

Par rnd_forest(Lcg& rng, std::size_t n) {
  Par order(n);
  for (std::size_t i = 0; i < n; ++i) order[i] = static_cast<std::uint32_t>(i);
  for (std::size_t i = n; i-- > 1;) {
    const std::size_t j = rng.next() % (i + 1);
    const std::uint32_t tmp = order[i];
    order[i] = order[j];
    order[j] = tmp;
  }
  Par par(n, kNoParent);
  for (std::size_t i = 0; i < n; ++i) {
    if (i == 0 || rng.next() % 4 == 0) par[order[i]] = kNoParent;
    else par[order[i]] = order[rng.next() % i];
  }
  return par;
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.h == digest);
}

// The large patterns (the same as tools/treeq_ref.py).
Par pat(const std::string& name, std::size_t n) {
  Par p(n, kNoParent);
  const std::uint32_t N = static_cast<std::uint32_t>(n);
  if (name == "path_inc") {
    for (std::uint32_t i = 1; i < N; ++i) p[i] = i - 1;
  } else if (name == "path_dec") {
    for (std::uint32_t i = 0; i + 1 < N; ++i) p[i] = i + 1;
  } else if (name == "star0") {
    for (std::uint32_t i = 1; i < N; ++i) p[i] = 0;
  } else if (name == "star_last") {
    for (std::uint32_t i = 0; i + 1 < N; ++i) p[i] = N - 1;
  } else if (name == "binary") {
    for (std::uint32_t i = 1; i < N; ++i) p[i] = (i - 1) / 2;
  } else if (name == "binary_rev") {
    for (std::uint32_t i = 1; i < N; ++i) p[N - 1 - i] = N - 1 - (i - 1) / 2;
  } else if (name == "caterpillar") {
    const std::uint32_t s = N / 2;
    for (std::uint32_t i = 1; i < s; ++i) p[i] = i - 1;
    for (std::uint32_t j = 0; j < N - s; ++j) p[s + j] = j;
  } else if (name == "two_paths") {
    const std::uint32_t h = N / 2;
    for (std::uint32_t i = 0; i < N; ++i) p[i] = (i == 0 || i == h) ? kNoParent : i - 1;
  } else if (name == "path_small") {
    const std::uint32_t h = N / 2;
    for (std::uint32_t i = 1; i < h; ++i) p[i] = i - 1;
    std::uint32_t i = h;
    while (i < N) {
      p[i] = kNoParent;
      for (std::uint32_t k = 1; k < 3; ++k)
        if (i + k < N) p[i + k] = i;
      i += 3;
    }
  }  // all_roots: every entry stays kNoParent
  return p;
}

void queries(std::size_t n, std::size_t q, std::uint64_t seed, Par& qa, Par& qb) {
  const std::uint32_t N = static_cast<std::uint32_t>(n);
  qa.assign({0, N - 1, N - 1});
  qb.assign({N - 1, 0, N - 1});
  Lcg rng(seed);
  while (qa.size() < q) {
    const std::uint32_t u = rng.next() % N;
    const std::uint32_t v = rng.next() % N;
    qa.push_back(u);
    qb.push_back(v);
  }
  qa.resize(q);
  qb.resize(q);
}

struct BigCase {
  const char* name;      // the reference name (digest group)
  const char* pattern;   // empty: a random forest
  std::size_t n;
  std::size_t q;
  std::uint64_t seed;    // the random forest seed
  std::uint64_t digest;
  std::size_t cases;
};

}  // namespace

// Every parent array on n = 0..6 nodes: entries kNoParent or 0..n-1, so invalid arrays (self parents, cycles) are included. Exhaustive ONLY up to 6 nodes.
TEST_CASE("every parent array on up to 6 nodes (valid and invalid), exhaustive only up to 6 nodes", "[treeq]") {
  const std::uint64_t digests[7] = {kRefDigest_all_0, kRefDigest_all_1, kRefDigest_all_2, kRefDigest_all_3, kRefDigest_all_4, kRefDigest_all_5, kRefDigest_all_6};
  const std::size_t cases[7] = {kRefCases_all_0, kRefCases_all_1, kRefCases_all_2, kRefCases_all_3, kRefCases_all_4, kRefCases_all_5, kRefCases_all_6};
  for (std::size_t n = 0; n <= 6; ++n) {
    Fold f;
    Tally t;
    std::size_t total = 1;
    for (std::size_t i = 0; i < n; ++i) total *= n + 1;
    Par qa, qb;
    all_pairs(n, qa, qb);
    for (std::size_t c = 0; c < total; ++c) {
      Par par(n);
      std::size_t x = c;
      for (std::size_t i = 0; i < n; ++i) {
        const std::size_t e = x % (n + 1);
        x /= n + 1;
        par[i] = e == n ? kNoParent : static_cast<std::uint32_t>(e);
      }
      check_forest(par, qa, qb, f, t);
    }
    INFO("n = " << n);
    expect_digest(f, t, cases[n], digests[n]);
    CHECK(f.cases == total);
  }
}

// Fixed-seed SAMPLES on 7..10 nodes: 2000 uniform random arrays and 2000 random valid forests per size (not exhaustive).
TEST_CASE("fixed-seed samples on 7..10 nodes: random arrays and random forests (sampled, not exhaustive)", "[treeq]") {
  const std::uint64_t dr[4] = {kRefDigest_rnd_7, kRefDigest_rnd_8, kRefDigest_rnd_9, kRefDigest_rnd_10};
  const std::uint64_t df[4] = {kRefDigest_rf_7, kRefDigest_rf_8, kRefDigest_rf_9, kRefDigest_rf_10};
  const std::size_t cr[4] = {kRefCases_rnd_7, kRefCases_rnd_8, kRefCases_rnd_9, kRefCases_rnd_10};
  const std::size_t cf[4] = {kRefCases_rf_7, kRefCases_rf_8, kRefCases_rf_9, kRefCases_rf_10};
  for (std::size_t n = 7; n <= 10; ++n) {
    Par qa, qb;
    all_pairs(n, qa, qb);
    Fold f1, f2;
    Tally t1, t2;
    Lcg r1(1000 + n);
    for (int i = 0; i < 2000; ++i) {
      Par par(n);
      for (std::size_t k = 0; k < n; ++k) {
        const std::uint32_t e = r1.next() % static_cast<std::uint32_t>(n + 1);
        par[k] = e == n ? kNoParent : e;
      }
      check_forest(par, qa, qb, f1, t1);
    }
    Lcg r2(2000 + n);
    for (int i = 0; i < 2000; ++i) check_forest(rnd_forest(r2, n), qa, qb, f2, t2);
    INFO("n = " << n);
    expect_digest(f1, t1, cr[n - 7], dr[n - 7]);
    expect_digest(f2, t2, cf[n - 7], df[n - 7]);
  }
}

TEST_CASE("large forests: 65536-node patterns and fixed-seed random forests (listed patterns, random ones sampled)", "[treeq]") {
  const BigCase cases[] = {
      {"path_inc", "path_inc", 8192, 128, 0, kRefDigest_big_path_inc, kRefCases_big_path_inc},
      {"path_dec", "path_dec", 8192, 128, 0, kRefDigest_big_path_dec, kRefCases_big_path_dec},
      {"star0", "star0", 65536, 65536, 0, kRefDigest_big_star0, kRefCases_big_star0},
      {"star_last", "star_last", 65536, 65536, 0, kRefDigest_big_star_last, kRefCases_big_star_last},
      {"binary", "binary", 65536, 65536, 0, kRefDigest_big_binary, kRefCases_big_binary},
      {"binary_rev", "binary_rev", 65536, 65536, 0, kRefDigest_big_binary_rev, kRefCases_big_binary_rev},
      {"caterpillar", "caterpillar", 8192, 128, 0, kRefDigest_big_caterpillar, kRefCases_big_caterpillar},
      {"all_roots", "all_roots", 65536, 65536, 0, kRefDigest_big_all_roots, kRefCases_big_all_roots},
      {"two_paths", "two_paths", 8192, 128, 0, kRefDigest_big_two_paths, kRefCases_big_two_paths},
      {"path_small", "path_small", 8192, 128, 0, kRefDigest_big_path_small, kRefCases_big_path_small},
      {"random_65536", "", 65536, 65536, 7001, kRefDigest_big_random_65536, kRefCases_big_random_65536},
      {"random_40000", "", 40000, 65536, 7002, kRefDigest_big_random_40000, kRefCases_big_random_40000},
      {"random_1000", "", 1000, 65536, 7003, kRefDigest_big_random_1000, kRefCases_big_random_1000},
      {"path_inc_8191", "path_inc", 8191, 128, 0, kRefDigest_big_path_inc_8191, kRefCases_big_path_inc_8191},
      {"star0_65535", "star0", 65535, 65536, 0, kRefDigest_big_star0_65535, kRefCases_big_star0_65535},
      {"path_inc_2", "path_inc", 2, 128, 0, kRefDigest_big_path_inc_2, kRefCases_big_path_inc_2},
      {"binary_65535", "binary", 65535, 65536, 0, kRefDigest_big_binary_65535, kRefCases_big_binary_65535},
  };
  for (const BigCase& c : cases) {
    const Par par = c.pattern[0] ? pat(c.pattern, c.n) : [&] { Lcg r(c.seed); return rnd_forest(r, c.n); }();
    Par qa, qb;
    queries(c.n, c.q, 9000 + std::string(c.name).size(), qa, qb);
    Fold f;
    Tally t;
    check_forest(par, qa, qb, f, t);
    INFO(c.name);
    expect_digest(f, t, c.cases, c.digest);
  }
}

// The exact size of a root at the largest sizes (a size of 65536 does not fit 16 bits), and depths of the chain.
TEST_CASE("exact root sizes and depths at 65536 nodes: chain, star, balanced binary", "[treeq]") {
  const char* names[3] = {"path_inc", "star0", "binary"};
  for (const char* name : names) {
    const Par par = pat(name, 65536);
    Par scratch(3 * 65536 + 1), depth(65536), size(65536);
    REQUIRE(treeq_depth_size(65536, par.data(), scratch.data(), 3 * 65536 + 1, depth.data(), size.data()) == TreeResult::Ok);
    INFO(name);
    CHECK(size[0] == 65536u);
    CHECK(depth[0] == 0u);
  }
  const Par chain = pat("path_inc", 65536), rev = pat("path_dec", 65536);
  Par scratch(3 * 65536 + 1), depth(65536), size(65536);
  REQUIRE(treeq_depth_size(65536, chain.data(), scratch.data(), 3 * 65536 + 1, depth.data(), size.data()) == TreeResult::Ok);
  CHECK(depth[65535] == 65535u);
  CHECK(size[65535] == 1u);
  CHECK(size[1] == 65535u);
  REQUIRE(treeq_depth_size(65536, rev.data(), scratch.data(), 3 * 65536 + 1, depth.data(), size.data()) == TreeResult::Ok);
  CHECK(size[65535] == 65536u);
  CHECK(depth[0] == 65535u);
  std::uint32_t len = 0, a = 0, b = 0;
  REQUIRE(treeq_diameter(65536, chain.data(), scratch.data(), 3 * 65536 + 1, &len, &a, &b) == TreeResult::Ok);
  CHECK(len == 65535u);
  CHECK(a == 0u);
  CHECK(b == 65535u);
}

TEST_CASE("named invalid arrays: self parent, 2-cycle, 3-cycle, cycle without node 0, tree hanging on a cycle, out-of-range parents", "[treeq]") {
  const std::uint32_t N = kNoParent;
  const Par cases[] = {
      {0},                    // self parent
      {N, 1},                 // self parent of node 1
      {1, 0},                 // 2-cycle
      {N, 2, 1},              // 2-cycle that does not contain node 0
      {1, 2, 0},              // 3-cycle
      {N, 0, 3, 4, 2},        // 3-cycle without node 0, next to a valid tree
      {N, 0, 3, 2, 3},        // 2-cycle (2, 3) with node 4 hanging on it
      {N, 0, 2, 2, 4, 5, 6},  // self parent in the middle
      {2, N, 4, 5, 3, N},     // cycle (3, 4, 5)? 3 -> 5 -> N is fine: valid tree edges below
      {1, 2, 3, 4, 5, 0},     // 6-cycle
      {2, 3},                 // parent value equal to n
      {3, N, N},              // parent value above n
      {0xFFFFFFFEu, N},       // parent value just below kNoParent
      {N, 0x80000000u},       // large parent value
  };
  const bool expect_valid[] = {false, false, false, false, false, false, false, false, true, false, false, false, false, false};
  std::size_t idx = 0;
  for (const Par& par : cases) {
    Par qa, qb;
    all_pairs(par.size(), qa, qb);
    Fold f;
    Tally t;
    check_forest(par, qa, qb, f, t);
    INFO("case " << idx);
    CHECK(t.bad == 0);
    CHECK(oracle_valid(par) == expect_valid[idx]);
    ++idx;
  }
}

TEST_CASE("hand diameters: tie rule, ties across trees, forests of single nodes, n = 0", "[treeq]") {
  const std::uint32_t N = kNoParent;
  struct Row { Par par; std::uint32_t len, a, b; };
  const Row rows[] = {
      {{N, 0, 0}, 2, 1, 2},                  // a star: the leaves 1 and 2
      {{N, 0, 0, N, 3, 3}, 2, 1, 2},         // two equal trees: the smaller pair wins
      {{N, N, 1, 0, 1}, 2, 2, 4},            // the longer tree is the second one
      {{N, N, N}, 0, 0, 0},                  // only single nodes
      {{N}, 0, 0, 0},
      {{3, 3, 3, N}, 2, 0, 1},               // star centred at the last node
      {{N, 0, 1, 2}, 3, 0, 3},               // a path
      {{1, 2, 3, N}, 3, 0, 3},               // the same path, reversed labels
      {{N, 0, 1, 1, 0}, 3, 2, 4},            // two paths of length 3 share nodes: (2,4) and (3,4): the smaller is (2, 4)
  };
  for (const Row& r : rows) {
    Par scratch(3 * r.par.size() + 1);
    std::uint32_t len = kSent, a = kSent, b = kSent;
    REQUIRE(treeq_diameter(static_cast<std::uint32_t>(r.par.size()), r.par.data(), scratch.data(), static_cast<std::uint32_t>(scratch.size()), &len, &a, &b) ==
            TreeResult::Ok);
    CHECK(len == r.len);
    CHECK(a == r.a);
    CHECK(b == r.b);
  }
  // A root with two chains of L1 and L2 nodes (nodes 1..L1 and L1+1..L1+L2): the path joins the two chain ends, length L1 + L2, so both branch heights
  // of the root are above 255 (a second branch height must not be cut to 8 bits). The ends are the leaves L1 and L1 + L2.
  const std::uint32_t chains[3][2] = {{1000, 700}, {700, 1000}, {300, 300}};
  for (const auto& c : chains) {
    const std::uint32_t L1 = c[0], L2 = c[1], n = 1 + L1 + L2;
    Par par(n, kNoParent);
    for (std::uint32_t v = 1; v <= L1; ++v) par[v] = v - 1;
    par[L1 + 1] = 0;
    for (std::uint32_t v = L1 + 2; v < n; ++v) par[v] = v - 1;
    Par scratch(3 * n + 1);
    std::uint32_t l2 = kSent, a2 = kSent, b2 = kSent;
    REQUIRE(treeq_diameter(n, par.data(), scratch.data(), static_cast<std::uint32_t>(scratch.size()), &l2, &a2, &b2) == TreeResult::Ok);
    CHECK(l2 == L1 + L2);
    CHECK(a2 == L1);
    CHECK(b2 == L1 + L2);
  }
  std::uint32_t len = kSent;
  CHECK(treeq_diameter(0, nullptr, nullptr, 0, &len, nullptr, nullptr) == TreeResult::Ok);
  CHECK(len == 0u);
}

TEST_CASE("check order matrix: null arguments, TooLarge, ScratchTooSmall, BadParent, BadQuery; nothing written on failure", "[treeq]") {
  const Par ok = {kNoParent, 0, 0};
  const Par cyc = {1, 0, 0};
  Par scratch(10, 7u), small(10, 7u);
  Guarded d(3), s(3), o(3);
  std::uint32_t len = kSent, a = kSent, b = kSent;
  const Par qa = {0, 1, 2}, qb = {2, 1, 0}, bad = {0, 3, 2};
  const std::uint32_t sw = 10;
  // scratch_words
  std::uint32_t w = kSent;
  CHECK(treeq_scratch_words(0, nullptr) == TreeResult::NullArgument);
  CHECK(treeq_scratch_words(kMaxNodes + 1, nullptr) == TreeResult::NullArgument);
  CHECK(treeq_scratch_words(kMaxNodes + 1, &w) == TreeResult::TooLarge);
  CHECK(w == kSent);
  CHECK(treeq_scratch_words(0, &w) == TreeResult::Ok);
  CHECK(w == 1u);
  CHECK(treeq_scratch_words(3, &w) == TreeResult::Ok);
  CHECK(w == 10u);
  CHECK(treeq_scratch_words(kMaxNodes, &w) == TreeResult::Ok);
  CHECK(w == 3u * kMaxNodes + 1u);
  // depth_size
  CHECK(treeq_depth_size(3, nullptr, scratch.data(), sw, d.p(), s.p()) == TreeResult::NullArgument);
  CHECK(treeq_depth_size(3, ok.data(), nullptr, sw, d.p(), s.p()) == TreeResult::NullArgument);
  CHECK(treeq_depth_size(3, ok.data(), scratch.data(), sw, nullptr, s.p()) == TreeResult::NullArgument);
  CHECK(treeq_depth_size(3, ok.data(), scratch.data(), sw, d.p(), nullptr) == TreeResult::NullArgument);
  CHECK(treeq_depth_size(kMaxNodes + 1, nullptr, scratch.data(), sw, d.p(), s.p()) == TreeResult::NullArgument);
  CHECK(treeq_depth_size(kMaxNodes + 1, ok.data(), scratch.data(), sw, d.p(), s.p()) == TreeResult::TooLarge);
  CHECK(treeq_depth_size(0xFFFFFFFFu, ok.data(), scratch.data(), sw, d.p(), s.p()) == TreeResult::TooLarge);
  CHECK(treeq_depth_size(3, ok.data(), scratch.data(), 9, d.p(), s.p()) == TreeResult::ScratchTooSmall);
  CHECK(treeq_depth_size(3, ok.data(), scratch.data(), 0, d.p(), s.p()) == TreeResult::ScratchTooSmall);
  CHECK(treeq_depth_size(3, cyc.data(), scratch.data(), 9, d.p(), s.p()) == TreeResult::ScratchTooSmall);  // scratch is judged before BadParent
  CHECK(treeq_depth_size(3, cyc.data(), scratch.data(), sw, d.p(), s.p()) == TreeResult::BadParent);
  CHECK(d.untouched());
  CHECK(s.untouched());
  CHECK(treeq_depth_size(0, nullptr, nullptr, 0, nullptr, nullptr) == TreeResult::Ok);  // n = 0: nothing is needed
  // lca
  CHECK(treeq_lca(3, nullptr, scratch.data(), sw, 3, qa.data(), qb.data(), o.p()) == TreeResult::NullArgument);
  CHECK(treeq_lca(3, ok.data(), nullptr, sw, 3, qa.data(), qb.data(), o.p()) == TreeResult::NullArgument);
  CHECK(treeq_lca(3, ok.data(), scratch.data(), sw, 3, nullptr, qb.data(), o.p()) == TreeResult::NullArgument);
  CHECK(treeq_lca(3, ok.data(), scratch.data(), sw, 3, qa.data(), nullptr, o.p()) == TreeResult::NullArgument);
  CHECK(treeq_lca(3, ok.data(), scratch.data(), sw, 3, qa.data(), qb.data(), nullptr) == TreeResult::NullArgument);
  CHECK(treeq_lca(kMaxNodes + 1, nullptr, scratch.data(), sw, 3, qa.data(), qb.data(), o.p()) == TreeResult::NullArgument);
  CHECK(treeq_lca(3, ok.data(), scratch.data(), sw, kMaxQueries + 1, nullptr, qb.data(), o.p()) == TreeResult::NullArgument);
  CHECK(treeq_lca(kMaxNodes + 1, ok.data(), scratch.data(), sw, 3, qa.data(), qb.data(), o.p()) == TreeResult::TooLarge);
  CHECK(treeq_lca(3, ok.data(), scratch.data(), sw, kMaxQueries + 1, qa.data(), qb.data(), o.p()) == TreeResult::TooLarge);
  CHECK(treeq_lca(kMaxNodes + 1, ok.data(), scratch.data(), sw, kMaxQueries + 1, qa.data(), qb.data(), o.p()) == TreeResult::TooLarge);
  CHECK(treeq_lca(3, ok.data(), scratch.data(), 9, 3, qa.data(), qb.data(), o.p()) == TreeResult::ScratchTooSmall);
  CHECK(treeq_lca(3, cyc.data(), scratch.data(), sw, 3, bad.data(), qb.data(), o.p()) == TreeResult::BadParent);  // BadParent before BadQuery
  CHECK(treeq_lca(3, ok.data(), scratch.data(), sw, 3, bad.data(), qb.data(), o.p()) == TreeResult::BadQuery);
  CHECK(treeq_lca(3, ok.data(), scratch.data(), sw, 3, qa.data(), bad.data(), o.p()) == TreeResult::BadQuery);
  CHECK(o.untouched());
  CHECK(treeq_lca(0, nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr) == TreeResult::Ok);
  CHECK(treeq_lca(0, nullptr, nullptr, 0, 1, qa.data(), qb.data(), o.p()) == TreeResult::BadQuery);
  CHECK(treeq_lca(0, nullptr, nullptr, 0, 1, nullptr, qb.data(), o.p()) == TreeResult::NullArgument);
  CHECK(treeq_lca(3, ok.data(), scratch.data(), sw, 0, nullptr, nullptr, nullptr) == TreeResult::Ok);  // no queries: nothing to write
  CHECK(treeq_lca(3, cyc.data(), scratch.data(), sw, 0, nullptr, nullptr, nullptr) == TreeResult::BadParent);  // the array is checked even with no queries
  CHECK(o.untouched());
  // diameter
  CHECK(treeq_diameter(3, ok.data(), scratch.data(), sw, nullptr, &a, &b) == TreeResult::NullArgument);
  CHECK(treeq_diameter(3, nullptr, scratch.data(), sw, &len, &a, &b) == TreeResult::NullArgument);
  CHECK(treeq_diameter(3, ok.data(), nullptr, sw, &len, &a, &b) == TreeResult::NullArgument);
  CHECK(treeq_diameter(3, ok.data(), scratch.data(), sw, &len, nullptr, &b) == TreeResult::NullArgument);
  CHECK(treeq_diameter(3, ok.data(), scratch.data(), sw, &len, &a, nullptr) == TreeResult::NullArgument);
  CHECK(treeq_diameter(0, nullptr, nullptr, 0, nullptr, nullptr, nullptr) == TreeResult::NullArgument);  // out_len is always needed
  CHECK(treeq_diameter(kMaxNodes + 1, ok.data(), scratch.data(), sw, &len, &a, &b) == TreeResult::TooLarge);
  CHECK(treeq_diameter(3, ok.data(), scratch.data(), 9, &len, &a, &b) == TreeResult::ScratchTooSmall);
  CHECK(treeq_diameter(3, cyc.data(), scratch.data(), sw, &len, &a, &b) == TreeResult::BadParent);
  CHECK(len == kSent);
  CHECK(a == kSent);
  CHECK(b == kSent);
  CHECK(treeq_diameter(0, nullptr, nullptr, 0, &len, nullptr, nullptr) == TreeResult::Ok);
  CHECK(len == 0u);
  CHECK(a == kSent);
  CHECK(b == kSent);
}
