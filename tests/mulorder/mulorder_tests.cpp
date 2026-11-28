// Tests for core/mulorder. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/mulorder_ref.py, an independent Python program (brute-force definitions
//      for small moduli: the order by multiplying until 1, lambda as the least exponent that kills every unit, the root by scanning and walking orders; and a
//      second formulation with Python integers whose order is the least DIVISOR of lambda whose power is 1; the two are compared by the script). The tests
//      re-run every group against the real functions and recompute the 64-bit digest (the moduli, residues, statuses and values) and the case count.
//  (2) ORACLES inside the test, sharing no code with core/mulorder/mulorder.cpp: a smallest-prime-factor sieve (moduli up to 2^20) and division by the sieved
//      primes below 2^16 (larger moduli); lambda and phi from the prime power formulas; the order as the least divisor of lambda whose power is 1; the root
//      as the least g coprime with m whose power g^(phi / q) is not 1 for every prime q of phi, existing exactly when lambda = phi; and BRUTE oracles: the
//      order by multiplying until 1, lambda by raising the exponent until every unit is killed, the root by walking the order of each g.
//  (3) CLOSED FORMS and properties: the header examples, lambda of primes and prime powers, order(1, m) = 1, order(m - 1, m) = 2 for m > 2, the check-order
//      matrix with the *out sentinel unchanged after every failure, primitive_root(998244353) = 3.
// Extents (labelled): EXHAUSTIVE carmichael and primitive_root for every m = 1 .. 2^20 (eight ranges of 2^17 each, one test case per range, against the sieve
// oracle and the digests); EXHAUSTIVE mult_order for every m = 1 .. 2000 and every a = 0 .. m - 1 plus a = m, m + 1, 2 m - 1, 2^32 - 1 against the brute oracle
// (four cases); EXHAUSTIVE over m: mult_order for EVERY m = 1 .. 4096 with 64 stated residues each (0 .. 7, m - 2 .. m + 2, 2 m - 1 .. 2 m + 1, 2^32 - 1, 2^32 - 2,
// 2^31 - 1 .. 2^31 + 1, then seeded LCG draws; four cases) against the fast oracle and the digests; carmichael for every m = 1 .. 3000 against the definition; primitive_root for every m = 1 .. 4096 against the definition (so the set of moduli with
// a root is checked against the oracle everywhere below 4097). SAMPLED (labelled; each modulus is checked for carmichael, primitive_root and mult_order for the 8
// residues 1, 2, 3, m - 1, 0, m, m + 1, 2^32 - 1): ALL prime squares p^2 (p <= 65521, four cases by index) and prime cubes up to 2^32; 2 p^2 and 4 p^2 for the primes
// up to 200 and every 4th prime above; ALL p^k, 2 p^k, 4 p^k below 2^32 for p = 2, 3, 5, 7, 11, 13; the largest 30 primes (and 2 p, 4 p for the largest 30 below 2^31
// and 2^30), the 15 largest safe primes, 998244353 and other well known primes, 2^32 - 64 .. 2^32 - 1; 19 Carmichael numbers and the largest Chernick number below 2^32;
// 2^32 - 1 and all its divisors; the primorial 223092870 times 1 .. 19 and the eight numbers below 2^32 with the most divisors; 200 seeded random moduli.
// Not done: Windows is CI-only; no timing assertion; no sweep of all of 2^32 for any function. Large cases carry the tag [big]; every case is sized to stay
// under about 30 seconds in the sanitizer build.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "mulorder.h"

using namespace lumora::mulorder;

// BEGIN GENERATED REFERENCE (tools/mulorder_ref.py)
struct RefRow {
  const char* name;
  std::size_t cases;
  std::uint64_t digest;
};
constexpr RefRow kRef[] = {
    {"lam_0", 131072, 0x9B9CCD520F40111Full},
    {"lam_1", 131072, 0x94FEDC77B32C89A5ull},
    {"lam_2", 131072, 0xAC840B2358155783ull},
    {"lam_3", 131072, 0x3801E96AD056A375ull},
    {"lam_4", 131072, 0x6981DC446308CCB7ull},
    {"lam_5", 131072, 0xECEA0AEC507B3367ull},
    {"lam_6", 131072, 0xFC8C1A3CB21025D9ull},
    {"lam_7", 131072, 0x41645861521B0C31ull},
    {"root_0", 131072, 0xA8324D66E868A105ull},
    {"root_1", 131072, 0x2058064A22DB6FA8ull},
    {"root_2", 131072, 0x3073F33B44DE6FD5ull},
    {"root_3", 131072, 0x9D9BE90F90B9CDFEull},
    {"root_4", 131072, 0x0FBC6E43CAC29575ull},
    {"root_5", 131072, 0x5D8FF9C0C8280039ull},
    {"root_6", 131072, 0x8DE07C58037CADFCull},
    {"root_7", 131072, 0x1C04130611B10326ull},
    {"ord_0", 127248, 0x345D14ECA4050653ull},
    {"ord_1", 377250, 0x3CB848CE98DDA4CBull},
    {"ord_2", 627250, 0x56013975E02F29B9ull},
    {"ord_3", 877250, 0x7B637735F60BDF69ull},
    {"ord64_0", 65536, 0xA1DBAA6F6C7C0782ull},
    {"ord64_1", 65536, 0x0FC3BD9AF560C0B7ull},
    {"ord64_2", 65536, 0xF73557899B34266Bull},
    {"ord64_3", 65536, 0x0E4F8777D291AA6Eull},
    {"lamdef", 3000, 0x82FBA8283B74B367ull},
    {"rootdef", 4096, 0x4B090062927A79DCull},
    {"sq_0", 1636, 0x7D7361D44B21544Aull},
    {"sq_1", 1636, 0xE8E1CC0D8DC50F52ull},
    {"sq_2", 1635, 0xD5E63BB99B1B521Eull},
    {"sq_3", 1635, 0x00A33B84B3468BE0ull},
    {"cube_0", 257, 0x195E142385A0982Full},
    {"fam_a", 2144, 0xB9FB7610DF32F20Dull},
    {"fam_b", 207, 0x35344006FCF1C745ull},
    {"top", 172, 0x2A3DBCF54AA2B65Bull},
    {"carm", 20, 0x01DFD451E4EEB053ull},
    {"umax", 32, 0x9C9DAB007B08A827ull},
    {"hc", 27, 0x6DEC2F311336B4E5ull},
    {"rnd", 200, 0xF6EBFFA63D89D061ull},
};
// END GENERATED REFERENCE

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Vec = std::vector<u32>;

