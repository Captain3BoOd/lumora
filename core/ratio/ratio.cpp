// Exact rationals over int64. See ratio.h for the contract and docs/ratio.md for the method and the bounds.
//
// Method. Every value is handled as a sign plus an unsigned magnitude (uint64_t), so INT64_MIN is never negated as a signed number. Products and
// sums that can reach about 2^127 live in a two-word unsigned helper (Wide: hi and lo, both uint64_t) written with ordinary integer operations
// only: no compiler 128-bit type, builtin, intrinsic or extension. Every loop has a fixed bound (64 steps for the 32-bit limb multiply, 128
// shift-subtract steps for a division, 128 steps for a gcd that needs at most 93), so nothing depends on the data for its running time. No
// recursion, no allocation, no global state.
//
// Add and subtract use the classic gcd-of-denominators scheme: with g = gcd(db, dd), the sum a/b + c/d has numerator a*(d/g) + c*(b/g) and
// denominator (b/g)*d; the only common factor that can remain divides g, so it is gcd(numerator mod g, g). Multiply and divide cancel
// crosswise first, after which the product is already in lowest terms.
#include "ratio.h"

#include <cstdint>

namespace lumora::ratio {
namespace {

constexpr std::uint64_t kMaxPos = 0x7FFFFFFFFFFFFFFFull;   // INT64_MAX
constexpr std::uint64_t kMinMag = 0x8000000000000000ull;   // |INT64_MIN|
constexpr int kGcdSteps = 128;                             // Euclid on 64-bit values needs at most 93 steps

struct Wide {
  std::uint64_t hi;
  std::uint64_t lo;
};

std::uint64_t magnitude(std::int64_t v) {
  const std::uint64_t u = static_cast<std::uint64_t>(v);
  return v < 0 ? (~u + 1ull) : u;
}

// gcd of two uint64 values; gcd(0, x) = x. Fixed step bound.
std::uint64_t gcd64(std::uint64_t a, std::uint64_t b) {
  for (int i = 0; i < kGcdSteps && b != 0; ++i) {
    const std::uint64_t t = a % b;
    a = b;
    b = t;
  }
  return a;
}

// 64 x 64 -> 128 with 32-bit limbs.
Wide mul64(std::uint64_t a, std::uint64_t b) {
  const std::uint64_t a0 = a & 0xFFFFFFFFull, a1 = a >> 32;
  const std::uint64_t b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
  const std::uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  const std::uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
  Wide r;
  r.lo = (p00 & 0xFFFFFFFFull) | (mid << 32);
  r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
  return r;
}

bool wide_less(const Wide& a, const Wide& b) { return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo); }
bool wide_equal(const Wide& a, const Wide& b) { return a.hi == b.hi && a.lo == b.lo; }

Wide wide_add(const Wide& a, const Wide& b) {  // callers guarantee no carry out of 128 bits
  Wide r;
  r.lo = a.lo + b.lo;
  r.hi = a.hi + b.hi + (r.lo < a.lo ? 1ull : 0ull);
  return r;
}

Wide wide_sub(const Wide& a, const Wide& b) {  // requires a >= b
  Wide r;
  r.lo = a.lo - b.lo;
  r.hi = a.hi - b.hi - (a.lo < b.lo ? 1ull : 0ull);
  return r;
}

// q = n / d and the remainder, for d != 0, by 128 shift-subtract steps.
Wide wide_divmod(const Wide& n, std::uint64_t d, std::uint64_t* rem) {
  Wide q{0, 0};
  std::uint64_t r = 0;
  for (int i = 127; i >= 0; --i) {
    const std::uint64_t bit = i >= 64 ? ((n.hi >> (i - 64)) & 1ull) : ((n.lo >> i) & 1ull);
    const bool carry = (r >> 63) != 0;
    r = (r << 1) | bit;
    if (carry || r >= d) {
      r -= d;
      if (i >= 64) {
        q.hi |= 1ull << (i - 64);
      } else {
        q.lo |= 1ull << i;
      }
    }
  }
  *rem = r;
  return q;
}

bool valid(const Ratio* a) { return a->den > 0 && gcd64(magnitude(a->num), static_cast<std::uint64_t>(a->den)) == 1ull; }

// Stores sign * num_mag / den (den in lowest terms with num_mag, as a Wide pair) if it fits.
bool store(bool neg, const Wide& num, const Wide& den, Ratio* out) {
  if (den.hi != 0 || den.lo > kMaxPos || num.hi != 0) return false;
  if (neg) {
    if (num.lo > kMinMag) return false;
    out->num = static_cast<std::int64_t>(~num.lo + 1ull);  // two's complement; INT64_MIN when num.lo == 2^63
  } else {
    if (num.lo > kMaxPos) return false;
    out->num = static_cast<std::int64_t>(num.lo);
  }
  out->den = static_cast<std::int64_t>(den.lo);
  return true;
}

// (sa * ma / da) + (sb * mb / db), exact, in lowest terms; false on Overflow.
bool add_parts(bool na, std::uint64_t ma, std::uint64_t da, bool nb, std::uint64_t mb, std::uint64_t db, Ratio* out) {
  const std::uint64_t g = gcd64(da, db);
  const std::uint64_t da1 = da / g, db1 = db / g;
  const Wide t1 = mul64(ma, db1);
  const Wide t2 = mul64(mb, da1);
  Wide num;
  bool neg;
  if (na == nb) {
    num = wide_add(t1, t2);
    neg = na;
  } else if (wide_less(t1, t2)) {
    num = wide_sub(t2, t1);
    neg = nb;
  } else {
    num = wide_sub(t1, t2);
    neg = na;
  }
  if (num.hi == 0 && num.lo == 0) {
    out->num = 0;
    out->den = 1;
    return true;
  }
  Wide den = mul64(da1, db);
  std::uint64_t rem = 0;
  (void)wide_divmod(num, g, &rem);
  const std::uint64_t g2 = gcd64(g, rem);
  std::uint64_t ignored = 0;
  num = wide_divmod(num, g2, &ignored);
  den = wide_divmod(den, g2, &ignored);
  Ratio tmp;
  if (!store(neg, num, den, &tmp)) return false;
  *out = tmp;
  return true;
}

// (sa * ma / da) * (sb * mb / db), both parts in lowest terms; false on Overflow.
bool mul_parts(bool na, std::uint64_t ma, std::uint64_t da, bool nb, std::uint64_t mb, std::uint64_t db, Ratio* out) {
  if (ma == 0 || mb == 0) {
    out->num = 0;
    out->den = 1;
    return true;
  }
  const std::uint64_t g1 = gcd64(ma, db);
  const std::uint64_t g2 = gcd64(mb, da);
  const Wide num = mul64(ma / g1, mb / g2);
  const Wide den = mul64(da / g2, db / g1);
  Ratio tmp;
  if (!store(na != nb, num, den, &tmp)) return false;
  *out = tmp;
  return true;
}

}  // namespace

