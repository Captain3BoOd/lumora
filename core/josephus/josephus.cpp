// Josephus problem (contract in josephus.h; derivation, bounds and step counts in docs/josephus.md).
//
// Label the people of a circle of size m by their distance from the person where the count starts: 0 .. m - 1. The k-th person counted is the one
// with label (k - 1) mod m. After that removal the count restarts at the NEXT person, whose label (in the circle of size m) is k mod m; in the
// circle of size m - 1 that person is relabelled 0, so a label idx of the smaller circle is the label (idx + k) mod m of the larger one.
// Survivor: in a circle of size 1 it is 0, and J(m) = (J(m - 1) + k) mod m, for m = 2 .. n.
// Removed at step s (0-based): at that step the circle has m0 = n - s people, the removed one has label (k - 1) mod m0, and lifting that label through the
// sizes m0 + 1 .. n with the same map gives the number in the full circle (the labels of the full circle are the person numbers).
// One modulo per size, no table, no recursion. idx < m <= 2^24 and k < 2^32, so idx + k < 2^33: the sum is a u64 (k is not reduced first).
#include "josephus.h"

namespace lumora::josephus {
namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

// The label in the circle of size m of the person with label idx in the circle of size m - 1.
inline u32 lift(u32 idx, u32 k, u32 m) { return static_cast<u32>((static_cast<u64>(idx) + k) % m); }

}  // namespace

JosResult josephus_survivor(std::uint32_t n, std::uint32_t k, std::uint32_t* out) {
  if (out == nullptr) return JosResult::NullArgument;
  if (n == 0 || k == 0) return JosResult::BadArgument;
  if (n > kMaxN) return JosResult::TooLarge;
  u32 idx = 0;
  for (u32 m = 2; m <= n; ++m) idx = lift(idx, k, m);
  *out = idx;
  return JosResult::Ok;
}

JosResult josephus_removed(std::uint32_t n, std::uint32_t k, std::uint32_t step, std::uint32_t* out) {
  if (out == nullptr) return JosResult::NullArgument;
  if (n == 0 || k == 0) return JosResult::BadArgument;
  if (n > kMaxN) return JosResult::TooLarge;
  if (step >= n) return JosResult::OutOfRange;
  const u32 m0 = n - step;
  u32 idx = (k - 1u) % m0;
  for (u32 m = m0 + 1u; m <= n; ++m) idx = lift(idx, k, m);
  *out = idx;
  return JosResult::Ok;
}

}  // namespace lumora::josephus