constexpr u32 kSent = 0xA5A5A5A5u;
constexpr u32 kMax32 = 0xFFFFFFFFu;
constexpr u32 kLim = 1u << 20;

struct Fold {
  u64 h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  void one(u64 v) { h = (h ^ v) * 0x100000001B3ull; }
};

struct Acc {
  bool ok = true;
  std::string first;
  template <class A = int, class B = int, class C = int>
  void expect(bool cond, const char* what, A a = 0, B b = 0, C c = 0) {
    if (!cond && ok) {
      ok = false;
      first = std::string(what) + " (" + std::to_string(static_cast<u64>(a)) + ", " + std::to_string(static_cast<u64>(b)) + ", " +
              std::to_string(static_cast<u64>(c)) + ")";
    }
  }
};

void finish(const Acc& acc) {
  INFO(acc.first);
  CHECK(acc.ok);
}

void check_row(const char* name, const Fold& f) {
  bool found = false;
  for (const RefRow& row : kRef) {
    if (std::string(row.name) != name) continue;
    found = true;
    INFO(name);
    CHECK(f.cases == row.cases);
    CHECK(f.h == row.digest);
  }
  CHECK(found);
}

struct Res {
  MoResult r;
  u32 v;
};

Res c_lam(u32 m) {
  u32 out = kSent;
  const MoResult r = carmichael(m, &out);
  return {r, out};
}
Res c_ord(u32 a, u32 m) {
  u32 out = kSent;
  const MoResult r = mult_order(a, m, &out);
  return {r, out};
}
Res c_root(u32 m) {
  u32 out = kSent;
  const MoResult r = primitive_root(m, &out);
  return {r, out};
}

// ---- ORACLES (no shared code with the library) -----------------------------------------------------------------------------------------------

struct Sieve {
  Vec spf;
  Vec primes;  // the primes below 2^16
  Sieve() : spf(kLim + 1u) {
    for (u32 i = 0; i <= kLim; ++i) spf[i] = i;
    for (u32 i = 2; static_cast<u64>(i) * i <= kLim; ++i)
      if (spf[i] == i)
        for (u32 j = i * i; j <= kLim; j += i)
          if (spf[j] == j) spf[j] = i;
    for (u32 p = 2; p < 65536u; ++p)
      if (spf[p] == p) primes.push_back(p);
  }
};

const Sieve& sieve() {
  static const Sieve s;
  return s;
}

using Fac = std::vector<std::pair<u64, u32>>;

Fac factor(u64 m) {
  const Sieve& s = sieve();
  Fac out;
  if (m <= kLim) {
    while (m > 1) {
      const u64 p = s.spf[m];
      u32 e = 0;
      while (m % p == 0) {
        m /= p;
        ++e;
      }
      out.emplace_back(p, e);
    }
    return out;
  }
  for (const u32 p : s.primes) {
    if (static_cast<u64>(p) * p > m) break;
    if (m % p == 0) {
      u32 e = 0;
      while (m % p == 0) {
        m /= p;
        ++e;
      }
      out.emplace_back(p, e);
    }
  }
  if (m > 1) out.emplace_back(m, 1u);
  return out;
}

bool is_prime(u64 n) {
  if (n < 2) return false;
  const Fac f = factor(n);
  return f.size() == 1 && f[0].second == 1;
}

u64 gcd64(u64 a, u64 b) {
  while (b != 0) {
    const u64 t = a % b;
    a = b;
    b = t;
  }
  return a;
}

u64 ipow(u64 p, u32 e) {
  u64 r = 1;
  for (u32 i = 0; i < e; ++i) r *= p;
  return r;
}

void lam_phi(u64 m, u64& lam, u64& phi) {
  lam = 1;
  phi = 1;
  for (const auto& pe : factor(m)) {
    const u64 p = pe.first;
    const u32 e = pe.second;
    u64 lp = 0;
    u64 ph = 0;
    if (p == 2) {
      lp = e == 1 ? 1u : (e == 2 ? 2u : (u64{1} << (e - 2u)));
      ph = u64{1} << (e - 1u);
    } else {
      lp = ipow(p, e - 1u) * (p - 1u);
      ph = lp;
    }
    lam = lam / gcd64(lam, lp) * lp;
    phi *= ph;
  }
}

