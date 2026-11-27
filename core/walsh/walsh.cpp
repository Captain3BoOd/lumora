// Subset-lattice transforms (contract in walsh.h; bounds, check order and step counts in docs/walsh.md).
//
// All four transforms are iterative butterfly passes over the array: for each level (block length len = 1, 2, 4, .. 2^(k-1)) every pair of entries
// (j, j + len) with the bit len clear in j is combined in place. A level touches 2^(k-1) pairs, so a transform takes exactly k * 2^(k-1) butterflies
// (each one an add and a subtract, or one add or one subtract). No recursion, no allocation, no scratch beyond the arrays.
// Arithmetic: residues are below kMod = 998244353 < 2^30, so the sum of two residues is below 2 * kMod < 2^32 and needs at most one subtraction
// (u32, no widening); a difference adds kMod back when it would go below zero. The only products are the pointwise product in xor_convolve and the
// scale by 2^-k, both taken as u64 (< kMod^2 < 2^60) and reduced with one %. No 128-bit types, no builtins.
#include "walsh.h"

namespace lumora::walsh {
namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kHalf = (kMod + 1u) / 2u;  // 2^-1 modulo kMod (kMod is odd)

inline u32 add_mod(u32 x, u32 y) {
  const u32 s = x + y;
  return s >= kMod ? s - kMod : s;
}

inline u32 sub_mod(u32 x, u32 y) { return x >= y ? x - y : x + kMod - y; }

inline u32 mul_mod(u32 x, u32 y) { return static_cast<u32>(static_cast<u64>(x) * y % kMod); }

// True when some entry of the 2^k entries is kMod or more (every entry is scanned).
bool any_out_of_range(const u32* a, u32 k) {
  const u32 n = 1u << k;
  bool bad = false;
  for (u32 i = 0; i < n; ++i)
    if (a[i] >= kMod) bad = true;
  return bad;
}

// The three butterfly passes. Unchecked: the callers have validated k <= kMaxK and the entries.
void xor_pass(u32* a, u32 k) {
  const u32 n = 1u << k;
  for (u32 len = 1; len < n; len <<= 1)
    for (u32 i = 0; i < n; i += 2u * len)
      for (u32 j = i; j < i + len; ++j) {
        const u32 x = a[j];
        const u32 y = a[j + len];
        a[j] = add_mod(x, y);
        a[j + len] = sub_mod(x, y);
      }
}

void zeta_pass(u32* a, u32 k) {
  const u32 n = 1u << k;
  for (u32 len = 1; len < n; len <<= 1)
    for (u32 i = 0; i < n; i += 2u * len)
      for (u32 j = i; j < i + len; ++j) a[j + len] = add_mod(a[j + len], a[j]);
}

void mobius_pass(u32* a, u32 k) {
  const u32 n = 1u << k;
  for (u32 len = 1; len < n; len <<= 1)
    for (u32 i = 0; i < n; i += 2u * len)
      for (u32 j = i; j < i + len; ++j) a[j + len] = sub_mod(a[j + len], a[j]);
}

// Multiplies every entry by 2^-k: the factor is kHalf^k, built by k multiplications.
void scale_by_inverse_power_of_two(u32* a, u32 k) {
  u32 f = 1;
  for (u32 i = 0; i < k; ++i) f = mul_mod(f, kHalf);
  const u32 n = 1u << k;
  for (u32 i = 0; i < n; ++i) a[i] = mul_mod(a[i], f);
}

// The shared checks of the four transforms, in the header order.
WalshResult check_transform(const u32* a, u32 k) {
  if (a == nullptr) return WalshResult::NullArgument;
  if (k > kMaxK) return WalshResult::TooLarge;
  if (any_out_of_range(a, k)) return WalshResult::ValueOutOfRange;
  return WalshResult::Ok;
}

}  // namespace

WalshResult walsh_xor(std::uint32_t* a, std::uint32_t k) {
  const WalshResult r = check_transform(a, k);
  if (r != WalshResult::Ok) return r;
  xor_pass(a, k);
  return WalshResult::Ok;
}

WalshResult walsh_xor_inverse(std::uint32_t* a, std::uint32_t k) {
  const WalshResult r = check_transform(a, k);
  if (r != WalshResult::Ok) return r;
  xor_pass(a, k);
  scale_by_inverse_power_of_two(a, k);
  return WalshResult::Ok;
}

WalshResult subset_zeta(std::uint32_t* a, std::uint32_t k) {
  const WalshResult r = check_transform(a, k);
  if (r != WalshResult::Ok) return r;
  zeta_pass(a, k);
  return WalshResult::Ok;
}

WalshResult subset_mobius(std::uint32_t* a, std::uint32_t k) {
  const WalshResult r = check_transform(a, k);
  if (r != WalshResult::Ok) return r;
  mobius_pass(a, k);
  return WalshResult::Ok;
}

WalshResult xor_convolve(const std::uint32_t* a, const std::uint32_t* b, std::uint32_t k, std::uint32_t* scratch, std::uint32_t scratch_words,
                         std::uint32_t* out) {
  if (a == nullptr || b == nullptr || out == nullptr || scratch == nullptr) return WalshResult::NullArgument;
  if (k > kMaxK) return WalshResult::TooLarge;
  if (scratch_words < (1u << k)) return WalshResult::ScratchTooSmall;
  if (any_out_of_range(a, k) || any_out_of_range(b, k)) return WalshResult::ValueOutOfRange;
  // Every check is done: from here on out and scratch are written. out holds the transform of a, scratch the transform of b.
  const u32 n = 1u << k;
  for (u32 i = 0; i < n; ++i) {
    out[i] = a[i];
    scratch[i] = b[i];
  }
  xor_pass(out, k);
  xor_pass(scratch, k);
  for (u32 i = 0; i < n; ++i) out[i] = mul_mod(out[i], scratch[i]);
  xor_pass(out, k);
  scale_by_inverse_power_of_two(out, k);
  return WalshResult::Ok;
}

}  // namespace lumora::walsh
