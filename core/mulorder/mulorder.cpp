// Multiplicative order, Carmichael function and least primitive root for every u32 modulus (contract in mulorder.h; method and proofs in docs/mulorder.md).
// Integer only: no 128-bit types, no builtins, no recursion, no arrays, no allocation. The modular multiply is a u64 product of two residues below m (at most
// (2^32 - 2)^2 < 2^64) reduced at once. Factoring is trial division by 2 and then by odd d with d * d <= r (d is a u64).
#include "mulorder.h"
namespace lumora::mulorder {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

u32 mulmod(u32 x, u32 y, u32 m) { return static_cast<u32>(static_cast<u64>(x) * y % m); }

// b^e modulo m for m >= 2 and b < m (binary powers, every intermediate below m).
u32 powmod(u32 b, u32 e, u32 m) {
  u32 r = 1;
  while (e != 0) {
    if ((e & 1u) != 0) r = mulmod(r, b, m);
    e >>= 1;
    if (e != 0) b = mulmod(b, b, m);
  }
  return r;
}

u32 gcd(u32 a, u32 b) {
  while (b != 0) {
    const u32 t = a % b;
    a = b;
    b = t;
  }
  return a;
}

// lcm(l, x) = l / gcd(l, x) * x in u64: every partial lcm below divides lambda(m) < 2^32, so the result fits u32.
u32 lcm(u32 l, u32 x) { return static_cast<u32>(static_cast<u64>(l) / gcd(l, x) * x); }

struct Shape {
  u32 lambda;      // Carmichael function of m
  u32 phi;         // Euler totient of m
  unsigned twos;   // exponent of 2 in m
  unsigned odd;    // number of distinct odd primes of m
};

// One factoring pass over m (m >= 1): trial division, then the lambda and phi of each prime power.
Shape shape_of(u32 m) {
  Shape s{1, 1, 0, 0};
  u32 r = m;
  while ((r & 1u) == 0) {
    r >>= 1;
    ++s.twos;
  }
  if (s.twos != 0) {
    s.lambda = s.twos == 1 ? 1u : (s.twos == 2 ? 2u : (1u << (s.twos - 2)));
    s.phi = 1u << (s.twos - 1);
  }
  u32 d = 3;
  while (static_cast<u64>(d) * d <= r) {
    if (r % d == 0) {
      u32 below = 1;  // d^(e - 1)
      r /= d;
      while (r % d == 0) {
        r /= d;
        below *= d;
      }
      s.lambda = lcm(s.lambda, below * (d - 1));
      s.phi *= below * (d - 1);
      ++s.odd;
    }
    d += 2;
  }
  if (r > 1) {
    s.lambda = lcm(s.lambda, r - 1);
    s.phi *= r - 1;
    ++s.odd;
  }
  return s;
}

}  // namespace

MoResult carmichael(std::uint32_t m, std::uint32_t* out) {
  if (out == nullptr) return MoResult::NullArgument;
  if (m == 0) return MoResult::BadModulus;
  *out = shape_of(m).lambda;
  return MoResult::Ok;
}

MoResult mult_order(std::uint32_t a, std::uint32_t m, std::uint32_t* out) {
  if (out == nullptr) return MoResult::NullArgument;
  if (m == 0) return MoResult::BadModulus;
  const u32 x = a % m;
  if (gcd(x, m) != 1) return MoResult::NotCoprime;
  if (m == 1) {
    *out = 1;
    return MoResult::Ok;
  }
  u32 e = shape_of(m).lambda;  // the order divides lambda(m)
  u32 rest = e;                // what is left to factor
  u32 q = 2;
  // For every prime q of lambda: strip q from e while x^(e / q) = 1. At most Omega + omega <= 31 + 9 powers.
  while (static_cast<u64>(q) * q <= rest) {
    if (rest % q == 0) {
      while (rest % q == 0) rest /= q;
      while (e % q == 0 && powmod(x, e / q, m) == 1) e /= q;
    }
    q += (q == 2) ? 1u : 2u;
  }
  if (rest > 1) {
    while (e % rest == 0 && powmod(x, e / rest, m) == 1) e /= rest;
  }
  *out = e;
  return MoResult::Ok;
}

MoResult primitive_root(std::uint32_t m, std::uint32_t* out) {
  if (out == nullptr) return MoResult::NullArgument;
  if (m == 0) return MoResult::BadModulus;
  const Shape s = shape_of(m);
  // The moduli with a primitive root: 1, 2, 4, p^k, 2 p^k (p an odd prime).
  const bool has = s.odd == 0 ? s.twos <= 2 : (s.odd == 1 && s.twos <= 1);
  if (!has) return MoResult::NoPrimitiveRoot;
  if (m == 1) {
    *out = 0;
    return MoResult::Ok;
  }
  // The group is cyclic of order phi = lambda. The least g in 1..m-1 coprime to m with g^(phi / q) != 1 for every prime q of phi.
  for (u32 g = 1; g < m; ++g) {
    if (gcd(g, m) != 1) continue;
    bool root = true;
    u32 rest = s.phi;
    u32 q = 2;
    while (root && static_cast<u64>(q) * q <= rest) {
      if (rest % q == 0) {
        while (rest % q == 0) rest /= q;
        if (powmod(g, s.phi / q, m) == 1) root = false;
      }
      q += (q == 2) ? 1u : 2u;
    }
    if (root && rest > 1 && powmod(g, s.phi / rest, m) == 1) root = false;
    if (root) {
      *out = g;
      return MoResult::Ok;
    }
  }
  return MoResult::NoPrimitiveRoot;  // not reached: a cyclic group has a generator
}
}  // namespace lumora::mulorder