// left-to-right binary powers (the library uses right-to-left)
u64 o_pow(u64 b, u64 e, u64 m) {
  u64 r = 1 % m;
  b %= m;
  for (int bit = 63; bit >= 0; --bit) {
    r = r * r % m;
    if (((e >> bit) & 1u) != 0) r = r * b % m;
  }
  return r;
}

Vec divisors(u64 n) {
  Vec ds{1u};
  for (const auto& pe : factor(n)) {
    const std::size_t have = ds.size();
    u64 pw = 1;
    for (u32 i = 1; i <= pe.second; ++i) {
      pw *= pe.first;
      for (std::size_t j = 0; j < have; ++j) ds.push_back(static_cast<u32>(ds[j] * pw));
    }
  }
  std::sort(ds.begin(), ds.end());
  return ds;
}

// the least divisor of lambda whose power is 1; false when a and m are not coprime
bool o_order(u64 a, u32 m, u32& out) {
  if (m == 1) {
    out = 1;
    return true;
  }
  a %= m;
  if (gcd64(a, m) != 1) return false;
  u64 lam = 0;
  u64 phi = 0;
  lam_phi(m, lam, phi);
  for (const u32 d : divisors(lam)) {
    if (o_pow(a, d, m) == 1) {
      out = d;
      return true;
    }
  }
  out = 0;
  return true;
}

// the least primitive root: lambda = phi, then the least g coprime with m with g^(phi / q) != 1 for each prime q of phi; false when there is none
bool o_root(u32 m, u32& out) {
  if (m == 1) {
    out = 0;
    return true;
  }
  u64 lam = 0;
  u64 phi = 0;
  lam_phi(m, lam, phi);
  if (lam != phi) return false;
  const Fac qs = factor(phi);
  for (u32 g = 1; g < m; ++g) {
    if (gcd64(g, m) != 1) continue;
    bool all = true;
    for (const auto& q : qs) all = all && o_pow(g, phi / q.first, m) != 1;
    if (all) {
      out = g;
      return true;
    }
  }
  out = 0;
  return true;
}

// BRUTE oracles: the definitions
bool brute_order(u32 a, u32 m, u32& out) {
  if (m == 1) {
    out = 1;
    return true;
  }
  const u64 x0 = a % m;
  if (gcd64(x0, m) != 1) return false;
  u64 x = x0;
  u32 e = 1;
  while (x != 1) {
    x = x * x0 % m;
    ++e;
  }
  out = e;
  return true;
}

u32 brute_lambda(u32 m) {
  if (m == 1) return 1;
  for (u32 e = 1;; ++e) {
    bool kills = true;
    for (u32 x = 1; x < m && kills; ++x)
      if (gcd64(x, m) == 1 && o_pow(x, e, m) != 1) kills = false;
    if (kills) return e;
  }
}

bool brute_root(u32 m, u32& out) {
  if (m == 1) {
    out = 0;
    return true;
  }
  u32 units = 0;
  for (u32 x = 1; x < m; ++x)
    if (gcd64(x, m) == 1) ++units;
  if (brute_lambda(m) != units) return false;
  for (u32 g = 1; g < m; ++g) {
    u32 o = 0;
    if (brute_order(g, m, o) && o == units) {
      out = g;
      return true;
    }
  }
  return false;
}

// ---- checks that fold ------------------------------------------------------------------------------------------------------------------------

void do_lam(Acc& acc, Fold& f, u32 m, u32 expect) {
  const Res x = c_lam(m);
  acc.expect(x.r == MoResult::Ok, "carmichael status", m);
  acc.expect(x.v == expect, "carmichael value", m, x.v, expect);
  f.one(m);
  f.one(x.v);
  ++f.cases;
}

// the library root against a claimed (exists, g); folds m, status, value
void do_root(Acc& acc, Fold& f, u32 m, bool exists, u32 g) {
  const Res x = c_root(m);
  if (exists) {
    acc.expect(x.r == MoResult::Ok, "primitive_root status", m);
    acc.expect(x.v == g, "primitive_root value", m, x.v, g);
  } else {
    acc.expect(x.r == MoResult::NoPrimitiveRoot, "primitive_root must fail", m, static_cast<u32>(x.r));
    acc.expect(x.v == kSent, "primitive_root wrote on failure", m, x.v);
  }
  f.one(m);
  f.one(x.r == MoResult::Ok ? 0u : static_cast<u32>(x.r));
  f.one(x.r == MoResult::Ok ? x.v : 0u);
  ++f.cases;
}

// the library order against a claimed (coprime, o); folds a, status, value (the caller folds m)
void do_ord(Acc& acc, Fold& f, u32 a, u32 m, bool coprime, u32 o) {
  const Res x = c_ord(a, m);
  if (coprime) {
    acc.expect(x.r == MoResult::Ok, "mult_order status", a, m);
    acc.expect(x.v == o, "mult_order value", a, m, x.v);
  } else {
    acc.expect(x.r == MoResult::NotCoprime, "mult_order must fail", a, m, static_cast<u32>(x.r));
    acc.expect(x.v == kSent, "mult_order wrote on failure", a, m, x.v);
  }
  f.one(a);
  f.one(x.r == MoResult::Ok ? 0u : static_cast<u32>(x.r));
  f.one(x.r == MoResult::Ok ? x.v : 0u);
}

