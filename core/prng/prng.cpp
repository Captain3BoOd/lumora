// PCG32 (PCG-XSH-RR 64/32) with a capped, unbiased-when-accepted bounded draw and an O(64) jump-ahead. See prng.h for the contract and
// docs/prng.md for the check order and the bounds. Integer only: no floating point, no libm, no 128-bit types, no allocation.
#include "prng.h"

#include <cstdint>

namespace lumora::prng {

namespace {

constexpr std::uint64_t kMultiplier = 6364136223846793005ull;

// One step: the output is computed from the old state, then the state advances.
std::uint32_t step(Pcg32& rng) {
  const std::uint64_t old = rng.state;
  rng.state = old * kMultiplier + rng.inc;
  const std::uint32_t xorshifted = static_cast<std::uint32_t>(((old >> 18) ^ old) >> 27);
  const std::uint32_t rot = static_cast<std::uint32_t>(old >> 59);
  return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
}

}  // namespace

bool pcg32_seed(Pcg32* rng, std::uint64_t initstate, std::uint64_t initseq) {
  if (rng == nullptr) return false;
  rng->state = 0;
  rng->inc = (initseq << 1) | 1u;
  step(*rng);
  rng->state += initstate;
  step(*rng);
  return true;
}

std::uint32_t pcg32_next(Pcg32* rng) {
  if (rng == nullptr) return 0;
  return step(*rng);
}

std::uint32_t pcg32_bounded(Pcg32* rng, std::uint32_t bound) {
  if (rng == nullptr) return 0;
  if (bound == 0) return 0;
  const std::uint32_t threshold = (0u - bound) % bound;
  std::uint32_t r = 0;
  for (std::uint32_t i = 0; i < kMaxDraws; ++i) {
    r = step(*rng);
    if (r >= threshold) break;
  }
  return r % bound;
}

bool pcg32_advance(Pcg32* rng, std::uint64_t delta) {
  if (rng == nullptr) return false;
  std::uint64_t acc_mult = 1;
  std::uint64_t acc_plus = 0;
  std::uint64_t cur_mult = kMultiplier;
  std::uint64_t cur_plus = rng->inc;
  while (delta > 0) {
    if ((delta & 1u) != 0) {
      acc_mult *= cur_mult;
      acc_plus = acc_plus * cur_mult + cur_plus;
    }
    cur_plus = (cur_mult + 1u) * cur_plus;
    cur_mult *= cur_mult;
    delta >>= 1;
  }
  rng->state = acc_mult * rng->state + acc_plus;
  return true;
}

}  // namespace lumora::prng
