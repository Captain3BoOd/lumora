// Tests for core/iroot. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/iroot_ref.py, an independent Python implementation (exact integers;
//      a float guess with exact correction for roots, rounding plus an exact power test for perfect powers; cross-checked by the script against prime
//      factorization for x <= 20000 and the largest-fitting-base table). The tests re-run every scenario against the real functions and recompute the
//      64-bit digest and the counts.
//  (2) ORACLES inside the test that share no code with core/iroot/iroot.cpp: a saturating power built from a different overflow test (32-bit halves
//      and a carry check, no division). A root r is correct when r^k <= x and (r+1)^k > x (saturated counts as above x); for x <= 5000 also a naive
//      linear search. A perfect-power result (b, e) is correct when b^e == x and no exponent e*q for a prime q (at most 63) is an exact power of x
//      (if the true largest exponent E were above e, then e divides E and E/e has a prime factor q, so x would be an e*q-th power). That maximality
//      check runs on every case except in the "pp_pow" scenario, where it runs on every 5th case (b^e == x is checked on all). A power is
//      correct when it equals the saturating oracle (exponents above 200 with a base of at least 2 are Overflow).
//  (3) CONSTANTS: the header values and the hand rows; the null, BadExponent and untouched-output rows are written from the header text.
// After every failure the outputs are compared with the sentinels that were passed (the header promises "untouched").
// The grids with k = 2 and k = 3 are exhaustive only over the stated subsets, NOT over all square and cube bases (every k from 4 to 64 is exhaustive
// over every fitting base). What is NOT done: no timing test (every loop has a fixed bound, argued in the docs); Windows is CI-only.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "iroot.h"

using lumora::iroot::RootResult;
using lumora::iroot::ipow_u64;
using lumora::iroot::iroot_u64;
using lumora::iroot::perfect_power_u64;

namespace {

constexpr std::uint64_t kU64 = 0xFFFFFFFFFFFFFFFFull;
constexpr std::uint64_t kSentinel = 0xA5A5A5A55A5A5A5Aull;
constexpr std::uint32_t kSentinel32 = 0xC3C3C3C3u;

// BEGIN GENERATED REFERENCE (tools/iroot_ref.py)
const std::uint64_t kRefBmax[65] = {0ull, 0ull, 4294967295ull, 2642245ull, 65535ull, 7131ull, 1625ull, 565ull, 255ull, 138ull, 84ull, 56ull, 40ull, 30ull, 23ull, 19ull, 15ull, 13ull, 11ull, 10ull, 9ull, 8ull, 7ull, 6ull, 6ull, 5ull, 5ull, 5ull, 4ull, 4ull, 4ull, 4ull, 3ull, 3ull, 3ull, 3ull, 3ull, 3ull, 3ull, 3ull, 3ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 2ull, 1ull};
const std::uint64_t kRefPrimes[10] = {4294967291ull, 4294967279ull, 4294967231ull, 4294967197ull, 4294967189ull, 4294967161ull, 4294967143ull, 4294967111ull, 4294967087ull, 4294967029ull};
const std::size_t kRefSizeSq = 76001, kRefSizeCu = 82003;
const std::uint64_t kRefDigest_ir_small = 0x5BAF27D9A871230AULL;
const std::size_t kRefCases_ir_small = 320064;
const std::uint64_t kRefDigest_ir_pow = 0xF58DACC44A6EFEE4ULL;
const std::size_t kRefCases_ir_pow = 227073;
const std::uint64_t kRefDigest_ir_sq = 0xB1245E40A6014A8EULL;
const std::size_t kRefCases_ir_sq = 228003;
const std::uint64_t kRefDigest_ir_cu = 0x5EBD411E76570CC7ULL;
const std::size_t kRefCases_ir_cu = 240009;
const std::uint64_t kRefDigest_ir_rand = 0x4CD3EA2A8B31DFF1ULL;
const std::size_t kRefCases_ir_rand = 200000;
const std::uint64_t kRefDigest_ir_edge = 0xC3D187B7C4DA269AULL;
const std::size_t kRefCases_ir_edge = 37952;
const std::uint64_t kRefDigest_pp_small = 0x7EC5ED446137290AULL;
const std::size_t kRefCases_pp_small = 200001;
const std::uint64_t kRefDigest_pp_pow = 0xF1AB184EF3415A43ULL;
const std::size_t kRefCases_pp_pow = 265149;
const std::uint64_t kRefDigest_pp_mix = 0x05711E6C64CE3EFDULL;
const std::size_t kRefCases_pp_mix = 25215;
const std::uint64_t kRefDigest_pp_big = 0x0C671F98A0CF87B7ULL;
const std::size_t kRefCases_pp_big = 41;
const std::uint64_t kRefDigest_pw_small = 0x84096D5388F0FB97ULL;
const std::size_t kRefCases_pw_small = 21371;
const std::uint64_t kRefDigest_pw_edge = 0x1D7138002715A7E9ULL;
const std::size_t kRefCases_pw_edge = 193;
const std::uint64_t kRefDigest_pw_small_b = 0xF373E11FCE79FC72ULL;
const std::size_t kRefCases_pw_small_b = 828;
const std::uint64_t kRefDigest_pw_big = 0x24F88A85F8461064ULL;
const std::size_t kRefCases_pw_big = 48;
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
  std::uint64_t next64() {
    const std::uint64_t hi = next();
    const std::uint64_t lo = next();
    return (hi << 32) | lo;
  }
};

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Oracle: saturating multiply and power (inf = at least 2^64). Not shared with iroot.cpp: 32-bit halves and a carry check, no division.
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct Sat {
  std::uint64_t v;
  bool inf;
};
Sat smul(Sat a, std::uint64_t b) {
  if (a.inf) return Sat{0, true};
  std::uint64_t x = a.v, y = b;
  if (x < y) {
    const std::uint64_t t = x;
    x = y;
    y = t;
  }
  if ((x >> 32) != 0 && (y >> 32) != 0) return Sat{0, true};
  const std::uint64_t p1 = (x >> 32) * y;  // y < 2^32 here, x >> 32 < 2^32
  if ((p1 >> 32) != 0) return Sat{0, true};
  const std::uint64_t hi_part = p1 << 32;
  const std::uint64_t p0 = (x & 0xFFFFFFFFull) * y;
  const std::uint64_t s = hi_part + p0;
  if (s < hi_part) return Sat{0, true};
  return Sat{s, false};
}
Sat spow(std::uint64_t b, std::uint64_t e) {
  Sat r{1, false};
  for (std::uint64_t i = 0; i < e; ++i) {
    r = smul(r, b);
    if (r.inf) break;
    if (b <= 1) break;  // 0^e and 1^e are settled after one step (e >= 1)
  }
  return r;
}
bool sat_le(Sat a, std::uint64_t x) { return !a.inf && a.v <= x; }

