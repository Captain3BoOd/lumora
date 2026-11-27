// Best rational approximations under a denominator bound. See ratapprox.h for the contract and docs/ratapprox.md for the method and the proofs.
//
// Method. The input is reduced with unsigned magnitudes (INT64_MIN is never negated as a signed number): x = sign * P / Q in lowest terms, Q above 0.
// When Q <= max_den, x is its own candidate. Otherwise the Farey neighbours of |x| = P / Q are found by a Stern-Brocot walk with batched steps: the
// current lower bound L = a / b and upper bound U = c / d (c * b - a * d = 1) start as floor(|x|) / 1 and (floor(|x|) + 1) / 1, together with the
// residuals eL = P * b - a * Q and eU = c * Q - P * d (both above 0 while x lies strictly between). One batch moves the bound that is on the far side
// of the mediant by the largest multiple that keeps it on its side of x and keeps its denominator at most max_den; the multiple comes from a division,
// never from a product that can overflow. Every number is a uint64_t: numerators never exceed P <= 2^63, denominators never exceed max_den, the
// residuals never exceed Q <= 2^63. No 128-bit type, builtin or intrinsic, no recursion, no allocation. Every loop has a constant bound (a gcd of at
// most 91 steps for values below 2^63, a walk of at most 93 batches, both capped at 128).
//
// Nearest: eL * d + eU * b = Q, so the distances |x - L| = eL / (Q * b) and |U - x| = eU / (Q * d) compare as eL * d against eU * b, two numbers
// that add up to Q <= 2^63. A negative x is solved on |x| and mirrored (lo = -U, hi = -L); ties are then decided in the ORIGINAL orientation.
#include "ratapprox.h"

#include <cstdint>

namespace lumora::ratapprox {
namespace {

constexpr std::uint64_t kMaxPos = 0x7FFFFFFFFFFFFFFFull;  // INT64_MAX
constexpr int kSteps = 128;

std::uint64_t magnitude(std::int64_t v) {
  const std::uint64_t u = static_cast<std::uint64_t>(v);
  return v < 0 ? (~u + 1ull) : u;
}

std::uint64_t gcd64(std::uint64_t a, std::uint64_t b) {
  for (int i = 0; i < kSteps && b != 0; ++i) {
    const std::uint64_t t = a % b;
    a = b;
    b = t;
  }
  return a;
}

std::uint64_t min64(std::uint64_t a, std::uint64_t b) { return a < b ? a : b; }

// A signed numerator from a magnitude that is known to fit (callers guarantee m <= 2^63 for a negative sign and m <= 2^63 - 1 otherwise).
std::int64_t signed_num(bool neg, std::uint64_t m) { return static_cast<std::int64_t>(neg ? (~m + 1ull) : m); }

struct Solution {
  bool candidate;        // x itself is a candidate (reduced denominator at most max_den)
  bool neg;              // x is negative
  std::uint64_t p, q;    // |x| = p / q in lowest terms
  std::uint64_t a, b;    // lower neighbour of |x|: a / b (valid when !candidate)
  std::uint64_t c, d;    // upper neighbour of |x|: c / d
  std::uint64_t el, eu;  // residuals: el = p * b - a * q, eu = c * q - p * d
};

// Returns false for the one family of inputs whose result would need the numerator +2^63 (x = +2^63 / q in lowest terms with q <= max_den).
bool solve(std::int64_t num, std::int64_t den, std::uint64_t max_den, Solution* s) {
  const std::uint64_t mn = magnitude(num), md = magnitude(den);
  const std::uint64_t g = gcd64(mn, md);  // md above 0, so g above 0
  s->p = mn / g;
  s->q = md / g;
  s->neg = ((num < 0) != (den < 0)) && s->p != 0;
  s->candidate = s->q <= max_den;
  if (s->candidate) return s->neg || s->p <= kMaxPos;
  const std::uint64_t a0 = s->p / s->q;
  s->a = a0;
  s->b = 1;
  s->c = a0 + 1;
  s->d = 1;
  s->el = s->p - a0 * s->q;
  s->eu = s->q - s->el;
  for (int i = 0; i < kSteps; ++i) {
    if (s->b + s->d > max_den) break;
    if (s->el > s->eu) {
      const std::uint64_t k = min64((max_den - s->b) / s->d, s->el / s->eu);
      s->a += k * s->c;
      s->b += k * s->d;
      s->el -= k * s->eu;
    } else {
      const std::uint64_t k = min64((max_den - s->d) / s->b, (s->eu - 1ull) / s->el);
      s->c += k * s->a;
      s->d += k * s->b;
      s->eu -= k * s->el;
    }
  }
  return true;
}

}  // namespace

ApproxResult ratapprox_bounds(std::int64_t num, std::int64_t den, std::uint64_t max_den, Frac* lo, Frac* hi) {
  if (lo == nullptr || hi == nullptr) return ApproxResult::NullArgument;
  if (den == 0) return ApproxResult::ZeroDivision;
  if (max_den == 0 || max_den > kMaxPos) return ApproxResult::BadBound;
  Solution s;
  if (!solve(num, den, max_den, &s)) return ApproxResult::Overflow;
  Frac l, h;
  if (s.candidate) {
    l = Frac{signed_num(s.neg, s.p), static_cast<std::int64_t>(s.q)};
    h = l;
  } else if (s.neg) {
    l = Frac{signed_num(true, s.c), static_cast<std::int64_t>(s.d)};
    h = Frac{signed_num(true, s.a), static_cast<std::int64_t>(s.b)};
  } else {
    l = Frac{signed_num(false, s.a), static_cast<std::int64_t>(s.b)};
    h = Frac{signed_num(false, s.c), static_cast<std::int64_t>(s.d)};
  }
  *lo = l;
  *hi = h;
  return ApproxResult::Ok;
}

ApproxResult ratapprox_nearest(std::int64_t num, std::int64_t den, std::uint64_t max_den, Frac* out) {
  if (out == nullptr) return ApproxResult::NullArgument;
  if (den == 0) return ApproxResult::ZeroDivision;
  if (max_den == 0 || max_den > kMaxPos) return ApproxResult::BadBound;
  Solution s;
  if (!solve(num, den, max_den, &s)) return ApproxResult::Overflow;
  Frac r;
  if (s.candidate) {
    r = Frac{signed_num(s.neg, s.p), static_cast<std::int64_t>(s.q)};
  } else {
    // Distances: to a / b it is el / (q * b), to c / d it is eu / (q * d); el * d + eu * b = q, so both products are at most q.
    const std::uint64_t near_l = s.el * s.d;  // proportional to |x - L| * b * d
    const std::uint64_t near_u = s.eu * s.b;  // proportional to |U - x| * b * d
    bool pick_l;
    if (near_l != near_u) {
      pick_l = near_l < near_u;
    } else if (s.b != s.d) {
      pick_l = s.b < s.d;  // tie: the smaller denominator
    } else {
      pick_l = !s.neg;  // tie, equal denominators: the smaller value (L for x > 0, the mirror of U for x < 0)
    }
    r = pick_l ? Frac{signed_num(s.neg, s.a), static_cast<std::int64_t>(s.b)} : Frac{signed_num(s.neg, s.c), static_cast<std::int64_t>(s.d)};
  }
  *out = r;
  return ApproxResult::Ok;
}

}  // namespace lumora::ratapprox
