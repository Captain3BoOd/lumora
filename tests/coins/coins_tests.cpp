// Tests for core/coins. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/coins_ref.py, an independent Python program (the definitions:
//      breadth-first search, enumeration of multiplicity vectors and of bit masks, exact integers; and a second formulation by residue classes with
//      prefix sums and prefix minima, compared with the definitions by the script). The tests re-run every case against the real functions and
//      recompute the 64-bit digest (m, the coins, n and the three tables) and the number of cases.
//  (2) ORACLE inside the test, sharing no code with core/coins/coins.cpp: amount by amount minimum over the coins, and two-dimensional tables over
//      (number of coin kinds used so far, amount) for the multisets and the subsets, in u64 with a reduction after every sum.
//  (3) CONSTANTS: the hand rows, the closed forms at n = 2^20 (64 coins of value 1: ways are C(v + 63, 63), subsets C(64, v); the 20 powers of
//      two; the lists {1, 1}, {}, {0xFFFFFFFF}, {2^20}, {2^20 + 1}) and the check-order matrix, written from the header.
// Output buffers carry sentinels: 8 entries before and 16 after the n + 1 entries; on success the entries after the table must stay unchanged and
// on every failure the whole buffer must stay unchanged.
// Extents (labelled): EXHAUSTIVE: every ordered coin list of length 0..5 over {1,2,3,4}, of length 6 over {1,2,3} and of length 7 over {1,2}, each
// for every n = 0..30 (digests) and with the prefix property of the tables checked; every ordered list of one and two coins over 1..64 at n = 130
// (digest) and at every n = 0..130 (each n called on its own: oracle and prefix property). SAMPLED: 3 x 200 fixed-seed random lists of 3..64 coins with n up to 5000; the
// listed large cases at n = 2^20; four lists found by a search (a sum of exactly kMod, a sum of kMod - 1, a final entry kMod - 1, and a reachable amount with a count of 0 mod kMod). No timing assertion: the step counts are argued in docs/coins.md.
// What is NOT done: Windows is CI-only; overlapping arrays are not tested (the header leaves them open).
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "coins.h"

using namespace lumora::coins;

// BEGIN GENERATED REFERENCE (tools/coins_ref.py)
struct RefRow {
  const char* name;
  std::size_t cases;
  std::uint64_t digest;
};
constexpr RefRow kRef[] = {
    {"g4", 42315, 0x651DB3C1D9F03FD9ull},
    {"g3", 22599, 0xD17D19B27F124E79ull},
    {"g2", 3968, 0x4E217162824C07E5ull},
    {"p64", 4160, 0xFA04ADE6B1C1501Aull},
    {"rsS", 200, 0x25719BBC32CE3C26ull},
    {"rsM", 200, 0x70082C6802BE1091ull},
    {"rsF", 200, 0xD1927DDEF9DE1B50ull},
    {"big_mix64", 1, 0x62FA0B4951F8713Dull},
    {"big_pow2", 1, 0x8096D8FFC00A0E82ull},
    {"big_small", 5, 0x6DB43D99A6CBBDB7ull},
    {"c64", 1, 0x17948F2EE3DF1FE4ull},
    {"hit_p", 1, 0xF5574EB32693C6ADull},
    {"hit_p1", 1, 0x0211C131D64EA4CFull},
    {"hit_pm1", 1, 0x72AC418C9D4F02DCull},
    {"zero_w", 1, 0x3641510C5C845B93ull},
};
constexpr std::size_t kC64WaysAtLeastMod = 3886;    // entries of the exact (unreduced) tables of the case c64 that are >= 998244353
constexpr std::size_t kC64SubsetsAtLeastMod = 1647;
constexpr std::uint64_t kZeroExact = 148631314946321454ull;    // exact count of multisets for coins {1, 18, 40, 29, 12} at n = 972268
// END GENERATED REFERENCE

