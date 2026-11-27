// Decimal text for 64-bit integers; see decfmt.h for the contract and docs/decfmt.md for the check order and bounds. Integer only, includes
// <cstdint> only, no 128-bit types, no compiler builtins, no allocation, no recursion, no global state.
// Format: the digit count is found first, so a capacity that is too small is rejected before anything is written to dst. Parse: a first pass
// over the whole input decides the form (so Invalid wins over Overflow wherever the bad byte is), a second pass accumulates the magnitude in a
// u64 and tests for overflow before each multiply-add. Failures leave every out value untouched.
#include "decfmt.h"

#include <cstdint>

namespace lumora::decfmt {

namespace {

constexpr std::uint64_t kU64Max = 0xFFFFFFFFFFFFFFFFull;
constexpr std::uint64_t kI64MaxMag = 0x7FFFFFFFFFFFFFFFull;  // largest i64 value
constexpr std::uint64_t kI64MinMag = 0x8000000000000000ull;  // magnitude of the smallest i64 value

// Number of decimal digits of v (1 for 0).
std::uint32_t digit_count(std::uint64_t v) {
  std::uint32_t n = 1;
  while (v >= 10u) {
    v /= 10u;
    ++n;
  }
  return n;
}

// Writes the optional '-' and the digits of mag. Preconditions: dst holds the whole text; len is the text length.
void write_text(std::uint64_t mag, bool neg, char* dst, std::uint32_t len) {
  std::uint32_t i = len;
  do {
    --i;
    dst[i] = static_cast<char>('0' + static_cast<int>(mag % 10u));
    mag /= 10u;
  } while (mag != 0u);
  if (neg) dst[0] = '-';
}

DecResult format_mag(std::uint64_t mag, bool neg, char* dst, std::uint32_t dst_cap, std::uint32_t* out_len) {
  if ((dst == nullptr && dst_cap > 0u) || out_len == nullptr) return DecResult::NullArgument;
  const std::uint32_t len = digit_count(mag) + (neg ? 1u : 0u);
  if (dst_cap < len) return DecResult::BufferTooSmall;
  write_text(mag, neg, dst, len);
  *out_len = len;
  return DecResult::Ok;
}

// Shared parse: magnitude of the digits after an optional '-' (only when allow_sign). limit is the largest magnitude the caller accepts.
DecResult parse_mag(const char* src, std::uint32_t n, bool allow_sign, bool* neg, std::uint64_t* mag, std::uint64_t limit_pos,
                    std::uint64_t limit_neg) {
  if (n > kMaxParseLen) return DecResult::TooLarge;
  std::uint32_t first = 0;
  const bool is_neg = allow_sign && n > 0u && src[0] == '-';
  if (is_neg) first = 1u;
  if (first >= n) return DecResult::Invalid;  // empty, or a lone '-'
  for (std::uint32_t i = first; i < n; ++i) {
    const unsigned char c = static_cast<unsigned char>(src[i]);
    if (c < static_cast<unsigned char>('0') || c > static_cast<unsigned char>('9')) return DecResult::Invalid;
  }
  const std::uint64_t limit = is_neg ? limit_neg : limit_pos;
  std::uint64_t v = 0;
  for (std::uint32_t i = first; i < n; ++i) {
    const std::uint64_t d = static_cast<std::uint64_t>(static_cast<unsigned char>(src[i]) - static_cast<unsigned char>('0'));
    if (v > (limit - d) / 10u) return DecResult::Overflow;
    v = v * 10u + d;
  }
  *neg = is_neg;
  *mag = v;
  return DecResult::Ok;
}

}  // namespace

DecResult format_u64(std::uint64_t value, char* dst, std::uint32_t dst_cap, std::uint32_t* out_len) {
  return format_mag(value, false, dst, dst_cap, out_len);
}

DecResult format_i64(std::int64_t value, char* dst, std::uint32_t dst_cap, std::uint32_t* out_len) {
  const bool neg = value < 0;
  const std::uint64_t mag = neg ? 0u - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);  // no signed overflow
  return format_mag(mag, neg, dst, dst_cap, out_len);
}

DecResult parse_u64(const char* src, std::uint32_t n, std::uint64_t* out) {
  if ((src == nullptr && n > 0u) || out == nullptr) return DecResult::NullArgument;
  bool neg = false;
  std::uint64_t mag = 0;
  const DecResult r = parse_mag(src, n, false, &neg, &mag, kU64Max, kU64Max);
  if (r != DecResult::Ok) return r;
  *out = mag;
  return DecResult::Ok;
}

DecResult parse_i64(const char* src, std::uint32_t n, std::int64_t* out) {
  if ((src == nullptr && n > 0u) || out == nullptr) return DecResult::NullArgument;
  bool neg = false;
  std::uint64_t mag = 0;
  const DecResult r = parse_mag(src, n, true, &neg, &mag, kI64MaxMag, kI64MinMag);
  if (r != DecResult::Ok) return r;
  *out = static_cast<std::int64_t>(neg ? 0u - mag : mag);  // mag is at most 2^63 when negative, so the result is in range
  return DecResult::Ok;
}

}  // namespace lumora::decfmt
