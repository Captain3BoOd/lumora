// Palindromes in a byte string: radii at every center, the longest palindromic substring and the number of palindromic substrings. See palin.h for
// the contract and docs/palin.md for the method, the check order and the bounds.
//
// Method: Manacher's algorithm, once for odd lengths and once for even lengths, writing the radii straight into the output arrays (for palin_radii
// the caller's odd and even; for palin_longest and palin_count the two halves of the caller's scratch). No allocation, no recursion, no large
// automatic arrays. Each pass keeps the rightmost palindrome found so far as the half-open range [lo, hi): a center inside it starts from the radius
// of its mirror (capped at the distance to hi); only the extension loop reads bytes, and every step of it moves hi forward, which never moves back.
#include "palin.h"

namespace lumora::palin {

namespace {

// odd[i] = number of odd palindromes centered at byte i; even[i] = number of even palindromes centered between bytes i - 1 and i.
void manacher(std::uint32_t n, const std::uint8_t* s, std::uint32_t* odd, std::uint32_t* even) {
  std::uint32_t lo = 0, hi = 0;  // the rightmost palindrome s[lo .. hi) found so far (odd pass)
  for (std::uint32_t i = 0; i < n; ++i) {
    std::uint32_t k = 1;
    if (i < hi) {
      const std::uint32_t mirror = odd[lo + hi - 1 - i];
      const std::uint32_t room = hi - i;
      k = mirror < room ? mirror : room;
    }
    while (k <= i && i + k < n && s[i + k] == s[i - k]) ++k;
    odd[i] = k;
    if (i + k > hi) {
      lo = i + 1 - k;
      hi = i + k;
    }
  }
  lo = 0;
  hi = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    std::uint32_t k = 0;
    if (i < hi) {
      const std::uint32_t mirror = even[lo + hi - i];
      const std::uint32_t room = hi - i;
      k = mirror < room ? mirror : room;
    }
    while (k < i && i + k < n && s[i + k] == s[i - k - 1]) ++k;
    even[i] = k;
    if (i + k > hi) {
      lo = i - k;
      hi = i + k;
    }
  }
}

}  // namespace

PalinResult palin_radii(std::uint32_t n, const std::uint8_t* s, std::uint32_t* odd, std::uint32_t* even) {
  if (n > 0 && (s == nullptr || odd == nullptr || even == nullptr)) return PalinResult::NullArgument;
  if (n > kMaxLen) return PalinResult::TooLarge;
  manacher(n, s, odd, even);
  return PalinResult::Ok;
}

PalinResult palin_scratch_words(std::uint32_t n, std::uint32_t* out_words) {
  if (out_words == nullptr) return PalinResult::NullArgument;
  if (n > kMaxLen) return PalinResult::TooLarge;
  *out_words = 2 * n;
  return PalinResult::Ok;
}

PalinResult palin_longest(std::uint32_t n, const std::uint8_t* s, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_start,
                          std::uint32_t* out_len) {
  if ((n > 0 && (s == nullptr || scratch == nullptr)) || out_start == nullptr || out_len == nullptr) return PalinResult::NullArgument;
  if (n > kMaxLen) return PalinResult::TooLarge;
  if (n > 0 && scratch_words < 2 * n) return PalinResult::ScratchTooSmall;
  std::uint32_t best_start = 0, best_len = 0;
  if (n > 0) {
    std::uint32_t* const odd = scratch;
    std::uint32_t* const even = scratch + n;
    manacher(n, s, odd, even);
    for (std::uint32_t i = 0; i < n; ++i) {
      // Scanning centers in increasing order, a candidate with the same length starts at a larger or equal index than an earlier one of that length
      // only when its start is not smaller; the explicit start comparison keeps the rule "smallest start" independent of the scan order.
      const std::uint32_t len_odd = 2 * odd[i] - 1, start_odd = i + 1 - odd[i];
      if (len_odd > best_len || (len_odd == best_len && start_odd < best_start)) {
        best_len = len_odd;
        best_start = start_odd;
      }
      const std::uint32_t len_even = 2 * even[i], start_even = i - even[i];
      if (len_even > best_len || (len_even == best_len && start_even < best_start)) {
        best_len = len_even;
        best_start = start_even;
      }
    }
  }
  *out_start = best_start;
  *out_len = best_len;
  return PalinResult::Ok;
}

PalinResult palin_count(std::uint32_t n, const std::uint8_t* s, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint64_t* out_count) {
  if ((n > 0 && (s == nullptr || scratch == nullptr)) || out_count == nullptr) return PalinResult::NullArgument;
  if (n > kMaxLen) return PalinResult::TooLarge;
  if (n > 0 && scratch_words < 2 * n) return PalinResult::ScratchTooSmall;
  std::uint64_t total = 0;
  if (n > 0) {
    std::uint32_t* const odd = scratch;
    std::uint32_t* const even = scratch + n;
    manacher(n, s, odd, even);
    for (std::uint32_t i = 0; i < n; ++i) total += static_cast<std::uint64_t>(odd[i]) + even[i];
  }
  *out_count = total;
  return PalinResult::Ok;
}

}  // namespace lumora::palin
