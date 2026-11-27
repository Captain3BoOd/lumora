// Exact 64-bit modular arithmetic: mulmod, powmod, modinv and a deterministic is_prime; see modarith.h for the contract and docs/modarith.md
// for the algorithms, the check order and the primality argument. Integer only, includes <cstdint> only, no 128-bit type, no builtin or
// intrinsic, no allocation, no recursion, no global state.
#include "modarith.h"

#include <cstdint>

namespace lumora::modarith {

namespace {

// (x + y) mod m for x, y < m, without ever forming a sum above 2^64 - 1: x + y >= m exactly when x >= m - y.
std::uint64_t add_mod(std::uint64_t x, std::uint64_t y, std::uint64_t m) { return x >= m - y ? x - (m - y) : x + y; }

// (a * b) mod m for m > 0 and any a, b. A modulus below 2^32 keeps the product of two residues below 2^64, so one multiplication does it.
// Otherwise a 64-step double-and-add over the bits of b, from the top, with every partial value kept below m.
std::uint64_t mul_mod(std::uint64_t a, std::uint64_t b, std::uint64_t m) {
  a %= m;
  b %= m;
  if (m <= 0xFFFFFFFFu) return a * b % m;
  std::uint64_t r = 0;
  for (std::uint32_t i = 64; i > 0u; --i) {
    r = add_mod(r, r, m);
    if (((b >> (i - 1u)) & 1u) != 0u) r = add_mod(r, a, m);
  }
  return r;
}

// (base ^ exp) mod m for m > 0: square and multiply over the bits of exp from the lowest.
std::uint64_t pow_mod(std::uint64_t base, std::uint64_t exp, std::uint64_t m) {
  std::uint64_t result = 1u % m;
  base %= m;
  while (exp != 0u) {
    if ((exp & 1u) != 0u) result = mul_mod(result, base, m);
    exp >>= 1;
    if (exp != 0u) base = mul_mod(base, base, m);
  }
  return result;
}

// One Miller-Rabin round for the odd n > 2 with n - 1 = d * 2^s, d odd, to the base a (0 < a < n). True when n passes (is a strong probable
// prime to the base a).
bool strong_probable_prime(std::uint64_t n, std::uint64_t d, std::uint32_t s, std::uint64_t a) {
  std::uint64_t x = pow_mod(a, d, n);
  if (x == 1u || x == n - 1u) return true;
  for (std::uint32_t i = 1; i < s; ++i) {
    x = mul_mod(x, x, n);
    if (x == n - 1u) return true;
  }
  return false;
}

}  // namespace

ModResult mulmod_u64(std::uint64_t a, std::uint64_t b, std::uint64_t m, std::uint64_t* out) {
  if (out == nullptr) return ModResult::NullArgument;
  if (m == 0u) return ModResult::ZeroModulus;
  *out = mul_mod(a, b, m);
  return ModResult::Ok;
}

ModResult powmod_u64(std::uint64_t base, std::uint64_t exp, std::uint64_t m, std::uint64_t* out) {
  if (out == nullptr) return ModResult::NullArgument;
  if (m == 0u) return ModResult::ZeroModulus;
  *out = pow_mod(base, exp, m);
  return ModResult::Ok;
}

// Extended Euclid on (m, a mod m). Invariant: t0 * a == r0 and t1 * a == r1 (mod m), with t0 and t1 kept as residues in [0, m), so no signed
// value and no overflow appear. The quotient q is at most m, and q * r1 <= r0, so the remainder step cannot wrap.
ModResult modinv_u64(std::uint64_t a, std::uint64_t m, std::uint64_t* out) {
  if (out == nullptr) return ModResult::NullArgument;
  if (m == 0u) return ModResult::ZeroModulus;
  if (m == 1u) {
    *out = 0;
    return ModResult::Ok;
  }
  std::uint64_t r0 = m;
  std::uint64_t r1 = a % m;
  std::uint64_t t0 = 0;
  std::uint64_t t1 = 1;
  while (r1 != 0u) {
    const std::uint64_t q = r0 / r1;
    const std::uint64_t r2 = r0 - q * r1;
    const std::uint64_t qt = mul_mod(q, t1, m);
    const std::uint64_t t2 = t0 >= qt ? t0 - qt : t0 + (m - qt);
    r0 = r1;
    r1 = r2;
    t0 = t1;
    t1 = t2;
  }
  if (r0 != 1u) return ModResult::NotInvertible;
  *out = t0;
  return ModResult::Ok;
}

// Trial division by the 12 primes 2 to 37, then Miller-Rabin to those same 12 bases. No 64-bit composite is a strong pseudoprime to all of
// them (the smallest composite that is has 24 digits; see docs/modarith.md), so the answer is exact for every n up to 2^64 - 1.
ModResult is_prime_u64(std::uint64_t n, bool* out) {
  if (out == nullptr) return ModResult::NullArgument;
  static const std::uint32_t kBases[12] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
  if (n < 2u) {
    *out = false;
    return ModResult::Ok;
  }
  for (std::uint32_t i = 0; i < 12u; ++i) {
    if (n == kBases[i]) {
      *out = true;
      return ModResult::Ok;
    }
    if (n % kBases[i] == 0u) {
      *out = false;
      return ModResult::Ok;
    }
  }
  if (n < 41u * 41u) {  // no prime factor below 41, and 41 * 41 is the smallest such composite
    *out = true;
    return ModResult::Ok;
  }
  std::uint64_t d = n - 1u;
  std::uint32_t s = 0;
  while ((d & 1u) == 0u) {
    d >>= 1;
    ++s;
  }
  for (std::uint32_t i = 0; i < 12u; ++i) {
    if (!strong_probable_prime(n, d, s, kBases[i])) {
      *out = false;
      return ModResult::Ok;
    }
  }
  *out = true;
  return ModResult::Ok;
}

}  // namespace lumora::modarith
