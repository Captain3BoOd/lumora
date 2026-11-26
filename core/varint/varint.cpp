// LEB128 variable-length integers and zigzag mapping. See varint.h for the contract and docs/varint.md for the bounds and the
// failure rules. Integer only: no floating point, no libm, no 128-bit types, no allocation.
#include "varint.h"

#include <cstdint>

namespace lumora::varint {

std::uint32_t encoded_size(std::uint64_t v) {
  std::uint32_t n = 1;
  while (v >= 128u) {
    v >>= 7;
    ++n;
  }
  return n;
}

bool encode(std::uint64_t v, std::uint8_t* out, std::uint32_t capacity, std::uint32_t* written) {
  if (out == nullptr || written == nullptr) return false;
  const std::uint32_t size = encoded_size(v);
  if (capacity < size) return false;
  for (std::uint32_t i = 0; i < size; ++i) {
    const std::uint8_t group = static_cast<std::uint8_t>(v & 0x7Fu);
    v >>= 7;
    out[i] = (i + 1 < size) ? static_cast<std::uint8_t>(group | 0x80u) : group;
  }
  *written = size;
  return true;
}

bool decode(const std::uint8_t* in, std::uint32_t length, std::uint64_t* value, std::uint32_t* consumed) {
  if (in == nullptr || value == nullptr || consumed == nullptr) return false;
  if (length == 0) return false;
  const std::uint32_t limit = length < kMaxBytes ? length : kMaxBytes;
  std::uint64_t result = 0;
  for (std::uint32_t i = 0; i < limit; ++i) {
    const std::uint8_t b = in[i];
    if (i == kMaxBytes - 1 && b > 1u) return false;
    result |= static_cast<std::uint64_t>(b & 0x7Fu) << (7 * i);
    if ((b & 0x80u) == 0) {
      if (i > 0 && b == 0) return false;
      *value = result;
      *consumed = i + 1;
      return true;
    }
  }
  return false;
}

std::uint64_t zigzag_encode(std::int64_t v) {
  if (v >= 0) return static_cast<std::uint64_t>(v) * 2u;
  return static_cast<std::uint64_t>(-(v + 1)) * 2u + 1u;
}

std::int64_t zigzag_decode(std::uint64_t u) {
  const std::uint64_t half = u / 2u;
  if ((u & 1u) == 0) return static_cast<std::int64_t>(half);
  return -static_cast<std::int64_t>(half) - 1;
}

bool encode_signed(std::int64_t v, std::uint8_t* out, std::uint32_t capacity, std::uint32_t* written) {
  return encode(zigzag_encode(v), out, capacity, written);
}

bool decode_signed(const std::uint8_t* in, std::uint32_t length, std::int64_t* value, std::uint32_t* consumed) {
  if (value == nullptr || consumed == nullptr) return false;
  std::uint64_t raw = 0;
  std::uint32_t used = 0;
  if (!decode(in, length, &raw, &used)) return false;
  *value = zigzag_decode(raw);
  *consumed = used;
  return true;
}

}  // namespace lumora::varint