std::uint64_t oracle_root(std::uint64_t x, std::uint64_t k) {  // largest r with r^k <= x, by bisection on the oracle power
  std::uint64_t lo = 0, hi = 0x100000000ull;
  while (lo < hi) {
    const std::uint64_t mid = lo + (hi - lo + 1) / 2;
    if (sat_le(spow(mid, k), x)) lo = mid;
    else hi = mid - 1;
  }
  return lo;
}

bool check_root(std::uint64_t x, std::uint32_t k, std::uint64_t r) {
  if (k == 1) return r == x;
  if (!sat_le(spow(r, k), x)) return false;
  const Sat n = spow(r + 1, k);
  return n.inf || n.v > x;
}

const std::uint32_t kPrimes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31};
bool check_pp(std::uint64_t x, std::uint64_t b, std::uint32_t e, bool maximal) {
  if (x < 2) return b == x && e == 1;
  if (e < 1 || e > 63) return false;
  const Sat p = spow(b, e);
  if (p.inf || p.v != x) return false;
  if (!maximal) return true;
  for (std::uint32_t q : kPrimes) {
    const std::uint32_t d = e * q;
    if (d > 63) break;
    if ((x >> d) == 0) break;  // x < 2^d: no base of at least 2 has that power
    const std::uint64_t r = oracle_root(x, d);
    if (r >= 2 && spow(r, d).v == x && !spow(r, d).inf) return false;
  }
  return true;
}

std::uint32_t code(RootResult r) { return static_cast<std::uint32_t>(r); }

