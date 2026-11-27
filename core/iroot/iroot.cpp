// Exact integer roots and powers over u64. See iroot.h for the contract and docs/iroot.md for the method and the bounds.
//
// Method. A power is built by repeated multiplication of an unsigned 64-bit value; every product is tested before it is formed (a * b exceeds
// UINT64_MAX exactly when a > UINT64_MAX / b), so no wide product, no 128-bit type, no builtin and no intrinsic is needed. The root is a binary search over
// 1 .. 2^ceil(bits / k) - 1 (at most 2^32 - 1, the k-th root of a u64 is below 2^32 for k >= 2) with the comparison r^k <= x, which stops as soon as the partial product passes x.
// Every loop has a constant bound: ipow at most 63 multiplications, the root search 32 steps of at most 63 multiplications, perfect_power 62 exponents.
#include "iroot.h"

#include <cstdint>

namespace lumora::iroot {
namespace {

constexpr std::uint64_t kMaxU64 = 0xFFFFFFFFFFFFFFFFull;
constexpr std::uint32_t kMaxExp = 63;  // 2^64 is above every u64, so a base of at least 2 cannot have an exponent above 63

// r^k for r >= 1 and 2 <= k <= 63, stopping as soon as a partial product would pass limit: returns true and the value when r^k <= limit, false when
// it is above. A partial product above limit can never come back below it (r >= 1 and the product only grows), so the early exit is exact.
bool pow_le(std::uint64_t r, std::uint32_t k, std::uint64_t limit, std::uint64_t* out) {
  std::uint64_t p = 1;
  for (std::uint32_t i = 0; i < kMaxExp; ++i) {
    if (i >= k) break;
    if (p > limit / r) return false;  // p * r > limit
    p *= r;
  }
  *out = p;
  return true;
}

// floor(x^(1/k)) for x >= 2 and 2 <= k <= 63.
std::uint64_t root_core(std::uint64_t x, std::uint32_t k) {
  // x < 2^bits, so the root is below 2^ceil(bits / k): the answer is in lo .. hi (at most 2^32 - 1, as k >= 2).
  std::uint32_t bits = 0;
  for (std::uint32_t i = 0; i < 64; ++i)
    if ((x >> i) != 0) bits = i + 1;
  const std::uint32_t sh = (bits + k - 1) / k;
  std::uint64_t lo = 1, hi = (1ull << sh) - 1;
  for (int it = 0; it < 32; ++it) {
    if (lo >= hi) break;
    const std::uint64_t mid = lo + (hi - lo + 1) / 2;
    std::uint64_t p = 0;
    if (pow_le(mid, k, x, &p)) lo = mid;
    else hi = mid - 1;  // mid^k > x: every larger candidate is too large
  }
  return lo;
}

}  // namespace

RootResult iroot_u64(std::uint64_t x, std::uint32_t k, std::uint64_t* out) {
  if (out == nullptr) return RootResult::NullArgument;
  if (k == 0 || k > 64) return RootResult::BadExponent;
  std::uint64_t v;
  if (x < 2 || k == 1) v = x;
  else if (k == 64) v = 1;  // 2^64 is above every u64
  else v = root_core(x, k);
  *out = v;
  return RootResult::Ok;
}

RootResult perfect_power_u64(std::uint64_t x, std::uint64_t* out_base, std::uint32_t* out_exp) {
  if (out_base == nullptr || out_exp == nullptr) return RootResult::NullArgument;
  std::uint64_t base = x;
  std::uint32_t e = 1;
  if (x >= 2) {
    // The largest exponent first: the first exact power found has the largest e, hence the smallest base.
    for (std::uint32_t ee = kMaxExp; ee >= 2; --ee) {
      if ((x >> ee) == 0) continue;  // x < 2^ee: no base of at least 2 has that power
      const std::uint64_t r = root_core(x, ee);
      std::uint64_t p = 0;
      if (r >= 2 && pow_le(r, ee, x, &p) && p == x) {
        base = r;
        e = ee;
        break;
      }
    }
  }
  *out_base = base;
  *out_exp = e;
  return RootResult::Ok;
}

RootResult ipow_u64(std::uint64_t base, std::uint32_t exp, std::uint64_t* out) {
  if (out == nullptr) return RootResult::NullArgument;
  std::uint64_t r = 1;
  if (exp == 0) {
    r = 1;
  } else if (base == 0) {
    r = 0;
  } else if (base == 1) {
    r = 1;
  } else {
    if (exp > kMaxExp) return RootResult::Overflow;
    for (std::uint32_t i = 0; i < kMaxExp; ++i) {
      if (i >= exp) break;
      if (r > kMaxU64 / base) return RootResult::Overflow;
      r *= base;
    }
  }
  *out = r;
  return RootResult::Ok;
}

}  // namespace lumora::iroot