namespace {

using Coins = std::vector<std::uint32_t>;
constexpr std::uint32_t kSent = 0xA5A5A5A5u;
constexpr std::size_t kGb = 8, kGa = 16;
constexpr std::uint64_t kP = 998244353ull;

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
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

struct Tally {
  std::size_t bad = 0;
  std::string first;
  void fail(const std::string& s) {
    if (bad == 0) first = s;
    ++bad;
  }
};

// One output table with guards.
struct Guarded {
  std::vector<std::uint32_t> buf;
  std::size_t n = 0;  // number of entries the call may write
  void reset(std::size_t count) {
    n = count;
    buf.assign(kGb + count + kGa, kSent);
  }
  std::uint32_t* p() { return buf.data() + kGb; }
  bool guards_ok() const {
    for (std::size_t i = 0; i < kGb; ++i)
      if (buf[i] != kSent) return false;
    for (std::size_t i = 0; i < kGa; ++i)
      if (buf[kGb + n + i] != kSent) return false;
    return true;
  }
  bool all_sentinel() const {
    for (std::uint32_t x : buf)
      if (x != kSent) return false;
    return true;
  }
};

using Fn = CoinsResult (*)(const std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t*);
const Fn kFns[3] = {coins_min_count, coins_ways, coins_subset_ways};
const char* const kFnNames[3] = {"coins_min_count", "coins_ways", "coins_subset_ways"};

struct Tables {
  std::vector<std::uint32_t> t[3];
};

// The three tables from the real functions for n; guards and the Ok result are checked here.
bool run_real(const Coins& c, std::uint32_t n, Tables& out, Tally& tally, const char* what) {
  bool ok = true;
  for (int k = 0; k < 3; ++k) {
    Guarded g;
    g.reset(static_cast<std::size_t>(n) + 1);
    const CoinsResult r = kFns[k](c.empty() ? nullptr : c.data(), static_cast<std::uint32_t>(c.size()), n, g.p());
    if (r != CoinsResult::Ok || !g.guards_ok()) {
      tally.fail(std::string(what) + ": " + kFnNames[k] + " result or guards, m=" + std::to_string(c.size()) + " n=" + std::to_string(n));
      ok = false;
    }
    out.t[k].assign(g.p(), g.p() + static_cast<std::size_t>(n) + 1);
  }
  return ok;
}

// ORACLE (shares no code with coins.cpp). reductions counts the sums that had to be reduced modulo kP.
struct OracleOut {
  Tables t;
  std::size_t reductions_ways = 0, reductions_subset = 0;
  std::size_t ways_sum_is_p = 0, ways_sum_is_p_minus_1 = 0;  // sums of exactly kP and kP - 1 before the reduction (ways table)
};

OracleOut oracle(const Coins& c, std::uint32_t n) {
  OracleOut o;
  const std::size_t N = static_cast<std::size_t>(n) + 1;
  // fewest coins, amount by amount
  o.t.t[0].assign(N, kUnreachable);
  o.t.t[0][0] = 0;
  for (std::size_t v = 1; v < N; ++v) {
    std::uint64_t best = 0xFFFFFFFFull;
    for (std::uint32_t coin : c) {
      if (coin <= v && o.t.t[0][v - coin] != kUnreachable && o.t.t[0][v - coin] + 1ull < best) best = o.t.t[0][v - coin] + 1ull;
    }
    o.t.t[0][v] = static_cast<std::uint32_t>(best);
  }
  // multisets and subsets: table over (first k kinds, amount)
  const std::size_t m = c.size();
  std::vector<std::vector<std::uint64_t>> W(m + 1, std::vector<std::uint64_t>(N, 0)), S = W;
  W[0][0] = 1;
  S[0][0] = 1;
  for (std::size_t k = 1; k <= m; ++k) {
    const std::uint64_t coin = c[k - 1];
    for (std::size_t v = 0; v < N; ++v) {
      std::uint64_t w = W[k - 1][v] + (v >= coin ? W[k][v - static_cast<std::size_t>(coin)] : 0ull);
      if (w == kP) ++o.ways_sum_is_p;
      if (w == kP - 1) ++o.ways_sum_is_p_minus_1;
      if (w >= kP) {
        w %= kP;
        ++o.reductions_ways;
      }
      W[k][v] = w;
      std::uint64_t s = S[k - 1][v] + (v >= coin ? S[k - 1][v - static_cast<std::size_t>(coin)] : 0ull);
      if (s >= kP) {
        s %= kP;
        ++o.reductions_subset;
      }
      S[k][v] = s;
    }
  }
  o.t.t[1].resize(N);
  o.t.t[2].resize(N);
  for (std::size_t v = 0; v < N; ++v) {
    o.t.t[1][v] = static_cast<std::uint32_t>(W[m][v]);
    o.t.t[2][v] = static_cast<std::uint32_t>(S[m][v]);
  }
  return o;
}

void fold_case(Fold& f, const Coins& c, std::uint32_t n, const Tables& t) {
  f.add(c.size());
  for (std::uint32_t x : c) f.add(x);
  f.add(n);
  for (int k = 0; k < 3; ++k)
    for (std::uint32_t v = 0; v <= n; ++v) f.add(t.t[k][v]);
  ++f.cases;
}

// The first n + 1 entries of the table of the larger n must equal the table of n.
bool is_prefix(const Tables& small, const Tables& big) {
  for (int k = 0; k < 3; ++k)
    for (std::size_t v = 0; v < small.t[k].size(); ++v)
      if (small.t[k][v] != big.t[k][v]) return false;
  return true;
}

bool tables_equal(const Tables& a, const Tables& b) {
  for (int k = 0; k < 3; ++k)
    if (a.t[k] != b.t[k]) return false;
  return true;
}

const RefRow& ref(const std::string& name) {
  for (const RefRow& r : kRef)
    if (name == r.name) return r;
  FAIL("no reference row " << name);
  return kRef[0];
}

void expect_digest(const std::string& name, const Fold& f, const Tally& tally) {
  INFO("first problem: " << tally.first);
  CHECK(tally.bad == 0);
  CHECK(f.cases == ref(name).cases);
  CHECK(f.h == ref(name).digest);
}

// All lists of length lo..hi over values 1..alpha, by length and then lexicographic (the order of the Python reference).
template <typename Visit>
void each_list(std::uint32_t alpha, std::size_t lo, std::size_t hi, Visit visit) {
  for (std::size_t len = lo; len <= hi; ++len) {
    Coins c(len, 1u);
    for (;;) {
      visit(c);
      std::size_t i = len;
      while (i > 0 && c[i - 1] == alpha) {
        c[i - 1] = 1u;
        --i;
      }
      if (i == 0) break;
      ++c[i - 1];
    }
  }
}

void exhaustive_all_n(const std::string& name, std::uint32_t alpha, std::size_t lo, std::size_t hi) {
  Fold f;
  Tally tally;
  each_list(alpha, lo, hi, [&](const Coins& c) {
    Tables top;
    if (!run_real(c, 30, top, tally, "n=30")) return;
    const OracleOut o = oracle(c, 30);
    if (!tables_equal(o.t, top)) tally.fail("oracle differs for m=" + std::to_string(c.size()));
    for (std::uint32_t n = 0; n <= 30; ++n) {
      Tables t;
      if (!run_real(c, n, t, tally, "each n")) continue;
      Tables head;
      for (int k = 0; k < 3; ++k) head.t[k].assign(top.t[k].begin(), top.t[k].begin() + n + 1);
      if (!tables_equal(t, head)) tally.fail("not a prefix of the n = 30 tables, n=" + std::to_string(n));
      fold_case(f, c, n, t);
    }
  });
  expect_digest(name, f, tally);
}

}  // namespace

