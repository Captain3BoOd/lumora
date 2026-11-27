// Tests for core/limbs. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/limbs_ref.py, an independent Python implementation on big integers.
// Regenerate it with: python3 tools/limbs_ref.py
// The tests also carry a naive oracle that shares no code or representation with core/limbs/limbs.cpp: numbers as 16-bit digits (base 65536,
// 32-bit intermediates), schoolbook add, subtract, multiply and long division, results converted back to normalized 32-bit limbs.
// Inputs are heap buffers of exactly the stated length (AddressSanitizer sees any access past the end); dst is a buffer of exactly the
// capacity, or the capacity plus 3 canaries. Success tail (slots from out_len up to the capacity) is unspecified and never asserted; a failure
// must leave the whole buffer and every out pointer unchanged, and a write at or above the capacity is a failure of the test.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

#include "limbs.h"

using namespace lumora::limbs;

namespace {

// BEGIN GENERATED REFERENCE (tools/limbs_ref.py)
const std::uint32_t kRefL5[] = {0x0u, 0x1u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu};
const std::uint32_t kRefDivisors[] = {0x0u, 0x1u, 0x2u, 0x3u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu};
const std::uint32_t kRefSmallMax = 3, kRefCapExtra = 7;
const std::uint32_t kRefRandSeeds = 400, kRefRandLen = 65, kRefSpecK = 24;
const std::uint64_t kRefDigest_small = 0xAD7FBBF9FF72B698ull;
const std::size_t kRefCases_small = 24336, kRefOps_small = 354564;
const std::uint64_t kRefDigest_div = 0x5149FDB50306AD02ull;
const std::size_t kRefCases_div = 1248, kRefOps_div = 5645;
const std::uint64_t kRefDigest_rand = 0xBA192EF244F3492Eull;
const std::size_t kRefCases_rand = 400, kRefOps_rand = 5731;
const std::uint64_t kRefDigest_spec = 0x19346F76FF895D9Bull;
const std::size_t kRefCases_spec = 2401, kRefOps_spec = 9604;
// END GENERATED REFERENCE

constexpr std::uint32_t kSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kMax32 = 0xFFFFFFFFu;

using Vec = std::vector<std::uint32_t>;
using Dig = std::vector<std::uint32_t>;  // 16-bit digits, little-endian

std::uint32_t code(LimbResult r) { return static_cast<std::uint32_t>(r); }

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  std::size_t ops = 0;
  void add(std::uint64_t v) { h = (h ^ v) * 0x100000001B3ull; }
};

// --- oracle ---
void trim(Dig& d) {
  while (!d.empty() && d.back() == 0u) d.pop_back();
}

Dig to_dig(const Vec& v) {
  Dig d;
  for (std::uint32_t x : v) {
    d.push_back(x & 0xFFFFu);
    d.push_back(x >> 16);
  }
  trim(d);
  return d;
}

Vec from_dig(const Dig& d) {
  Vec v;
  for (std::size_t i = 0; i < d.size(); i += 2) {
    const std::uint32_t lo = d[i];
    const std::uint32_t hi = i + 1 < d.size() ? d[i + 1] : 0u;
    v.push_back(lo | (hi << 16));
  }
  while (!v.empty() && v.back() == 0u) v.pop_back();
  return v;
}

int o_cmp(const Dig& a, const Dig& b) {
  if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
  for (std::size_t i = a.size(); i > 0; --i) {
    if (a[i - 1] != b[i - 1]) return a[i - 1] < b[i - 1] ? -1 : 1;
  }
  return 0;
}

Dig o_add(const Dig& a, const Dig& b) {
  Dig r;
  std::uint32_t carry = 0;
  for (std::size_t i = 0; i < (a.size() > b.size() ? a.size() : b.size()); ++i) {
    const std::uint32_t s = (i < a.size() ? a[i] : 0u) + (i < b.size() ? b[i] : 0u) + carry;
    r.push_back(s & 0xFFFFu);
    carry = s >> 16;
  }
  if (carry != 0u) r.push_back(carry);
  trim(r);
  return r;
}

Dig o_sub(const Dig& a, const Dig& b) {  // a >= b
  Dig r;
  std::uint32_t borrow = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const std::uint32_t y = (i < b.size() ? b[i] : 0u) + borrow;
    if (a[i] >= y) {
      r.push_back(a[i] - y);
      borrow = 0;
    } else {
      r.push_back(a[i] + 65536u - y);
      borrow = 1;
    }
  }
  trim(r);
  return r;
}

Dig o_mul(const Dig& a, const Dig& b) {
  Dig r(a.size() + b.size(), 0u);
  for (std::size_t i = 0; i < a.size(); ++i) {
    std::uint32_t carry = 0;
    for (std::size_t j = 0; j < b.size(); ++j) {
      const std::uint32_t cur = r[i + j] + a[i] * b[j] + carry;  // at most 65535 + 65535^2 + 65535 = 2^32 - 1
      r[i + j] = cur & 0xFFFFu;
      carry = cur >> 16;
    }
    r[i + b.size()] += carry;
  }
  trim(r);
  return r;
}

void o_divmod(const Dig& a, std::uint32_t d, Dig& q, std::uint32_t& rem) {
  q.assign(a.size(), 0u);
  std::uint64_t r = 0;
  for (std::size_t i = a.size(); i > 0; --i) {
    const std::uint64_t cur = (r << 16) | a[i - 1];
    q[i - 1] = static_cast<std::uint32_t>(cur / d);
    r = cur % d;
  }
  trim(q);
  rem = static_cast<std::uint32_t>(r);
}

// --- buffers ---
struct Buf {
  std::unique_ptr<std::uint32_t[]> p;
  std::size_t n;
  explicit Buf(const Vec& v) : p(new std::uint32_t[v.size()]), n(v.size()) {
    for (std::size_t i = 0; i < v.size(); ++i) p[i] = v[i];
  }
  const std::uint32_t* get() const { return p.get(); }
};

