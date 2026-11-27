// Tests for core/lagrange. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/lagrange_ref.py, an independent Python program (the definition with
//      a modular inverse per factor, a barycentric form with a table of inverse factorials, and Horner's rule / pow for polynomials and monomials;
//      compared with each other by the script). The tests re-run every case against the real function and recompute the 64-bit digest (n, y, x and
//      the result; for the polynomial and monomial cases n, the coefficients or exponent, and every x with its result) and the number of cases.
//  (2) ORACLE inside the test, sharing no code with core/lagrange/lagrange.cpp: the definition as a double loop over (i, j) with one Fermat inverse
//      per node, in u64; Horner's rule for polynomials; square and multiply for monomials.
//  (3) CONSTANTS and identities: the hand rows, the roots (a polynomial evaluated at its root gives 0), the residue rule (x and x + kMod agree),
//      the check-order and failure matrix.
// After EVERY call y is compared with a copy (it is never modified) and *out is compared with a sentinel on every failure (nothing is written).
// Extents (labelled): EXHAUSTIVE: every y over {0, 1, p - 1} for n = 1..6 and over {0, 1, 2, p - 1} for n = 1..4, each at every x of X(n) (0..n + 6 and
// 14 residue-boundary values); every polynomial of degree below n with coefficients in {0, 1, p - 1} for n = 1..8, y_i = f(i), at the same x list
// (the result must equal Horner's f(x)). SAMPLED, fixed seeds: random y for n = 1..64 and 100, 255, 256, 257, 1000, 4096 (5 values of x each, oracle for
// all); at n = 2^20, 2^20 - 1 and 2^19 + 1 the polynomials 1, p - 1, i, i^2 and random of degree 0, 1, 2, 3, 7, 40, and the monomials i^e for
// e = n - 1, n - 2, n / 2, each at 30 values of x including the node shortcuts (x = 0, 1, n - 1, n, n + 1, p - 1, p, p + n - 1, p + n, 2p, UINT32_MAX);
// two random y at n = 2^20. A bad entry at the first, middle and last position of y at n = 2^20, with x on a node, on the bad node and off the nodes.
// What is NOT done: Windows is CI-only; no timing assertion; no exhaustive y beyond the listed alphabets and n; no other n at 2^20 than the three listed.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lagrange.h"

using namespace lumora::lagrange;

// BEGIN GENERATED REFERENCE (tools/lagrange_ref.py)
struct RefRow {
  const char* name;
  std::size_t cases;
  std::uint64_t digest;
};
constexpr RefRow kRef[] = {
    {"ex3", 28932, 0xA4654A0BB210D29Bull},
    {"ex4", 8368, 0x97ED0D89F1C681D7ull},
    {"poly8", 9840, 0x472362944C550E77ull},
    {"rnd", 350, 0x499AFBB934A789DFull},
    {"bigpoly", 30, 0xDC3F140D6AC6D289ull},
    {"bigmono", 9, 0xE3EDA7D30BB260EAull},
    {"bigrnd", 2, 0x8D7FC86810B53E4Cull},
};
// END GENERATED REFERENCE

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Vec = std::vector<u32>;

constexpr u64 kP = kMod;
constexpr u32 kSent = 0xA5A5A5A5u;
constexpr u32 kMax32 = 0xFFFFFFFFu;

struct Lcg {
  u64 x;
  explicit Lcg(u64 seed) : x(seed) {}
  u32 next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(x >> 32);
  }
};

struct Fold {
  u64 h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  void one(u64 v) { h = (h ^ v) * 0x100000001B3ull; }
  void many(const Vec& a) {
    for (const u32 v : a) one(v);
  }
};