TEST_CASE("exhaustive: every ordered coin list of length 0..5 over {1,2,3,4}, every n = 0..30", "[coins]") { exhaustive_all_n("g4", 4, 0, 5); }

TEST_CASE("exhaustive: every ordered coin list of length 6 over {1,2,3}, every n = 0..30", "[coins]") { exhaustive_all_n("g3", 3, 6, 6); }

TEST_CASE("exhaustive: every ordered coin list of length 7 over {1,2}, every n = 0..30", "[coins]") { exhaustive_all_n("g2", 2, 7, 7); }

TEST_CASE("exhaustive: every ordered list of one and two coins over 1..64, every n = 0..130 against the oracle", "[coins]") {
  Fold f;
  Tally tally;
  each_list(64, 1, 2, [&](const Coins& c) {
    Tables top;
    if (!run_real(c, 130, top, tally, "n=130")) return;
    fold_case(f, c, 130, top);
    for (std::uint32_t n = 0; n <= 130; ++n) {
      Tables t;
      if (!run_real(c, n, t, tally, "each n")) continue;
      if (!tables_equal(oracle(c, n).t, t)) tally.fail("oracle differs, n=" + std::to_string(n) + " m=" + std::to_string(c.size()));
      if (!is_prefix(t, top)) tally.fail("not a prefix, n=" + std::to_string(n));
    }
  });
  expect_digest("p64", f, tally);
}

TEST_CASE("fixed-seed samples: 3 x 200 random lists of 3..64 coins, n up to 5000 (sampled, not exhaustive)", "[coins]") {
  const char* names[3] = {"rsS", "rsM", "rsF"};
  const std::uint64_t seeds[3] = {8001, 8002, 8003};
  for (int kind = 0; kind < 3; ++kind) {
    Lcg g(seeds[kind]);
    Fold f;
    Tally tally;
    for (int i = 0; i < 200; ++i) {
      const std::uint32_t m = 3u + g.next() % 62u;
      const std::uint32_t n = g.next() % 5001u;
      Coins c(m);
      for (std::uint32_t j = 0; j < m; ++j) {
        if (kind == 0) c[j] = 1u + g.next() % 6u;
        if (kind == 1) c[j] = 1u + g.next() % 200u;
        if (kind == 2) {
          c[j] = g.next();
          if (c[j] == 0) c[j] = 1u;
        }
      }
      Tables t;
      if (!run_real(c, n, t, tally, names[kind])) continue;
      if (!tables_equal(oracle(c, n).t, t)) tally.fail("oracle differs, case " + std::to_string(i));
      fold_case(f, c, n, t);
    }
    expect_digest(names[kind], f, tally);
  }
}