// A canary-filled buffer of cap + pad elements.
struct Out {
  std::unique_ptr<std::uint32_t[]> p;
  std::uint32_t cap;
  std::uint32_t pad;
  Vec snap;
  Out(std::uint32_t c, std::uint32_t pd) : p(new std::uint32_t[static_cast<std::size_t>(c) + pd]), cap(c), pad(pd) {
    for (std::uint32_t i = 0; i < c + pd; ++i) p[i] = 0xC0DE0000u + i;
    snap.assign(p.get(), p.get() + c + pd);
  }
  bool unchanged() const {
    for (std::uint32_t i = 0; i < cap + pad; ++i) {
      if (p[i] != snap[i]) return false;
    }
    return true;
  }
  bool pad_unchanged() const {
    for (std::uint32_t i = cap; i < cap + pad; ++i) {
      if (p[i] != snap[i]) return false;
    }
    return true;
  }
  std::uint32_t* get() { return p.get(); }
};

enum BinOp { kAdd, kSub, kMul };

LimbResult call_bin(BinOp op, const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst,
                    std::uint32_t cap, std::uint32_t* len) {
  if (op == kAdd) return limbs_add(a, na, b, nb, dst, cap, len);
  if (op == kSub) return limbs_sub(a, na, b, nb, dst, cap, len);
  return limbs_mul(a, na, b, nb, dst, cap, len);
}

// Expected result of a binary operation; `under` is set for an Underflow.
Vec expect_bin(BinOp op, const Vec& a, const Vec& b, bool& under) {
  const Dig da = to_dig(a), db = to_dig(b);
  under = false;
  if (op == kAdd) return from_dig(o_add(da, db));
  if (op == kMul) return from_dig(o_mul(da, db));
  if (o_cmp(da, db) < 0) {
    under = true;
    return Vec();
  }
  return from_dig(o_sub(da, db));
}

void do_cmp(Fold* f, std::size_t* bad, const Vec& a, const Vec& b) {
  const Buf ba(a), bb(b);
  std::int32_t out = 77;
  const LimbResult r = limbs_cmp(ba.get(), static_cast<std::uint32_t>(a.size()), bb.get(), static_cast<std::uint32_t>(b.size()), &out);
  const int want = o_cmp(to_dig(a), to_dig(b));
  if (r != LimbResult::Ok || out != want) ++*bad;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (ba.get()[i] != a[i]) ++*bad;
  }
  if (f != nullptr) {
    ++f->ops;
    f->add(static_cast<std::uint64_t>(out + 1));
  }
}

void do_bin(Fold* f, std::size_t* bad, BinOp op, const Vec& a, const Vec& b, std::uint32_t cap, std::uint32_t pad) {
  const Buf ba(a), bb(b);
  bool under = false;
  const Vec want = expect_bin(op, a, b, under);
  Out dst(cap, pad);
  std::uint32_t len = kSentinel;
  const LimbResult r = call_bin(op, ba.get(), static_cast<std::uint32_t>(a.size()), bb.get(), static_cast<std::uint32_t>(b.size()), dst.get(), cap, &len);
  const LimbResult want_code = under ? LimbResult::Underflow : (cap < want.size() ? LimbResult::BufferTooSmall : LimbResult::Ok);
  if (r != want_code) {
    ++*bad;
  } else if (r == LimbResult::Ok) {
    if (len != want.size()) ++*bad;
    for (std::size_t i = 0; i < want.size() && i < cap; ++i) {
      if (dst.get()[i] != want[i]) ++*bad;
    }
    if (!dst.pad_unchanged()) ++*bad;
    if (want.empty() && !dst.unchanged()) ++*bad;  // a zero result writes nothing
  } else {
    if (len != kSentinel || !dst.unchanged()) ++*bad;  // a failure writes nothing
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (ba.get()[i] != a[i]) ++*bad;
  }
  for (std::size_t i = 0; i < b.size(); ++i) {
    if (bb.get()[i] != b[i]) ++*bad;
  }
  if (f != nullptr) {
    ++f->ops;
    if (r == LimbResult::Ok) {
      f->add((static_cast<std::uint64_t>(code(r)) << 32) | len);
      for (std::size_t i = 0; i < want.size(); ++i) f->add(dst.get()[i]);
    } else {
      f->add(static_cast<std::uint64_t>(code(r)) << 32);
    }
  }
}

void do_div(Fold* f, std::size_t* bad, const Vec& a, std::uint32_t d, std::uint32_t cap, std::uint32_t pad) {
  const Buf ba(a);
  Dig q;
  std::uint32_t rem_want = 0;
  if (d != 0u) o_divmod(to_dig(a), d, q, rem_want);
  const Vec want = from_dig(q);
  Out dst(cap, pad);
  std::uint32_t len = kSentinel, rem = kSentinel;
  const LimbResult r = limbs_divmod_u32(ba.get(), static_cast<std::uint32_t>(a.size()), d, dst.get(), cap, &len, &rem);
  const LimbResult want_code = d == 0u ? LimbResult::DivideByZero : (cap < want.size() ? LimbResult::BufferTooSmall : LimbResult::Ok);
  if (r != want_code) {
    ++*bad;
  } else if (r == LimbResult::Ok) {
    if (len != want.size() || rem != rem_want) ++*bad;
    for (std::size_t i = 0; i < want.size() && i < cap; ++i) {
      if (dst.get()[i] != want[i]) ++*bad;
    }
    if (!dst.pad_unchanged()) ++*bad;
    if (want.empty() && !dst.unchanged()) ++*bad;
  } else {
    if (len != kSentinel || rem != kSentinel || !dst.unchanged()) ++*bad;
  }
  if (f != nullptr) {
    ++f->ops;
    if (r == LimbResult::Ok) {
      f->add((static_cast<std::uint64_t>(code(r)) << 32) | len);
      f->add(rem);
      for (std::size_t i = 0; i < want.size(); ++i) f->add(dst.get()[i]);
    } else {
      f->add(static_cast<std::uint64_t>(code(r)) << 32);
    }
  }
}