void do_root(std::uint64_t x, std::uint32_t k, Fold& f, Tally& t) {
  std::uint64_t out = kSentinel;
  const RootResult r = iroot_u64(x, k, &out);
  bool ok = (r == RootResult::Ok) && check_root(x, k, out);
  if (ok && x <= 5000 && k >= 2) {  // naive linear search
    std::uint64_t n = 0;
    while (sat_le(spow(n + 1, k), x)) ++n;
    ok = (n == out);
  }
  if (!ok) t.fail("iroot(" + std::to_string(x) + ", " + std::to_string(k) + ") gave " + std::to_string(code(r)) + " / " + std::to_string(out));
  ++f.cases;
  f.add(out);
}

void do_pp(std::uint64_t x, bool maximal, Fold& f, Tally& t) {
  std::uint64_t b = kSentinel;
  std::uint32_t e = kSentinel32;
  const RootResult r = perfect_power_u64(x, &b, &e);
  const bool ok = (r == RootResult::Ok) && check_pp(x, b, e, maximal);
  if (!ok) t.fail("perfect_power(" + std::to_string(x) + ") gave " + std::to_string(code(r)) + " / " + std::to_string(b) + "^" + std::to_string(e));
  ++f.cases;
  f.add(b);
  f.add(e);
}

void do_pow(std::uint64_t b, std::uint32_t e, Fold& f, Tally& t) {
  std::uint64_t out = kSentinel;
  const RootResult r = ipow_u64(b, e, &out);
  bool want_over = false;
  std::uint64_t want = 0;
  if (e == 0) want = 1;
  else if (b == 0) want = 0;
  else if (b == 1) want = 1;
  else if (e > 200) want_over = true;
  else {
    const Sat p = spow(b, e);
    if (p.inf) want_over = true;
    else want = p.v;
  }
  bool ok;
  if (want_over) ok = (r == RootResult::Overflow) && out == kSentinel;
  else ok = (r == RootResult::Ok) && out == want;
  if (!ok) t.fail("ipow(" + std::to_string(b) + ", " + std::to_string(e) + ") gave " + std::to_string(code(r)) + " / " + std::to_string(out));
  ++f.cases;
  if (r == RootResult::Ok) {
    f.add(0);
    f.add(out);
  } else {
    f.add(3);
  }
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.h == digest);
}

// x = b^k + d for the offsets lo..hi where the value stays inside 0 .. 2^64 - 1 (b^k itself fits for every base used).
std::vector<std::uint64_t> trip(std::uint64_t b, std::uint32_t k, int lo, int hi) {
  std::vector<std::uint64_t> v;
  const Sat ps = spow(b, k);
  if (ps.inf) return v;  // b^k is above 2^64 - 1 (and never exactly 2^64 for the bases used)
  const std::uint64_t p = ps.v;
  for (int d = lo; d <= hi; ++d) {
    if (d < 0 && p < static_cast<std::uint64_t>(-d)) continue;
    if (d > 0 && p > kU64 - static_cast<std::uint64_t>(d)) continue;
    v.push_back(d < 0 ? p - static_cast<std::uint64_t>(-d) : p + static_cast<std::uint64_t>(d));
  }
  return v;
}

void add_range(std::vector<std::uint64_t>& v, std::uint64_t lo, std::uint64_t hi) {
  for (std::uint64_t i = lo; i <= hi; ++i) v.push_back(i);
}
std::vector<std::uint64_t> sq_bases() {  // 1..70000 and the windows, ascending, no duplicates by construction
  std::vector<std::uint64_t> v;
  add_range(v, 1, 70000);
  add_range(v, (1ull << 31) - 2000, (1ull << 31) + 2000);
  add_range(v, (1ull << 32) - 2000, (1ull << 32) - 1);
  return v;
}
std::vector<std::uint64_t> cu_bases() {
  std::vector<std::uint64_t> v;
  add_range(v, 1, 70000);
  add_range(v, (1ull << 20) - 2000, (1ull << 20) + 2000);
  add_range(v, (1ull << 21) - 2000, (1ull << 21) + 2000);
  add_range(v, 2642245 - 2000, 2642245 + 2000);
  return v;
}

// The pp_pow subset of a square or cube list: b <= 1000, or every 16th entry, or within 100 of one of the centres.
std::vector<std::uint64_t> pp_sub(const std::vector<std::uint64_t>& lst) {
  const std::uint64_t centres[7] = {65535, 65536, 1ull << 20, 1ull << 21, 2642245, 1ull << 31, 1ull << 32};
  std::vector<std::uint64_t> v;
  for (std::size_t i = 0; i < lst.size(); ++i) {
    const std::uint64_t b = lst[i];
    bool keep = b <= 1000 || i % 16 == 0;
    for (std::uint64_t c : centres)
      if ((b > c ? b - c : c - b) <= 100) keep = true;
    if (keep) v.push_back(b);
  }
  return v;
}

}  // namespace