RatioResult ratio_make(std::int64_t num, std::int64_t den, Ratio* out) {
  if (out == nullptr) return RatioResult::NullArgument;
  if (den == 0) return RatioResult::ZeroDivision;
  const std::uint64_t mn = magnitude(num), md = magnitude(den);
  const std::uint64_t g = gcd64(mn, md);  // md above 0, so g above 0
  const std::uint64_t rn = mn / g, rd = md / g;
  const bool neg = (num < 0) != (den < 0);
  Ratio tmp;
  if (!store(neg && rn != 0, Wide{0, rn}, Wide{0, rd}, &tmp)) return RatioResult::Overflow;
  *out = tmp;
  return RatioResult::Ok;
}

RatioResult ratio_add(const Ratio* a, const Ratio* b, Ratio* out) {
  if (a == nullptr || b == nullptr || out == nullptr) return RatioResult::NullArgument;
  if (!valid(a) || !valid(b)) return RatioResult::BadRatio;
  return add_parts(a->num < 0, magnitude(a->num), static_cast<std::uint64_t>(a->den), b->num < 0, magnitude(b->num),
                   static_cast<std::uint64_t>(b->den), out)
             ? RatioResult::Ok
             : RatioResult::Overflow;
}

RatioResult ratio_sub(const Ratio* a, const Ratio* b, Ratio* out) {
  if (a == nullptr || b == nullptr || out == nullptr) return RatioResult::NullArgument;
  if (!valid(a) || !valid(b)) return RatioResult::BadRatio;
  return add_parts(a->num < 0, magnitude(a->num), static_cast<std::uint64_t>(a->den), !(b->num < 0), magnitude(b->num),
                   static_cast<std::uint64_t>(b->den), out)
             ? RatioResult::Ok
             : RatioResult::Overflow;
}