std::vector<std::uint32_t> caps_for(std::size_t need) {
  std::vector<std::uint32_t> c;
  const std::uint32_t n = static_cast<std::uint32_t>(need);
  // sorted unique {0, need - 1, need, need + 1, kRefCapExtra}
  std::uint32_t vals[5] = {0u, n >= 1u ? n - 1u : 0u, n, n + 1u, kRefCapExtra};
  for (std::uint32_t round = 0; round < 5u; ++round) {
    std::uint32_t best = kMax32;
    for (std::uint32_t v : vals) {
      if (v < best && (c.empty() || v > c.back())) best = v;
    }
    if (best == kMax32) break;
    c.push_back(best);
  }
  return c;
}

template <typename Fn>
void for_each_small(Fn fn) {
  for (std::uint32_t n = 0; n <= kRefSmallMax; ++n) {
    std::vector<std::uint32_t> idx(n, 0u);
    for (;;) {
      Vec a(n);
      for (std::uint32_t i = 0; i < n; ++i) a[i] = kRefL5[idx[i]];
      fn(a);
      std::uint32_t i = n;
      bool advanced = false;
      while (i > 0u) {
        --i;
        if (++idx[i] < 5u) {
          advanced = true;
          break;
        }
        idx[i] = 0u;
      }
      if (!advanced) break;
    }
  }
}

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

Vec gen_array(Lcg& g, std::uint32_t n, std::uint32_t mode) {
  Vec v;
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint32_t r = g.next();
    if (mode == 0u) {
      v.push_back(r);
    } else if (mode == 1u) {
      const std::uint32_t sel = r % 3u;
      v.push_back(sel == 0u ? 0u : (sel == 1u ? kMax32 : r));
    } else if (mode == 2u) {
      v.push_back(r % 4u == 0u ? r : 0u);
    } else {
      v.push_back(r % 8u != 0u ? kMax32 : r);
    }
  }
  return v;
}


// All operations on a pair for the small and special scenarios.
void small_pair(Fold* f, std::size_t* bad, const Vec& a, const Vec& b, std::uint32_t pad) {
  do_cmp(f, bad, a, b);
  for (BinOp op : {kAdd, kSub, kMul}) {
    bool under = false;
    const Vec want = expect_bin(op, a, b, under);
    for (std::uint32_t cap : caps_for(under ? 0u : want.size())) do_bin(f, bad, op, a, b, cap, pad);
  }
}

}  // namespace

TEST_CASE("header anchors", "[limbs]") {
  CHECK(kMaxLimbs == (1u << 16));
  CHECK(code(LimbResult::Ok) == 0u);
  CHECK(code(LimbResult::NullArgument) == 1u);
  CHECK(code(LimbResult::TooLarge) == 2u);
  CHECK(code(LimbResult::Underflow) == 3u);
  CHECK(code(LimbResult::BufferTooSmall) == 4u);
  CHECK(code(LimbResult::DivideByZero) == 5u);
}

TEST_CASE("every pair of arrays of length 0 to 3 over 5 limb values: cmp, add, sub, mul", "[sweep][limbs]") {
  Fold f;
  std::size_t bad = 0;
  std::vector<Vec> arrs;
  for_each_small([&](const Vec& a) { arrs.push_back(a); });
  for (const Vec& a : arrs) {
    for (const Vec& b : arrs) {
      small_pair(&f, &bad, a, b, 0u);
      ++f.cases;
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_small);
  CHECK(f.ops == kRefOps_small);
  CHECK(f.h == kRefDigest_small);
}

TEST_CASE("the same pairs with canary padding after dst", "[sweep][limbs]") {
  std::size_t bad = 0;
  std::size_t cases = 0;
  std::vector<Vec> arrs;
  for_each_small([&](const Vec& a) { arrs.push_back(a); });
  for (const Vec& a : arrs) {
    for (const Vec& b : arrs) {
      small_pair(nullptr, &bad, a, b, 3u);
      ++cases;
    }
  }
  CHECK(bad == 0u);
  CHECK(cases == kRefCases_small);
}

TEST_CASE("divmod: every array of length 0 to 3 x 8 divisors x quotient capacities", "[sweep][limbs]") {
  Fold f;
  std::size_t bad = 0;
  std::size_t padded = 0;
  for_each_small([&](const Vec& a) {
    for (std::uint32_t d : kRefDivisors) {
      std::size_t need = 0;
      if (d != 0u) {
        Dig q;
        std::uint32_t r = 0;
        o_divmod(to_dig(a), d, q, r);
        need = from_dig(q).size();
      }
      for (std::uint32_t cap : caps_for(need)) {
        do_div(&f, &bad, a, d, cap, 0u);
        do_div(nullptr, &bad, a, d, cap, 3u);
        ++padded;
      }
      ++f.cases;
    }
  });
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_div);
  CHECK(f.ops == kRefOps_div);
  CHECK(padded == kRefOps_div);
  CHECK(f.h == kRefDigest_div);
}

TEST_CASE("pseudo-random arrays of 0 to 64 limbs (not exhaustive)", "[sweep][limbs]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t seed = 1; seed <= kRefRandSeeds; ++seed) {
    Lcg g(seed);
    const std::uint32_t n1 = g.next() % kRefRandLen;
    const std::uint32_t n2 = g.next() % kRefRandLen;
    const Vec a = gen_array(g, n1, seed % 4u);
    const Vec b = gen_array(g, n2, (seed / 4u) % 4u);
    const std::uint32_t d0 = g.next();
    const std::uint32_t d1 = g.next() % 1000u;
    const std::uint32_t d3 = 1u << (g.next() % 32u);
    do_cmp(&f, &bad, a, b);
    for (BinOp op : {kAdd, kSub, kMul}) {
      bool under = false;
      const Vec want = expect_bin(op, a, b, under);
      do_bin(&f, &bad, op, a, b, under ? 1u : static_cast<std::uint32_t>(want.size()), 0u);
      if (!under && !want.empty()) do_bin(&f, &bad, op, a, b, static_cast<std::uint32_t>(want.size() - 1), seed % 2u == 0u ? 3u : 0u);
    }
    for (std::uint32_t d : {d0, d1, kMax32, d3}) {
      std::size_t need = 0;
      if (d != 0u) {
        Dig q;
        std::uint32_t r = 0;
        o_divmod(to_dig(a), d, q, r);
        need = from_dig(q).size();
      }
      do_div(&f, &bad, a, d, static_cast<std::uint32_t>(need), 0u);
      if (need > 0) do_div(&f, &bad, a, d, static_cast<std::uint32_t>(need - 1), 3u);
    }
    ++f.cases;
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_rand);
  CHECK(f.ops == kRefOps_rand);
  CHECK(f.h == kRefDigest_rand);
}