TEST_CASE("table and list sizes match the reference", "[iroot]") {
  CHECK(sq_bases().size() == kRefSizeSq);
  CHECK(cu_bases().size() == kRefSizeCu);
  for (std::uint32_t k = 2; k <= 64; ++k) {  // the largest fitting base per k, its power fits, the next base's power does not
    CHECK(sat_le(spow(kRefBmax[k], k), kU64));
    CHECK(spow(kRefBmax[k] + 1, k).inf);
  }
}

// EXHAUSTIVE: every x in 0..5000 for every k in 1..64 (also against a naive linear search).
TEST_CASE("iroot: every x 0..5000, every k 1..64", "[iroot]") {
  Fold f;
  Tally t;
  for (std::uint64_t x = 0; x <= 5000; ++x)
    for (std::uint32_t k = 1; k <= 64; ++k) do_root(x, k, f, t);
  expect_digest(f, t, kRefCases_ir_small, kRefDigest_ir_small);
}

// EXHAUSTIVE over every fitting base for each k = 4..64: x = b^k - 1, b^k, b^k + 1 (where they fit).
TEST_CASE("iroot: k 4..64, EVERY fitting base, b^k and its neighbours", "[iroot]") {
  Fold f;
  Tally t;
  for (std::uint32_t k = 4; k <= 64; ++k)
    for (std::uint64_t b = 1; b <= kRefBmax[k]; ++b)
      for (std::uint64_t x : trip(b, k, -1, 1)) do_root(x, k, f, t);
  expect_digest(f, t, kRefCases_ir_pow, kRefDigest_ir_pow);
}

// Exhaustive only over the stated subsets, not all square bases: b in 1..70000 and the windows 2^31 +- 2000, 2^32 - 2000 .. 2^32 - 1.
TEST_CASE("iroot k = 2: exhaustive only over the stated subsets, not all square bases", "[iroot]") {
  Fold f;
  Tally t;
  for (std::uint64_t b : sq_bases())
    for (std::uint64_t x : trip(b, 2, -1, 1)) do_root(x, 2, f, t);
  expect_digest(f, t, kRefCases_ir_sq, kRefDigest_ir_sq);
}

// Exhaustive only over the stated subsets, not all cube bases: b in 1..70000 and the windows 2^20 +- 2000, 2^21 +- 2000, 2642245 +- 2000.
TEST_CASE("iroot k = 3: exhaustive only over the stated subsets, not all cube bases", "[iroot]") {
  Fold f;
  Tally t;
  for (std::uint64_t b : cu_bases())
    for (std::uint64_t x : trip(b, 3, -1, 1)) do_root(x, 3, f, t);
  expect_digest(f, t, kRefCases_ir_cu, kRefDigest_ir_cu);
}

// SAMPLED (fixed seed): 20000 bases for k = 2 (1 .. 2^32 - 1) and 20000 for k = 3 (1 .. 2642245), x = b^k + d for d in -2..2.
TEST_CASE("iroot k = 2 and 3: fixed-seed samples of 20000 bases each (sampled, not exhaustive)", "[iroot]") {
  Fold f;
  Tally t;
  Lcg q(0x9E3779B97F4A7C15ull);
  for (int i = 0; i < 20000; ++i) {
    const std::uint64_t b = q.next64() % 4294967295ull + 1ull;
    for (std::uint64_t x : trip(b, 2, -2, 2)) do_root(x, 2, f, t);
  }
  for (int i = 0; i < 20000; ++i) {
    const std::uint64_t b = q.next64() % 2642245ull + 1ull;
    for (std::uint64_t x : trip(b, 3, -2, 2)) do_root(x, 3, f, t);
  }
  expect_digest(f, t, kRefCases_ir_rand, kRefDigest_ir_rand);
}

