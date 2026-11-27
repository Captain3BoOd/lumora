// CRC-32 (reflected, polynomial 0xEDB88320) and Adler-32 over byte ranges. See checksum.h for the contract and docs/checksum.md for the
// check order and the bounds. The CRC uses a 256-entry constant table built at compile time (no mutable state). Adler-32 defers the
// modulo: at most kBlock bytes are summed between reductions, which cannot overflow 32 bits when both sums start below 65521.
// No floating point, no libm, no allocation.
#include "checksum.h"

#include <cstdint>

namespace lumora::checksum {

namespace {

constexpr std::uint32_t kPoly = 0xEDB88320u;
constexpr std::uint32_t kBase = 65521u;
// largest n with 255 * n * (n + 1) / 2 + (n + 1) * (kBase - 1) <= 2^32 - 1 (the zlib bound)
constexpr std::uint32_t kBlock = 5552u;

struct Table {
  std::uint32_t v[256];
};

constexpr Table make_table() {
  Table t{};
  for (std::uint32_t i = 0; i < 256u; ++i) {
    std::uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c & 1u) != 0u ? (c >> 1) ^ kPoly : c >> 1;
    t.v[i] = c;
  }
  return t;
}

constexpr Table kTable = make_table();

}  // namespace

std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* data, std::uint32_t n) {
  if (data == nullptr || n == 0u) return crc;
  std::uint32_t c = crc ^ 0xFFFFFFFFu;
  for (std::uint32_t i = 0; i < n; ++i) c = kTable.v[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}

std::uint32_t crc32(const std::uint8_t* data, std::uint32_t n) { return crc32_update(0u, data, n); }

std::uint32_t adler32_update(std::uint32_t adler, const std::uint8_t* data, std::uint32_t n) {
  std::uint32_t s1 = (adler & 0xFFFFu) % kBase;
  std::uint32_t s2 = (adler >> 16) % kBase;
  if (data != nullptr) {
    std::uint32_t left = n;
    std::uint32_t pos = 0;
    while (left > 0u) {
      const std::uint32_t take = left < kBlock ? left : kBlock;
      for (std::uint32_t i = 0; i < take; ++i) {
        s1 += data[pos + i];
        s2 += s1;
      }
      s1 %= kBase;
      s2 %= kBase;
      pos += take;
      left -= take;
    }
  }
  return (s2 << 16) | s1;
}

std::uint32_t adler32(const std::uint8_t* data, std::uint32_t n) { return adler32_update(1u, data, n); }

}  // namespace lumora::checksum
