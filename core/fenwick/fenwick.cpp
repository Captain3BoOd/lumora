// Fenwick tree. See fenwick.h for the contract and docs/fenwick.md for the layout and the bounds. Integer only, no heap, no recursion.
#include "fenwick.h"

namespace lumora::fenwick {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

bool valid(const void* tree, u32 n) { return n <= kMaxSize && (n == 0 || tree != nullptr); }

// two's complement wrap in both directions, without relying on implementation-defined conversions
i64 to_signed(u64 v) {
  if (v <= static_cast<u64>(INT64_MAX)) return static_cast<i64>(v);
  return -static_cast<i64>(~v) - 1;
}

u64 to_unsigned(i64 v) { return v >= 0 ? static_cast<u64>(v) : ~static_cast<u64>(-(v + 1)); }

// sum of the first count elements as an unsigned wrapped value
u64 prefix(const i64* tree, u32 count) {
  u64 s = 0;
  for (u32 k = count; k > 0; k &= k - 1) s += to_unsigned(tree[k - 1]);
  return s;
}

}  // namespace

bool init(i64* tree, u32 n) {
  if (!valid(tree, n)) return false;
  for (u32 i = 0; i < n; ++i) tree[i] = 0;
  return true;
}

bool add(i64* tree, u32 n, u32 i, i64 delta) {
  if (!valid(tree, n) || i >= n) return false;
  const u64 d = to_unsigned(delta);
  for (u32 k = i + 1; k <= n; k += k & (~k + 1u)) tree[k - 1] = to_signed(to_unsigned(tree[k - 1]) + d);
  return true;
}

bool prefix_sum(const i64* tree, u32 n, u32 count, i64* out) {
  if (!valid(tree, n) || out == nullptr || count > n) return false;
  *out = to_signed(prefix(tree, count));
  return true;
}

bool range_sum(const i64* tree, u32 n, u32 begin, u32 end, i64* out) {
  if (!valid(tree, n) || out == nullptr || begin > end || end > n) return false;
  *out = to_signed(prefix(tree, end) - prefix(tree, begin));
  return true;
}

bool first_prefix_at_least(const i64* tree, u32 n, i64 target, u32* out) {
  if (!valid(tree, n) || out == nullptr) return false;
  if (target <= 0) {
    *out = 0;
    return true;
  }
  u32 step = 1;
  while (step <= n / 2 && n >= 2) step <<= 1;  // largest power of two not above n (1 when n is 0 or 1)
  u32 pos = 0;
  i64 rem = target;
  for (; step > 0; step >>= 1) {
    if (n - pos >= step && tree[pos + step - 1] < rem) {
      pos += step;
      rem -= tree[pos - 1];
    }
  }
  *out = pos + 1;  // pos == n gives n + 1: no prefix reaches the target
  return true;
}

}  // namespace lumora::fenwick
