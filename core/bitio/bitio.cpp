// LSB-first bit fields over a caller buffer. See bitio.h for the contract and docs/bitio.md for the check order and the bounds.
// A field is moved one byte at a time: only the bytes that overlap the field are read or written, and the buffer is never accessed past
// byte cap - 1. Positions are at most 8 * kMaxBytes = 2^31 once validated, so the 64-bit end sum never overflows and the rounding in
// align_to_byte stays below 2^32. No floating point, no libm, no allocation.
#include "bitio.h"

#include <cstdint>

namespace lumora::bitio {

namespace {

constexpr std::uint32_t kMaxFieldBits = 32;

// cap and position are valid when cap <= kMaxBytes and pos <= 8 * cap (8 * cap fits in 32 bits then)
bool valid_span(std::uint32_t cap, std::uint32_t pos) { return cap <= kMaxBytes && pos <= 8u * cap; }

// true when a field of nbits at pos fits in cap bytes; the sum is taken in 64 bits
bool fits(std::uint32_t cap, std::uint32_t pos, std::uint32_t nbits) {
  return nbits <= kMaxFieldBits && static_cast<std::uint64_t>(pos) + nbits <= 8ull * cap;
}

std::uint32_t low_mask(std::uint32_t n) { return (1u << n) - 1u; }  // n in 1 .. 8

}  // namespace

bool write_bits(std::uint8_t* buf, std::uint32_t cap, std::uint32_t* pos, std::uint32_t value, std::uint32_t nbits) {
  if (buf == nullptr || pos == nullptr || !valid_span(cap, *pos) || !fits(cap, *pos, nbits)) return false;
  std::uint32_t p = *pos;
  std::uint32_t left = nbits;
  while (left > 0) {
    const std::uint32_t offset = p & 7u;
    const std::uint32_t room = 8u - offset;
    const std::uint32_t take = left < room ? left : room;
    const std::uint32_t mask = low_mask(take) << offset;
    const std::uint32_t old = buf[p >> 3];
    buf[p >> 3] = static_cast<std::uint8_t>((old & ~mask) | ((value << offset) & mask));
    value >>= take;
    p += take;
    left -= take;
  }
  *pos = p;
  return true;
}

bool read_bits(const std::uint8_t* buf, std::uint32_t cap, std::uint32_t* pos, std::uint32_t nbits, std::uint32_t* out) {
  if (buf == nullptr || pos == nullptr || out == nullptr || !valid_span(cap, *pos) || !fits(cap, *pos, nbits)) return false;
  std::uint32_t p = *pos;
  std::uint32_t left = nbits;
  std::uint32_t got = 0;
  std::uint32_t result = 0;
  while (left > 0) {
    const std::uint32_t offset = p & 7u;
    const std::uint32_t room = 8u - offset;
    const std::uint32_t take = left < room ? left : room;
    result |= ((static_cast<std::uint32_t>(buf[p >> 3]) >> offset) & low_mask(take)) << got;
    got += take;
    p += take;
    left -= take;
  }
  *out = result;
  *pos = p;
  return true;
}

bool align_to_byte(std::uint32_t cap, std::uint32_t* pos) {
  if (pos == nullptr || !valid_span(cap, *pos)) return false;
  *pos = (*pos + 7u) & ~7u;
  return true;
}

std::uint32_t bits_left(std::uint32_t cap, const std::uint32_t* pos) {
  if (pos == nullptr || !valid_span(cap, *pos)) return 0;
  return 8u * cap - *pos;
}

}  // namespace lumora::bitio
