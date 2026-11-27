// Operations on ascending u32 arrays (binary search bounds, merge, in-place unique); see sortedops.h for the contract and docs/sortedops.md
// for the check order and bounds. The input arrays are assumed ascending (a precondition that is not checked). No floating point, no libm,
// no allocation, no recursion, no global state.
#include "sortedops.h"

#include <cstdint>

namespace lumora::sortedops {

namespace {

// Smallest index i in [0, n] with !(keys[i] < key) when strict is false (lower bound), or with keys[i] > key when strict is true (upper
// bound). keys[mid] is read only for mid < n.
std::uint32_t bound(const std::uint32_t* keys, std::uint32_t n, std::uint32_t key, bool strict) {
  std::uint32_t lo = 0;
  std::uint32_t hi = n;
  while (lo < hi) {
    const std::uint32_t mid = lo + (hi - lo) / 2u;
    const bool go_right = strict ? keys[mid] <= key : keys[mid] < key;
    if (go_right) {
      lo = mid + 1u;
    } else {
      hi = mid;
    }
  }
  return lo;
}

OpResult search(const std::uint32_t* keys, std::uint32_t n, std::uint32_t key, std::uint32_t* out_index, bool strict) {
  if ((keys == nullptr && n > 0u) || out_index == nullptr) return OpResult::NullArgument;
  if (n > kMaxCount) return OpResult::TooLarge;
  *out_index = bound(keys, n, key, strict);
  return OpResult::Ok;
}

}  // namespace

OpResult lower_bound_u32(const std::uint32_t* keys, std::uint32_t n, std::uint32_t key, std::uint32_t* out_index) {
  return search(keys, n, key, out_index, false);
}

OpResult upper_bound_u32(const std::uint32_t* keys, std::uint32_t n, std::uint32_t key, std::uint32_t* out_index) {
  return search(keys, n, key, out_index, true);
}

OpResult merge_sorted_u32(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst,
                          std::uint32_t dst_cap, std::uint32_t* out_len) {
  const std::uint64_t total = static_cast<std::uint64_t>(na) + static_cast<std::uint64_t>(nb);
  if ((a == nullptr && na > 0u) || (b == nullptr && nb > 0u) || (dst == nullptr && total > 0u) || out_len == nullptr) {
    return OpResult::NullArgument;
  }
  if (na > kMaxCount || nb > kMaxCount) return OpResult::TooLarge;
  if (static_cast<std::uint64_t>(dst_cap) < total) return OpResult::BufferTooSmall;
  std::uint32_t i = 0;
  std::uint32_t j = 0;
  std::uint32_t k = 0;
  while (i < na && j < nb) {
    if (b[j] < a[i]) {
      dst[k] = b[j];
      ++j;
    } else {
      dst[k] = a[i];
      ++i;
    }
    ++k;
  }
  while (i < na) {
    dst[k] = a[i];
    ++i;
    ++k;
  }
  while (j < nb) {
    dst[k] = b[j];
    ++j;
    ++k;
  }
  *out_len = k;
  return OpResult::Ok;
}

OpResult unique_sorted_u32(std::uint32_t* keys, std::uint32_t n, std::uint32_t* out_len) {
  if ((keys == nullptr && n > 0u) || out_len == nullptr) return OpResult::NullArgument;
  if (n > kMaxCount) return OpResult::TooLarge;
  if (n == 0u) {
    *out_len = 0u;
    return OpResult::Ok;
  }
  std::uint32_t w = 1;  // keys[0 .. w) holds the distinct keys so far
  for (std::uint32_t r = 1; r < n; ++r) {
    if (keys[r] != keys[w - 1u]) {
      keys[w] = keys[r];
      ++w;
    }
  }
  *out_len = w;
  return OpResult::Ok;
}

}  // namespace lumora::sortedops
