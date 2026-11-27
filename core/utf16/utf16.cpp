// Strict UTF-16 (host-order units) validation, code point count and single code point decode and encode. See utf16.h for the contract
// and docs/utf16.md for the check order and the bounds. A unit outside D800-DFFF is one code point; a high surrogate must be followed by a
// low surrogate (the second unit is read only when n holds it); a low surrogate cannot start a sequence. No floating point, no libm,
// no allocation, no global state.
#include "utf16.h"

#include <cstdint>

namespace lumora::utf16 {

namespace {

bool is_high(std::uint32_t u) { return u >= 0xD800u && u <= 0xDBFFu; }
bool is_low(std::uint32_t u) { return u >= 0xDC00u && u <= 0xDFFFu; }

// Length (1 or 2) of the well-formed sequence at s[0], or 0 when it is ill-formed or cut off. n is at least 1. At most min(n, 2) units
// are read, and s[1] only for a high surrogate when n holds it.
std::uint32_t seq_len(const std::uint16_t* s, std::uint32_t n) {
  const std::uint32_t u = s[0];
  if (is_low(u)) return 0u;
  if (!is_high(u)) return 1u;
  if (n < 2u) return 0u;
  return is_low(s[1]) ? 2u : 0u;
}

}  // namespace

Utf16Result validate(const std::uint16_t* s, std::uint32_t n, std::uint32_t* bad_offset) {
  if (s == nullptr && n > 0u) return Utf16Result::NullArgument;
  if (n > kMaxUnits) return Utf16Result::TooLarge;
  std::uint32_t i = 0;
  while (i < n) {
    const std::uint32_t len = seq_len(s + i, n - i);
    if (len == 0u) {
      if (bad_offset != nullptr) *bad_offset = i;
      return Utf16Result::Malformed;
    }
    i += len;
  }
  if (bad_offset != nullptr) *bad_offset = n;
  return Utf16Result::Ok;
}

Utf16Result count_code_points(const std::uint16_t* s, std::uint32_t n, std::uint32_t* out) {
  if ((s == nullptr && n > 0u) || out == nullptr) return Utf16Result::NullArgument;
  if (n > kMaxUnits) return Utf16Result::TooLarge;
  std::uint32_t i = 0;
  std::uint32_t count = 0;
  while (i < n) {
    const std::uint32_t len = seq_len(s + i, n - i);
    if (len == 0u) return Utf16Result::Malformed;
    i += len;
    ++count;
  }
  *out = count;
  return Utf16Result::Ok;
}

Utf16Result decode_one(const std::uint16_t* s, std::uint32_t n, std::uint32_t* cp, std::uint32_t* len) {
  if (s == nullptr || cp == nullptr || len == nullptr) return Utf16Result::NullArgument;
  if (n == 0u) return Utf16Result::Malformed;
  const std::uint32_t l = seq_len(s, n);
  if (l == 0u) return Utf16Result::Malformed;
  if (l == 1u) {
    *cp = s[0];
  } else {
    *cp = 0x10000u + ((static_cast<std::uint32_t>(s[0]) - 0xD800u) << 10) + (static_cast<std::uint32_t>(s[1]) - 0xDC00u);
  }
  *len = l;
  return Utf16Result::Ok;
}

Utf16Result encode_one(std::uint32_t cp, std::uint16_t* dst, std::uint32_t dst_cap, std::uint32_t* len) {
  if ((dst == nullptr && dst_cap > 0u) || len == nullptr) return Utf16Result::NullArgument;
  if (cp > kMaxCodePoint || (cp >= 0xD800u && cp <= 0xDFFFu)) return Utf16Result::Malformed;
  const std::uint32_t need = cp <= 0xFFFFu ? 1u : 2u;
  if (need > dst_cap) return Utf16Result::BufferTooSmall;
  if (need == 1u) {
    dst[0] = static_cast<std::uint16_t>(cp);
  } else {
    const std::uint32_t v = cp - 0x10000u;
    dst[0] = static_cast<std::uint16_t>(0xD800u + (v >> 10));
    dst[1] = static_cast<std::uint16_t>(0xDC00u + (v & 0x3FFu));
  }
  *len = need;
  return Utf16Result::Ok;
}

}  // namespace lumora::utf16
