// Smallest prime factor, Euler's totient and Moebius tables over 0 .. limit. See sieve.h for the contract and docs/sieve.md for the method, the
// check order and the bounds.
//
// Method. Eratosthenes-style sieves that use only the caller's array (a linear sieve would need a stored prime list, which is scratch). Every
// loop is bounded by limit alone. All loop counters stay below 2^21, so no 32-bit value wraps. Temporary values are written only inside
// out[0 .. limit], and only after the checks passed.
#include "sieve.h"

namespace lumora::sieve {

namespace {

// mu working flags, held in the output bytes until the final pass: a prime has been seen to divide the entry (composite mark), an odd number of
// prime factors so far, a square factor.
constexpr std::uint8_t kFlagComposite = 1;
constexpr std::uint8_t kFlagOdd = 2;
constexpr std::uint8_t kFlagSquare = 4;

}  // namespace

SieveResult spf_table(std::uint32_t limit, std::uint32_t* out) {
  if (out == nullptr) return SieveResult::NullArgument;
  if (limit > kMaxLimit) return SieveResult::TooLarge;
  for (std::uint32_t n = 0; n <= limit; ++n) out[n] = n;
  // A prime p up to sqrt(limit) (p <= limit / p, so p * p <= limit) claims its multiples from p * p that nothing smaller has claimed.
  for (std::uint32_t p = 2; p <= limit / p; ++p) {
    if (out[p] != p) continue;  // p is composite: a smaller prime claimed it
    for (std::uint32_t m = p * p; m <= limit; m += p) {
      if (out[m] == m) out[m] = p;
    }
  }
  return SieveResult::Ok;
}

SieveResult phi_table(std::uint32_t limit, std::uint32_t* out) {
  if (out == nullptr) return SieveResult::NullArgument;
  if (limit > kMaxLimit) return SieveResult::TooLarge;
  for (std::uint32_t n = 0; n <= limit; ++n) out[n] = n;
  // out[m] / p is exact: out[m] is m times (q - 1) / q over the primes q already processed, and p still divides what is left of m.
  for (std::uint32_t p = 2; p <= limit; ++p) {
    if (out[p] != p) continue;  // composite: its smallest prime already lowered it
    for (std::uint32_t m = p; m <= limit; m += p) out[m] -= out[m] / p;
  }
  return SieveResult::Ok;
}

SieveResult mu_table(std::uint32_t limit, std::uint8_t* out) {
  if (out == nullptr) return SieveResult::NullArgument;
  if (limit > kMaxLimit) return SieveResult::TooLarge;
  for (std::uint32_t n = 0; n <= limit; ++n) out[n] = 0;
  for (std::uint32_t p = 2; p <= limit; ++p) {
    if ((out[p] & kFlagComposite) != 0) continue;  // composite: a smaller prime marked it
    for (std::uint32_t m = p; m <= limit; m += p) {
      out[m] = static_cast<std::uint8_t>(out[m] ^ kFlagOdd);
      if (m != p) out[m] = static_cast<std::uint8_t>(out[m] | kFlagComposite);
    }
    if (p <= limit / p) {
      const std::uint32_t sq = p * p;
      for (std::uint32_t m = sq; m <= limit; m += sq) out[m] = static_cast<std::uint8_t>(out[m] | kFlagSquare);
    }
  }
  // Final pass: every entry from 2 on becomes exactly 0, 1 or 255; out[0] is 0 already and out[1] is set here.
  if (limit >= 1) out[1] = 1;
  for (std::uint32_t n = 2; n <= limit; ++n) {
    const std::uint8_t v = out[n];
    if ((v & kFlagSquare) != 0) out[n] = 0;
    else if ((v & kFlagOdd) != 0) out[n] = 255;
    else out[n] = 1;
  }
  return SieveResult::Ok;
}

}  // namespace lumora::sieve
