// Lagrange interpolation at the nodes 0 .. n - 1 (contract in lagrange.h; derivation, bounds and step counts in docs/lagrange.md).
//
// With r = x mod kMod and r >= n, the value is f(r) = sum over i of y_i * prod over j != i of (r - j) / (i - j). Writing
//   prod over j != i of (i - j) = i! * (n-1-i)! * (-1)^(n-1-i)   and   prod over j != i of (r - j) = P / (r - i),   P = prod over j < n of (r - j),
//   f(r) = P / (n-1)! * sum over i of s_i * y_i * C(n-1, i) / (r - i),   s_i = (-1)^(n-1-i),   C(n-1, i) = A_i / B_i,
//   A_i = (n-1)(n-2) .. (n-i),  B_i = i!.
// The sum is kept as ONE fraction N / D over the nodes seen so far (term i is a_i / b_i with a_i = s_i * y_i * A_i, b_i = B_i * (r - i)):
//   N <- N * b_i + a_i * D,  D <- D * b_i.
// No division happens per node. After the loop f(r) = P * N / (D * (n-1)!): one modular inverse. State: six residues; no table, no recursion.
// Arithmetic: residues are below kMod < 2^30, products are u64 (< kMod^2 < 2^60) reduced with one %, the sum of two residues is below
// 2 * kMod < 2^32 (u32) and needs one subtraction. r - i is a positive residue (r >= n > i, so no wrap) and every factorial up to (n-1)! is a
// product of numbers below kMod, so no denominator is zero.
#include "lagrange.h"

namespace lumora::lagrange {
namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

inline u32 add_mod(u32 x, u32 y) {
  const u32 s = x + y;
  return s >= kMod ? s - kMod : s;
}

inline u32 sub_mod(u32 x, u32 y) { return x >= y ? x - y : x + kMod - y; }

inline u32 mul_mod(u32 x, u32 y) { return static_cast<u32>(static_cast<u64>(x) * y % kMod); }

// base^(kMod - 2) = base^-1 for a base that is not 0 modulo kMod (Fermat), by square and multiply over the bits of kMod - 2.
u32 inverse(u32 base) {
  u32 result = 1;
  u32 e = kMod - 2u;
  while (e != 0) {
    if ((e & 1u) != 0) result = mul_mod(result, base);
    base = mul_mod(base, base);
    e >>= 1;
  }
  return result;
}

bool any_out_of_range(const u32* y, u32 n) {
  bool bad = false;
  for (u32 i = 0; i < n; ++i)
    if (y[i] >= kMod) bad = true;
  return bad;
}

}  // namespace

LagResult lagrange_eval(const std::uint32_t* y, std::uint32_t n, std::uint32_t x, std::uint32_t* out) {
  if (y == nullptr || out == nullptr) return LagResult::NullArgument;
  if (n == 0) return LagResult::EmptyInput;
  if (n > kMaxN) return LagResult::TooLarge;
  if (any_out_of_range(y, n)) return LagResult::ValueOutOfRange;
  const u32 r = x % kMod;
  if (r < n) {
    *out = y[r];
    return LagResult::Ok;
  }
  u32 num = 0;  // N: the sum of the terms seen so far, times D
  u32 den = 1;  // D: the product of b_t over the nodes seen so far
  u32 prod = 1;  // P: the product of (r - t)
  u32 a_i = 1;  // A_i = (n-1)(n-2) .. (n-i)
  u32 b_i = 1;  // B_i = i!
  for (u32 i = 0; i < n; ++i) {
    if (i > 0) {
      a_i = mul_mod(a_i, n - i);
      b_i = mul_mod(b_i, i);
    }
    const u32 d = r - i;
    const u32 b = mul_mod(b_i, d);
    u32 a = mul_mod(y[i], a_i);
    if (((n - 1u - i) & 1u) != 0) a = sub_mod(0, a);
    num = add_mod(mul_mod(num, b), mul_mod(a, den));
    den = mul_mod(den, b);
    prod = mul_mod(prod, d);
  }
  // Here b_i = (n-1)!.
  *out = mul_mod(prod, mul_mod(num, inverse(mul_mod(den, b_i))));
  return LagResult::Ok;
}

}  // namespace lumora::lagrange