TEST_CASE("special arrays: 2^(32k) - 1 and 2^(32k) (carry and borrow chains)", "[sweep][limbs]") {
  Fold f;
  std::size_t bad = 0;
  std::vector<Vec> arrs;
  for (std::uint32_t k = 1; k <= kRefSpecK; ++k) arrs.push_back(Vec(k, kMax32));
  for (std::uint32_t k = 0; k <= kRefSpecK; ++k) {
    Vec v(k, 0u);
    v.push_back(1u);
    arrs.push_back(v);
  }
  for (const Vec& a : arrs) {
    for (const Vec& b : arrs) {
      do_cmp(&f, &bad, a, b);
      for (BinOp op : {kAdd, kSub, kMul}) {
        bool under = false;
        const Vec want = expect_bin(op, a, b, under);
        do_bin(&f, &bad, op, a, b, (under || want.empty()) ? (under ? 1u : 0u) : static_cast<std::uint32_t>(want.size()), 0u);
      }
      ++f.cases;
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_spec);
  CHECK(f.ops == kRefOps_spec);
  CHECK(f.h == kRefDigest_spec);
}

namespace {

constexpr std::uint32_t kBig = kMaxLimbs + 1u;

// Runs one binary function with raw arguments and checks the code, that out_len stays the sentinel and that the buffer is unchanged.
struct Raw {
  std::uint32_t a1[2] = {5u, 6u};
  std::uint32_t b1[2] = {7u, 8u};
  std::uint32_t d1[4] = {1u, 2u, 3u, 4u};
  std::uint32_t len = kSentinel;
  std::uint32_t rem = kSentinel;
  std::int32_t cmp = 77;
  bool d1_untouched() const { return d1[0] == 1u && d1[1] == 2u && d1[2] == 3u && d1[3] == 4u; }
};

LimbResult bin_raw(BinOp op, const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst, std::uint32_t cap,
                   std::uint32_t* len) {
  return call_bin(op, a, na, b, nb, dst, cap, len);
}

}  // namespace

TEST_CASE("null rules: add, sub, mul", "[args][limbs]") {
  for (BinOp op : {kAdd, kSub, kMul}) {
    Raw r;
    CHECK(bin_raw(op, nullptr, 2, r.b1, 2, r.d1, 4, &r.len) == LimbResult::NullArgument);
    CHECK(bin_raw(op, r.a1, 2, nullptr, 2, r.d1, 4, &r.len) == LimbResult::NullArgument);
    CHECK(bin_raw(op, r.a1, 2, r.b1, 2, nullptr, 4, &r.len) == LimbResult::NullArgument);
    CHECK(bin_raw(op, r.a1, 2, r.b1, 2, r.d1, 4, nullptr) == LimbResult::NullArgument);
    CHECK(r.len == kSentinel);
    CHECK(r.d1_untouched());
    // a null input array counts only while its length is above 0
    CHECK(bin_raw(op, nullptr, 0, r.b1, 2, r.d1, 4, &r.len) == (op == kSub ? LimbResult::Underflow : LimbResult::Ok));
    CHECK(bin_raw(op, r.b1, 2, nullptr, 0, r.d1, 4, &r.len) == LimbResult::Ok);
    CHECK(bin_raw(op, nullptr, 0, nullptr, 0, r.d1, 4, &r.len) == LimbResult::Ok);
    CHECK(r.len == 0u);
    // a null dst with capacity 0 is fine for a zero result only; capacity above 0 with a null dst is a NullArgument even for a zero result
    r.len = kSentinel;
    CHECK(bin_raw(op, nullptr, 0, nullptr, 0, nullptr, 0, &r.len) == LimbResult::Ok);
    CHECK(r.len == 0u);
    r.len = kSentinel;
    CHECK(bin_raw(op, nullptr, 0, nullptr, 0, nullptr, 1, &r.len) == LimbResult::NullArgument);
    CHECK(r.len == kSentinel);
    CHECK(bin_raw(op, r.a1, 2, r.b1, 2, nullptr, 0, &r.len) == (op == kSub ? LimbResult::Underflow : LimbResult::BufferTooSmall));
    CHECK(r.len == kSentinel);
  }
}

TEST_CASE("null rules: cmp and divmod", "[args][limbs]") {
  Raw r;
  CHECK(limbs_cmp(nullptr, 2, r.b1, 2, &r.cmp) == LimbResult::NullArgument);
  CHECK(limbs_cmp(r.a1, 2, nullptr, 2, &r.cmp) == LimbResult::NullArgument);
  CHECK(limbs_cmp(r.a1, 2, r.b1, 2, nullptr) == LimbResult::NullArgument);
  CHECK(limbs_cmp(nullptr, 0, nullptr, 0, nullptr) == LimbResult::NullArgument);
  CHECK(r.cmp == 77);
  CHECK(limbs_cmp(nullptr, 0, nullptr, 0, &r.cmp) == LimbResult::Ok);
  CHECK(r.cmp == 0);
  CHECK(limbs_cmp(nullptr, 0, r.b1, 2, &r.cmp) == LimbResult::Ok);
  CHECK(r.cmp == -1);
  CHECK(limbs_cmp(r.a1, 2, nullptr, 0, &r.cmp) == LimbResult::Ok);
  CHECK(r.cmp == 1);

  CHECK(limbs_divmod_u32(nullptr, 2, 3u, r.d1, 4, &r.len, &r.rem) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(r.a1, 2, 3u, nullptr, 4, &r.len, &r.rem) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(r.a1, 2, 3u, r.d1, 4, nullptr, &r.rem) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(r.a1, 2, 3u, r.d1, 4, &r.len, nullptr) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(nullptr, 0, 3u, nullptr, 1, &r.len, &r.rem) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(nullptr, 0, 3u, nullptr, 0, nullptr, &r.rem) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(nullptr, 0, 3u, nullptr, 0, &r.len, nullptr) == LimbResult::NullArgument);
  CHECK(r.len == kSentinel);
  CHECK(r.rem == kSentinel);
  CHECK(r.d1_untouched());
  // null a with na == 0, null q with q_cap == 0: zero quotient
  CHECK(limbs_divmod_u32(nullptr, 0, 3u, nullptr, 0, &r.len, &r.rem) == LimbResult::Ok);
  CHECK(r.len == 0u);
  CHECK(r.rem == 0u);
  r.len = kSentinel;
  r.rem = kSentinel;
  // a < d: the quotient is zero, the remainder is a
  CHECK(limbs_divmod_u32(r.a1, 1, 9u, nullptr, 0, &r.len, &r.rem) == LimbResult::Ok);
  CHECK(r.len == 0u);
  CHECK(r.rem == 5u);
  r.len = kSentinel;
  r.rem = kSentinel;
  CHECK(limbs_divmod_u32(r.a1, 1, 9u, nullptr, 1, &r.len, &r.rem) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(r.a1, 2, 9u, nullptr, 0, &r.len, &r.rem) == LimbResult::BufferTooSmall);
  CHECK(r.len == kSentinel);
  CHECK(r.rem == kSentinel);
}

TEST_CASE("check order: Null, TooLarge, Underflow or DivideByZero, BufferTooSmall", "[args][limbs]") {
  std::uint32_t one[1] = {1u};
  std::uint32_t dst[1] = {9u};
  std::uint32_t len = kSentinel, rem = kSentinel;
  std::int32_t c = 77;
  for (BinOp op : {kAdd, kSub, kMul}) {
    // TooLarge on either length (nothing is read: the claimed length is never walked)
    CHECK(bin_raw(op, one, kBig, one, 1, dst, 1, &len) == LimbResult::TooLarge);
    CHECK(bin_raw(op, one, 1, one, kBig, dst, 1, &len) == LimbResult::TooLarge);
    CHECK(bin_raw(op, one, kBig, one, kBig, dst, 1, &len) == LimbResult::TooLarge);
    CHECK(bin_raw(op, one, 0xFFFFFFFFu, one, 1, dst, 0, &len) == LimbResult::TooLarge);
    // Null beats TooLarge
    CHECK(bin_raw(op, nullptr, kBig, one, 1, dst, 1, &len) == LimbResult::NullArgument);
    CHECK(bin_raw(op, one, kBig, one, 1, dst, 1, nullptr) == LimbResult::NullArgument);
    CHECK(bin_raw(op, one, kBig, one, 1, nullptr, 1, &len) == LimbResult::NullArgument);
    // TooLarge beats Underflow and BufferTooSmall
    CHECK(bin_raw(op, one, 1, one, kBig, dst, 0, &len) == LimbResult::TooLarge);
    CHECK(len == kSentinel);
    CHECK(dst[0] == 9u);
  }
  // a capacity is never TooLarge
  const std::uint32_t big_cap_a[2] = {1u, 0u};
  std::uint32_t small_dst[2] = {0u, 0u};
  CHECK(limbs_add(big_cap_a, 1, big_cap_a, 1, small_dst, 0xFFFFFFFFu, &len) == LimbResult::Ok);
  CHECK(len == 1u);
  CHECK(small_dst[0] == 2u);
  len = kSentinel;
  // the kMaxLimbs boundary: 1 limb of zero value with a claimed length above kMaxLimbs is TooLarge, but at kMaxLimbs it is accepted (see max tests)
  CHECK(limbs_cmp(one, kBig, one, 1, &c) == LimbResult::TooLarge);
  CHECK(limbs_cmp(one, 1, one, kBig, &c) == LimbResult::TooLarge);
  CHECK(limbs_cmp(nullptr, kBig, one, 1, &c) == LimbResult::NullArgument);
  CHECK(limbs_cmp(one, kBig, one, 1, nullptr) == LimbResult::NullArgument);
  CHECK(c == 77);
  CHECK(limbs_divmod_u32(one, kBig, 3u, dst, 1, &len, &rem) == LimbResult::TooLarge);
  CHECK(limbs_divmod_u32(one, kBig, 0u, dst, 1, &len, &rem) == LimbResult::TooLarge);
  CHECK(limbs_divmod_u32(nullptr, kBig, 0u, dst, 1, &len, &rem) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(one, kBig, 3u, dst, 1, nullptr, &rem) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(one, kBig, 3u, dst, 1, &len, nullptr) == LimbResult::NullArgument);
  CHECK(len == kSentinel);
  CHECK(rem == kSentinel);
  CHECK(dst[0] == 9u);
  len = kSentinel;
  // DivideByZero comes after Null and TooLarge and before BufferTooSmall
  CHECK(limbs_divmod_u32(one, 1, 0u, dst, 0, &len, &rem) == LimbResult::DivideByZero);
  CHECK(limbs_divmod_u32(nullptr, 0, 0u, nullptr, 0, &len, &rem) == LimbResult::DivideByZero);
  CHECK(limbs_divmod_u32(nullptr, 0, 0u, nullptr, 1, &len, &rem) == LimbResult::NullArgument);
  CHECK(len == kSentinel);
  CHECK(rem == kSentinel);
  // Underflow beats BufferTooSmall: 1 - 2 with capacity 0
  const std::uint32_t two[1] = {2u};
  CHECK(limbs_sub(one, 1, two, 1, dst, 0, &len) == LimbResult::Underflow);
  CHECK(limbs_sub(one, 1, two, 1, dst, 1, &len) == LimbResult::Underflow);
  CHECK(len == kSentinel);
  CHECK(limbs_sub(two, 1, one, 1, dst, 0, &len) == LimbResult::BufferTooSmall);
  CHECK(len == kSentinel);
  CHECK(limbs_sub(two, 1, one, 1, dst, 1, &len) == LimbResult::Ok);
  CHECK(len == 1u);
  CHECK(dst[0] == 1u);
}

TEST_CASE("input-input aliasing: a and b the same array", "[alias][limbs]") {
  std::size_t bad = 0;
  for_each_small([&](const Vec& a) {
    const Buf ba(a);
    const Dig da = to_dig(a);
    const std::uint32_t n = static_cast<std::uint32_t>(a.size());
    std::int32_t c = 77;
    if (limbs_cmp(ba.get(), n, ba.get(), n, &c) != LimbResult::Ok || c != 0) ++bad;
    for (BinOp op : {kAdd, kSub, kMul}) {
      const Vec want = from_dig(op == kAdd ? o_add(da, da) : (op == kSub ? Dig() : o_mul(da, da)));
      Out dst(static_cast<std::uint32_t>(want.size()), 3u);
      std::uint32_t len = kSentinel;
      if (call_bin(op, ba.get(), n, ba.get(), n, dst.get(), dst.cap, &len) != LimbResult::Ok || len != want.size()) {
        ++bad;
        continue;
      }
      for (std::size_t i = 0; i < want.size(); ++i) {
        if (dst.get()[i] != want[i]) ++bad;
      }
      if (!dst.pad_unchanged()) ++bad;
      for (std::size_t i = 0; i < a.size(); ++i) {
        if (ba.get()[i] != a[i]) ++bad;
      }
    }
  });
  CHECK(bad == 0u);
}

TEST_CASE("leading zero limbs do not raise the needed capacity; zero results need capacity 0", "[zero][limbs]") {
  const std::uint32_t a[5] = {7u, 0u, 0u, 0u, 0u};
  const std::uint32_t b[4] = {3u, 0u, 0u, 0u};
  const std::uint32_t z[3] = {0u, 0u, 0u};
  std::uint32_t d[8] = {kSentinel, kSentinel, kSentinel, kSentinel, kSentinel, kSentinel, kSentinel, kSentinel};
  std::uint32_t len = kSentinel, rem = kSentinel;
  CHECK(limbs_add(a, 5, b, 4, d, 1, &len) == LimbResult::Ok);
  CHECK(len == 1u);
  CHECK(d[0] == 10u);
  CHECK(limbs_sub(a, 5, b, 4, d, 1, &len) == LimbResult::Ok);
  CHECK(len == 1u);
  CHECK(d[0] == 4u);
  CHECK(limbs_mul(a, 5, b, 4, d, 1, &len) == LimbResult::Ok);
  CHECK(len == 1u);
  CHECK(d[0] == 21u);
  CHECK(limbs_divmod_u32(a, 5, 2u, d, 1, &len, &rem) == LimbResult::Ok);
  CHECK(len == 1u);
  CHECK(d[0] == 3u);
  CHECK(rem == 1u);
  // zero results: nothing written, capacity 0 is enough
  for (std::uint32_t& x : d) x = kSentinel;
  len = kSentinel;
  CHECK(limbs_sub(a, 5, a, 5, d, 0, &len) == LimbResult::Ok);
  CHECK(len == 0u);
  len = kSentinel;
  CHECK(limbs_mul(a, 5, z, 3, d, 0, &len) == LimbResult::Ok);
  CHECK(len == 0u);
  len = kSentinel;
  CHECK(limbs_mul(z, 3, a, 5, d, 0, &len) == LimbResult::Ok);
  CHECK(len == 0u);
  len = kSentinel;
  CHECK(limbs_add(z, 3, z, 3, d, 0, &len) == LimbResult::Ok);
  CHECK(len == 0u);
  len = kSentinel;
  CHECK(limbs_divmod_u32(a, 5, 9u, d, 0, &len, &rem) == LimbResult::Ok);
  CHECK(len == 0u);
  CHECK(rem == 7u);
  for (std::uint32_t x : d) CHECK(x == kSentinel);
  // a zero result with a canary-filled dst of capacity 2 writes nothing
  len = kSentinel;
  CHECK(limbs_sub(a, 5, a, 5, d, 2, &len) == LimbResult::Ok);
  CHECK(len == 0u);
  for (std::uint32_t x : d) CHECK(x == kSentinel);
  // equal-valued arrays of different lengths compare equal
  std::int32_t c = 77;
  CHECK(limbs_cmp(a, 5, a, 1, &c) == LimbResult::Ok);
  CHECK(c == 0);
  CHECK(limbs_cmp(z, 3, nullptr, 0, &c) == LimbResult::Ok);
  CHECK(c == 0);
}

TEST_CASE("sizes: 65536 limbs are accepted, 65537 are TooLarge", "[max][limbs]") {
  const Vec ones(kMaxLimbs, kMax32);
  const Buf bo(ones);
  const Vec zeros(kMaxLimbs, 0u);
  const Buf bz(zeros);
  const Vec one1(1, 1u);
  const Buf b1(one1);
  std::int32_t c = 77;
  CHECK(limbs_cmp(bo.get(), kMaxLimbs, bo.get(), kMaxLimbs, &c) == LimbResult::Ok);
  CHECK(c == 0);
  CHECK(limbs_cmp(bo.get(), kMaxLimbs, b1.get(), 1, &c) == LimbResult::Ok);
  CHECK(c == 1);
  CHECK(limbs_cmp(b1.get(), 1, bo.get(), kMaxLimbs, &c) == LimbResult::Ok);
  CHECK(c == -1);
  CHECK(limbs_cmp(bz.get(), kMaxLimbs, nullptr, 0, &c) == LimbResult::Ok);
  CHECK(c == 0);
  CHECK(limbs_cmp(bz.get(), kMaxLimbs, b1.get(), 1, &c) == LimbResult::Ok);
  CHECK(c == -1);

  // add: all-ones + 1 is 2^(32*65536), 65537 limbs
  {
    Out dst(kMaxLimbs, 3u);
    std::uint32_t len = kSentinel;
    CHECK(limbs_add(bo.get(), kMaxLimbs, b1.get(), 1, dst.get(), kMaxLimbs, &len) == LimbResult::BufferTooSmall);
    CHECK(len == kSentinel);
    CHECK(dst.unchanged());
    CHECK(limbs_add(b1.get(), 1, bo.get(), kMaxLimbs, dst.get(), kMaxLimbs, &len) == LimbResult::BufferTooSmall);
    CHECK(dst.unchanged());
  }
  {
    Out dst(kMaxLimbs + 1u, 3u);
    std::uint32_t len = kSentinel;
    CHECK(limbs_add(bo.get(), kMaxLimbs, b1.get(), 1, dst.get(), kMaxLimbs + 1u, &len) == LimbResult::Ok);
    CHECK(len == kMaxLimbs + 1u);
    bool ok = dst.get()[kMaxLimbs] == 1u;
    for (std::uint32_t i = 0; i < kMaxLimbs; ++i) ok = ok && dst.get()[i] == 0u;
    CHECK(ok);
    CHECK(dst.pad_unchanged());
    // sub back: 2^(32*65536) cannot be an input (65537 limbs); instead all-ones - all-ones = 0 and all-ones - 1
    CHECK(limbs_sub(bo.get(), kMaxLimbs, bo.get(), kMaxLimbs, nullptr, 0, &len) == LimbResult::Ok);
    CHECK(len == 0u);
  }
  {
    Out dst(kMaxLimbs, 3u);
    std::uint32_t len = kSentinel;
    CHECK(limbs_sub(bo.get(), kMaxLimbs, b1.get(), 1, dst.get(), kMaxLimbs, &len) == LimbResult::Ok);
    CHECK(len == kMaxLimbs);
    bool ok = dst.get()[0] == 0xFFFFFFFEu;
    for (std::uint32_t i = 1; i < kMaxLimbs; ++i) ok = ok && dst.get()[i] == kMax32;
    CHECK(ok);
    CHECK(dst.pad_unchanged());
    // 1 - max underflows, nothing written
    Out dst2(2u, 0u);
    len = kSentinel;
    CHECK(limbs_sub(b1.get(), 1, bo.get(), kMaxLimbs, dst2.get(), 2, &len) == LimbResult::Underflow);
    CHECK(len == kSentinel);
    CHECK(dst2.unchanged());
    // short capacity: max - 1 needs 65536
    Out dst3(kMaxLimbs - 1u, 3u);
    CHECK(limbs_sub(bo.get(), kMaxLimbs, b1.get(), 1, dst3.get(), kMaxLimbs - 1u, &len) == LimbResult::BufferTooSmall);
    CHECK(len == kSentinel);
    CHECK(dst3.unchanged());
    // max - (max - 1) = 1
    Vec m1(kMaxLimbs, kMax32);
    m1[0] = 0xFFFFFFFEu;
    const Buf bm1(m1);
    len = kSentinel;
    Out dst4(1u, 3u);
    CHECK(limbs_sub(bo.get(), kMaxLimbs, bm1.get(), kMaxLimbs, dst4.get(), 1, &len) == LimbResult::Ok);
    CHECK(len == 1u);
    CHECK(dst4.get()[0] == 1u);
    CHECK(dst4.pad_unchanged());
  }

  // mul by one limb, max x 1-limb and 1-limb x max: (2^N - 1) * 3 = 3 * 2^N - 3
  {
    const Vec three(1, 3u);
    const Buf b3(three);
    Out dst(kMaxLimbs + 1u, 3u);
    std::uint32_t len = kSentinel;
    CHECK(limbs_mul(bo.get(), kMaxLimbs, b3.get(), 1, dst.get(), kMaxLimbs + 1u, &len) == LimbResult::Ok);
    CHECK(len == kMaxLimbs + 1u);
    bool ok = dst.get()[0] == 0xFFFFFFFDu && dst.get()[kMaxLimbs] == 2u;
    for (std::uint32_t i = 1; i < kMaxLimbs; ++i) ok = ok && dst.get()[i] == kMax32;
    CHECK(ok);
    CHECK(dst.pad_unchanged());
    Out dst2(kMaxLimbs + 1u, 0u);
    len = kSentinel;
    CHECK(limbs_mul(b3.get(), 1, bo.get(), kMaxLimbs, dst2.get(), kMaxLimbs + 1u, &len) == LimbResult::Ok);
    CHECK(len == kMaxLimbs + 1u);
    bool same = true;
    for (std::uint32_t i = 0; i <= kMaxLimbs; ++i) same = same && dst2.get()[i] == dst.get()[i];
    CHECK(same);
    // capacity one short: nothing written
    Out dst3(kMaxLimbs, 0u);
    len = kSentinel;
    CHECK(limbs_mul(bo.get(), kMaxLimbs, b3.get(), 1, dst3.get(), kMaxLimbs, &len) == LimbResult::BufferTooSmall);
    CHECK(len == kSentinel);
    CHECK(dst3.unchanged());
    // times one: the same number, exact capacity
    const Buf bone(one1);
    Out dst4(kMaxLimbs, 3u);
    CHECK(limbs_mul(bo.get(), kMaxLimbs, bone.get(), 1, dst4.get(), kMaxLimbs, &len) == LimbResult::Ok);
    CHECK(len == kMaxLimbs);
    bool eq = true;
    for (std::uint32_t i = 0; i < kMaxLimbs; ++i) eq = eq && dst4.get()[i] == kMax32;
    CHECK(eq);
    CHECK(dst4.pad_unchanged());
  }
  // mul with a zero-valued max-length factor: length 0, writes nothing, capacity 0 is enough
  {
    std::uint32_t len = kSentinel;
    CHECK(limbs_mul(bo.get(), kMaxLimbs, bz.get(), kMaxLimbs, nullptr, 0, &len) == LimbResult::Ok);
    CHECK(len == 0u);
    len = kSentinel;
    CHECK(limbs_mul(bz.get(), kMaxLimbs, bz.get(), kMaxLimbs, nullptr, 0, &len) == LimbResult::Ok);
    CHECK(len == 0u);
    len = kSentinel;
    CHECK(limbs_add(bz.get(), kMaxLimbs, bz.get(), kMaxLimbs, nullptr, 0, &len) == LimbResult::Ok);
    CHECK(len == 0u);
  }

  // divmod of all-ones (65536 limbs)
  {
    // by 1: q = a, rem 0
    Out q1(kMaxLimbs, 3u);
    std::uint32_t len = kSentinel, rem = kSentinel;
    CHECK(limbs_divmod_u32(bo.get(), kMaxLimbs, 1u, q1.get(), kMaxLimbs, &len, &rem) == LimbResult::Ok);
    CHECK(len == kMaxLimbs);
    CHECK(rem == 0u);
    bool eq = true;
    for (std::uint32_t i = 0; i < kMaxLimbs; ++i) eq = eq && q1.get()[i] == kMax32;
    CHECK(eq);
    CHECK(q1.pad_unchanged());
    Out q1s(kMaxLimbs - 1u, 3u);
    len = kSentinel;
    rem = kSentinel;
    CHECK(limbs_divmod_u32(bo.get(), kMaxLimbs, 1u, q1s.get(), kMaxLimbs - 1u, &len, &rem) == LimbResult::BufferTooSmall);
    CHECK(len == kSentinel);
    CHECK(rem == kSentinel);
    CHECK(q1s.unchanged());
    // by 0xFFFFFFFF: q = 1 + 2^32 + ... (every limb 1), rem 0
    Out q2(kMaxLimbs, 3u);
    CHECK(limbs_divmod_u32(bo.get(), kMaxLimbs, kMax32, q2.get(), kMaxLimbs, &len, &rem) == LimbResult::Ok);
    CHECK(len == kMaxLimbs);
    CHECK(rem == 0u);
    bool ones_q = true;
    for (std::uint32_t i = 0; i < kMaxLimbs; ++i) ones_q = ones_q && q2.get()[i] == 1u;
    CHECK(ones_q);
    CHECK(q2.pad_unchanged());
    // by 3: q limbs 0x55555555, rem 0 (2^32 - 1 = 3 * 0x55555555)
    Out q3(kMaxLimbs, 3u);
    CHECK(limbs_divmod_u32(bo.get(), kMaxLimbs, 3u, q3.get(), kMaxLimbs, &len, &rem) == LimbResult::Ok);
    CHECK(len == kMaxLimbs);
    CHECK(rem == 0u);
    bool q3ok = true;
    for (std::uint32_t i = 0; i < kMaxLimbs; ++i) q3ok = q3ok && q3.get()[i] == 0x55555555u;
    CHECK(q3ok);
    CHECK(q3.pad_unchanged());
    // by 0x80000000: q limbs 1, 1, ... shifted: all-ones / 2^31 has 65536 limbs; rem = 2^31 - 1
    Out q4(kMaxLimbs, 3u);
    CHECK(limbs_divmod_u32(bo.get(), kMaxLimbs, 0x80000000u, q4.get(), kMaxLimbs, &len, &rem) == LimbResult::Ok);
    CHECK(len == kMaxLimbs);
    CHECK(rem == 0x7FFFFFFFu);
    CHECK(q4.get()[kMaxLimbs - 1u] == 1u);
    CHECK(q4.get()[0] == 0xFFFFFFFFu);
    CHECK(q4.pad_unchanged());
    // by 0: DivideByZero, nothing written
    Out q5(kMaxLimbs, 0u);
    len = kSentinel;
    rem = kSentinel;
    CHECK(limbs_divmod_u32(bo.get(), kMaxLimbs, 0u, q5.get(), kMaxLimbs, &len, &rem) == LimbResult::DivideByZero);
    CHECK(len == kSentinel);
    CHECK(rem == kSentinel);
    CHECK(q5.unchanged());
    // zero-valued max-length dividend: zero quotient, remainder 0, capacity 0
    CHECK(limbs_divmod_u32(bz.get(), kMaxLimbs, 7u, nullptr, 0, &len, &rem) == LimbResult::Ok);
    CHECK(len == 0u);
    CHECK(rem == 0u);
  }
}

TEST_CASE("a null input array with length 1 is a NullArgument (cmp, add, sub, mul, divmod)", "[args][limbs]") {
  std::uint32_t one[1] = {1u};
  std::uint32_t dst[1] = {9u};
  std::uint32_t len = kSentinel, rem = kSentinel;
  std::int32_t c = 77;
  CHECK(limbs_cmp(nullptr, 1, one, 1, &c) == LimbResult::NullArgument);
  CHECK(limbs_cmp(one, 1, nullptr, 1, &c) == LimbResult::NullArgument);
  CHECK(c == 77);
  for (BinOp op : {kAdd, kSub, kMul}) {
    CHECK(bin_raw(op, nullptr, 1, one, 1, dst, 1, &len) == LimbResult::NullArgument);
    CHECK(bin_raw(op, one, 1, nullptr, 1, dst, 1, &len) == LimbResult::NullArgument);
    CHECK(bin_raw(op, one, 1, one, 1, nullptr, 1, &len) == LimbResult::NullArgument);
  }
  CHECK(limbs_divmod_u32(nullptr, 1, 3u, dst, 1, &len, &rem) == LimbResult::NullArgument);
  CHECK(limbs_divmod_u32(one, 1, 3u, nullptr, 1, &len, &rem) == LimbResult::NullArgument);
  CHECK(len == kSentinel);
  CHECK(rem == kSentinel);
  CHECK(dst[0] == 9u);
}
