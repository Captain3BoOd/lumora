// Fixed-capacity byte FIFO. See byte_ring.h for the contract and docs/byte_ring.md for the check order and the bounds. Every index is
// kept below kCapacity before an addition (head, count and offsets are all at most 4096 once the ring is valid and the offset has been
// checked against count), so no sum can overflow whatever n is. Copies are done in at most two contiguous segments. No floating point,
// no libm, no allocation.
#include "byte_ring.h"

#include <cstdint>
#include <cstring>

namespace lumora::ring {

namespace {

bool is_valid(const ByteRing* ring) { return ring != nullptr && ring->head < kCapacity && ring->count <= kCapacity; }

std::uint32_t min_u32(std::uint32_t a, std::uint32_t b) { return a < b ? a : b; }

// Index of the byte `offset` places after the oldest. Requires a valid ring and offset <= kCapacity.
std::uint32_t index_of(const ByteRing* ring, std::uint32_t offset) { return (ring->head + offset) % kCapacity; }

// Copies k bytes (k > 0) starting at ring index `start` to dst, wrapping once at the end of data.
void copy_out(const ByteRing* ring, std::uint32_t start, std::uint8_t* dst, std::uint32_t k) {
  const std::uint32_t first = min_u32(k, kCapacity - start);
  std::memcpy(dst, ring->data + start, first);
  if (k > first) std::memcpy(dst + first, ring->data, k - first);
}

void advance_head(ByteRing* ring, std::uint32_t k) {
  ring->head = (ring->head + k) % kCapacity;
  ring->count -= k;
}

}  // namespace

void reset(ByteRing* ring) {
  if (ring == nullptr) return;
  ring->head = 0;
  ring->count = 0;
}

std::uint32_t size(const ByteRing* ring) { return is_valid(ring) ? ring->count : 0; }

std::uint32_t free_space(const ByteRing* ring) { return is_valid(ring) ? kCapacity - ring->count : 0; }

std::uint32_t write(ByteRing* ring, const std::uint8_t* src, std::uint32_t n) {
  if (!is_valid(ring) || src == nullptr || n == 0) return 0;
  const std::uint32_t k = min_u32(n, kCapacity - ring->count);
  if (k == 0) return 0;
  const std::uint32_t start = index_of(ring, ring->count);
  const std::uint32_t first = min_u32(k, kCapacity - start);
  std::memcpy(ring->data + start, src, first);
  if (k > first) std::memcpy(ring->data, src + first, k - first);
  ring->count += k;
  return k;
}

std::uint32_t read(ByteRing* ring, std::uint8_t* dst, std::uint32_t n) {
  if (!is_valid(ring) || dst == nullptr || n == 0) return 0;
  const std::uint32_t k = min_u32(n, ring->count);
  if (k == 0) return 0;
  copy_out(ring, ring->head, dst, k);
  advance_head(ring, k);
  return k;
}

std::uint32_t peek(const ByteRing* ring, std::uint8_t* dst, std::uint32_t n, std::uint32_t offset) {
  if (!is_valid(ring) || dst == nullptr || n == 0 || offset >= ring->count) return 0;
  const std::uint32_t k = min_u32(n, ring->count - offset);
  copy_out(ring, index_of(ring, offset), dst, k);
  return k;
}

std::uint32_t skip(ByteRing* ring, std::uint32_t n) {
  if (!is_valid(ring) || n == 0) return 0;
  const std::uint32_t k = min_u32(n, ring->count);
  if (k == 0) return 0;
  advance_head(ring, k);
  return k;
}

}  // namespace lumora::ring