// ---- the sweeps ------------------------------------------------------------------------------------------------------------------------------

void run_lam(unsigned r, const char* name) {
  Acc acc;
  Fold f;
  const Sieve& s = sieve();
  (void)s;
  for (u32 m = 1u + r * (1u << 17); m <= (r + 1u) * (1u << 17); ++m) {
    u64 lam = 0;
    u64 phi = 0;
    lam_phi(m, lam, phi);
    do_lam(acc, f, m, static_cast<u32>(lam));
  }
  finish(acc);
  check_row(name, f);
}

void run_root(unsigned r, const char* name) {
  Acc acc;
  Fold f;
  for (u32 m = 1u + r * (1u << 17); m <= (r + 1u) * (1u << 17); ++m) {
    u32 g = 0;
    const bool has = o_root(m, g);
    do_root(acc, f, m, has, g);
  }
  finish(acc);
  check_row(name, f);
}

Vec a_list(u32 m) {
  Vec out;
  for (u32 a = 0; a < m; ++a) out.push_back(a);
  Vec extra;
  for (const u64 v : {u64{m}, u64{m} + 1u, 2u * u64{m} - 1u, u64{kMax32}}) {
    bool seen = false;
    for (const u32 w : extra) seen = seen || w == v;
    if (v <= kMax32 && !seen) extra.push_back(static_cast<u32>(v));
  }
  out.insert(out.end(), extra.begin(), extra.end());
  return out;
}

void run_ord(u32 lo, u32 hi, const char* name) {
  Acc acc;
  Fold f;
  for (u32 m = lo; m <= hi; ++m) {
    for (const u32 a : a_list(m)) {
      u32 o = 0;
      const bool coprime = brute_order(a, m, o);
      f.one(m);
      do_ord(acc, f, a, m, coprime, o);
      ++f.cases;
    }
  }
  finish(acc);
  check_row(name, f);
}

// 64 distinct stated residues for the modulus m (the same list as a64 in the reference): 0 .. 7, m - 2 .. m + 2, 2 m - 1, 2 m, 2 m + 1, 2^32 - 1, 2^32 - 2,
// 2^31 - 1, 2^31, 2^31 + 1 (inside u32, a value listed twice kept once), then LCG draws (seed 0x6D6F ^ m; the draw for even i, draw % m for odd i) until 64.
Vec a64(u32 m) {
  Vec out;
  const auto has = [&out](u64 v) {
    for (const u32 w : out)
      if (w == v) return true;
    return false;
  };
  const u64 mm = m;
  const u64 cand[] = {0, 1, 2, 3, 4, 5, 6, 7, mm - 2u, mm - 1u, mm, mm + 1u, mm + 2u, 2u * mm - 1u, 2u * mm, 2u * mm + 1u, kMax32, kMax32 - 1u, (u64{1} << 31) - 1u,
                      u64{1} << 31, (u64{1} << 31) + 1u};
  for (const u64 v : cand)
    if (v <= kMax32 && !has(v)) out.push_back(static_cast<u32>(v));
  u64 x = 0x6D6Full ^ mm;
  unsigned i = 0;
  while (out.size() < 64) {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    const u32 d = static_cast<u32>(x >> 32);
    const u32 v = (i % 2 == 0) ? d : d % m;
    ++i;
    if (!has(v)) out.push_back(v);
  }
  return out;
}

// mult_order for every m in lo..hi and the 64 stated residues of each, against the fast oracle (the least divisor of lambda whose power is 1)
void run_ord64(u32 lo, u32 hi, const char* name) {
  Acc acc;
  Fold f;
  for (u32 m = lo; m <= hi; ++m) {
    for (const u32 a : a64(m)) {
      u32 o = 0;
      const bool coprime = o_order(a, m, o);
      f.one(m);
      do_ord(acc, f, a, m, coprime, o);
      ++f.cases;
    }
  }
  finish(acc);
  check_row(name, f);
}

// ---- the large sets (the same lists as the reference) ----------------------------------------------------------------------------------------

Vec a_big(u32 m) {
  Vec out;
  for (const u64 v : {u64{1}, u64{2}, u64{3}, u64{m} - 1u, u64{0}, u64{m}, u64{m} + 1u, u64{kMax32}}) {
    bool seen = false;
    for (const u32 w : out) seen = seen || w == v;
    if (v <= kMax32 && !seen) out.push_back(static_cast<u32>(v));
  }
  return out;
}

void run_big(const Vec& ms, const char* name) {
  Acc acc;
  Fold f;
  for (const u32 m : ms) {
    u64 lam = 0;
    u64 phi = 0;
    lam_phi(m, lam, phi);
    f.one(m);
    f.one(lam);
    {
      const Res x = c_lam(m);
      acc.expect(x.r == MoResult::Ok, "carmichael status", m);
      acc.expect(x.v == lam, "carmichael value", m, x.v, lam);
    }
    {
      u32 g = 0;
      const bool has = o_root(m, g);
      const Res x = c_root(m);
      if (has) {
        acc.expect(x.r == MoResult::Ok, "primitive_root status", m);
        acc.expect(x.v == g, "primitive_root value", m, x.v, g);
      } else {
        acc.expect(x.r == MoResult::NoPrimitiveRoot, "primitive_root must fail", m);
        acc.expect(x.v == kSent, "primitive_root wrote on failure", m, x.v);
      }
      f.one(x.r == MoResult::Ok ? 0u : static_cast<u32>(x.r));
      f.one(x.r == MoResult::Ok ? x.v : 0u);
    }
    for (const u32 a : a_big(m)) {
      u32 o = 0;
      const bool coprime = o_order(a, m, o);
      do_ord(acc, f, a, m, coprime, o);
    }
    ++f.cases;
  }
  finish(acc);
  check_row(name, f);
}