// EXHAUSTIVE over the list: x = 2^64 - 1 - j (j 0..199), x = 2^63 + j (j -100..100), x = 2^p - 1, 2^p, 2^p + 1 (p 0..63), for every k 1..64.
TEST_CASE("iroot: the top of the range and powers of two, every k", "[iroot]") {
  Fold f;
  Tally t;
  std::vector<std::uint64_t> xs;
  for (std::uint64_t j = 0; j < 200; ++j) xs.push_back(kU64 - j);
  for (int j = -100; j <= 100; ++j) xs.push_back(j < 0 ? (std::uint64_t{1} << 63) - static_cast<std::uint64_t>(-j) : (std::uint64_t{1} << 63) + static_cast<std::uint64_t>(j));
  for (std::uint32_t p = 0; p < 64; ++p) {
    xs.push_back((1ull << p) - 1ull);
    xs.push_back(1ull << p);
    if (p < 64) xs.push_back((1ull << p) + 1ull);
  }
  for (std::uint32_t k = 1; k <= 64; ++k)
    for (std::uint64_t x : xs) do_root(x, k, f, t);
  expect_digest(f, t, kRefCases_ir_edge, kRefDigest_ir_edge);
}

TEST_CASE("perfect_power: every x 0..200000", "[iroot]") {
  Fold f;
  Tally t;
  for (std::uint64_t x = 0; x <= 200000; ++x) do_pp(x, true, f, t);
  expect_digest(f, t, kRefCases_pp_small, kRefDigest_pp_small);
}

// e = 4..63: EVERY fitting base; e = 2 and 3: only the stated subsets (see pp_sub), not all square and cube bases. x = b^e - 1, b^e, b^e + 1. The
// maximality oracle runs on every 5th case (b^e == x on all).
TEST_CASE("perfect_power: b^e and neighbours, exhaustive only over the stated subsets for e = 2, 3 (not all square and cube bases)", "[iroot]") {
  Fold f;
  Tally t;
  std::size_t n = 0;
  const std::vector<std::uint64_t> sq = pp_sub(sq_bases()), cu = pp_sub(cu_bases());
  for (std::uint32_t e = 2; e <= 63; ++e) {
    const std::uint64_t top = (e == 2 || e == 3) ? 0 : kRefBmax[e];
    const std::size_t count = (e == 2) ? sq.size() : (e == 3) ? cu.size() : top - 1;
    for (std::size_t i = 0; i < count; ++i) {
      const std::uint64_t b = (e == 2) ? sq[i] : (e == 3) ? cu[i] : 2ull + i;
      if (b < 2) continue;
      for (std::uint64_t x : trip(b, e, -1, 1)) do_pp(x, (n++ % 5) == 0, f, t);
    }
  }
  expect_digest(f, t, kRefCases_pp_pow, kRefDigest_pp_pow);
}

// Products of distinct prime powers: p^a q^b (p < q among the first 12 primes) and p^a q^b r^c (first 6 primes, exponents 1..8), where they fit.
TEST_CASE("perfect_power: products of prime powers", "[iroot]") {
  Fold f;
  Tally t;
  const std::uint64_t pr[12] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
  for (int i = 0; i < 12; ++i)
    for (int j = i + 1; j < 12; ++j)
      for (Sat pa{pr[i], false}; !pa.inf; pa = smul(pa, pr[i]))
        for (Sat v = smul(pa, pr[j]); !v.inf; v = smul(v, pr[j])) do_pp(v.v, true, f, t);
  for (int i = 0; i < 6; ++i)
    for (int j = i + 1; j < 6; ++j)
      for (int l = j + 1; l < 6; ++l)
        for (std::uint64_t a = 1; a <= 8; ++a)
          for (std::uint64_t b = 1; b <= 8; ++b)
            for (std::uint64_t c = 1; c <= 8; ++c) {
              const Sat v = smul(smul(smul(spow(pr[i], a), 1), 1), 1);
              Sat w = smul(v, 1);
              w = Sat{w.v, w.inf};
              const Sat pj = spow(pr[j], b), pl = spow(pr[l], c);
              if (v.inf || pj.inf || pl.inf) continue;
              const Sat m = smul(smul(v, pj.v), pl.v);
              if (!m.inf) do_pp(m.v, true, f, t);
            }
  expect_digest(f, t, kRefCases_pp_mix, kRefDigest_pp_mix);
}

