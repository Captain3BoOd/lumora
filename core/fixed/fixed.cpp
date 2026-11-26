// 16.16 fixed-point arithmetic. See fixed.h for the contract and docs/fixed.md for the bounds. Integer only: no floating point,
// no libm, no 128-bit types, no allocation. Every function is defined for every input.
#include "fixed.h"

#include <cstdint>

namespace lumora::fixed {

namespace {

// Magnitude of a signed 64-bit value as an unsigned one (defined for INT64_MIN too).
std::uint64_t magnitude(std::int64_t v) {
  return v < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
}

// Applies the sign to a rounded magnitude and saturates to [kMin, kMax].
Q16 saturate(bool negative, std::uint64_t mag) {
  if (negative) {
    if (mag >= std::uint64_t{1} << 31) return kMin;
    return static_cast<Q16>(-static_cast<std::int64_t>(mag));
  }
  if (mag > static_cast<std::uint64_t>(kMax)) return kMax;
  return static_cast<Q16>(mag);
}

}  // namespace

Q16 from_int(std::int32_t n) {
  const std::int64_t v = static_cast<std::int64_t>(n) * 65536;
  if (v > kMax) return kMax;
  if (v < kMin) return kMin;
  return static_cast<Q16>(v);
}

std::int32_t to_int_floor(Q16 a) {
  const std::int64_t v = a;
  if (v >= 0) return static_cast<std::int32_t>(v / 65536);
  return static_cast<std::int32_t>(-((-v + 65535) / 65536));
}

Q16 mul(Q16 a, Q16 b) {
  const std::int64_t p = static_cast<std::int64_t>(a) * b;
  const std::uint64_t rounded = (magnitude(p) + 32768u) >> 16;
  return saturate(p < 0, rounded);
}

Q16 div(Q16 a, Q16 b) {
  if (b == 0) return a > 0 ? kMax : (a < 0 ? kMin : 0);
  const std::uint64_t num = magnitude(a) << 16;
  const std::uint64_t den = magnitude(b);
  std::uint64_t q = num / den;
  const std::uint64_t rem = num % den;
  if (rem * 2u >= den) q += 1u;
  return saturate((a < 0) != (b < 0), q);
}

Q16 lerp(Q16 a, Q16 b, Q16 t) {
  const std::int64_t d = static_cast<std::int64_t>(b) - a;
  const std::int64_t p = d * t;
  const std::uint64_t rounded = (magnitude(p) + 32768u) >> 16;
  const std::int64_t term = p < 0 ? -static_cast<std::int64_t>(rounded) : static_cast<std::int64_t>(rounded);
  const std::int64_t sum = static_cast<std::int64_t>(a) + term;
  if (sum > kMax) return kMax;
  if (sum < kMin) return kMin;
  return static_cast<Q16>(sum);
}

std::uint32_t isqrt_u64(std::uint64_t v) {
  std::uint64_t result = 0;
  std::uint64_t bit = std::uint64_t{1} << 62;
  while (bit > v) bit >>= 2;
  while (bit != 0) {
    if (v >= result + bit) {
      v -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }
  return static_cast<std::uint32_t>(result);
}

Q16 sqrt(Q16 a) {
  if (a <= 0) return 0;
  return static_cast<Q16>(isqrt_u64(static_cast<std::uint64_t>(a) << 16));
}

}  // namespace lumora::fixed