Vec primes_upto(u32 n) {
  Vec out;
  for (const u32 p : sieve().primes)
    if (p <= n) out.push_back(p);
  return out;
}

Vec prev_primes(u64 limit, std::size_t count) {
  Vec out;
  for (u64 n = limit; out.size() < count; --n)
    if (is_prime(n)) out.push_back(static_cast<u32>(n));
  return out;
}

Vec safe_primes_below(u64 limit, std::size_t count) {
  Vec out;
  for (u64 n = limit; out.size() < count; --n)
    if (n % 2 == 1 && is_prime(n) && is_prime((n - 1) / 2)) out.push_back(static_cast<u32>(n));
  return out;
}

void add_unique(Vec& out, u64 v) {
  if (v < 1 || v > kMax32) return;
  for (const u32 w : out)
    if (w == v) return;
  out.push_back(static_cast<u32>(v));
}

Vec set_sq(unsigned c) {
  Vec out;
  const Vec ps = primes_upto(65521);
  for (std::size_t i = 0; i < ps.size(); ++i) {
    const u64 sq = u64{ps[i]} * ps[i];
    if (i % 4 == c && sq <= kMax32) out.push_back(static_cast<u32>(sq));
  }
  return out;
}

Vec set_cube() {
  Vec out;
  for (const u32 p : primes_upto(1626)) {
    const u64 cu = u64{p} * p * p;
    if (cu <= kMax32) out.push_back(static_cast<u32>(cu));
  }
  return out;
}

Vec set_fam_a() {
  Vec out;
  const Vec ps = primes_upto(46340);
  for (std::size_t i = 0; i < ps.size(); ++i) {
    const u64 p = ps[i];
    if (p <= 200 || i % 4 == 0) {
      if (2 * p * p <= kMax32) out.push_back(static_cast<u32>(2 * p * p));
      if (4 * p * p <= kMax32) out.push_back(static_cast<u32>(4 * p * p));
    }
  }
  return out;
}