namespace {

// Closed forms. inverse of 1..N modulo kP by the linear recurrence (shares nothing with coins.cpp).
std::vector<std::uint64_t> inverses(std::size_t count) {
  std::vector<std::uint64_t> inv(count + 1, 1);
  for (std::size_t i = 2; i <= count; ++i) inv[i] = (kP - (kP / i) * inv[kP % i] % kP) % kP;
  return inv;
}

std::uint32_t popcount_slow(std::uint32_t v) {
  std::uint32_t c = 0;
  while (v != 0) {
    c += v & 1u;
    v >>= 1;
  }
  return c;
}

}  // namespace

TEST_CASE("large cases at n = 2^20: 64 ones, the powers of two, the digest cases", "[coins]") {
  const std::uint32_t n = kMaxAmount;
  Tally tally;
  {  // 64 coins of value 1: fewest = v, ways = C(v + 63, 63), subsets = C(64, v)
    const Coins c(64, 1u);
    Tables t;
    REQUIRE(run_real(c, n, t, tally, "ones64"));
    const std::vector<std::uint64_t> inv = inverses(n + 1);
    std::uint64_t w = 1;  // C(v + 63, 63)
    std::uint64_t pascal[65] = {1};
    for (int row = 1; row <= 64; ++row)
      for (int k = row; k >= 1; --k) pascal[k] = (pascal[k] + pascal[k - 1]) % kP;  // C(64, k) mod kP (the exact values are below 2^61)
    for (std::uint32_t v = 0; v <= n; ++v) {
      if (v > 0) w = w * ((v + 63ull) % kP) % kP * inv[v] % kP;
      if (t.t[0][v] != v || t.t[1][v] != w || t.t[2][v] != (v <= 64 ? pascal[v] : 0u)) {
        tally.fail("ones64 at v=" + std::to_string(v));
        break;
      }
    }
    INFO(tally.first);
    CHECK(tally.bad == 0);
  }
  {  // 20 powers of two 1 .. 2^19: fewest = popcount(v) below 2^20 and 2 at 2^20; subsets: every v < 2^20 exactly once, 2^20 never
    Coins c;
    for (int k = 0; k < 20; ++k) c.push_back(1u << k);
    Tables t;
    REQUIRE(run_real(c, n, t, tally, "pow2"));
    Fold f;
    fold_case(f, c, n, t);
    CHECK(f.cases == ref("big_pow2").cases);
    CHECK(f.h == ref("big_pow2").digest);
    bool ok = true;
    for (std::uint32_t v = 0; v < n && ok; ++v) ok = t.t[0][v] == popcount_slow(v) && t.t[2][v] == 1u;
    CHECK(ok);
    CHECK(t.t[0][n] == 2u);
    CHECK(t.t[2][n] == 0u);
    // ways(v) = binary partitions with parts up to 2^19: b(2v) = b(2v - 1) + b(v) style recurrence below 2^19, checked by the digest only.
  }
  {  // 64 coins: values 1..3000 with special positions (the list of tools/coins_ref.py mix64)
    Lcg g(9101);
    Coins c(64);
    for (std::size_t j = 0; j < 64; ++j) c[j] = 1u + g.next() % 3000u;
    c[5] = 0xFFFFFFFFu;
    c[9] = 1048576u;
    c[20] = c[0];
    c[30] = 1048577u;
    c[40] = c[41] = 7u;
    Tables t;
    REQUIRE(run_real(c, n, t, tally, "mix64"));
    Fold f;
    fold_case(f, c, n, t);
    CHECK(f.cases == ref("big_mix64").cases);
    CHECK(f.h == ref("big_mix64").digest);
    // coin 1048576 = n gives one coin for n itself when nothing smaller is cheaper; the fewest never exceeds that
    CHECK(t.t[0][n] == 1u);
  }
  {  // small lists at n = 2^20 (closed forms and the digest of the group big_small)
    Fold f;
    const Coins lists[5] = {{1u, 1u}, {}, {0xFFFFFFFFu}, {1048576u}, {1048577u}};
    for (int i = 0; i < 5; ++i) {
      Tables t;
      REQUIRE(run_real(lists[i], n, t, tally, "small"));
      fold_case(f, lists[i], n, t);
      bool ok = true;
      for (std::uint32_t v = 0; v <= n && ok; ++v) {
        std::uint32_t emin = kUnreachable, ew = 0, es = 0;
        if (v == 0) {
          emin = 0;
          ew = 1;
          es = 1;
        } else if (i == 0) {
          emin = v;
          ew = v + 1;
          es = v == 1 ? 2u : v == 2 ? 1u : 0u;
        } else if (i == 3 && v == n) {
          emin = 1;
          ew = 1;
          es = 1;
        }
        ok = t.t[0][v] == emin && t.t[1][v] == ew && t.t[2][v] == es;
      }
      INFO("list " << i);
      CHECK(ok);
    }
    CHECK(f.cases == ref("big_small").cases);
    CHECK(f.h == ref("big_small").digest);
  }
}