RatioResult ratio_mul(const Ratio* a, const Ratio* b, Ratio* out) {
  if (a == nullptr || b == nullptr || out == nullptr) return RatioResult::NullArgument;
  if (!valid(a) || !valid(b)) return RatioResult::BadRatio;
  return mul_parts(a->num < 0, magnitude(a->num), static_cast<std::uint64_t>(a->den), b->num < 0, magnitude(b->num),
                   static_cast<std::uint64_t>(b->den), out)
             ? RatioResult::Ok
             : RatioResult::Overflow;
}

RatioResult ratio_div(const Ratio* a, const Ratio* b, Ratio* out) {
  if (a == nullptr || b == nullptr || out == nullptr) return RatioResult::NullArgument;
  if (!valid(a) || !valid(b)) return RatioResult::BadRatio;
  if (b->num == 0) return RatioResult::ZeroDivision;
  // a / b = a * (den_b / num_b): the reciprocal has numerator den_b and denominator |num_b|, carrying the sign of num_b.
  return mul_parts(a->num < 0, magnitude(a->num), static_cast<std::uint64_t>(a->den), b->num < 0, static_cast<std::uint64_t>(b->den),
                   magnitude(b->num), out)
             ? RatioResult::Ok
             : RatioResult::Overflow;
}

RatioResult ratio_compare(const Ratio* a, const Ratio* b, std::int32_t* out) {
  if (a == nullptr || b == nullptr || out == nullptr) return RatioResult::NullArgument;
  if (!valid(a) || !valid(b)) return RatioResult::BadRatio;
  const int sa = a->num < 0 ? -1 : (a->num > 0 ? 1 : 0);
  const int sb = b->num < 0 ? -1 : (b->num > 0 ? 1 : 0);
  std::int32_t r = 0;
  if (sa != sb) {
    r = sa < sb ? -1 : 1;
  } else if (sa != 0) {
    // Same nonzero sign: compare |a.num| * b.den with |b.num| * a.den exactly.
    const Wide l = mul64(magnitude(a->num), static_cast<std::uint64_t>(b->den));
    const Wide rr = mul64(magnitude(b->num), static_cast<std::uint64_t>(a->den));
    if (!wide_equal(l, rr)) {
      const bool lower = wide_less(l, rr);  // |a| below |b|
      r = (lower != (sa < 0)) ? -1 : 1;
    }
  }
  *out = r;
  return RatioResult::Ok;
}

RatioResult ratio_floor(const Ratio* a, std::int64_t* out) {
  if (a == nullptr || out == nullptr) return RatioResult::NullArgument;
  if (!valid(a)) return RatioResult::BadRatio;
  std::int64_t q = a->num / a->den;  // den above 0, so no INT64_MIN / -1
  if (a->num % a->den < 0) q -= 1;   // a negative remainder means den above 1, so q above INT64_MIN
  *out = q;
  return RatioResult::Ok;
}

RatioResult ratio_ceil(const Ratio* a, std::int64_t* out) {
  if (a == nullptr || out == nullptr) return RatioResult::NullArgument;
  if (!valid(a)) return RatioResult::BadRatio;
  std::int64_t q = a->num / a->den;
  if (a->num % a->den > 0) q += 1;  // a positive remainder means den above 1, so q below INT64_MAX
  *out = q;
  return RatioResult::Ok;
}

}  // namespace lumora::ratio