Vec set_fam_b() {
  Vec out;
  for (const u64 p : {u64{2}, u64{3}, u64{5}, u64{7}, u64{11}, u64{13}}) {
    u64 pk = p;
    while (pk <= kMax32) {
      for (const u64 c : {u64{1}, u64{2}, u64{4}}) add_unique(out, c * pk);
      pk *= p;
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

Vec set_top() {
  Vec out;
  for (const u32 p : prev_primes(kMax32, 30)) add_unique(out, p);
  for (const u32 p : prev_primes((u64{1} << 31) - 1u, 30)) add_unique(out, 2u * u64{p});
  for (const u32 p : prev_primes((u64{1} << 30) - 1u, 30)) add_unique(out, 4u * u64{p});
  for (const u32 p : safe_primes_below(kMax32, 15)) add_unique(out, p);
  for (const u64 v : {u64{998244353}, u64{469762049}, u64{167772161}, u64{1000000007}, u64{1000000009}, (u64{1} << 31) - 1u, u64{65537}, u64{65521} * 65519u})
    add_unique(out, v);
  for (u64 v = (u64{1} << 32) - 64u; v < (u64{1} << 32); ++v) add_unique(out, v);
  return out;
}

Vec set_carm() {
  Vec out{561u, 1105u, 1729u, 2465u, 2821u, 6601u, 8911u, 41041u, 825265u, 1024651u, 1033669u, 1050985u, 1152271u, 5481451u, 10024561u, 118901521u,
          172947529u, 216821881u, 228842209u};
  u64 best = 0;
  for (u64 k = 1; k < 500; ++k) {
    const u64 a = 6 * k + 1;
    const u64 b = 12 * k + 1;
    const u64 c = 18 * k + 1;
    if (a * b * c <= kMax32 && is_prime(a) && is_prime(b) && is_prime(c)) best = a * b * c;
  }
  out.push_back(static_cast<u32>(best));
  return out;
}

Vec set_hc() {
  Vec out;
  for (u64 k = 1; k < 20; ++k)
    if (223092870u * k <= kMax32) out.push_back(static_cast<u32>(223092870u * k));
  for (const u32 v : {4190266080u, 3491888400u, 3859455600u, 3675672000u, 2793510720u, 4073869800u, 3724680960u, 3430627200u}) out.push_back(v);
  return out;
}

Vec set_rnd() {
  Vec out;
  u64 x = 20261128u;
  for (unsigned i = 0; i < 200; ++i) {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    const u32 d = static_cast<u32>(x >> 32);
    const u32 m = (i % 2 == 0) ? (d | 1u) : d;
    out.push_back(m != 0 ? m : 1u);
  }
  return out;
}

}  // namespace

// ---- test cases ------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("mulorder: the header examples", "[mulorder]") {
  Acc acc;
  const std::pair<u32, u32> lam[] = {{1, 1}, {2, 1}, {4, 2}, {8, 2}, {16, 4}, {15, 4}, {24, 2}, {100, 20}, {561, 80}, {1000, 100}};
  for (const auto& p : lam) {
    const Res x = c_lam(p.first);
    acc.expect(x.r == MoResult::Ok && x.v == p.second, "lambda example", p.first, x.v, p.second);
  }
  struct Ord {
    u32 a, m, o;
  };
  const Ord ords[] = {{2, 7, 3}, {3, 7, 6}, {2, 9, 6}, {10, 7, 6}, {2, 1, 1}, {5, 13, 4}, {7, 1000, 20}, {1, 5, 1}, {4, 15, 2}, {2, 101, 100}, {3, 100, 20}};
  for (const Ord& o : ords) {
    const Res x = c_ord(o.a, o.m);
    acc.expect(x.r == MoResult::Ok && x.v == o.o, "order example", o.a, o.m, x.v);
  }
  const std::pair<u32, u32> roots[] = {{1, 0}, {2, 1}, {3, 2}, {4, 3}, {5, 2}, {6, 5}, {7, 3}, {9, 2}, {10, 3}, {14, 3}, {17, 3}, {18, 5}, {25, 2}, {27, 2}, {101, 2}, {998, 7}};
  for (const auto& p : roots) {
    const Res x = c_root(p.first);
    acc.expect(x.r == MoResult::Ok && x.v == p.second, "root example", p.first, x.v, p.second);
  }
  for (const u32 m : {8u, 15u, 100u}) {
    const Res x = c_root(m);
    acc.expect(x.r == MoResult::NoPrimitiveRoot && x.v == kSent, "no root example", m);
  }
  finish(acc);
}

TEST_CASE("mulorder: check order and writes", "[mulorder]") {
  Acc acc;
  // a null out wins over everything, even m = 0 and a non-coprime pair
  acc.expect(carmichael(0, nullptr) == MoResult::NullArgument, "lam null m=0");
  acc.expect(carmichael(7, nullptr) == MoResult::NullArgument, "lam null");
  acc.expect(mult_order(0, 0, nullptr) == MoResult::NullArgument, "ord null m=0");
  acc.expect(mult_order(2, 4, nullptr) == MoResult::NullArgument, "ord null not coprime");
  acc.expect(mult_order(3, 7, nullptr) == MoResult::NullArgument, "ord null");
  acc.expect(primitive_root(0, nullptr) == MoResult::NullArgument, "root null m=0");
  acc.expect(primitive_root(8, nullptr) == MoResult::NullArgument, "root null no root");
  acc.expect(primitive_root(7, nullptr) == MoResult::NullArgument, "root null");
  // m = 0 wins over the coprimality check; nothing is written
  for (const u32 a : {0u, 1u, 5u, kMax32}) {
    const Res x = c_ord(a, 0);
    acc.expect(x.r == MoResult::BadModulus && x.v == kSent, "ord m=0", a, static_cast<u32>(x.r), x.v);
  }
  Res z = c_lam(0);
  acc.expect(z.r == MoResult::BadModulus && z.v == kSent, "lam m=0");
  z = c_root(0);
  acc.expect(z.r == MoResult::BadModulus && z.v == kSent, "root m=0");
  // not coprime (also a = 0 for m > 1, a = m, a a multiple of m); nothing is written
  const std::pair<u32, u32> bad[] = {{0, 2}, {0, 7}, {2, 4}, {4, 6}, {6, 9}, {7, 7}, {14, 7}, {kMax32, 3}, {kMax32, 5}, {10, 1000}, {65537, 65537}, {0, kMax32}};
  for (const auto& p : bad) {
    const Res x = c_ord(p.first, p.second);
    acc.expect(x.r == MoResult::NotCoprime && x.v == kSent, "ord not coprime", p.first, p.second, static_cast<u32>(x.r));
  }
  // m = 1: every a is coprime, the order is 1; and the other values of the singleton
  for (const u32 a : {0u, 1u, 2u, kMax32}) {
    const Res x = c_ord(a, 1);
    acc.expect(x.r == MoResult::Ok && x.v == 1u, "ord m=1", a, x.v);
  }
  z = c_lam(1);
  acc.expect(z.r == MoResult::Ok && z.v == 1u, "lam 1");
  z = c_root(1);
  acc.expect(z.r == MoResult::Ok && z.v == 0u, "root 1");
  // no primitive root: nothing is written, the code is NoPrimitiveRoot (the moduli 8, 12, 15, 16, 21, 24, 100, 2^31, 2^32 - 1, 4 p, p q)
  for (const u32 m : {8u, 12u, 15u, 16u, 21u, 24u, 100u, 1u << 31, kMax32, 4u * 65537u, 65521u * 65519u}) {
    const Res x = c_root(m);
    acc.expect(x.r == MoResult::NoPrimitiveRoot && x.v == kSent, "no root", m, static_cast<u32>(x.r), x.v);
  }
  // a modulus with a root and an a above m: a is taken modulo m
  for (const u32 m : {7u, 9u, 1000u, 65537u}) {
    const Res x = c_ord(2u + m, m);
    const Res y = c_ord(2u, m);
    acc.expect(x.r == y.r && x.v == y.v, "a reduced modulo m", m, x.v, y.v);
  }
  finish(acc);
}

TEST_CASE("mulorder: lambda, order and root closed forms", "[mulorder]") {
  Acc acc;
  // lambda of 2^e: 1, 2, 2^(e - 2); of an odd prime power p^e: p^(e - 1) (p - 1); order(1) = 1; order(m - 1) = 2 for m > 2
  for (u32 e = 1; e <= 31; ++e) {
    const u32 m = 1u << e;
    const u32 want = e == 1 ? 1u : (e == 2 ? 2u : (1u << (e - 2u)));
    const Res x = c_lam(m);
    acc.expect(x.r == MoResult::Ok && x.v == want, "lambda 2^e", e, x.v, want);
    const Res o1 = c_ord(1, m);
    acc.expect(o1.r == MoResult::Ok && o1.v == 1u, "order(1)", m, o1.v);
    if (m > 2) {
      const Res o2 = c_ord(m - 1u, m);
      acc.expect(o2.r == MoResult::Ok && o2.v == 2u, "order(m - 1)", m, o2.v);
    }
    // 5 has order 2^(e - 2) modulo 2^e for e >= 3 (the generator of the cyclic factor)
    if (e >= 3) {
      const Res o5 = c_ord(5, m);
      acc.expect(o5.r == MoResult::Ok && o5.v == (1u << (e - 2u)), "order(5, 2^e)", e, o5.v);
    }
    const Res rt = c_root(m);
    acc.expect((e <= 2) == (rt.r == MoResult::Ok), "root of 2^e only for e <= 2", e);
  }
  for (const u32 p : {3u, 5u, 7u, 13u, 65521u, 4294967291u}) {
    const Res x = c_lam(p);
    acc.expect(x.r == MoResult::Ok && x.v == p - 1u, "lambda of a prime", p, x.v);
    const Res o = c_ord(p - 1u, p);
    acc.expect(o.r == MoResult::Ok && o.v == 2u, "order(p - 1)", p, o.v);
  }
  for (const u32 p : {3u, 5u, 7u, 11u, 13u}) {
    u64 pk = p;
    u32 e = 1;
    while (pk * p <= kMax32) {
      pk *= p;
      ++e;
      const Res x = c_lam(static_cast<u32>(pk));
      acc.expect(x.r == MoResult::Ok && x.v == pk / p * (p - 1u), "lambda p^e", static_cast<u32>(pk), x.v);
      const Res r = c_root(static_cast<u32>(pk));
      acc.expect(r.r == MoResult::Ok, "p^e has a root", static_cast<u32>(pk));
      if (2u * pk <= kMax32) {
        const Res r2 = c_root(static_cast<u32>(2u * pk));
        acc.expect(r2.r == MoResult::Ok, "2 p^e has a root", static_cast<u32>(pk));
      }
    }
  }
  // 998244353 = 119 * 2^23 + 1 is prime and 3 is its least primitive root: the oracle derives it from the definition (the powers g^(phi / q) for the prime
  // divisors q = 2, 7, 17 of phi) and not only the library returns it
  {
    const u32 p = 998244353u;
    acc.expect(is_prime(p), "998244353 prime");
    acc.expect(o_pow(1, (p - 1u) / 2u, p) == 1 && o_pow(2, (p - 1u) / 2u, p) == 1, "1 and 2 are not roots");
    acc.expect(o_pow(3, (p - 1u) / 2u, p) != 1 && o_pow(3, (p - 1u) / 7u, p) != 1 && o_pow(3, (p - 1u) / 17u, p) != 1 && o_pow(3, p - 1u, p) == 1, "3 is a root");
    u32 g = 0;
    acc.expect(o_root(p, g) && g == 3u, "oracle root 998244353");
    const Res r = c_root(p);
    acc.expect(r.r == MoResult::Ok && r.v == 3u, "primitive_root(998244353) = 3", r.v);
    const Res o = c_ord(3, p);
    acc.expect(o.r == MoResult::Ok && o.v == p - 1u, "order(3, 998244353)", o.v);
  }
  finish(acc);
}

TEST_CASE("mulorder: carmichael m = 1 .. 2^17", "[mulorder][lam]") { run_lam(0, "lam_0"); }
TEST_CASE("mulorder: carmichael m = 2^17 + 1 .. 2 * 2^17", "[mulorder][lam]") { run_lam(1, "lam_1"); }
TEST_CASE("mulorder: carmichael m = 2 * 2^17 + 1 .. 3 * 2^17", "[mulorder][lam]") { run_lam(2, "lam_2"); }
TEST_CASE("mulorder: carmichael m = 3 * 2^17 + 1 .. 4 * 2^17", "[mulorder][lam]") { run_lam(3, "lam_3"); }
TEST_CASE("mulorder: carmichael m = 4 * 2^17 + 1 .. 5 * 2^17", "[mulorder][lam]") { run_lam(4, "lam_4"); }
TEST_CASE("mulorder: carmichael m = 5 * 2^17 + 1 .. 6 * 2^17", "[mulorder][lam]") { run_lam(5, "lam_5"); }
TEST_CASE("mulorder: carmichael m = 6 * 2^17 + 1 .. 7 * 2^17", "[mulorder][lam]") { run_lam(6, "lam_6"); }
TEST_CASE("mulorder: carmichael m = 7 * 2^17 + 1 .. 2^20", "[mulorder][lam]") { run_lam(7, "lam_7"); }

TEST_CASE("mulorder: primitive_root m = 1 .. 2^17", "[mulorder][root]") { run_root(0, "root_0"); }
TEST_CASE("mulorder: primitive_root m = 2^17 + 1 .. 2 * 2^17", "[mulorder][root]") { run_root(1, "root_1"); }
TEST_CASE("mulorder: primitive_root m = 2 * 2^17 + 1 .. 3 * 2^17", "[mulorder][root]") { run_root(2, "root_2"); }
TEST_CASE("mulorder: primitive_root m = 3 * 2^17 + 1 .. 4 * 2^17", "[mulorder][root]") { run_root(3, "root_3"); }
TEST_CASE("mulorder: primitive_root m = 4 * 2^17 + 1 .. 5 * 2^17", "[mulorder][root]") { run_root(4, "root_4"); }
TEST_CASE("mulorder: primitive_root m = 5 * 2^17 + 1 .. 6 * 2^17", "[mulorder][root]") { run_root(5, "root_5"); }
TEST_CASE("mulorder: primitive_root m = 6 * 2^17 + 1 .. 7 * 2^17", "[mulorder][root]") { run_root(6, "root_6"); }
TEST_CASE("mulorder: primitive_root m = 7 * 2^17 + 1 .. 2^20", "[mulorder][root]") { run_root(7, "root_7"); }

TEST_CASE("mulorder: mult_order against brute force, m = 1 .. 500, every a", "[mulorder][ord]") { run_ord(1, 500, "ord_0"); }
TEST_CASE("mulorder: mult_order against brute force, m = 501 .. 1000, every a", "[mulorder][ord]") { run_ord(501, 1000, "ord_1"); }
TEST_CASE("mulorder: mult_order against brute force, m = 1001 .. 1500, every a", "[mulorder][ord]") { run_ord(1001, 1500, "ord_2"); }
TEST_CASE("mulorder: mult_order against brute force, m = 1501 .. 2000, every a", "[mulorder][ord]") { run_ord(1501, 2000, "ord_3"); }

TEST_CASE("mulorder: mult_order, 64 stated residues, m = 1 .. 1024", "[mulorder][ord64]") { run_ord64(1, 1024, "ord64_0"); }
TEST_CASE("mulorder: mult_order, 64 stated residues, m = 1025 .. 2048", "[mulorder][ord64]") { run_ord64(1025, 2048, "ord64_1"); }
TEST_CASE("mulorder: mult_order, 64 stated residues, m = 2049 .. 3072", "[mulorder][ord64]") { run_ord64(2049, 3072, "ord64_2"); }
TEST_CASE("mulorder: mult_order, 64 stated residues, m = 3073 .. 4096", "[mulorder][ord64]") { run_ord64(3073, 4096, "ord64_3"); }

TEST_CASE("mulorder: carmichael against the definition, m = 1 .. 3000", "[mulorder][def]") {
  Acc acc;
  Fold f;
  for (u32 m = 1; m <= 3000; ++m) do_lam(acc, f, m, brute_lambda(m));
  finish(acc);
  check_row("lamdef", f);
}

TEST_CASE("mulorder: primitive_root against the definition, m = 1 .. 4096", "[mulorder][def]") {
  Acc acc;
  Fold f;
  for (u32 m = 1; m <= 4096; ++m) {
    u32 g = 0;
    const bool has = brute_root(m, g);
    do_root(acc, f, m, has, g);
  }
  finish(acc);
  check_row("rootdef", f);
}

TEST_CASE("mulorder: prime squares, part 0", "[mulorder][big]") { run_big(set_sq(0), "sq_0"); }
TEST_CASE("mulorder: prime squares, part 1", "[mulorder][big]") { run_big(set_sq(1), "sq_1"); }
TEST_CASE("mulorder: prime squares, part 2", "[mulorder][big]") { run_big(set_sq(2), "sq_2"); }
TEST_CASE("mulorder: prime squares, part 3", "[mulorder][big]") { run_big(set_sq(3), "sq_3"); }
TEST_CASE("mulorder: prime cubes", "[mulorder][big]") { run_big(set_cube(), "cube_0"); }
TEST_CASE("mulorder: 2 p^2 and 4 p^2", "[mulorder][big]") { run_big(set_fam_a(), "fam_a"); }
TEST_CASE("mulorder: p^k, 2 p^k, 4 p^k for p <= 13", "[mulorder][big]") { run_big(set_fam_b(), "fam_b"); }
TEST_CASE("mulorder: the largest primes, safe primes and the top of the range", "[mulorder][big]") { run_big(set_top(), "top"); }
TEST_CASE("mulorder: Carmichael numbers", "[mulorder][big]") { run_big(set_carm(), "carm"); }
TEST_CASE("mulorder: 2^32 - 1 and its divisors", "[mulorder][big]") {
  run_big(divisors(kMax32), "umax");
}
TEST_CASE("mulorder: highly composite moduli", "[mulorder][big]") { run_big(set_hc(), "hc"); }
TEST_CASE("mulorder: seeded random moduli", "[mulorder][big]") { run_big(set_rnd(), "rnd"); }