TEST_CASE("counts above the modulus before reduction: 64 ones and the coins 1..64", "[coins]") {
  Tally tally;
  {  // 64 coins of value 1: subsets C(64, 32) = 1832624140942590534 is far above the modulus; compare with an exact u64 Pascal triangle
    const Coins c(64, 1u);
    Tables t;
    REQUIRE(run_real(c, 64, t, tally, "ones64"));
    std::uint64_t exact[65] = {1};
    for (int row = 1; row <= 64; ++row)
      for (int k = row; k >= 1; --k) exact[k] += exact[k - 1];
    CHECK(exact[32] > kP * 1000000ull);
    for (std::uint32_t v = 0; v <= 64; ++v) CHECK(t.t[2][v] == exact[v] % kP);
  }
  {  // the coins 1 .. 64 at n = 4000
    Coins c;
    for (std::uint32_t k = 1; k <= 64; ++k) c.push_back(k);
    Tables t;
    REQUIRE(run_real(c, 4000, t, tally, "c64"));
    const OracleOut o = oracle(c, 4000);
    CHECK(tables_equal(o.t, t));
    CHECK(o.reductions_ways > 0);
    CHECK(o.reductions_subset > 0);
    CHECK(kC64WaysAtLeastMod > 0);
    CHECK(kC64SubsetsAtLeastMod > 0);
    Fold f;
    fold_case(f, c, 4000, t);
    CHECK(f.cases == ref("c64").cases);
    CHECK(f.h == ref("c64").digest);
  }
  CHECK(tally.bad == 0);
}

TEST_CASE("a sum of exactly kMod and of kMod - 1 before the reduction (found by a search over lists)", "[coins]") {
  const std::uint64_t seeds[2] = {1172, 1341};
  const char* names[2] = {"hit_p", "hit_p1"};
  for (int which = 0; which < 2; ++which) {
    Lcg g(seeds[which]);
    const std::uint32_t m = 40u + g.next() % 25u;
    const std::uint32_t maxv = 1u + g.next() % 300u;
    Coins c(m);
    for (std::uint32_t j = 0; j < m; ++j) c[j] = 1u + g.next() % maxv;
    Tally tally;
    Tables t;
    REQUIRE(run_real(c, 65536, t, tally, names[which]));
    const OracleOut o = oracle(c, 65536);
    CHECK(tables_equal(o.t, t));
    if (which == 0) CHECK(o.ways_sum_is_p >= 1u);
    if (which == 1) CHECK(o.ways_sum_is_p_minus_1 >= 1u);
    Fold f;
    fold_case(f, c, 65536, t);
    CHECK(f.cases == ref(names[which]).cases);
    CHECK(f.h == ref(names[which]).digest);
  }
}

TEST_CASE("a reachable positive amount whose count is 0 modulo kMod: coins {1, 18, 40, 29, 12} at n = 972268", "[coins]") {
  const Coins c = {1u, 18u, 40u, 29u, 12u};
  const std::uint32_t n = 972268u;
  Tally tally;
  Tables t;
  REQUIRE(run_real(c, n, t, tally, "zero"));
  // The exact number of multisets (computed in Python with integers of any size, see tools/coins_ref.py) is a positive multiple of kMod.
  CHECK(kZeroExact % kP == 0u);
  CHECK(kZeroExact / kP > 0u);
  CHECK(t.t[0][n] != kUnreachable);  // the amount is reachable
  CHECK(t.t[1][n] == 0u);            // the table entry is 0 although the count is positive
  CHECK(t.t[2][n] == 0u);            // (subsets: every coin once reaches only 100, so 0 here is the plain absence of subsets)
  Fold f;
  fold_case(f, c, n, t);
  CHECK(f.cases == ref("zero_w").cases);
  CHECK(f.h == ref("zero_w").digest);
}

