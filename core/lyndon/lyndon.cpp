// Lyndon factorization and least rotation of a byte string. See lyndon.h for the contract and docs/lyndon.md for the method, the check order and the
// bounds.
//
// Method: Duval's algorithm. The factors are found left to right with three indices (the start i of the round, the end j of the scan and the
// comparison index k); no memory beyond the caller's lens array, no allocation, no recursion. The least rotation runs the same scan over the string
// followed by itself (indices wrap by one subtraction, so no copy is made) and keeps the start of the last round that begins inside the string.
#include "lyndon.h"

namespace lumora::lyndon {

namespace {

// The view of the string followed by itself: s[x] for x < n and s[x - n] for x >= n (x < 2 * n).
inline std::uint8_t at(const std::uint8_t* s, std::uint32_t n, std::uint32_t x) { return s[x >= n ? x - n : x]; }

}  // namespace

LyndonResult lyndon_factor(std::uint32_t n, const std::uint8_t* s, std::uint32_t* lens, std::uint32_t* out_count) {
  if ((n > 0 && (s == nullptr || lens == nullptr)) || out_count == nullptr) return LyndonResult::NullArgument;
  if (n > kMaxLen) return LyndonResult::TooLarge;
  std::uint32_t count = 0;
  std::uint32_t i = 0;
  while (i < n) {
    std::uint32_t j = i + 1, k = i;
    while (j < n && s[k] <= s[j]) {
      k = s[k] < s[j] ? i : k + 1;
      ++j;
    }
    const std::uint32_t len = j - k;
    while (i <= k) {
      lens[count++] = len;
      i += len;
    }
  }
  *out_count = count;
  return LyndonResult::Ok;
}

LyndonResult lyndon_min_rotation(std::uint32_t n, const std::uint8_t* s, std::uint32_t* out_start) {
  if ((n > 0 && s == nullptr) || out_start == nullptr) return LyndonResult::NullArgument;
  if (n > kMaxLen) return LyndonResult::TooLarge;
  std::uint32_t ans = 0;
  std::uint32_t i = 0;
  while (i < n) {
    ans = i;
    std::uint32_t j = i + 1, k = i;
    while (j < 2 * n && at(s, n, k) <= at(s, n, j)) {
      k = at(s, n, k) < at(s, n, j) ? i : k + 1;
      ++j;
    }
    const std::uint32_t len = j - k;
    while (i <= k) i += len;
  }
  *out_start = ans;
  return LyndonResult::Ok;
}

}  // namespace lumora::lyndon
