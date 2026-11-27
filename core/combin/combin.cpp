// Exact combinatorial counts over u64. See combin.h for the contract and docs/combin.md for the method and the bounds.
//
// Method. A binomial is built as r(i) = r(i-1) * (a + i) / i for i = 1..k', where k' is the smaller of the two symmetric arguments and a + k' is the
// upper argument. Every partial value r(i) = C(a + i, i) is an integer, is at most the final value (a >= 1) and so fits when the result fits; the
// product r(i-1) * (a + i) = i * r(i) is formed in two words (a 64 x 64 -> 128 bit product from 32-bit limbs, a factor of up to 65 bits adds one
// more partial product) and divided exactly by the small i. Any carry beyond two words, or a high word at least i (the quotient would be 2^64 or
// more), is Overflow. No 128-bit type, no builtin, no intrinsic; every loop has a constant bound.
#include "combin.h"

#include <cstdint>

namespace lumora::combin {
namespace {

constexpr std::uint64_t kLow32 = 0xFFFFFFFFull;
constexpr std::uint64_t kMaxK = 33;  // C(68, 34) is above 2^64 - 1 and C(2m, m) grows: k' above 33 always overflows
constexpr std::uint32_t kMaxFactorial = 20;
constexpr std::uint32_t kMaxCatalan = 36;

struct Wide {
  std::uint64_t hi;
  std::uint64_t lo;
};

// a * b as two words (four 32-bit partial products; no sum below can exceed 2^64 - 1).
Wide mul_wide(std::uint64_t a, std::uint64_t b) {
  const std::uint64_t a0 = a & kLow32, a1 = a >> 32, b0 = b & kLow32, b1 = b >> 32;
  const std::uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  const std::uint64_t mid = (p00 >> 32) + (p01 & kLow32) + (p10 & kLow32);
  Wide w;
  w.lo = (p00 & kLow32) | (mid << 32);
  w.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
  return w;
}

// (hi, lo) / d for 1 <= d <= 2^32 - 1 and hi < d (so the quotient fits): schoolbook division on two 32-bit limbs.
std::uint64_t div_small(std::uint64_t hi, std::uint64_t lo, std::uint64_t d) {
  std::uint64_t rem = hi;
  std::uint64_t cur = (rem << 32) | (lo >> 32);
  const std::uint64_t q1 = cur / d;
  rem = cur % d;
  cur = (rem << 32) | (lo & kLow32);
  const std::uint64_t q0 = cur / d;
  return (q1 << 32) | q0;
}

// r * f / d exactly, where f = fh * 2^64 + fl (fh is 0 or 1) and d is small, when the quotient fits; false when it does not (never wraps).
bool step(std::uint64_t r, std::uint64_t fh, std::uint64_t fl, std::uint64_t d, std::uint64_t* out) {
  Wide w = mul_wide(r, fl);
  if (fh != 0) {
    const std::uint64_t h = w.hi + r;
    if (h < w.hi) return false;  // the numerator needs more than two words
    w.hi = h;
  }
  if (w.hi >= d) return false;  // the quotient is 2^64 or more
  *out = div_small(w.hi, w.lo, d);
  return true;
}

// C(a + kk, kk) for a >= 1 (a = al) and kk <= 33: the product of (a + i) / i; the factor a + i can need 65 bits (fh is the 65th).
bool choose(std::uint64_t al, std::uint64_t kk, std::uint64_t* out) {
  if (kk > kMaxK) return false;
  std::uint64_t r = 1;
  for (std::uint64_t i = 1; i <= kMaxK; ++i) {
    if (i > kk) break;
    const std::uint64_t fl = al + i;
    const std::uint64_t fh = (fl < al) ? 1ull : 0ull;
    if (!step(r, fh, fl, i, &r)) return false;
  }
  *out = r;
  return true;
}

}  // namespace

CombResult binom_u64(std::uint64_t n, std::uint64_t k, std::uint64_t* out) {
  if (out == nullptr) return CombResult::NullArgument;
  if (k > n) {
    *out = 0;
    return CombResult::Ok;
  }
  const std::uint64_t kk = (n - k < k) ? n - k : k;
  // C(n, kk) = C(a + kk, kk) with a = n - kk >= kk >= 1 (for kk = 0 the loop does nothing and the value is 1).
  std::uint64_t v = 0;
  if (!choose(n - kk, kk, &v)) return CombResult::Overflow;
  *out = v;
  return CombResult::Ok;
}

CombResult multichoose_u64(std::uint64_t n, std::uint64_t k, std::uint64_t* out) {
  if (out == nullptr) return CombResult::NullArgument;
  if (n == 0) {
    *out = (k == 0) ? 1ull : 0ull;
    return CombResult::Ok;
  }
  // multichoose(n, k) = C(m + k, k) = C(m + k, m) with m = n - 1; kk is the smaller of k and m, and a = m + k - kk (up to 65 bits).
  const std::uint64_t m = n - 1;
  const std::uint64_t kk = (m < k) ? m : k;
  const std::uint64_t other = (m < k) ? k : m;  // the larger of the two: a = other (the sum m + k - kk)
  std::uint64_t v = 0;
  // m + k - kk = other exactly, so a fits in one word; the factors other + i can need 65 bits (for kk = 0 the loop does nothing and the value is 1).
  if (!choose(other, kk, &v)) return CombResult::Overflow;
  *out = v;
  return CombResult::Ok;
}

CombResult factorial_u64(std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return CombResult::NullArgument;
  if (n > kMaxFactorial) return CombResult::Overflow;
  std::uint64_t r = 1;
  for (std::uint32_t i = 2; i <= kMaxFactorial; ++i) {
    if (i > n) break;
    r *= i;
  }
  *out = r;
  return CombResult::Ok;
}

CombResult catalan_u64(std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return CombResult::NullArgument;
  if (n > kMaxCatalan) return CombResult::Overflow;
  // C(i + 1) = C(i) * (4i + 2) / (i + 2)
  std::uint64_t r = 1;
  for (std::uint64_t i = 0; i < kMaxCatalan; ++i) {
    if (i >= n) break;
    if (!step(r, 0, 4 * i + 2, i + 2, &r)) return CombResult::Overflow;
  }
  *out = r;
  return CombResult::Ok;
}

}  // namespace lumora::combin