struct Acc {
  bool ok = true;
  std::string first;
  template <class A = int, class B = int>
  void expect(bool cond, const char* what, A a = 0, B b = 0) {
    if (!cond && ok) {
      ok = false;
      first = std::string(what) + " (" + std::to_string(static_cast<u64>(a)) + ", " + std::to_string(static_cast<u64>(b)) + ")";
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

// ---- ORACLE (no shared code with the library) -----------------------------------------------------------------------------------------------

u64 power(u64 base, u64 e) {
  u64 r = 1;
  base %= kP;
  while (e != 0) {
    if ((e & 1u) != 0) r = r * base % kP;
    base = base * base % kP;
    e >>= 1;
  }
  return r;
}

// The definition: sum over i of y_i * prod over j != i of (r - j) / (i - j), one Fermat inverse per node.
u32 o_eval(const Vec& y, u32 x) {
  const u64 n = y.size();
  const u64 r = x % kP;
  if (r < n) return y[r];
  u64 total = 0;
  for (u64 i = 0; i < n; ++i) {
    u64 num = y[i];
    u64 den = 1;
    for (u64 j = 0; j < n; ++j) {
      if (j == i) continue;
      num = num * ((r + kP - j) % kP) % kP;
      den = den * ((i + kP - j) % kP) % kP;
    }
    total = (total + num * power(den, kP - 2)) % kP;
  }
  return static_cast<u32>(total);
}

u32 horner(const Vec& coef, u32 x) {
  const u64 r = x % kP;
  u64 acc = 0;
  for (std::size_t k = coef.size(); k > 0; --k) acc = (acc * r + coef[k - 1]) % kP;
  return static_cast<u32>(acc);
}

// ---- real call with the guards --------------------------------------------------------------------------------------------------------------

// Calls lagrange_eval on y (the vector is compared with its own copy afterwards); on Ok returns the value, on failure checks the sentinel.
u32 call_ok(Acc& acc, const Vec& y, u32 x) {
  const Vec snap = y;
  u32 out = kSent;
  const LagResult r = lagrange_eval(y.data(), static_cast<u32>(y.size()), x, &out);
  acc.expect(r == LagResult::Ok, "Ok expected", y.size(), x);
  acc.expect(y == snap, "y modified", y.size(), x);
  return out;
}

// Same for a large y whose copy is made once by the caller.
u32 call_big(Acc& acc, const Vec& y, const Vec& snap, u32 x) {
  u32 out = kSent;
  const LagResult r = lagrange_eval(y.data(), static_cast<u32>(y.size()), x, &out);
  acc.expect(r == LagResult::Ok, "Ok expected (big)", y.size(), x);
  acc.expect(y == snap, "y modified (big)", y.size(), x);
  return out;
}

void expect_fail(Acc& acc, const Vec& y, u32 n, u32 x, LagResult want, const char* what) {
  const Vec snap = y;
  u32 out = kSent;
  const LagResult r = lagrange_eval(y.data(), n, x, &out);
  acc.expect(r == want, what, n, x);
  acc.expect(out == kSent, "out written on failure", n, x);
  acc.expect(y == snap, "y modified on failure", n, x);
}

Vec make_array(const Vec& alpha, std::size_t n, u64 counter) {
  Vec a(n);
  const u64 base = alpha.size();
  for (std::size_t i = 0; i < n; ++i) {
    a[i] = alpha[counter % base];
    counter /= base;
  }
  return a;
}

u64 ipow(u64 base, u64 e) {
  u64 r = 1;
  for (u64 i = 0; i < e; ++i) r *= base;
  return r;
}

Vec xs_small(u32 n) {
  Vec out;
  for (u32 i = 0; i < n + 7u; ++i) out.push_back(i);
  const u64 cand[] = {kP - 2, kP - 1, kP, kP + 1, kP + n - 1u, kP + n, kP + n + 1u, 2 * kP - 1, 2 * kP, 2 * kP + 1, 3 * kP, 4 * kP, kMax32, kMax32 - 1u};
  for (const u64 v : cand) {
    if (v > kMax32) continue;
    bool seen = false;
    for (const u32 w : out) seen = seen || w == v;
    if (!seen) out.push_back(static_cast<u32>(v));
  }
  return out;
}

Vec xs_big(u32 n) {
  Vec out = {0u, 1u, n - 1u, n, n + 1u, kMod - 1u, kMod, kMod + 5u, kMod + n - 1u, kMod + n, 2u * kMod, kMax32};
  Lcg g(n);
  for (int i = 0; i < 18; ++i) out.push_back(g.next());
  return out;
}

u32 draw_entry(Lcg& g) {
  const u32 d1 = g.next();
  const u32 d2 = g.next();
  switch (d1 % 5u) {
    case 0: return 0;
    case 1: return 1;
    case 2: return kMod - 1u;
    case 3: return kMod - 2u;
    default: return static_cast<u32>(d2 % kP);
  }
}

void value_case(Fold& f, const Vec& y, u32 x, u32 res) {
  f.one(y.size());
  f.many(y);
  f.one(x);
  f.one(res);
  ++f.cases;
}

const u32 kBigN[3] = {1u << 20, (1u << 20) - 1u, (1u << 19) + 1u};

std::vector<Vec> big_specs() {
  std::vector<Vec> specs = {{1u}, {kMod - 1u}, {0u, 1u}, {0u, 0u, 1u}};
  for (const u32 deg : {0u, 1u, 2u, 3u, 7u, 40u}) {
    Lcg g(9100u + deg);
    Vec c;
    for (u32 k = 0; k <= deg; ++k) c.push_back(static_cast<u32>(g.next() % kP));
    specs.push_back(c);
  }
  return specs;
}

}  // namespace

TEST_CASE("lagrange: exhaustive small y against the oracle and the digests", "[lagrange]") {
  struct Spec {
    const char* name;
    Vec alpha;
    u32 nmax;
  };
  const Spec specs[] = {{"ex3", {0u, 1u, kMod - 1u}, 6}, {"ex4", {0u, 1u, 2u, kMod - 1u}, 4}};
  for (const Spec& sp : specs) {
    Acc acc;
    Fold f;
    for (u32 n = 1; n <= sp.nmax; ++n) {
      const Vec xl = xs_small(n);
      const u64 total = ipow(sp.alpha.size(), n);
      for (u64 c = 0; c < total; ++c) {
        const Vec y = make_array(sp.alpha, n, c);
        for (const u32 x : xl) {
          const u32 got = call_ok(acc, y, x);
          acc.expect(got == o_eval(y, x), "value vs oracle", n, x);
          value_case(f, y, x, got);
        }
      }
    }
    finish(acc);
    check_row(sp.name, f);
  }
}

TEST_CASE("lagrange: every polynomial over {0, 1, p - 1} of degree below n, n = 1..8, against Horner and the digest", "[lagrange]") {
  Acc acc;
  Fold f;
  const Vec alpha = {0u, 1u, kMod - 1u};
  for (u32 n = 1; n <= 8; ++n) {
    const Vec xl = xs_small(n);
    const u64 total = ipow(3, n);
    for (u64 c = 0; c < total; ++c) {
      const Vec coef = make_array(alpha, n, c);
      Vec y(n);
      for (u32 i = 0; i < n; ++i) y[i] = horner(coef, i);
      f.one(n);
      f.one(coef.size());
      f.many(coef);
      for (const u32 x : xl) {
        const u32 got = call_ok(acc, y, x);
        acc.expect(got == horner(coef, x), "polynomial value", n, x);
        f.one(x);
        f.one(got);
      }
      ++f.cases;
    }
  }
  finish(acc);
  check_row("poly8", f);
}

TEST_CASE("lagrange: seeded random y, n = 1..64 and larger, against the oracle and the digest", "[lagrange]") {
  Acc acc;
  Fold f;
  std::vector<u32> ns;
  for (u32 n = 1; n <= 64; ++n) ns.push_back(n);
  for (const u32 n : {100u, 255u, 256u, 257u, 1000u, 4096u}) ns.push_back(n);
  for (const u32 n : ns) {
    Lcg g(4000u + n);
    Vec y(n);
    for (u32& v : y) v = draw_entry(g);
    Vec xl;
    xl.push_back(g.next() % n);
    xl.push_back(n);
    xl.push_back(n + g.next() % 7u);
    xl.push_back(g.next());
    const u64 big = kP + g.next() % (n + 3u);
    if (big <= kMax32) xl.push_back(static_cast<u32>(big));
    for (const u32 x : xl) {
      const u32 got = call_ok(acc, y, x);
      acc.expect(got == o_eval(y, x), "value vs oracle", n, x);
      value_case(f, y, x, got);
    }
  }
  finish(acc);
  check_row("rnd", f);
}

TEST_CASE("lagrange: a polynomial of degree d sampled at n > d nodes gives the polynomial for every n", "[lagrange]") {
  Acc acc;
  const Vec coef = {3u, kMod - 7u, 11u, 0u, 5u, kMod - 1u};  // degree 5
  for (u32 n = 6; n <= 40; ++n) {
    Vec y(n);
    for (u32 i = 0; i < n; ++i) y[i] = horner(coef, i);
    for (const u32 x : {n, n + 1u, n + 17u, kMod - 3u, kMod, kMod + n, kMax32}) acc.expect(call_ok(acc, y, x) == horner(coef, x), "poly of degree 5", n, x);
  }
  finish(acc);
}

TEST_CASE("lagrange: the residue rule, roots and hand rows", "[lagrange]") {
  Acc acc;
  // n = 1: y[0] for every x.
  for (const u32 x : {0u, 1u, 5u, kMod - 1u, kMod, kMod + 1u, kMax32}) acc.expect(call_ok(acc, {42u}, x) == 42u, "n = 1", x);
  acc.expect(call_ok(acc, {kMod - 1u}, 7u) == kMod - 1u, "n = 1, p - 1", 7);
  // x acts as its residue: x = p acts as 0, x = p + 2 as 2, x = 2^32 - 1 as 2^32 - 1 - 4p.
  const Vec y3 = {5u, 7u, 11u};
  acc.expect(call_ok(acc, y3, kMod) == 5u, "x = p acts as 0", 0);
  acc.expect(call_ok(acc, y3, kMod + 2u) == 11u, "x = p + 2 acts as 2", 2);
  acc.expect(call_ok(acc, y3, 2u * kMod + 1u) == 7u, "x = 2p + 1 acts as 1", 1);
  acc.expect(call_ok(acc, y3, 10u) == 115u, "f(10) = 115 for y = 5, 7, 11", 10);
  acc.expect(call_ok(acc, y3, 10u + kMod) == 115u, "x = p + 10", 10);
  // f(x) = 1 + x (n = 2): zero at x = p - 1, where the two terms add up to exactly p.
  const Vec lin = {1u, 2u};
  acc.expect(call_ok(acc, lin, kMod - 1u) == 0u, "root at p - 1 (terms sum to p)", 0);
  acc.expect(call_ok(acc, lin, 2u) == 3u, "f(2) = 3", 2);
  // f(x) = x^2 at n = 3.
  const Vec sq = {0u, 1u, 4u};
  acc.expect(call_ok(acc, sq, 3u) == 9u, "x^2 at 3", 3);
  acc.expect(call_ok(acc, sq, kMod + 3u) == 9u, "x^2 at p + 3", 3);
  acc.expect(call_ok(acc, sq, kMax32) == static_cast<u32>(power(kMax32, 2)), "x^2 at 2^32 - 1", 0);
  // roots: f(x) = x - c has the value 0 at c (the final fraction adds up to exactly p), for c off the nodes.
  for (const u32 n : {2u, 3u, 5u, 9u})
    for (const u32 c : {n, n + 1u, kMod - 2u, kMod - 1u}) {
      Vec y(n);
      for (u32 i = 0; i < n; ++i) y[i] = static_cast<u32>((i + kP - c) % kP);
      acc.expect(call_ok(acc, y, c) == 0u, "root of x - c", n, c);
      acc.expect(call_ok(acc, y, c + 1u) == 1u, "f(c + 1) = 1 for f = x - c", n, c);
    }
  // a polynomial with value p - 1 everywhere and values 0, 1 at one point.
  for (const u32 n : {2u, 5u}) acc.expect(call_ok(acc, Vec(n, kMod - 1u), n + 100u) == kMod - 1u, "constant p - 1", n);
  for (const u32 n : {2u, 5u}) acc.expect(call_ok(acc, Vec(n, 1u), n + 100u) == 1u, "constant 1", n);
  for (const u32 n : {2u, 5u}) acc.expect(call_ok(acc, Vec(n, 0u), n + 100u) == 0u, "constant 0", n);
  finish(acc);
}

TEST_CASE("lagrange: failures, check order, *out and y untouched", "[lagrange]") {
  Acc acc;
  const Vec good = {1u, 2u, 3u};
  u32 out = kSent;
  // Null pointers win over everything.
  for (const u32 n : {0u, 1u, 3u, (1u << 20) + 1u, kMax32}) {
    for (const u32 x : {0u, 7u, kMax32}) {
      out = kSent;
      acc.expect(lagrange_eval(nullptr, n, x, &out) == LagResult::NullArgument, "null y", n, x);
      acc.expect(out == kSent, "null y wrote", n, x);
      acc.expect(lagrange_eval(good.data(), n, x, nullptr) == LagResult::NullArgument, "null out", n, x);
      acc.expect(lagrange_eval(nullptr, n, x, nullptr) == LagResult::NullArgument, "both null", n, x);
    }
  }
  // n = 0 (EmptyInput) with valid pointers, also with a bad y entry that is never read.
  for (const u32 x : {0u, 5u, kMax32}) expect_fail(acc, Vec{kMod}, 0, x, LagResult::EmptyInput, "EmptyInput");
  // TooLarge beats ValueOutOfRange; y (one word holding p) is never read.
  for (const u32 n : {(1u << 20) + 1u, (1u << 20) + 2u, 0x7FFFFFFFu, kMax32})
    for (const u32 x : {0u, 5u, kMax32}) expect_fail(acc, Vec{kMod}, n, x, LagResult::TooLarge, "TooLarge");
  // ValueOutOfRange: every position class, three bad values, x on a good node, on the bad node, off the nodes.
  for (u32 n = 1; n <= 5; ++n) {
    const u32 poss[] = {0u, n / 2u, n - 1u};
    for (const u32 pos : poss)
      for (const u32 bad : {kMod, kMod + 1u, kMax32})
        for (const u32 x : {0u, n - 1u, pos, n, kMod + pos, kMax32}) {
          Vec y(n, 1u);
          y[pos] = bad;
          expect_fail(acc, y, n, x, LagResult::ValueOutOfRange, "ValueOutOfRange");
        }
    if (n >= 2) {
      Vec y(n, 1u);
      y[0] = kMod;
      y[n - 1u] = kMax32;
      expect_fail(acc, y, n, 1u, LagResult::ValueOutOfRange, "two bad entries");
    }
  }
  // The largest residue is accepted.
  acc.expect(call_ok(acc, Vec(3, kMod - 1u), 9u) == kMod - 1u, "p - 1 entries accepted", 3);
  // At the maximum n: a bad entry at the first, middle and last position, with requests that would hit the node shortcut.
  {
    const u32 n = 1u << 20;
    const u32 poss[] = {0u, n / 2u, n - 1u};
    for (const u32 pos : poss) {
      Vec y(n, 7u);
      y[pos] = kMod;
      const Vec snap = y;
      for (const u32 x : {0u, 1u, pos, n - 1u, n, kMod + pos, 2u * kMod, kMax32}) {
        out = kSent;
        const LagResult r = lagrange_eval(y.data(), n, x, &out);
        acc.expect(r == LagResult::ValueOutOfRange, "bad entry at the maximum n", pos, x);
        acc.expect(out == kSent, "out written at the maximum n", pos, x);
        acc.expect(y == snap, "y modified at the maximum n", pos, x);
      }
    }
  }
  // n = 2^20 + 1 with a real buffer of n words is still TooLarge and untouched.
  {
    const Vec y((1u << 20) + 1u, 3u);
    expect_fail(acc, y, (1u << 20) + 1u, 9u, LagResult::TooLarge, "TooLarge at 2^20 + 1");
  }
  finish(acc);
}

TEST_CASE("lagrange: n = 2^20, 2^20 - 1, 2^19 + 1, polynomials against Horner and the digest", "[lagrange]") {
  Acc acc;
  Fold f;
  for (const u32 n : kBigN) {
    const Vec xl = xs_big(n);
    for (const Vec& coef : big_specs()) {
      Vec y(n);
      for (u32 i = 0; i < n; ++i) y[i] = horner(coef, i);
      const Vec snap = y;
      f.one(n);
      f.one(coef.size());
      f.many(coef);
      for (const u32 x : xl) {
        const u32 got = call_big(acc, y, snap, x);
        acc.expect(got == horner(coef, x), "polynomial at the large n", n, x);
        f.one(x);
        f.one(got);
      }
      ++f.cases;
    }
  }
  finish(acc);
  check_row("bigpoly", f);
}

TEST_CASE("lagrange: n = 2^20, 2^20 - 1, 2^19 + 1, monomials y[i] = i^e against r^e and the digest", "[lagrange]") {
  Acc acc;
  Fold f;
  for (const u32 n : kBigN) {
    const Vec xl = xs_big(n);
    for (const u32 e : {n - 1u, n - 2u, n / 2u}) {
      Vec y(n);
      for (u32 i = 0; i < n; ++i) y[i] = static_cast<u32>(power(i, e));
      const Vec snap = y;
      f.one(n);
      f.one(e);
      for (const u32 x : xl) {
        const u32 got = call_big(acc, y, snap, x);
        acc.expect(got == power(x, e), "monomial at the large n", n, x);
        f.one(x);
        f.one(got);
      }
      ++f.cases;
    }
  }
  finish(acc);
  check_row("bigmono", f);
}

TEST_CASE("lagrange: n = 2^20, two random y against the digest", "[lagrange]") {
  Acc acc;
  Fold f;
  const u32 n = 1u << 20;
  Lcg g(9900);
  Vec y(n);
  for (u32& v : y) v = static_cast<u32>(g.next() % kP);
  const Vec snap = y;
  for (const u32 x : {n + 3u, static_cast<u32>(kP + 12345u)}) {
    const u32 got = call_big(acc, y, snap, x);
    value_case(f, y, x, got);
  }
  finish(acc);
  check_row("bigrnd", f);
}