TEST_CASE("perfect_power: large rows (3^40, 2^63, squares of primes below 2^32)", "[iroot]") {
  Fold f;
  Tally t;
  std::vector<std::uint64_t> bigs = {12157665459056928801ull, 1ull << 63, kU64, 18446744065119617025ull};
  for (std::uint64_t p : kRefPrimes) bigs.push_back(p * p);
  for (std::uint64_t v : bigs)
    for (int d = -1; d <= 1; ++d) {
      if (d < 0 && v == 0) continue;
      if (d > 0 && v == kU64) continue;
      do_pp(d < 0 ? v - 1 : v + static_cast<std::uint64_t>(d), true, f, t);
    }
  expect_digest(f, t, kRefCases_pp_big, kRefDigest_pp_big);
}

TEST_CASE("ipow: bases 0..300, exponents 0..70", "[iroot]") {
  Fold f;
  Tally t;
  for (std::uint64_t b = 0; b <= 300; ++b)
    for (std::uint32_t e = 0; e <= 70; ++e) do_pow(b, e, f, t);
  expect_digest(f, t, kRefCases_pw_small, kRefDigest_pw_small);
}

// For e = 2..63 the largest fitting base and its neighbours (the Overflow side), plus e = 1 and e = 0 rows.
TEST_CASE("ipow: the largest fitting base for every exponent, and the next base", "[iroot]") {
  Fold f;
  Tally t;
  for (std::uint32_t e = 2; e <= 63; ++e)
    for (std::uint64_t b = kRefBmax[e] - 1; b <= kRefBmax[e] + 1; ++b) do_pow(b, e, f, t);
  do_pow(kU64, 1, f, t);
  do_pow(kU64 - 1, 1, f, t);
  const std::uint64_t b0[5] = {0, 1, 2, 7, kU64};
  for (std::uint64_t b : b0) do_pow(b, 0, f, t);
  expect_digest(f, t, kRefCases_pw_edge, kRefDigest_pw_edge);
}

TEST_CASE("ipow: bases 0, 1, 2, 3 with every exponent 0..200 and huge exponents", "[iroot]") {
  Fold f;
  Tally t;
  std::vector<std::uint32_t> ex;
  for (std::uint32_t e = 0; e <= 200; ++e) ex.push_back(e);
  const std::uint32_t more[6] = {2147483647u, 2147483648u, 4294967294u, 4294967295u, 65535u, 65536u};
  for (std::uint32_t e : more) ex.push_back(e);
  for (std::uint64_t b = 0; b <= 3; ++b)
    for (std::uint32_t e : ex) do_pow(b, e, f, t);
  expect_digest(f, t, kRefCases_pw_small_b, kRefDigest_pw_small_b);
}

TEST_CASE("ipow: large bases", "[iroot]") {
  Fold f;
  Tally t;
  const std::uint64_t bs[6] = {1ull << 31, 4294967295ull, 4294967296ull, 1ull << 63, kU64 - 1, kU64};
  for (std::uint64_t b : bs) {
    for (std::uint32_t e = 0; e <= 4; ++e) do_pow(b, e, f, t);
    do_pow(b, 63, f, t);
    do_pow(b, 64, f, t);
    do_pow(b, 4294967295u, f, t);
  }
  expect_digest(f, t, kRefCases_pw_big, kRefDigest_pw_big);
}

#define WANT_ROOT(X, K, ER, EV) \
  do { \
    std::uint64_t o_ = kSentinel; \
    const RootResult r_ = iroot_u64((X), (K), &o_); \
    CHECK(r_ == (ER)); \
    CHECK(o_ == (EV)); \
  } while (0)
#define WANT_POW(B, E, ER, EV) \
  do { \
    std::uint64_t o_ = kSentinel; \
    const RootResult r_ = ipow_u64((B), (E), &o_); \
    CHECK(r_ == (ER)); \
    CHECK(o_ == (EV)); \
  } while (0)
#define WANT_PP(X, EB, EE) \
  do { \
    std::uint64_t b_ = kSentinel; \
    std::uint32_t e_ = kSentinel32; \
    const RootResult r_ = perfect_power_u64((X), &b_, &e_); \
    CHECK(r_ == RootResult::Ok); \
    CHECK(b_ == (EB)); \
    CHECK(e_ == (EE)); \
  } while (0)

