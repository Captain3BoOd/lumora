// Strict UTF-8 validation and single code point decode and encode. See utf8.h for the contract and docs/utf8.md for the check order and
// the bounds. A sequence is checked against the Unicode table of well-formed sequences: the second byte has its own range for the
// leads E0, ED, F0 and F4, every other continuation byte is 80-BF. A sequence cut off by the end of the input is ill-formed without
// reading past n. No floating point, no libm, no allocation, no global state.
#include "utf8.h"

#include <cstdint>

namespace lumora::utf8 {

namespace {

// Length (1 to 4) of the well-formed sequence that starts at s[0], or 0 when it is ill-formed or cut off. At most min(n, 4) bytes are
// read, and s[1] and later bytes only when n holds the whole sequence. n is at least 1.
std::uint32_t seq_len(const std::uint8_t* s, std::uint32_t n) {
  const std::uint32_t b = s[0];
  if (b < 0x80u) return 1u;
  std::uint32_t need = 0;
  std::uint32_t lo = 0x80u;
  std::uint32_t hi = 0xBFu;
  if (b >= 0xC2u && b <= 0xDFu) {
    need = 2u;
  } else if (b >= 0xE0u && b <= 0xEFu) {
    need = 3u;
    if (b == 0xE0u) lo = 0xA0u;
    if (b == 0xEDu) hi = 0x9Fu;
  } else if (b >= 0xF0u && b <= 0xF4u) {
    need = 4u;
    if (b == 0xF0u) lo = 0x90u;
    if (b == 0xF4u) hi = 0x8Fu;
  } else {
    return 0u;
  }
  if (n < need) return 0u;
  const std::uint32_t b1 = s[1];
  if (b1 < lo || b1 > hi) return 0u;
  for (std::uint32_t k = 2; k < need; ++k) {
    if ((static_cast<std::uint32_t>(s[k]) & 0xC0u) != 0x80u) return 0u;
  }
  return need;
}

std::uint32_t cont(std::uint8_t b) { return static_cast<std::uint32_t>(b) & 0x3Fu; }

}  // namespace

Utf8Result validate(const std::uint8_t* s, std::uint32_t n, std::uint32_t* bad_offset) {
  if (s == nullptr && n > 0u) return Utf8Result::NullArgument;
  if (n > kMaxInput) return Utf8Result::TooLarge;
  std::uint32_t i = 0;
  while (i < n) {
    const std::uint32_t len = seq_len(s + i, n - i);
    if (len == 0u) {
      if (bad_offset != nullptr) *bad_offset = i;
      return Utf8Result::Malformed;
    }
    i += len;
  }
  if (bad_offset != nullptr) *bad_offset = n;
  return Utf8Result::Ok;
}

Utf8Result decode_one(const std::uint8_t* s, std::uint32_t n, std::uint32_t* cp, std::uint32_t* len) {
  if (s == nullptr || cp == nullptr || len == nullptr) return Utf8Result::NullArgument;
  if (n == 0u) return Utf8Result::Malformed;
  const std::uint32_t l = seq_len(s, n);
  if (l == 0u) return Utf8Result::Malformed;
  std::uint32_t v = 0;
  if (l == 1u) {
    v = s[0];
  } else if (l == 2u) {
    v = ((static_cast<std::uint32_t>(s[0]) & 0x1Fu) << 6) | cont(s[1]);
  } else if (l == 3u) {
    v = ((static_cast<std::uint32_t>(s[0]) & 0x0Fu) << 12) | (cont(s[1]) << 6) | cont(s[2]);
  } else {
    v = ((static_cast<std::uint32_t>(s[0]) & 0x07u) << 18) | (cont(s[1]) << 12) | (cont(s[2]) << 6) | cont(s[3]);
  }
  *cp = v;
  *len = l;
  return Utf8Result::Ok;
}

Utf8Result encode_one(std::uint32_t cp, std::uint8_t* dst, std::uint32_t dst_cap, std::uint32_t* len) {
  if ((dst == nullptr && dst_cap > 0u) || len == nullptr) return Utf8Result::NullArgument;
  if (cp > kMaxCodePoint || (cp >= 0xD800u && cp <= 0xDFFFu)) return Utf8Result::Malformed;
  const std::uint32_t need = cp <= 0x7Fu ? 1u : cp <= 0x7FFu ? 2u : cp <= 0xFFFFu ? 3u : 4u;
  if (need > dst_cap) return Utf8Result::BufferTooSmall;
  if (need == 1u) {
    dst[0] = static_cast<std::uint8_t>(cp);
  } else if (need == 2u) {
    dst[0] = static_cast<std::uint8_t>(0xC0u | (cp >> 6));
    dst[1] = static_cast<std::uint8_t>(0x80u | (cp & 0x3Fu));
  } else if (need == 3u) {
    dst[0] = static_cast<std::uint8_t>(0xE0u | (cp >> 12));
    dst[1] = static_cast<std::uint8_t>(0x80u | ((cp >> 6) & 0x3Fu));
    dst[2] = static_cast<std::uint8_t>(0x80u | (cp & 0x3Fu));
  } else {
    dst[0] = static_cast<std::uint8_t>(0xF0u | (cp >> 18));
    dst[1] = static_cast<std::uint8_t>(0x80u | ((cp >> 12) & 0x3Fu));
    dst[2] = static_cast<std::uint8_t>(0x80u | ((cp >> 6) & 0x3Fu));
    dst[3] = static_cast<std::uint8_t>(0x80u | (cp & 0x3Fu));
  }
  *len = need;
  return Utf8Result::Ok;
}

}  // namespace lumora::utf8
