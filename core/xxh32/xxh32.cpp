// xxHash32, one-shot and streaming. See xxh32.h for the algorithm and the contract, and docs/xxh32.md for the check order and the bounds.
// All arithmetic is on std::uint32_t (modulo 2^32). Words are assembled from bytes with explicit shifts. No allocation, no libm.
#include "xxh32.h"

#include <cstdint>

namespace lumora::xxh32 {

namespace {

constexpr std::uint32_t kP1 = 2654435761u;
constexpr std::uint32_t kP2 = 2246822519u;
constexpr std::uint32_t kP3 = 3266489917u;
constexpr std::uint32_t kP4 = 668265263u;
constexpr std::uint32_t kP5 = 374761393u;
constexpr std::uint32_t kStripe = 16;

std::uint32_t rotl(std::uint32_t x, std::uint32_t r) { return (x << r) | (x >> (32u - r)); }

std::uint32_t read32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}

void init_lanes(std::uint32_t v[4], std::uint32_t seed) {
  v[0] = seed + kP1 + kP2;
  v[1] = seed + kP2;
  v[2] = seed;
  v[3] = seed - kP1;
}

// One 16-byte stripe: four little-endian words, one per lane.
void consume_stripe(std::uint32_t v[4], const std::uint8_t* p) {
  for (std::uint32_t k = 0; k < 4; ++k) v[k] = rotl(v[k] + read32(p + 4 * k) * kP2, 13) * kP1;
}

// Everything after the stripes: lane merge (total_len >= 16) or seed + P5, the length, the tail words and bytes, the final mix.
std::uint32_t finish(const std::uint32_t v[4], std::uint32_t total_len, std::uint32_t seed, const std::uint8_t* tail, std::uint32_t tail_len) {
  std::uint32_t h = total_len >= kStripe ? rotl(v[0], 1) + rotl(v[1], 7) + rotl(v[2], 12) + rotl(v[3], 18) : seed + kP5;
  h += total_len;
  std::uint32_t i = 0;
  while (tail_len - i >= 4) {
    h = rotl(h + read32(tail + i) * kP3, 17) * kP4;
    i += 4;
  }
  while (i < tail_len) {
    h = rotl(h + static_cast<std::uint32_t>(tail[i]) * kP5, 11) * kP1;
    ++i;
  }
  h ^= h >> 15;
  h *= kP2;
  h ^= h >> 13;
  h *= kP3;
  h ^= h >> 16;
  return h;
}

bool corrupt(const State* state) { return state->buf_len > 15; }

}  // namespace

bool hash(const std::uint8_t* data, std::uint32_t len, std::uint32_t seed, std::uint32_t* out) {
  if (out == nullptr || (data == nullptr && len > 0)) return false;
  std::uint32_t v[4];
  init_lanes(v, seed);
  std::uint32_t pos = 0;
  while (len - pos >= kStripe) {
    consume_stripe(v, data + pos);
    pos += kStripe;
  }
  *out = finish(v, len, seed, data + pos, len - pos);
  return true;
}

void reset(State* state, std::uint32_t seed) {
  if (state == nullptr) return;
  init_lanes(state->v, seed);
  state->total_len = 0;
  state->seed = seed;
  for (std::uint32_t i = 0; i < kStripe; ++i) state->buf[i] = 0;
  state->buf_len = 0;
}

bool update(State* state, const std::uint8_t* data, std::uint32_t len) {
  if (state == nullptr || corrupt(state)) return false;
  if (data == nullptr && len > 0) return false;
  if (static_cast<std::uint64_t>(state->total_len) + len > kMaxStreamLength) return false;
  if (len == 0) return true;
  state->total_len += len;
  std::uint32_t pos = 0;
  if (state->buf_len > 0) {
    while (state->buf_len < kStripe && pos < len) state->buf[state->buf_len++] = data[pos++];
    if (state->buf_len == kStripe) {
      consume_stripe(state->v, state->buf);
      state->buf_len = 0;
    }
  }
  while (len - pos >= kStripe) {
    consume_stripe(state->v, data + pos);
    pos += kStripe;
  }
  while (pos < len) state->buf[state->buf_len++] = data[pos++];
  return true;
}

bool digest(const State* state, std::uint32_t* out) {
  if (state == nullptr || out == nullptr || corrupt(state)) return false;
  *out = finish(state->v, state->total_len, state->seed, state->buf, state->buf_len);
  return true;
}

}  // namespace lumora::xxh32
