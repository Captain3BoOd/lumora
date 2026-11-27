// Tests for core/walsh. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/walsh_ref.py, an independent Python program (the definitions as
//      sums, a recursive-halving formulation, and sparse sums; compared with each other by the script). The tests re-run every case against the real
//      functions and recompute the 64-bit digest (k, the inputs and the outputs) and the number of cases.
//  (2) ORACLE inside the test, sharing no code with core/walsh/walsh.cpp: the definitions as sums over (s, t) with a bit-counting loop, the inverse
//      scaled by a power computed with Fermat's little theorem, the convolution as the double loop, all in u64 with a reduction at the end.
//  (3) CLOSED FORMS written from the header: transforms of unit vectors, of the all-ones array, of the parity pattern and of the upper-half indicator,
//      the convolution of two unit vectors, the check-order and failure matrix, and identities (inverse after transform, Mobius after zeta,
//      WHT twice is 2^k times the array, the convolution theorem, commutativity, e_0 as identity).
// Buffers carry sentinels: 4 words before and 4 after the used part; on success the words outside the written range stay unchanged and on every
// failure the whole buffer (out AND scratch for the convolution) stays unchanged.
// Extents (labelled): EXHAUSTIVE (all four transforms and the oracle): every array over {0, 1, p - 1} for k = 0..3 and over {0, 1, 2, p - 1} for
// k = 0..2; every unit vector for k = 0..12; EXHAUSTIVE convolution: every pair over {0, 1, p - 1} for k = 0..2, over {0, 1, 2, p - 1} for k = 0..1,
// over {0, 1} for k = 3, every pair of unit vectors for k = 0..6. SAMPLED: unit vectors for k = 13..20 on an index list (k = 13..16: 0, 1, every power of two, 2^j - 1 for j = 2..k, four
// seeded random; k = 17..20: a thinner list, see index_list); fixed-seed random arrays for k = 0..16 (oracle for k <= 8, identities for all); at k = 20 a sparse pair, a
// dense array with a sparse array, and three dense structured arrays (all ones, parity pattern, upper-half indicator) against closed forms.
// What is NOT done: Windows is CI-only; overlapping arrays are not tested (the header leaves them open); no timing assertion; no full sweep of the unit
// vectors for k = 13..20 and no digest for the unit vectors at k = 11 and 12 (closed forms only).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "walsh.h"

using namespace lumora::walsh;

// BEGIN GENERATED REFERENCE (tools/walsh_ref.py)
struct RefRow {
  const char* name;
  std::size_t cases;
  std::uint64_t digest;
};
constexpr RefRow kRef[] = {
    {"ex3", 6654, 0x40907D64C95C99B0ull},
    {"ex4", 276, 0x80C98B9C54593554ull},
    {"cv3", 6651, 0xA8174D93B3C33D68ull},
    {"cv4", 272, 0x3C8C2D215DB50681ull},
    {"cvb", 65536, 0x032BF01C21D490E5ull},
    {"unit", 2047, 0x9D508B01A747DCEDull},
    {"upair", 5461, 0x8C0C332D5C1A6858ull},
    {"rnd", 33, 0x91158E7507083D72ull},
    {"dense", 6, 0x5B74DCCC372ED37Dull},
    {"sp20", 1, 0x369EA838C8FAD314ull},
    {"cd20", 1, 0xB779F1401E31E80Full},
};
// END GENERATED REFERENCE

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Vec = std::vector<u32>;
using TFn = WalshResult (*)(u32*, u32);

constexpr u32 kSent = 0xA5A5A5A5u;
constexpr u32 kPad = 4;
constexpr u64 kP = kMod;

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

// Collects failures without an assertion per word: the first failure is remembered and reported once.
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

const TFn kTransforms[4] = {walsh_xor, walsh_xor_inverse, subset_zeta, subset_mobius};
const char* const kNames[4] = {"walsh_xor", "walsh_xor_inverse", "subset_zeta", "subset_mobius"};

// ---- ORACLE (definitions, no shared code with the library) ---------------------------------------------------------------------------------

bool par(u64 x) {
  x ^= x >> 32;
  x ^= x >> 16;
  x ^= x >> 8;
  x ^= x >> 4;
  x ^= x >> 2;
  x ^= x >> 1;
  return (x & 1u) != 0;
}

u32 pop(u64 x) {
  u32 c = 0;
  while (x != 0) {
    c += static_cast<u32>(x & 1u);
    x >>= 1;
  }
  return c;
}

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

u64 inv_pow2(u32 k) { return power(power(2, k), kP - 2); }  // (2^k)^(p - 2) = (2^k)^-1 by Fermat

Vec o_wht(const Vec& a) {
  const std::size_t n = a.size();
  Vec out(n);
  for (std::size_t s = 0; s < n; ++s) {
    u64 acc = 0;
    for (std::size_t t = 0; t < n; ++t) acc += par(s & t) ? kP - a[t] : a[t];
    out[s] = static_cast<u32>(acc % kP);
  }
  return out;
}

Vec o_inv(const Vec& a, u32 k) {
  Vec w = o_wht(a);
  const u64 f = inv_pow2(k);
  for (u32& v : w) v = static_cast<u32>(v * f % kP);
  return w;
}