// CONSTANTS: the values the header states, and hand rows computed once with Python integers.
TEST_CASE("header values and hand rows (constants)", "[iroot]") {
  WANT_ROOT(kU64, 2, RootResult::Ok, 4294967295ull);
  WANT_ROOT(kU64, 3, RootResult::Ok, 2642245ull);
  WANT_ROOT(1ull << 63, 63, RootResult::Ok, 2ull);
  WANT_ROOT((1ull << 63) - 1ull, 63, RootResult::Ok, 1ull);
  WANT_ROOT(kU64, 1, RootResult::Ok, kU64);
  WANT_ROOT(kU64, 64, RootResult::Ok, 1ull);
  WANT_ROOT(1, 64, RootResult::Ok, 1ull);
  WANT_ROOT(0, 64, RootResult::Ok, 0ull);
  WANT_ROOT(0, 1, RootResult::Ok, 0ull);
  WANT_ROOT(1, 1, RootResult::Ok, 1ull);
  WANT_ROOT(kU64, 4, RootResult::Ok, 65535ull);
  WANT_ROOT(1ull << 32, 2, RootResult::Ok, 65536ull);
  WANT_ROOT(4294967296ull * 4294967296ull - 1ull, 2, RootResult::Ok, 4294967295ull);
  WANT_PP(64, 2, 6u);
  WANT_PP(36, 6, 2u);
  WANT_PP(12, 12, 1u);
  WANT_PP(1ull << 63, 2, 63u);
  WANT_PP(12157665459056928801ull, 3, 40u);
  WANT_PP(0, 0, 1u);
  WANT_PP(1, 1, 1u);
  WANT_PP(2, 2, 1u);
  WANT_PP(4, 2, 2u);
  WANT_PP(kU64, kU64, 1u);
  WANT_PP(18446744065119617025ull, 4294967295ull, 2u);
  WANT_POW(0, 0, RootResult::Ok, 1ull);
  WANT_POW(5, 0, RootResult::Ok, 1ull);
  WANT_POW(0, 7, RootResult::Ok, 0ull);
  WANT_POW(1, 4294967295u, RootResult::Ok, 1ull);
  WANT_POW(0, 4294967295u, RootResult::Ok, 0ull);
  WANT_POW(2, 63, RootResult::Ok, 1ull << 63);
  WANT_POW(2, 64, RootResult::Overflow, kSentinel);
  WANT_POW(3, 40, RootResult::Ok, 12157665459056928801ull);
  WANT_POW(3, 41, RootResult::Overflow, kSentinel);
  WANT_POW(4294967295ull, 2, RootResult::Ok, 18446744065119617025ull);
  WANT_POW(4294967296ull, 2, RootResult::Overflow, kSentinel);
  WANT_POW(2, 4294967295u, RootResult::Overflow, kSentinel);
  WANT_POW(kU64, 1, RootResult::Ok, kU64);
  WANT_POW(kU64, 2, RootResult::Overflow, kSentinel);
}

TEST_CASE("null and BadExponent rules, outputs untouched", "[iroot]") {
  std::uint64_t o = kSentinel, b = kSentinel;
  std::uint32_t e = kSentinel32;
  CHECK(iroot_u64(5, 2, nullptr) == RootResult::NullArgument);
  CHECK(iroot_u64(5, 0, nullptr) == RootResult::NullArgument);  // null before BadExponent
  CHECK(iroot_u64(5, 99, nullptr) == RootResult::NullArgument);
  CHECK(iroot_u64(5, 0, &o) == RootResult::BadExponent);
  CHECK(iroot_u64(5, 65, &o) == RootResult::BadExponent);
  CHECK(iroot_u64(kU64, 4294967295u, &o) == RootResult::BadExponent);
  CHECK(iroot_u64(0, 0, &o) == RootResult::BadExponent);
  CHECK(o == kSentinel);
  CHECK(perfect_power_u64(64, nullptr, &e) == RootResult::NullArgument);
  CHECK(e == kSentinel32);
  CHECK(perfect_power_u64(64, &b, nullptr) == RootResult::NullArgument);
  CHECK(b == kSentinel);
  CHECK(perfect_power_u64(64, nullptr, nullptr) == RootResult::NullArgument);
  CHECK(ipow_u64(2, 3, nullptr) == RootResult::NullArgument);
  CHECK(ipow_u64(2, 64, nullptr) == RootResult::NullArgument);  // null before Overflow
  CHECK(ipow_u64(kU64, kU64 > 0 ? 4294967295u : 0u, nullptr) == RootResult::NullArgument);
  CHECK(ipow_u64(2, 64, &o) == RootResult::Overflow);
  CHECK(ipow_u64(kU64, 2, &o) == RootResult::Overflow);
  CHECK(o == kSentinel);
}