TEST_CASE("a final table entry equal to kMod - 1: 64 coins at n = 2^20 (found by a search over lists)", "[coins]") {
  Lcg g(300270);
  g.next();
  const std::uint32_t maxv = 1u + g.next() % 300u;
  Coins c(64);
  for (std::uint32_t j = 0; j < 64; ++j) c[j] = 1u + g.next() % maxv;
  Tally tally;
  Tables t;
  REQUIRE(run_real(c, kMaxAmount, t, tally, "pm1"));
  CHECK(t.t[1][33777] == 998244352u);
  CHECK(t.t[0][33777] != kUnreachable);
  Fold f;
  fold_case(f, c, kMaxAmount, t);
  CHECK(f.cases == ref("hit_pm1").cases);
  CHECK(f.h == ref("hit_pm1").digest);
}

namespace {

struct Row {
  const char* name;
  Coins coins;
  std::uint32_t n;
  Coins mn, ways, sub;
};

const Row kRows[] = {
    {"header example: two kinds of value 1", {1u, 1u}, 4, {0u, 1u, 2u, 3u, 4u}, {1u, 2u, 3u, 4u, 5u}, {1u, 2u, 1u, 0u, 0u}},
    {"no coins", {}, 5, {0u, kUnreachable, kUnreachable, kUnreachable, kUnreachable, kUnreachable}, {1u, 0u, 0u, 0u, 0u, 0u}, {1u, 0u, 0u, 0u, 0u, 0u}},
    {"one coin", {3u}, 10, {0u, kUnreachable, kUnreachable, 1u, kUnreachable, kUnreachable, 2u, kUnreachable, kUnreachable, 3u, kUnreachable}, {1u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 0u, 1u, 0u}, {1u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {"coin equal to n", {5u}, 5, {0u, kUnreachable, kUnreachable, kUnreachable, kUnreachable, 1u}, {1u, 0u, 0u, 0u, 0u, 1u}, {1u, 0u, 0u, 0u, 0u, 1u}},
    {"coin above n", {6u, 2u}, 5, {0u, kUnreachable, 1u, kUnreachable, 2u, kUnreachable}, {1u, 0u, 1u, 0u, 1u, 0u}, {1u, 0u, 1u, 0u, 0u, 0u}},
    {"duplicates and others", {2u, 2u, 3u}, 12, {0u, kUnreachable, 1u, 1u, 2u, 2u, 2u, 3u, 3u, 3u, 4u, 4u, 4u}, {1u, 0u, 2u, 1u, 3u, 2u, 5u, 3u, 7u, 5u, 9u, 7u, 12u}, {1u, 0u, 2u, 1u, 1u, 2u, 0u, 1u, 0u, 0u, 0u, 0u, 0u}},
    {"greedy fails", {1u, 3u, 4u}, 12, {0u, 1u, 2u, 1u, 1u, 2u, 2u, 2u, 2u, 3u, 3u, 3u, 3u}, {1u, 1u, 1u, 2u, 3u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 11u}, {1u, 1u, 0u, 1u, 2u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u}},
    {"greedy fails 2", {1u, 5u, 6u}, 15, {0u, 1u, 2u, 3u, 4u, 1u, 1u, 2u, 3u, 4u, 2u, 2u, 2u, 3u, 4u, 3u}, {1u, 1u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 3u, 4u, 5u, 6u, 6u, 6u, 7u}, {1u, 1u, 0u, 0u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 1u, 1u, 0u, 0u, 0u}},
    {"same value three times", {2u, 2u, 2u}, 8, {0u, kUnreachable, 1u, kUnreachable, 2u, kUnreachable, 3u, kUnreachable, 4u}, {1u, 0u, 3u, 0u, 6u, 0u, 10u, 0u, 15u}, {1u, 0u, 3u, 0u, 3u, 0u, 1u, 0u, 0u}},
    {"coins 1 2 5", {1u, 2u, 5u}, 20, {0u, 1u, 1u, 2u, 2u, 1u, 2u, 2u, 3u, 3u, 2u, 3u, 3u, 4u, 4u, 3u, 4u, 4u, 5u, 5u, 4u}, {1u, 1u, 2u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 10u, 11u, 13u, 14u, 16u, 18u, 20u, 22u, 24u, 26u, 29u}, {1u, 1u, 1u, 1u, 0u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {"all above n", {7u, 8u, 9u}, 6, {0u, kUnreachable, kUnreachable, kUnreachable, kUnreachable, kUnreachable, kUnreachable}, {1u, 0u, 0u, 0u, 0u, 0u, 0u}, {1u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {"odd coins only", {3u, 5u}, 20, {0u, kUnreachable, kUnreachable, 1u, kUnreachable, 1u, 2u, kUnreachable, 2u, 3u, 2u, 3u, 4u, 3u, 4u, 3u, 4u, 5u, 4u, 5u, 4u}, {1u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 1u, 1u, 2u, 1u, 2u}, {1u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {"n = 0", {1u, 2u, 3u}, 0, {0u}, {1u}, {1u}},
    {"n = 0 and no coins", {}, 0, {0u}, {1u}, {1u}},
    {"big coin value", {kUnreachable, 2u}, 10, {0u, kUnreachable, 1u, kUnreachable, 2u, kUnreachable, 3u, kUnreachable, 4u, kUnreachable, 5u}, {1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u}, {1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {"order does not matter", {4u, 1u, 3u}, 12, {0u, 1u, 2u, 1u, 1u, 2u, 2u, 2u, 2u, 3u, 3u, 3u, 3u}, {1u, 1u, 1u, 2u, 3u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 11u}, {1u, 1u, 0u, 1u, 2u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u}},
    {"coin 1 twice and 2", {1u, 1u, 2u}, 8, {0u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u}, {1u, 2u, 4u, 6u, 9u, 12u, 16u, 20u, 25u}, {1u, 2u, 2u, 2u, 1u, 0u, 0u, 0u, 0u}},
    {"power of two coins", {1u, 2u, 4u, 8u}, 16, {0u, 1u, 1u, 2u, 1u, 2u, 2u, 3u, 1u, 2u, 2u, 3u, 2u, 3u, 3u, 4u, 2u}, {1u, 1u, 2u, 2u, 4u, 4u, 6u, 6u, 10u, 10u, 14u, 14u, 20u, 20u, 26u, 26u, 35u}, {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u}},
    {"unreachable gaps", {4u, 6u}, 20, {0u, kUnreachable, kUnreachable, kUnreachable, 1u, kUnreachable, 1u, kUnreachable, 2u, kUnreachable, 2u, kUnreachable, 2u, kUnreachable, 3u, kUnreachable, 3u, kUnreachable, 3u, kUnreachable, 4u}, {1u, 0u, 0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 2u, 0u, 2u, 0u, 2u}, {1u, 0u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {"ten ones", {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u}, 12, {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u}, {1u, 10u, 55u, 220u, 715u, 2002u, 5005u, 11440u, 24310u, 48620u, 92378u, 167960u, 293930u}, {1u, 10u, 45u, 120u, 210u, 252u, 210u, 120u, 45u, 10u, 1u, 0u, 0u}},
};

}  // namespace

TEST_CASE("hand rows: expected values from the Python definitions", "[coins]") {
  for (const Row& r : kRows) {
    INFO(r.name);
    Tables t;
    Tally tally;
    REQUIRE(run_real(r.coins, r.n, t, tally, r.name));
    CHECK(t.t[0] == r.mn);
    CHECK(t.t[1] == r.ways);
    CHECK(t.t[2] == r.sub);
    CHECK(tables_equal(oracle(r.coins, r.n).t, t));
  }
}

namespace {

// Calls the three functions with raw arguments on a buffer of `entries` guarded words; the result must be `want` and the whole buffer unchanged.
void expect_failure(const Coins& c, bool null_coins, std::uint32_t m, std::uint32_t n, bool null_out, std::size_t entries, CoinsResult want,
                    const std::string& label) {
  for (int k = 0; k < 3; ++k) {
    Guarded g;
    g.reset(entries);
    const CoinsResult r = kFns[k](null_coins ? nullptr : c.data(), m, n, null_out ? nullptr : g.p());
    INFO(label << " / " << kFnNames[k]);
    CHECK(r == want);
    CHECK(g.all_sentinel());
  }
}

}  // namespace

TEST_CASE("check order and failures: nothing is written", "[coins]") {
  const Coins one = {1u, 2u, 3u};
  // NullArgument: out always; coins only while m > 0, and before every other check.
  expect_failure(one, false, 3, 10, true, 11, CoinsResult::NullArgument, "null out");
  expect_failure(one, false, 0, 0, true, 1, CoinsResult::NullArgument, "null out, m = 0");
  expect_failure(one, false, 65, 10, true, 11, CoinsResult::NullArgument, "null out before TooManyCoins");
  expect_failure(one, false, 3, kMaxAmount + 1, true, 11, CoinsResult::NullArgument, "null out before TooLarge");
  expect_failure(one, true, 3, 10, false, 11, CoinsResult::NullArgument, "null coins, m = 3");
  expect_failure(one, true, 65, 10, false, 11, CoinsResult::NullArgument, "null coins before TooManyCoins");
  expect_failure(one, true, 3, kMaxAmount + 1, false, 11, CoinsResult::NullArgument, "null coins before TooLarge");
  expect_failure(one, true, 1, 0, false, 1, CoinsResult::NullArgument, "null coins, m = 1, n = 0");
  expect_failure(one, true, 0xFFFFFFFFu, 10, false, 11, CoinsResult::NullArgument, "null coins, m = UINT32_MAX");
  // TooManyCoins: m above 64 (the coin array is never read: it has 3 entries), before TooLarge and BadCoin.
  expect_failure(one, false, 65, 10, false, 11, CoinsResult::TooManyCoins, "m = 65");
  expect_failure(one, false, 0xFFFFFFFFu, 10, false, 11, CoinsResult::TooManyCoins, "m = UINT32_MAX");
  expect_failure(one, false, 65, kMaxAmount + 1, false, 11, CoinsResult::TooManyCoins, "m = 65 before TooLarge");
  expect_failure(Coins{0u, 1u}, false, 65, 10, false, 11, CoinsResult::TooManyCoins, "m = 65 before BadCoin");
  // TooLarge: n above 2^20, before BadCoin (the buffer is small: nothing may be written or read beyond it).
  expect_failure(one, false, 3, kMaxAmount + 1, false, 11, CoinsResult::TooLarge, "n = 2^20 + 1");
  expect_failure(one, false, 3, 0xFFFFFFFFu, false, 11, CoinsResult::TooLarge, "n = UINT32_MAX");
  expect_failure(Coins{1u, 0u}, false, 2, kMaxAmount + 1, false, 11, CoinsResult::TooLarge, "n too large before BadCoin");
  expect_failure(Coins{0u}, false, 1, 0xFFFFFFFFu, false, 11, CoinsResult::TooLarge, "n = UINT32_MAX before BadCoin");
  // BadCoin: a zero at every position for m = 1, 2 and 64; every coin is scanned whatever n is (also n = 0 and coins above n).
  for (const std::uint32_t m : {1u, 2u, 64u}) {
    for (std::uint32_t pos = 0; pos < m; ++pos) {
      Coins c(m, 1u);
      c[pos] = 0u;
      for (const std::uint32_t n : {0u, 1u, 10u}) {
        expect_failure(c, false, m, n, false, static_cast<std::size_t>(n) + 1, CoinsResult::BadCoin,
                       "zero at " + std::to_string(pos) + " of " + std::to_string(m) + ", n = " + std::to_string(n));
      }
      Coins big(m, 0xFFFFFFFFu);
      big[pos] = 0u;
      expect_failure(big, false, m, 5, false, 6, CoinsResult::BadCoin, "zero among coins above n, position " + std::to_string(pos));
    }
  }
  expect_failure(Coins{1u, 2u, 0u}, false, 3, kMaxAmount, false, static_cast<std::size_t>(kMaxAmount) + 1, CoinsResult::BadCoin, "zero at the end, n = 2^20");
  // m counts: a zero beyond m is not scanned.
  {
    const Coins c = {2u, 0u};
    Guarded g;
    g.reset(5);
    CHECK(coins_ways(c.data(), 1, 4, g.p()) == CoinsResult::Ok);
    CHECK(g.guards_ok());
    CHECK(g.p()[0] == 1u);
    CHECK(g.p()[2] == 1u);
  }
  // Legal edges: m = 0 with null coins, n = 0, m = 64, n = 2^20.
  for (int k = 0; k < 3; ++k) {
    Guarded g;
    g.reset(1);
    CHECK(kFns[k](nullptr, 0, 0, g.p()) == CoinsResult::Ok);
    CHECK(g.guards_ok());
    CHECK(g.p()[0] == (k == 0 ? 0u : 1u));
    const Coins c64(64, 1u);
    CHECK(kFns[k](c64.data(), 64, 0, g.p()) == CoinsResult::Ok);
    CHECK(g.p()[0] == (k == 0 ? 0u : 1u));
  }
  // n = 0 writes exactly one entry, whatever the coins (one-entry buffer with guards, several coin lists).
  for (const Coins& c : {Coins{}, Coins{1u}, Coins{5u, 7u}, Coins{0xFFFFFFFFu}}) {
    Tables t;
    Tally tally;
    CHECK(run_real(c, 0, t, tally, "n = 0"));
    CHECK(t.t[0][0] == 0u);
    CHECK(t.t[1][0] == 1u);
    CHECK(t.t[2][0] == 1u);
  }
}
