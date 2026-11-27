// Arbitrary-length unsigned integers as little-endian u32 limb arrays; see limbs.h for the contract and docs/limbs.md for the check order
// and bounds. 64-bit intermediates only (no 128-bit types, no compiler builtins), no allocation, no recursion, no global state.
// Failure atomicity: nothing is written to dst or to an out pointer until every failure condition is ruled out. The normalized result
// length is therefore found by a read-only pass (carry for add, borrow for sub, column sums for mul, the top limb for divmod) before dst is
// written; the multiplication writes straight into dst only when the capacity already holds the largest possible length.
#include "limbs.h"

#include <cstdint>

namespace lumora::limbs {

namespace {

constexpr std::uint64_t kMask32 = 0xFFFFFFFFull;

// Length without high zero limbs.
std::uint32_t norm_len(const std::uint32_t* x, std::uint32_t n) {
  while (n > 0u && x[n - 1u] == 0u) --n;
  return n;
}

// Limb i of x, or 0 at and above the length.
std::uint32_t limb(const std::uint32_t* x, std::uint32_t n, std::uint32_t i) { return i < n ? x[i] : 0u; }

// Compares the values of a[0 .. la) and b[0 .. lb), both already normalized.
std::int32_t cmp_norm(const std::uint32_t* a, std::uint32_t la, const std::uint32_t* b, std::uint32_t lb) {
  if (la != lb) return la < lb ? -1 : 1;
  std::uint32_t i = la;
  while (i > 0u) {
    --i;
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  }
  return 0;
}

// Sum a + b over normalized lengths la, lb. Returns the normalized length; writes dst[0 .. length) when dst is not null.
std::uint32_t add_pass(const std::uint32_t* a, std::uint32_t la, const std::uint32_t* b, std::uint32_t lb, std::uint32_t* dst) {
  const std::uint32_t n = la > lb ? la : lb;
  std::uint64_t carry = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint64_t s = static_cast<std::uint64_t>(limb(a, la, i)) + static_cast<std::uint64_t>(limb(b, lb, i)) + carry;
    if (dst != nullptr) dst[i] = static_cast<std::uint32_t>(s & kMask32);
    carry = s >> 32;
  }
  if (carry != 0u) {
    if (dst != nullptr) dst[n] = static_cast<std::uint32_t>(carry);
    return n + 1u;
  }
  return n;
}

// Difference a - b for a >= b over normalized lengths. Returns the normalized length; writes dst[0 .. length) when dst is not null.
std::uint32_t sub_pass(const std::uint32_t* a, std::uint32_t la, const std::uint32_t* b, std::uint32_t lb, std::uint32_t* dst, std::uint32_t limit) {
  std::uint32_t len = 0;
  std::uint64_t borrow = 0;
  for (std::uint32_t i = 0; i < la; ++i) {
    const std::uint64_t x = static_cast<std::uint64_t>(a[i]);
    const std::uint64_t y = static_cast<std::uint64_t>(limb(b, lb, i)) + borrow;
    const std::uint32_t d = static_cast<std::uint32_t>((x - y) & kMask32);
    borrow = x < y ? 1u : 0u;
    if (dst != nullptr && i < limit) dst[i] = d;
    if (d != 0u) len = i + 1u;
  }
  return len;
}

// Product of normalized non-zero a and b by columns. The column sum is kept in two u64 words (lo, hi) because it can reach 2^80. Writes limb k to
// dst[k] for k < write_count when dst is not null. Returns the normalized length.
std::uint32_t mul_pass(const std::uint32_t* a, std::uint32_t la, const std::uint32_t* b, std::uint32_t lb, std::uint32_t* dst, std::uint32_t write_count) {
  const std::uint32_t total = la + lb;
  std::uint64_t lo = 0;
  std::uint64_t hi = 0;
  std::uint32_t len = 0;
  for (std::uint32_t k = 0; k < total; ++k) {
    const std::uint32_t i_first = k >= lb ? k - lb + 1u : 0u;
    const std::uint32_t i_last = k < la ? k : la - 1u;
    for (std::uint32_t i = i_first; i <= i_last; ++i) {
      const std::uint64_t p = static_cast<std::uint64_t>(a[i]) * static_cast<std::uint64_t>(b[k - i]);
      lo += p;
      if (lo < p) ++hi;
    }
    const std::uint32_t out = static_cast<std::uint32_t>(lo & kMask32);
    if (dst != nullptr && k < write_count) dst[k] = out;
    if (out != 0u) len = k + 1u;
    lo = (lo >> 32) | (hi << 32);
    hi >>= 32;
  }
  return len;
}

}  // namespace

