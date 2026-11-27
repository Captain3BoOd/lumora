// Coin tables (contract in coins.h). Method and step counts: docs/coins.md.
#include "coins.h"

namespace lumora::coins {

namespace {

// Check order of the header: a null out (always) or a null coins (only while m > 0), then m, then n, then every coin (all m are scanned).
CoinsResult check(const std::uint32_t* coins, std::uint32_t m, std::uint32_t n, const std::uint32_t* out) {
  if (out == nullptr || (m > 0 && coins == nullptr)) return CoinsResult::NullArgument;
  if (m > kMaxCoins) return CoinsResult::TooManyCoins;
  if (n > kMaxAmount) return CoinsResult::TooLarge;
  for (std::uint32_t i = 0; i < m; ++i)
    if (coins[i] == 0) return CoinsResult::BadCoin;
  return CoinsResult::Ok;
}

// Sum of two residues below kMod: the sum is below 2 * kMod < 2^32, so it fits a u32, and one subtraction brings it back below kMod.
std::uint32_t add_mod(std::uint32_t x, std::uint32_t y) {
  const std::uint32_t s = x + y;
  return s >= kMod ? s - kMod : s;
}

}  // namespace

CoinsResult coins_min_count(const std::uint32_t* coins, std::uint32_t m, std::uint32_t n, std::uint32_t* out) {
  const CoinsResult r = check(coins, m, n, out);
  if (r != CoinsResult::Ok) return r;
  out[0] = 0;
  for (std::uint32_t v = 1; v <= n; ++v) out[v] = kUnreachable;
  // One ascending pass per coin: out[v] may use this coin again through out[v - c], which was already improved in this pass (unlimited coins).
  // A stored count is at most n <= 2^20 (every coin is at least 1), so the +1 cannot reach kUnreachable; an unreachable entry is never extended.
  for (std::uint32_t i = 0; i < m; ++i) {
    const std::uint32_t c = coins[i];
    for (std::uint32_t v = c; v <= n; ++v) {
      const std::uint32_t prev = out[v - c];
      if (prev != kUnreachable && prev + 1u < out[v]) out[v] = prev + 1u;
    }
  }
  return CoinsResult::Ok;
}

CoinsResult coins_ways(const std::uint32_t* coins, std::uint32_t m, std::uint32_t n, std::uint32_t* out) {
  const CoinsResult r = check(coins, m, n, out);
  if (r != CoinsResult::Ok) return r;
  out[0] = 1u;
  for (std::uint32_t v = 1; v <= n; ++v) out[v] = 0u;
  // Ascending pass per coin kind: out[v] gains the ways that end with this kind, out[v - c], already including this kind (any number of uses).
  for (std::uint32_t i = 0; i < m; ++i) {
    const std::uint32_t c = coins[i];
    for (std::uint32_t v = c; v <= n; ++v) out[v] = add_mod(out[v], out[v - c]);
  }
  return CoinsResult::Ok;
}

CoinsResult coins_subset_ways(const std::uint32_t* coins, std::uint32_t m, std::uint32_t n, std::uint32_t* out) {
  const CoinsResult r = check(coins, m, n, out);
  if (r != CoinsResult::Ok) return r;
  out[0] = 1u;
  for (std::uint32_t v = 1; v <= n; ++v) out[v] = 0u;
  // Descending pass per list position: out[v - c] is still the value from before this coin, so the coin is used at most once.
  for (std::uint32_t i = 0; i < m; ++i) {
    const std::uint32_t c = coins[i];
    for (std::uint32_t v = n; v >= c; --v) out[v] = add_mod(out[v], out[v - c]);
  }
  return CoinsResult::Ok;
}

}  // namespace lumora::coins