Vec o_zeta(const Vec& a) {
  const std::size_t n = a.size();
  Vec out(n);
  for (std::size_t s = 0; s < n; ++s) {
    u64 acc = 0;
    for (std::size_t t = 0; t < n; ++t)
      if ((t & s) == t) acc += a[t];
    out[s] = static_cast<u32>(acc % kP);
  }
  return out;
}

Vec o_mobius(const Vec& a) {
  const std::size_t n = a.size();
  Vec out(n);
  for (std::size_t s = 0; s < n; ++s) {
    u64 acc = 0;
    for (std::size_t t = 0; t < n; ++t)
      if ((t & s) == t) acc += par(s ^ t) ? kP - a[t] : a[t];
    out[s] = static_cast<u32>(acc % kP);
  }
  return out;
}

Vec o_conv(const Vec& a, const Vec& b) {
  const std::size_t n = a.size();
  Vec out(n);
  for (std::size_t s = 0; s < n; ++s) {
    u64 acc = 0;
    for (std::size_t t = 0; t < n; ++t) acc = (acc + static_cast<u64>(a[t]) * b[s ^ t]) % kP;
    out[s] = static_cast<u32>(acc);
  }
  return out;
}

Vec o_apply(int which, const Vec& a, u32 k) {
  switch (which) {
    case 0: return o_wht(a);
    case 1: return o_inv(a, k);
    case 2: return o_zeta(a);
    default: return o_mobius(a);
  }
}

// ---- real calls -----------------------------------------------------------------------------------------------------------------------------

Vec apply(Acc& acc, int which, const Vec& a, u32 k) {
  Vec r = a;
  const WalshResult res = kTransforms[which](r.data(), k);
  acc.expect(res == WalshResult::Ok, kNames[which], k);
  return r;
}

