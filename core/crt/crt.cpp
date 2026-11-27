// Chinese remainder theorem for moduli that need not be coprime. See crt.h for the contract and docs/crt.md for the method and the bounds.
//
// Method. (1) Zero scan: every modulus is checked in one pass. (2) Consistency: for every pair i < j the residues must agree modulo
// g = gcd(m[i], m[j]) (exact mathematics, independent of the size of the lcm). (3) Merge the congruences one at a time. The state is x modulo L with
// x < L and L at most 2^64 - 1 (initially x = 0, L = 1). Merging x' = r mod m: g = gcd(L, m), m' = m / g, L' = L / g. The pairwise check guarantees that
// g divides r' - x, and the merged value is x + L * t with t = ((r' - x) / g) * inverse(L' mod m') mod m' (L' and m' are coprime). The new lcm is
// L * m'; when it would exceed 2^64 - 1 (tested as L > (2^64 - 1) / m', no product is formed) the answer is Overflow: a solution exists because the system
// is pairwise consistent. Otherwise x + L * t < L * m' fits. The modular product uses a shift-and-add loop of 64 steps (no 128-bit type, no builtin,
// no intrinsic); the inverse is the extended Euclid algorithm on unsigned magnitudes with the sign tracked by the step parity.
// Every loop has a constant bound: gcd and inverse at most 93 steps (capped at 128), at most 63 merges, at most 2016 pair checks, 64 steps per product.
#include "crt.h"

#include <cstdint>

namespace lumora::crt {
namespace {

constexpr std::uint64_t kMaxU64 = 0xFFFFFFFFFFFFFFFFull;
constexpr int kSteps = 128;

std::uint64_t gcd64(std::uint64_t a, std::uint64_t b) {
  for (int i = 0; i < kSteps && b != 0; ++i) {
    const std::uint64_t t = a % b;
    a = b;
    b = t;
  }
  return a;
}

// (a + b) mod m for a, b below m, without forming a sum above 2^64 - 1.
std::uint64_t add_mod(std::uint64_t a, std::uint64_t b, std::uint64_t m) { return a >= m - b ? a - (m - b) : a + b; }

// (a * b) mod m for a, b below m (m at least 1): 64 steps over the bits of b, high to low.
std::uint64_t mul_mod(std::uint64_t a, std::uint64_t b, std::uint64_t m) {
  std::uint64_t r = 0;
  for (int i = 63; i >= 0; --i) {
    r = add_mod(r, r, m);
    if (((b >> i) & 1ull) != 0) r = add_mod(r, a, m);
  }
  return r;
}

// The inverse of a modulo n for gcd(a, n) = 1 and 1 <= a < n (n above 1). Extended Euclid on magnitudes: s_{k+1} = s_{k-1} - q * s_k has alternating
// signs, so the magnitudes obey t_{k+1} = t_{k-1} + q * t_k and never exceed n.
std::uint64_t inverse_mod(std::uint64_t a, std::uint64_t n) {
  std::uint64_t r0 = n, r1 = a, t0 = 0, t1 = 1;
  int steps = 0;
  for (int i = 0; i < kSteps && r1 != 0; ++i) {
    const std::uint64_t q = r0 / r1;
    const std::uint64_t r2 = r0 - q * r1;
    const std::uint64_t t2 = t0 + q * t1;
    r0 = r1;
    r1 = r2;
    t0 = t1;
    t1 = t2;
    ++steps;
  }
  // t0 is |s_steps| with the sign (-1)^(steps + 1): positive for an odd step count.
  return (steps % 2 == 1) ? t0 : n - t0;
}

}  // namespace

CrtResult crt_solve(std::uint32_t n, const std::uint64_t* r, const std::uint64_t* m, std::uint64_t* out_r, std::uint64_t* out_m) {
  if (out_r == nullptr || out_m == nullptr) return CrtResult::NullArgument;
  if (n > 0 && (r == nullptr || m == nullptr)) return CrtResult::NullArgument;
  if (n > kMaxEquations) return CrtResult::TooLarge;
  for (std::uint32_t i = 0; i < n; ++i)
    if (m[i] == 0) return CrtResult::ZeroModulus;
  for (std::uint32_t i = 0; i < n; ++i)
    for (std::uint32_t j = i + 1; j < n; ++j) {
      const std::uint64_t g = gcd64(m[i], m[j]);
      if (r[i] % g != r[j] % g) return CrtResult::NoSolution;
    }
  std::uint64_t x = 0, big_l = 1;
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint64_t mi = m[i];
    const std::uint64_t ri = r[i] % mi;
    const std::uint64_t g = gcd64(big_l, mi);
    const std::uint64_t mp = mi / g;
    if (mp == 1) continue;  // m divides L: the equation is already implied (the pair checks made it consistent)
    if (big_l > kMaxU64 / mp) return CrtResult::Overflow;
    const std::uint64_t lp = (big_l / g) % mp;
    // (ri - x) / g modulo mp
    std::uint64_t d;
    if (ri >= x) {
      d = ((ri - x) / g) % mp;
    } else {
      d = ((x - ri) / g) % mp;
      if (d != 0) d = mp - d;
    }
    const std::uint64_t t = d == 0 ? 0 : mul_mod(d, inverse_mod(lp, mp), mp);
    x += big_l * t;
    big_l *= mp;
  }
  *out_r = x;
  *out_m = big_l;
  return CrtResult::Ok;
}

}  // namespace lumora::crt