LimbResult limbs_cmp(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::int32_t* out) {
  if ((a == nullptr && na > 0u) || (b == nullptr && nb > 0u) || out == nullptr) return LimbResult::NullArgument;
  if (na > kMaxLimbs || nb > kMaxLimbs) return LimbResult::TooLarge;
  *out = cmp_norm(a, norm_len(a, na), b, norm_len(b, nb));
  return LimbResult::Ok;
}

LimbResult limbs_add(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst,
                     std::uint32_t dst_cap, std::uint32_t* out_len) {
  if ((a == nullptr && na > 0u) || (b == nullptr && nb > 0u) || (dst == nullptr && dst_cap > 0u) || out_len == nullptr) {
    return LimbResult::NullArgument;
  }
  if (na > kMaxLimbs || nb > kMaxLimbs) return LimbResult::TooLarge;
  const std::uint32_t la = norm_len(a, na);
  const std::uint32_t lb = norm_len(b, nb);
  const std::uint32_t len = add_pass(a, la, b, lb, nullptr);
  if (dst_cap < len) return LimbResult::BufferTooSmall;
  add_pass(a, la, b, lb, dst);
  *out_len = len;
  return LimbResult::Ok;
}

LimbResult limbs_sub(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst,
                     std::uint32_t dst_cap, std::uint32_t* out_len) {
  if ((a == nullptr && na > 0u) || (b == nullptr && nb > 0u) || (dst == nullptr && dst_cap > 0u) || out_len == nullptr) {
    return LimbResult::NullArgument;
  }
  if (na > kMaxLimbs || nb > kMaxLimbs) return LimbResult::TooLarge;
  const std::uint32_t la = norm_len(a, na);
  const std::uint32_t lb = norm_len(b, nb);
  if (cmp_norm(a, la, b, lb) < 0) return LimbResult::Underflow;
  const std::uint32_t len = sub_pass(a, la, b, lb, nullptr, 0u);
  if (dst_cap < len) return LimbResult::BufferTooSmall;
  sub_pass(a, la, b, lb, dst, len);
  *out_len = len;
  return LimbResult::Ok;
}

LimbResult limbs_mul(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst,
                     std::uint32_t dst_cap, std::uint32_t* out_len) {
  if ((a == nullptr && na > 0u) || (b == nullptr && nb > 0u) || (dst == nullptr && dst_cap > 0u) || out_len == nullptr) {
    return LimbResult::NullArgument;
  }
  if (na > kMaxLimbs || nb > kMaxLimbs) return LimbResult::TooLarge;
  const std::uint32_t la = norm_len(a, na);
  const std::uint32_t lb = norm_len(b, nb);
  if (la == 0u || lb == 0u) {
    *out_len = 0u;
    return LimbResult::Ok;
  }
  const std::uint32_t most = la + lb;  // the product has most or most - 1 limbs
  if (dst_cap >= most) {
    *out_len = mul_pass(a, la, b, lb, dst, most);  // the capacity holds every possible limb: write while computing
    return LimbResult::Ok;
  }
  const std::uint32_t len = mul_pass(a, la, b, lb, nullptr, 0u);  // read-only pass: dst stays untouched on failure
  if (dst_cap < len) return LimbResult::BufferTooSmall;
  mul_pass(a, la, b, lb, dst, len);
  *out_len = len;
  return LimbResult::Ok;
}

LimbResult limbs_divmod_u32(const std::uint32_t* a, std::uint32_t na, std::uint32_t d, std::uint32_t* q, std::uint32_t q_cap,
                            std::uint32_t* q_len, std::uint32_t* rem) {
  if ((a == nullptr && na > 0u) || (q == nullptr && q_cap > 0u) || q_len == nullptr || rem == nullptr) return LimbResult::NullArgument;
  if (na > kMaxLimbs) return LimbResult::TooLarge;
  if (d == 0u) return LimbResult::DivideByZero;
  const std::uint32_t la = norm_len(a, na);
  // With a non-zero top limb the quotient has la limbs when top >= d and la - 1 limbs otherwise (the next quotient limb is then at least 1).
  const std::uint32_t len = la == 0u ? 0u : (a[la - 1u] >= d ? la : la - 1u);
  if (q_cap < len) return LimbResult::BufferTooSmall;
  std::uint64_t r = 0;
  std::uint32_t i = la;
  while (i > 0u) {
    --i;
    const std::uint64_t cur = (r << 32) | static_cast<std::uint64_t>(a[i]);
    const std::uint64_t qi = cur / static_cast<std::uint64_t>(d);
    r = cur % static_cast<std::uint64_t>(d);
    if (i < len) q[i] = static_cast<std::uint32_t>(qi);
  }
  *q_len = len;
  *rem = static_cast<std::uint32_t>(r);
  return LimbResult::Ok;
}

}  // namespace lumora::limbs