Vec convolve(Acc& acc, const Vec& a, const Vec& b, u32 k) {
  const std::size_t n = a.size();
  Vec out(n + 2u * kPad, kSent);
  Vec scr(n + 2u * kPad, kSent);
  const WalshResult res = xor_convolve(a.data(), b.data(), k, scr.data() + kPad, static_cast<u32>(n), out.data() + kPad);
  acc.expect(res == WalshResult::Ok, "xor_convolve", k);
  bool pads = true;
  for (u32 i = 0; i < kPad; ++i)
    pads = pads && out[i] == kSent && out[n + kPad + i] == kSent && scr[i] == kSent && scr[n + kPad + i] == kSent;
  acc.expect(pads, "sentinel words around out or scratch changed", k);
  return Vec(out.begin() + kPad, out.begin() + static_cast<std::ptrdiff_t>(kPad + n));
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

// One transform case: the digest folds k, a and the four outputs; every output is compared with the oracle when oracle is true.
void transform_case(Acc& acc, Fold& f, const Vec& a, u32 k, bool oracle) {
  f.one(k);
  f.many(a);
  for (int w = 0; w < 4; ++w) {
    const Vec r = apply(acc, w, a, k);
    f.many(r);
    if (oracle) acc.expect(r == o_apply(w, a, k), kNames[w], k);
  }
  ++f.cases;
}

void conv_case(Acc& acc, Fold& f, const Vec& a, const Vec& b, u32 k, bool oracle) {
  const Vec r = convolve(acc, a, b, k);
  f.one(k);
  f.many(a);
  f.many(b);
  f.many(r);
  if (oracle) acc.expect(r == o_conv(a, b), "xor_convolve vs oracle", k);
  ++f.cases;
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

// Identities that hold for every a, b: checked on top of the oracle or digest.
void identities(Acc& acc, const Vec& a, const Vec& b, u32 k) {
  const std::size_t n = a.size();
  const Vec w = apply(acc, 0, a, k);
  acc.expect(apply(acc, 1, w, k) == a, "inverse after transform", k);
  const Vec ww = apply(acc, 0, w, k);
  bool twice = true;
  const u64 nm = (static_cast<u64>(1) << k) % kP;
  for (std::size_t i = 0; i < n; ++i) twice = twice && ww[i] == a[i] * nm % kP;
  acc.expect(twice, "walsh twice is 2^k times a", k);
  const Vec z = apply(acc, 2, a, k);
  acc.expect(apply(acc, 3, z, k) == a, "mobius after zeta", k);
  const Vec m = apply(acc, 3, a, k);
  acc.expect(apply(acc, 2, m, k) == a, "zeta after mobius", k);
  const Vec c = convolve(acc, a, b, k);
  acc.expect(convolve(acc, b, a, k) == c, "convolution commutes", k);
  Vec e0(n, 0);
  e0[0] = 1;
  acc.expect(convolve(acc, a, e0, k) == a, "e_0 is the identity", k);
  const Vec wc = apply(acc, 0, c, k);
  const Vec wb = apply(acc, 0, b, k);
  bool thm = true;
  for (std::size_t i = 0; i < n; ++i) thm = thm && wc[i] == static_cast<u64>(w[i]) * wb[i] % kP;
  acc.expect(thm, "convolution theorem", k);
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

// Unit vector closed forms (written from the definitions): entry s of each transform of e_i.
u32 unit_expected(int which, u32 i, u32 s, u64 invn) {
  const bool odd = par(s & i);
  switch (which) {
    case 0: return odd ? kMod - 1u : 1u;
    case 1: return static_cast<u32>((odd ? kP - 1u : 1u) * invn % kP);
    case 2: return (s & i) == i ? 1u : 0u;
    default: return (s & i) == i ? (par(s ^ i) ? kMod - 1u : 1u) : 0u;
  }
}

// Runs the four transforms of the unit vector e_i in place on buf (n entries) and checks every entry against the closed form.
void unit_check(Acc& acc, Vec& buf, u32 k, u32 i, Fold* f) {
  const std::size_t n = std::size_t{1} << k;
  const u64 invn = inv_pow2(k);
  if (f != nullptr) {
    f->one(k);
    Vec e(n, 0);
    e[i] = 1;
    f->many(e);
  }
  for (int w = 0; w < 4; ++w) {
    std::fill(buf.begin(), buf.end(), 0u);
    buf[i] = 1;
    const WalshResult res = kTransforms[w](buf.data(), k);
    acc.expect(res == WalshResult::Ok, kNames[w], k, i);
    bool same = true;
    for (u32 s = 0; s < n; ++s) same = same && buf[s] == unit_expected(w, i, s, invn);
    acc.expect(same, kNames[w], k, i);
    if (f != nullptr) f->many(buf);
  }
  if (f != nullptr) ++f->cases;
}

Vec index_list(u32 k) {
  const u32 n = 1u << k;
  Vec idx = {0, 1};
  if (k <= 16) {
    for (u32 j = 1; j < k; ++j) idx.push_back(1u << j);
    for (u32 j = 2; j <= k; ++j) idx.push_back((1u << j) - 1u);
  } else {  // k = 17..20: a thinner list (the transforms of 2^20 entries are the cost of this test)
    for (const u32 j : {1u, 2u, k / 2u - 1u, k / 2u, k / 2u + 1u, k - 2u, k - 1u}) idx.push_back(1u << j);
    for (const u32 j : {2u, k / 2u, k - 1u, k}) idx.push_back((1u << j) - 1u);
  }
  Lcg g(7100u + k);
  for (int r = 0; r < (k <= 16 ? 4 : 2); ++r) idx.push_back(g.next() & (n - 1u));
  std::sort(idx.begin(), idx.end());
  idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
  return idx;
}

struct Nz {
  u32 pos;
  u32 val;
};

std::vector<Nz> sparse3(Lcg& g) {
  std::vector<Nz> nz(3);
  for (Nz& e : nz) e.pos = g.next() & ((1u << 20) - 1u);
  for (Nz& e : nz) e.val = static_cast<u32>(g.next() % kP);
  nz[1].val = kMod - 1u;
  return nz;
}

Vec dense_from(const std::vector<Nz>& nz, std::size_t n) {
  Vec a(n, 0);
  for (const Nz& e : nz) a[e.pos] = static_cast<u32>((a[e.pos] + static_cast<u64>(e.val)) % kP);
  return a;
}

std::vector<Nz> merged(const Vec& dense, const std::vector<Nz>& nz) {
  std::vector<u32> pos;
  for (const Nz& e : nz) pos.push_back(e.pos);
  std::sort(pos.begin(), pos.end());
  pos.erase(std::unique(pos.begin(), pos.end()), pos.end());
  std::vector<Nz> out;
  for (const u32 p : pos) out.push_back({p, dense[p]});
  return out;
}

}  // namespace

TEST_CASE("walsh: exhaustive small arrays, all four transforms against the oracle and the digests", "[walsh]") {
  Acc acc;
  Fold f3;
  const Vec alpha3 = {0u, 1u, kMod - 1u};
  for (u32 k = 0; k <= 3; ++k) {
    const std::size_t n = std::size_t{1} << k;
    const u64 total = ipow(3, n);
    for (u64 c = 0; c < total; ++c) transform_case(acc, f3, make_array(alpha3, n, c), k, true);
  }
  finish(acc);
  check_row("ex3", f3);
  Acc acc4;
  Fold f4;
  const Vec alpha4 = {0u, 1u, 2u, kMod - 1u};
  for (u32 k = 0; k <= 2; ++k) {
    const std::size_t n = std::size_t{1} << k;
    const u64 total = ipow(4, n);
    for (u64 c = 0; c < total; ++c) transform_case(acc4, f4, make_array(alpha4, n, c), k, true);
  }
  finish(acc4);
  check_row("ex4", f4);
}

TEST_CASE("walsh: exhaustive pairs for xor_convolve against the oracle and the digests", "[walsh]") {
  struct Spec {
    const char* name;
    Vec alpha;
    u32 kmin;
    u32 kmax;
  };
  const Spec specs[] = {{"cv3", {0u, 1u, kMod - 1u}, 0, 2}, {"cv4", {0u, 1u, 2u, kMod - 1u}, 0, 1}, {"cvb", {0u, 1u}, 3, 3}};
  for (const Spec& sp : specs) {
    Acc acc;
    Fold f;
    for (u32 k = sp.kmin; k <= sp.kmax; ++k) {
      const std::size_t n = std::size_t{1} << k;
      const u64 total = ipow(sp.alpha.size(), n);
      std::vector<Vec> arrs;
      for (u64 c = 0; c < total; ++c) arrs.push_back(make_array(sp.alpha, n, c));
      for (const Vec& a : arrs)
        for (const Vec& b : arrs) conv_case(acc, f, a, b, k, true);
    }
    finish(acc);
    check_row(sp.name, f);
  }
}

TEST_CASE("walsh: every unit vector for k = 0..12, four transforms, closed forms (digest through k = 10)", "[walsh]") {
  Acc acc;
  Fold f;
  for (u32 k = 0; k <= 12; ++k) {
    Vec buf(std::size_t{1} << k);
    for (u32 i = 0; i < (1u << k); ++i) unit_check(acc, buf, k, i, k <= 10 ? &f : nullptr);
  }
  finish(acc);
  check_row("unit", f);
  // The closed forms themselves against the oracle (definitions), every unit vector for k <= 6.
  Acc cf;
  for (u32 k = 0; k <= 6; ++k) {
    const std::size_t n = std::size_t{1} << k;
    const u64 invn = inv_pow2(k);
    for (u32 i = 0; i < n; ++i) {
      Vec e(n, 0);
      e[i] = 1;
      for (int w = 0; w < 4; ++w) {
        const Vec o = o_apply(w, e, k);
        bool same = true;
        for (u32 s = 0; s < n; ++s) same = same && o[s] == unit_expected(w, i, s, invn);
        cf.expect(same, "closed form vs oracle", k, i);
      }
    }
  }
  finish(cf);
}

TEST_CASE("walsh: unit vectors on an index list for k = 13..20, four transforms, closed forms", "[walsh]") {
  Acc acc;
  for (u32 k = 13; k <= kMaxK; ++k) {
    Vec buf(std::size_t{1} << k);
    for (const u32 i : index_list(k)) unit_check(acc, buf, k, i, nullptr);
  }
  finish(acc);
}

TEST_CASE("walsh: convolution of every pair of unit vectors for k = 0..6", "[walsh]") {
  Acc acc;
  Fold f;
  for (u32 k = 0; k <= 6; ++k) {
    const std::size_t n = std::size_t{1} << k;
    for (u32 i = 0; i < n; ++i)
      for (u32 j = 0; j < n; ++j) {
        Vec a(n, 0), b(n, 0), want(n, 0);
        a[i] = 1;
        b[j] = 1;
        want[i ^ j] = 1;
        const Vec r = convolve(acc, a, b, k);
        acc.expect(r == want, "e_i * e_j = e_(i xor j)", i, j);
        f.one(k);
        f.one(i);
        f.one(j);
        f.many(r);
        ++f.cases;
      }
  }
  finish(acc);
  check_row("upair", f);
}

TEST_CASE("walsh: seeded random arrays for k = 0..16, digests, oracle for k <= 8, identities", "[walsh]") {
  Acc acc;
  Fold fr, fd;
  for (u32 k = 0; k <= 16; ++k) {
    const int reps = k <= 10 ? 3 : 1;
    for (int r = 0; r < reps; ++r) {
      const u64 seed = k <= 10 ? 5000u + 10u * k + static_cast<u32>(r) : 6000u + k;
      Lcg g(seed);
      const std::size_t n = std::size_t{1} << k;
      Vec a(n), b(n);
      for (u32& v : a) v = draw_entry(g);
      for (u32& v : b) v = draw_entry(g);
      Fold& f = k <= 10 ? fr : fd;
      f.one(k);
      f.many(a);
      f.many(b);
      for (int w = 0; w < 4; ++w) {
        const Vec t = apply(acc, w, a, k);
        f.many(t);
        if (k <= 8) acc.expect(t == o_apply(w, a, k), kNames[w], k);
      }
      const Vec c = convolve(acc, a, b, k);
      f.many(c);
      if (k <= 8) acc.expect(c == o_conv(a, b), "xor_convolve vs oracle", k);
      ++f.cases;
      identities(acc, a, b, k);
    }
  }
  finish(acc);
  check_row("rnd", fr);
  check_row("dense", fd);
}

TEST_CASE("walsh: k = 20, a sparse pair and a dense array with a sparse array, against sparse sums and the digests", "[walsh]") {
  const std::size_t n = std::size_t{1} << 20;
  Acc acc;
  {
    Fold f;
    Lcg g(7020);
    const std::vector<Nz> nza = sparse3(g);
    const std::vector<Nz> nzb = sparse3(g);
    const Vec a = dense_from(nza, n);
    const Vec b = dense_from(nzb, n);
    const std::vector<Nz> ma = merged(a, nza);
    const std::vector<Nz> mb = merged(b, nzb);
    f.one(20);
    f.many(a);
    f.many(b);
    const u64 invn = inv_pow2(20);
    for (int w = 0; w < 4; ++w) {
      const Vec t = apply(acc, w, a, 20);
      f.many(t);
      bool same = true;
      for (u32 s = 0; s < n; ++s) {
        u64 want = 0;
        for (const Nz& e : ma) {
          const bool sub = (e.pos & s) == e.pos;
          const bool odd_wht = par(s & e.pos);
          const bool odd_mob = par(s ^ e.pos);
          if (w == 0) want += odd_wht ? kP - e.val : e.val;
          if (w == 1) want += (odd_wht ? kP - e.val : e.val) * invn % kP;
          if (w == 2 && sub) want += e.val;
          if (w == 3 && sub) want += odd_mob ? kP - e.val : e.val;
        }
        same = same && t[s] == want % kP;
      }
      acc.expect(same, kNames[w], 20);
    }
    const Vec c = convolve(acc, a, b, 20);
    f.many(c);
    Vec want(n, 0);
    for (const Nz& x : ma)
      for (const Nz& y : mb) {
        const std::size_t at = x.pos ^ y.pos;
        want[at] = static_cast<u32>((want[at] + static_cast<u64>(x.val) * y.val) % kP);
      }
    acc.expect(c == want, "xor_convolve sparse pair", 20);
    ++f.cases;
    check_row("sp20", f);
  }
  {
    Fold f;
    Lcg g(7021);
    Vec a(n);
    for (u32& v : a) v = static_cast<u32>(g.next() % kP);
    const std::vector<Nz> nzb = sparse3(g);
    const Vec b = dense_from(nzb, n);
    const std::vector<Nz> mb = merged(b, nzb);
    f.one(20);
    f.many(a);
    f.many(b);
    const Vec c = convolve(acc, a, b, 20);
    f.many(c);
    bool same = true;
    for (u32 s = 0; s < n; ++s) {
      u64 want = 0;
      for (const Nz& e : mb) want = (want + static_cast<u64>(e.val) * a[s ^ e.pos]) % kP;
      same = same && c[s] == want;
    }
    acc.expect(same, "xor_convolve dense with sparse", 20);
    ++f.cases;
    check_row("cd20", f);
  }
  finish(acc);
}

TEST_CASE("walsh: k = 20 dense structured arrays against closed forms", "[walsh]") {
  const u32 k = 20;
  const std::size_t n = std::size_t{1} << k;
  const u64 nm = static_cast<u64>(n) % kP;
  const u64 half = (kP + 1u) / 2u;  // 2^-1 modulo p (checked below against 2 * half == 1)
  Acc acc;
  acc.expect(2u * half % kP == 1u, "half", 0);
  std::vector<u64> p2(kMaxK + 1u, 1);
  for (std::size_t i = 1; i < p2.size(); ++i) p2[i] = p2[i - 1] * 2u % kP;
  // all ones: walsh = n at 0; inverse = 1 at 0; zeta = 2^popcount(s); mobius = 1 at 0.
  {
    const Vec ones(n, 1u);
    const Vec w = apply(acc, 0, ones, k), wi = apply(acc, 1, ones, k), z = apply(acc, 2, ones, k), m = apply(acc, 3, ones, k);
    bool okw = true, okwi = true, okz = true, okm = true;
    for (u32 s = 0; s < n; ++s) {
      okw = okw && w[s] == (s == 0 ? nm : 0u);
      okwi = okwi && wi[s] == (s == 0 ? 1u : 0u);
      okz = okz && z[s] == p2[pop(s)];
      okm = okm && m[s] == (s == 0 ? 1u : 0u);
    }
    acc.expect(okw && okwi && okz && okm, "all ones", okw, okz);
  }
  // parity pattern a[t] = (-1)^popcount(t): walsh = n at the last index; inverse = 1 there; zeta = 1 at 0; mobius = (-2)^popcount(s).
  {
    Vec pattern(n);
    for (u32 t = 0; t < n; ++t) pattern[t] = par(t) ? kMod - 1u : 1u;
    const Vec w = apply(acc, 0, pattern, k), wi = apply(acc, 1, pattern, k), z = apply(acc, 2, pattern, k), m = apply(acc, 3, pattern, k);
    bool okw = true, okwi = true, okz = true, okm = true;
    for (u32 s = 0; s < n; ++s) {
      okw = okw && w[s] == (s == n - 1u ? nm : 0u);
      okwi = okwi && wi[s] == (s == n - 1u ? 1u : 0u);
      okz = okz && z[s] == (s == 0 ? 1u : 0u);
      okm = okm && m[s] == (par(s) ? (kP - p2[pop(s)]) % kP : p2[pop(s)]);
    }
    acc.expect(okw && okwi && okz && okm, "parity pattern", okw, okm);
  }
  // upper-half indicator a[t] = bit 19 of t: walsh = +-2^19 at 0 and 2^19; inverse = +-1/2 there; zeta = 2^(popcount - 1) when bit 19 is set;
  // mobius = 1 at 2^19 only.
  {
    const u32 hi = 1u << 19;
    Vec ind(n);
    for (u32 t = 0; t < n; ++t) ind[t] = (t & hi) != 0 ? 1u : 0u;
    const Vec w = apply(acc, 0, ind, k), wi = apply(acc, 1, ind, k), z = apply(acc, 2, ind, k), m = apply(acc, 3, ind, k);
    bool okw = true, okwi = true, okz = true, okm = true;
    for (u32 s = 0; s < n; ++s) {
      u64 ew = 0, ewi = 0;
      if (s == 0) {
        ew = nm / 2u;
        ewi = half;
      } else if (s == hi) {
        ew = kP - nm / 2u;
        ewi = kP - half;
      }
      okw = okw && w[s] == ew;
      okwi = okwi && wi[s] == ewi;
      okz = okz && z[s] == ((s & hi) != 0 ? p2[pop(s) - 1u] : 0u);
      okm = okm && m[s] == (s == hi ? 1u : 0u);
    }
    acc.expect(okw && okwi && okz && okm, "upper-half indicator", okw, okz);
  }
  // all p - 1 (the largest residue everywhere) round-trips through every transform pair.
  {
    const Vec big(n, kMod - 1u);
    acc.expect(apply(acc, 1, apply(acc, 0, big, k), k) == big, "p - 1 everywhere: walsh round trip", k);
    acc.expect(apply(acc, 3, apply(acc, 2, big, k), k) == big, "p - 1 everywhere: zeta round trip", k);
    const Vec w = apply(acc, 0, big, k);
    bool okw = w[0] == (kP - nm) % kP;
    for (u32 s = 1; s < n; ++s) okw = okw && w[s] == 0u;
    acc.expect(okw, "p - 1 everywhere: walsh", k);
    const Vec c = convolve(acc, big, big, k);  // (-1)(-1) * 2^k = 2^k at every index
    bool okc = true;
    for (u32 s = 0; s < n; ++s) okc = okc && c[s] == nm;
    acc.expect(okc, "p - 1 everywhere: convolution", k);
  }
  finish(acc);
}

TEST_CASE("walsh: hand rows and boundary sums", "[walsh]") {
  Acc acc;
  auto row = [&](int w, Vec a, u32 k, const Vec& want) {
    const Vec r = apply(acc, w, a, k);
    acc.expect(r == want, kNames[w], k, a.empty() ? 0u : a[0]);
  };
  row(0, {7u}, 0, {7u});
  row(1, {7u}, 0, {7u});
  row(2, {7u}, 0, {7u});
  row(3, {7u}, 0, {7u});
  row(0, {3u, 5u}, 1, {8u, kMod - 2u});
  row(1, {8u, kMod - 2u}, 1, {3u, 5u});
  row(2, {3u, 5u}, 1, {3u, 8u});
  row(3, {3u, 8u}, 1, {3u, 5u});
  row(0, {1u, 2u, 3u, 4u}, 2, {10u, kMod - 2u, kMod - 4u, 0u});
  row(2, {1u, 2u, 3u, 4u}, 2, {1u, 3u, 4u, 10u});
  row(3, {1u, 3u, 4u, 10u}, 2, {1u, 2u, 3u, 4u});
  // sums and differences exactly at the modulus and at zero (the boundary of the one conditional correction)
  row(0, {1u, kMod - 1u}, 1, {0u, 2u});
  row(0, {kMod - 1u, kMod - 1u}, 1, {kMod - 2u, 0u});
  row(0, {kMod - 1u, 0u}, 1, {kMod - 1u, kMod - 1u});
  row(0, {kMod - 2u, 1u}, 1, {kMod - 1u, kMod - 3u});
  row(0, {5u, 5u}, 1, {10u, 0u});
  row(2, {1u, kMod - 1u}, 1, {1u, 0u});
  row(2, {kMod - 1u, kMod - 1u}, 1, {kMod - 1u, kMod - 2u});
  row(2, {kMod - 2u, 1u}, 1, {kMod - 2u, kMod - 1u});
  row(3, {1u, 0u}, 1, {1u, kMod - 1u});
  row(3, {5u, 5u}, 1, {5u, 0u});
  row(3, {kMod - 1u, kMod - 1u}, 1, {kMod - 1u, 0u});
  row(1, {0u, 0u}, 1, {0u, 0u});
  row(1, {kMod - 1u, kMod - 1u}, 1, {kMod - 1u, 0u});
  auto conv = [&](Vec a, Vec b, u32 k, const Vec& want) { acc.expect(convolve(acc, a, b, k) == want, "xor_convolve", k, a[0]); };
  conv({1u}, {1u}, 0, {1u});
  conv({0u}, {kMod - 1u}, 0, {0u});
  conv({kMod - 1u}, {kMod - 1u}, 0, {1u});
  conv({kMod - 1u}, {2u}, 0, {kMod - 2u});
  conv({1u, 2u}, {3u, 4u}, 1, {11u, 10u});
  conv({kMod - 1u, kMod - 1u}, {kMod - 1u, kMod - 1u}, 1, {2u, 2u});
  conv({1u, 2u, 3u, 4u}, {5u, 6u, 7u, 8u}, 2, {1u * 5u + 2u * 6u + 3u * 7u + 4u * 8u, 1u * 6u + 2u * 5u + 3u * 8u + 4u * 7u,
                                               1u * 7u + 2u * 8u + 3u * 5u + 4u * 6u, 1u * 8u + 2u * 7u + 3u * 6u + 4u * 5u});
  finish(acc);
}

TEST_CASE("walsh: transform failures, check order, nothing written", "[walsh]") {
  Acc acc;
  const u32 ks[] = {0u, 1u, 20u, 21u, 0xFFFFFFFFu};
  for (int w = 0; w < 4; ++w) {
    for (const u32 k : ks) acc.expect(kTransforms[w](nullptr, k) == WalshResult::NullArgument, "null a", w, k);
    // TooLarge beats ValueOutOfRange; the (tiny) array is never touched.
    for (const u32 k : {21u, 0xFFFFFFFFu}) {
      Vec buf(kPad * 2 + 4, kSent);
      buf[kPad] = kMod;
      const Vec copy = buf;
      acc.expect(kTransforms[w](buf.data() + kPad, k) == WalshResult::TooLarge, "TooLarge", w, k);
      acc.expect(buf == copy, "TooLarge wrote", w, k);
    }
    // ValueOutOfRange: every position class, three bad values, then two bad entries; the whole array is unchanged.
    for (u32 k = 0; k <= 4; ++k) {
      const std::size_t n = std::size_t{1} << k;
      const std::size_t poss[] = {0, n / 2, n - 1};
      for (const std::size_t pos : poss)
        for (const u32 bad : {kMod, kMod + 1u, 0xFFFFFFFFu}) {
          Vec buf(n + 2u * kPad, kSent);
          Lcg g(900u + k);
          for (std::size_t i = 0; i < n; ++i) buf[kPad + i] = static_cast<u32>(g.next() % kP);
          buf[kPad + pos] = bad;
          const Vec copy = buf;
          acc.expect(kTransforms[w](buf.data() + kPad, k) == WalshResult::ValueOutOfRange, "ValueOutOfRange", k, pos);
          acc.expect(buf == copy, "ValueOutOfRange wrote", k, pos);
        }
      if (n >= 2) {
        Vec buf(n + 2u * kPad, 1u);
        buf[kPad] = kMod;
        buf[kPad + n - 1u] = 0xFFFFFFFFu;
        const Vec copy = buf;
        acc.expect(kTransforms[w](buf.data() + kPad, k) == WalshResult::ValueOutOfRange, "two bad entries", k);
        acc.expect(buf == copy, "two bad entries wrote", k);
      }
    }
    // k = 20: a bad entry in the last place only (the whole array is scanned), the array unchanged.
    {
      Vec big(std::size_t{1} << 20, 1u);
      big.back() = kMod;
      const Vec copy = big;
      acc.expect(kTransforms[w](big.data(), 20) == WalshResult::ValueOutOfRange, "k = 20 last entry", w);
      acc.expect(big == copy, "k = 20 last entry wrote", w);
    }
    // The largest residue is accepted everywhere.
    {
      Vec x(4, kMod - 1u);
      acc.expect(kTransforms[w](x.data(), 2) == WalshResult::Ok, "p - 1 accepted", w);
      for (const u32 v : x) acc.expect(v < kMod, "result below p", w, v);
    }
  }
  finish(acc);
}

namespace {

struct CvBuf {
  Vec a, b, out, scr;
};

CvBuf make_cv(std::size_t n, std::size_t scratch_alloc, u64 seed) {
  CvBuf c;
  Lcg g(seed);
  c.a.resize(n);
  c.b.resize(n);
  for (u32& v : c.a) v = static_cast<u32>(g.next() % kP);
  for (u32& v : c.b) v = static_cast<u32>(g.next() % kP);
  c.out.assign(n + 2u * kPad, kSent);
  c.scr.assign(scratch_alloc + 2u * kPad, kSent);
  return c;
}

WalshResult call_conv(CvBuf& c, u32 k, u32 sw, unsigned null_mask) {
  const u32* a = (null_mask & 1u) != 0 ? nullptr : c.a.data();
  const u32* b = (null_mask & 2u) != 0 ? nullptr : c.b.data();
  u32* scr = (null_mask & 4u) != 0 ? nullptr : c.scr.data() + kPad;
  u32* out = (null_mask & 8u) != 0 ? nullptr : c.out.data() + kPad;
  return xor_convolve(a, b, k, scr, sw, out);
}

// Expects result `want` and, when it is a failure, that all four buffers are unchanged.
void expect_conv(Acc& acc, CvBuf c, u32 k, u32 sw, unsigned null_mask, WalshResult want, const char* what) {
  const CvBuf before = c;
  const WalshResult r = call_conv(c, k, sw, null_mask);
  acc.expect(r == want, what, k, sw);
  acc.expect(c.a == before.a && c.b == before.b, "inputs changed", k, sw);
  if (want != WalshResult::Ok) acc.expect(c.out == before.out && c.scr == before.scr, "out or scratch written on failure", k, sw);
}

}  // namespace

TEST_CASE("walsh: xor_convolve failures, check order, out and scratch unchanged", "[walsh]") {
  Acc acc;
  // NullArgument: each of the four pointers, for k = 0, 1, 21 and UINT32_MAX, with scratch_words 0 and large (null wins over everything).
  for (unsigned mask = 1; mask < 16; ++mask)
    for (const u32 k : {0u, 1u, 21u, 0xFFFFFFFFu})
      for (const u32 sw : {0u, 100u, 0xFFFFFFFFu}) {
        CvBuf c = make_cv(16, 16, 11);
        c.a[3] = kMod;  // a bad entry does not matter either
        expect_conv(acc, c, k, sw, mask, WalshResult::NullArgument, "null pointer");
      }
  // TooLarge beats ScratchTooSmall and ValueOutOfRange.
  for (const u32 k : {21u, 0xFFFFFFFFu})
    for (const u32 sw : {0u, 3u, 0xFFFFFFFFu}) {
      CvBuf c = make_cv(16, 16, 12);
      c.a[0] = kMod;
      c.b[0] = 0xFFFFFFFFu;
      expect_conv(acc, c, k, sw, 0, WalshResult::TooLarge, "TooLarge");
    }
  // ScratchTooSmall (2^k - 1 and 0) beats ValueOutOfRange; k = 0 needs one word.
  for (u32 k = 0; k <= 4; ++k) {
    const u32 n = 1u << k;
    for (const u32 sw : {0u, n - 1u}) {
      CvBuf c = make_cv(n, n, 20 + k);
      c.a[0] = kMod;
      c.b[n - 1u] = kMod + 1u;
      expect_conv(acc, c, k, sw, 0, WalshResult::ScratchTooSmall, "ScratchTooSmall");
      CvBuf d = make_cv(n, n, 30 + k);
      expect_conv(acc, d, k, sw, 0, WalshResult::ScratchTooSmall, "ScratchTooSmall, good entries");
    }
  }
  {
    CvBuf c = make_cv(std::size_t{1} << 20, std::size_t{1} << 20, 40);
    expect_conv(acc, c, 20, (1u << 20) - 1u, 0, WalshResult::ScratchTooSmall, "ScratchTooSmall at k = 20");
  }
  // ValueOutOfRange: a only, b only, both, at the first, middle and last entry, with three bad values.
  for (u32 k = 0; k <= 3; ++k) {
    const std::size_t n = std::size_t{1} << k;
    const std::size_t poss[] = {0, n / 2, n - 1};
    for (const std::size_t pos : poss)
      for (const u32 bad : {kMod, kMod + 1u, 0xFFFFFFFFu})
        for (unsigned which = 1; which <= 3; ++which) {
          CvBuf c = make_cv(n, n, 50 + k);
          if ((which & 1u) != 0) c.a[pos] = bad;
          if ((which & 2u) != 0) c.b[pos] = bad;
          expect_conv(acc, c, k, static_cast<u32>(n), 0, WalshResult::ValueOutOfRange, "ValueOutOfRange");
        }
  }
  for (unsigned which = 1; which <= 3; ++which) {
    CvBuf c = make_cv(std::size_t{1} << 20, std::size_t{1} << 20, 60);
    if ((which & 1u) != 0) c.a.back() = kMod;
    if ((which & 2u) != 0) c.b.back() = kMod;
    expect_conv(acc, c, 20, 1u << 20, 0, WalshResult::ValueOutOfRange, "ValueOutOfRange at k = 20, last entry");
  }
  // Success with scratch_words above 2^k: accepted, the result is right, nothing outside the used words changes.
  for (u32 k = 0; k <= 4; ++k) {
    const u32 n = 1u << k;
    for (const u32 extra : {0u, 1u, 5u}) {
      CvBuf c = make_cv(n, n + extra, 70 + k);
      const Vec want = o_conv(c.a, c.b);
      acc.expect(call_conv(c, k, n + extra, 0) == WalshResult::Ok, "Ok with extra scratch", k, extra);
      acc.expect(Vec(c.out.begin() + kPad, c.out.begin() + static_cast<std::ptrdiff_t>(kPad + n)) == want, "result", k, extra);
      bool pads = true;
      for (u32 i = 0; i < kPad; ++i)
        pads = pads && c.out[i] == kSent && c.out[kPad + n + i] == kSent && c.scr[i] == kSent && c.scr[kPad + n + extra + i] == kSent;
      acc.expect(pads, "sentinels around out or scratch", k, extra);
    }
  }
  finish(acc);
}

TEST_CASE("walsh: round trips and boundary residues at k = 20 and the convolution at the full size", "[walsh]") {
  Acc acc;
  const std::size_t n = std::size_t{1} << 20;
  Lcg g(8020);
  Vec a(n), b(n);
  for (u32& v : a) v = draw_entry(g);
  for (u32& v : b) v = draw_entry(g);
  for (int w = 0; w < 4; ++w) {
    const Vec t = apply(acc, w, a, 20);
    bool below = true;
    for (const u32 v : t) below = below && v < kMod;
    acc.expect(below, "result below p", w);
  }
  acc.expect(apply(acc, 1, apply(acc, 0, a, 20), 20) == a, "k = 20 walsh round trip", 20);
  acc.expect(apply(acc, 0, apply(acc, 1, a, 20), 20) == a, "k = 20 inverse then walsh", 20);
  acc.expect(apply(acc, 3, apply(acc, 2, a, 20), 20) == a, "k = 20 zeta round trip", 20);
  acc.expect(apply(acc, 2, apply(acc, 3, a, 20), 20) == a, "k = 20 mobius then zeta", 20);
  const Vec c = convolve(acc, a, b, 20);
  const Vec wc = apply(acc, 0, c, 20), wa = apply(acc, 0, a, 20), wb = apply(acc, 0, b, 20);
  bool thm = true;
  for (std::size_t i = 0; i < n; ++i) thm = thm && wc[i] == static_cast<u64>(wa[i]) * wb[i] % kP;
  acc.expect(thm, "k = 20 convolution theorem", 20);
  acc.expect(convolve(acc, b, a, 20) == c, "k = 20 convolution commutes", 20);
  finish(acc);
}
