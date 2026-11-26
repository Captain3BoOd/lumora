// Ordered 8x8 Bayer dither and 16-bit to 8-bit quantization. See dither.h for the contract and docs/dither.md for the
// check order and the bounds. Integer only: no libm, no 128-bit types, no allocation.
#include "dither.h"

#include <cstdint>

namespace lumora::dither {

std::uint8_t threshold(std::uint32_t x, std::uint32_t y) {
  const std::uint32_t d = (x ^ y) & 7u;
  const std::uint32_t yy = y & 7u;
  const std::uint32_t index = ((d & 1u) << 5) | ((yy & 1u) << 4) | (((d >> 1) & 1u) << 3) | (((yy >> 1) & 1u) << 2) |
                              (((d >> 2) & 1u) << 1) | ((yy >> 2) & 1u);
  return static_cast<std::uint8_t>(index);
}

std::uint8_t quantize_u16(std::uint16_t v, std::uint32_t x, std::uint32_t y) {
  const std::uint32_t scaled = static_cast<std::uint32_t>(v) * 255u;
  const std::uint32_t q = scaled / 65535u;
  const std::uint32_t rem = scaled % 65535u;
  const std::uint32_t t = threshold(x, y);
  return static_cast<std::uint8_t>(rem * 128u >= (2u * t + 1u) * 65535u ? q + 1u : q);
}

bool dither_row_u16(const std::uint16_t* src, std::uint8_t* dst, std::uint32_t count, std::uint32_t x0, std::uint32_t y) {
  if (count == 0) return true;
  if (count > kMaxRow) return false;
  if (src == nullptr || dst == nullptr) return false;
  const std::uintptr_t s = reinterpret_cast<std::uintptr_t>(src);
  const std::uintptr_t d = reinterpret_cast<std::uintptr_t>(dst);
  const std::uintptr_t s_end = s + 2u * static_cast<std::uintptr_t>(count);
  const std::uintptr_t d_end = d + static_cast<std::uintptr_t>(count);
  if (s < d_end && d < s_end) return false;
  for (std::uint32_t i = 0; i < count; ++i) dst[i] = quantize_u16(src[i], x0 + i, y);
  return true;
}

}  // namespace lumora::dither
