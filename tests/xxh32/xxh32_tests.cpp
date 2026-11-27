// Tests for core/xxh32. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/xxh32_ref.py, an independent Python implementation written from the
// algorithm text in xxh32.h. Regenerate it with: python3 tools/xxh32_ref.py
// The tests also carry a naive oracle that shares no code with core/xxh32/xxh32.cpp: std::uint64_t arithmetic reduced with a remainder
// after every step, bytes read from a std::vector, and no stripe buffer (a stream is the hash of all bytes added so far).
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "xxh32.h"

using namespace lumora::xxh32;

namespace {

// BEGIN GENERATED REFERENCE (tools/xxh32_ref.py)
const std::uint32_t kRefSeeds[] = {0u, 1u, 2654435761u, 4294967295u, 305419896u};
const std::uint32_t kRefLengths[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u, 41u, 42u, 43u, 44u, 45u, 46u, 47u, 48u, 49u, 50u, 51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u, 59u, 60u, 61u, 62u, 63u, 64u, 65u, 66u, 67u, 68u, 69u, 70u, 71u, 72u, 73u, 74u, 75u, 76u, 77u, 78u, 79u, 80u, 95u, 96u, 97u, 255u, 256u, 257u, 1000u, 4096u};
const std::uint32_t kRefForgedTotals[] = {0u, 5u, 15u, 16u, 17u, 100u, 4294967295u};
const std::uint32_t kRefForgedSeeds[] = {0u, 2654435761u};
const std::size_t kRefRandom = 2000;
const std::uint32_t kRefEmpty0 = 0x02CC5D05u;
const std::uint32_t kRefEmptyP1 = 0x36B78AE7u;
const std::uint32_t kRefBigLen = 1048576u;
const std::uint32_t kRefBigSeed = 19088743u;
const std::uint32_t kRefBig = 0xA8DF4468u;
const std::uint64_t kRefDigest_table = 0x3DB5A4744F3A5773ull;
const std::size_t kRefCases_table = 445;
const std::uint64_t kRefDigest_forged = 0x8F22548AE11295CFull;
const std::size_t kRefCases_forged = 224;
const std::uint64_t kRefDigest_random = 0x9F29555936B29113ull;
// END GENERATED REFERENCE

constexpr std::uint64_t kLcgMul = 6364136223846793005ull;
constexpr std::uint64_t kLcgAdd = 1442695040888963407ull;
constexpr std::uint32_t kSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kP1 = 2654435761u;

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t u32() {
    x = x * kLcgMul + kLcgAdd;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

std::vector<std::uint8_t> gen_data(std::uint32_t n, std::uint32_t add) {
  std::vector<std::uint8_t> d(n);
  for (std::uint32_t i = 0; i < n; ++i) d[i] = static_cast<std::uint8_t>((i * 131u + 17u + add) & 255u);
  return d;
}

// ---- naive oracle -------------------------------------------------------------------------------------------------------------
namespace oracle {
const std::uint64_t kMod = 4294967296ull;
const std::uint64_t kA = 2654435761ull, kB = 2246822519ull, kC = 3266489917ull, kD = 668265263ull, kE = 374761393ull;

std::uint64_t rol(std::uint64_t x, unsigned r) {
  x %= kMod;
  return ((x << r) | (x >> (32u - r))) % kMod;
}

std::uint64_t word(const std::vector<std::uint8_t>& d, std::size_t i) {
  return static_cast<std::uint64_t>(d[i]) + 256ull * d[i + 1] + 65536ull * d[i + 2] + 16777216ull * d[i + 3];
}

// lanes v (used when total >= 16), the total length, the seed and the tail bytes
std::uint32_t finish(const std::uint64_t v[4], std::uint64_t total, std::uint64_t seed, const std::vector<std::uint8_t>& tail) {
  std::uint64_t h = total >= 16 ? (rol(v[0], 1) + rol(v[1], 7) + rol(v[2], 12) + rol(v[3], 18)) % kMod : (seed + kE) % kMod;
  h = (h + total) % kMod;
  std::size_t i = 0;
  for (; i + 4 <= tail.size(); i += 4) h = rol(h + word(tail, i) * kC, 17) * kD % kMod;
  for (; i < tail.size(); ++i) h = rol(h + tail[i] * kE, 11) * kA % kMod;
  h ^= h >> 15;
  h = h * kB % kMod;
  h ^= h >> 13;
  h = h * kC % kMod;
  h ^= h >> 16;
  return static_cast<std::uint32_t>(h);
}

std::uint32_t hash(const std::vector<std::uint8_t>& d, std::uint32_t seed_in) {
  const std::uint64_t seed = seed_in;
  std::uint64_t v[4] = {(seed + kA + kB) % kMod, (seed + kB) % kMod, seed, (seed + kMod - kA) % kMod};
  std::size_t pos = 0;
  while (d.size() - pos >= 16) {
    for (std::size_t k = 0; k < 4; ++k) v[k] = rol(v[k] + word(d, pos + 4 * k) * kB, 13) * kA % kMod;
    pos += 16;
  }
  return finish(v, d.size(), seed, std::vector<std::uint8_t>(d.begin() + static_cast<std::ptrdiff_t>(pos), d.end()));
}
}  // namespace oracle

State forged_state(std::uint32_t total, std::uint32_t seed, std::uint32_t buf_len) {
  State s;
  for (std::uint32_t k = 0; k < 4; ++k) s.v[k] = 0x01000193u * (k + 1u) + total;
  s.total_len = total;
  s.seed = seed;
  for (std::uint32_t i = 0; i < 16; ++i) s.buf[i] = i < buf_len ? static_cast<std::uint8_t>((i * 37u + 5u) & 255u) : 0xAA;
  s.buf_len = buf_len;
  return s;
}

std::vector<std::uint8_t> forged_tail(std::uint32_t buf_len) {
  std::vector<std::uint8_t> t(buf_len);
  for (std::uint32_t i = 0; i < buf_len; ++i) t[i] = static_cast<std::uint8_t>((i * 37u + 5u) & 255u);
  return t;
}

bool same_state(const State& a, const State& b) {
  for (int k = 0; k < 4; ++k)
    if (a.v[k] != b.v[k]) return false;
  for (int i = 0; i < 16; ++i)
    if (a.buf[i] != b.buf[i]) return false;
  return a.total_len == b.total_len && a.seed == b.seed && a.buf_len == b.buf_len;
}

// stream the bytes with the given chunk sizes and return the digest
std::uint32_t stream_hash(const std::vector<std::uint8_t>& d, std::uint32_t seed, const std::vector<std::uint32_t>& chunks, bool* ok) {
  State s;
  reset(&s, seed);
  std::size_t pos = 0;
  for (const std::uint32_t c : chunks) {
    if (!update(&s, d.data() + pos, c)) *ok = false;
    pos += c;
  }
  if (pos != d.size()) *ok = false;
  std::uint32_t out = kSentinel;
  if (!digest(&s, &out)) *ok = false;
  return out;
}

}  // namespace

TEST_CASE("known values: the empty input", "[xxh32]") {
  CHECK(kRefEmpty0 == 0x02CC5D05u);
  CHECK(kRefEmptyP1 == 0x36B78AE7u);
  std::uint32_t out = kSentinel;
  REQUIRE(hash(nullptr, 0, 0, &out));
  CHECK(out == 0x02CC5D05u);
  REQUIRE(hash(nullptr, 0, kP1, &out));
  CHECK(out == 0x36B78AE7u);
  const std::uint8_t one = 7;
  REQUIRE(hash(&one, 0, 0, &out));
  CHECK(out == 0x02CC5D05u);
  State s;
  reset(&s, 0);
  REQUIRE(digest(&s, &out));
  CHECK(out == 0x02CC5D05u);
  reset(&s, kP1);
  REQUIRE(digest(&s, &out));
  CHECK(out == 0x36B78AE7u);
  CHECK(oracle::hash({}, 0) == 0x02CC5D05u);
  CHECK(oracle::hash({}, kP1) == 0x36B78AE7u);
}

TEST_CASE("hash: null arguments leave *out unchanged", "[xxh32][null]") {
  const std::uint8_t d[4] = {1, 2, 3, 4};
  std::uint32_t out = kSentinel;
  CHECK_FALSE(hash(d, 4, 0, nullptr));
  CHECK_FALSE(hash(nullptr, 0, 0, nullptr));
  CHECK_FALSE(hash(nullptr, 1, 0, &out));
  CHECK_FALSE(hash(nullptr, 0xFFFFFFFFu, 5, &out));
  CHECK(out == kSentinel);
  CHECK(hash(nullptr, 0, 9, &out));
  CHECK(out != kSentinel);
}

TEST_CASE("reset replaces any previous content", "[xxh32][reset]") {
  reset(nullptr, 5);
  State s;
  for (int k = 0; k < 4; ++k) s.v[k] = 0x11111111u * static_cast<std::uint32_t>(k + 1);
  s.total_len = 0xFFFFFFFFu;
  s.seed = 99;
  for (auto& b : s.buf) b = 0xCC;
  s.buf_len = 0xFFFFFFFFu;  // corrupt
  reset(&s, 12345u);
  CHECK(s.v[0] == 12345u + 2654435761u + 2246822519u);
  CHECK(s.v[1] == 12345u + 2246822519u);
  CHECK(s.v[2] == 12345u);
  CHECK(s.v[3] == 12345u - 2654435761u);
  CHECK(s.total_len == 0u);
  CHECK(s.seed == 12345u);
  CHECK(s.buf_len == 0u);
  std::size_t nonzero = 0;
  for (const auto b : s.buf)
    if (b != 0) ++nonzero;
  CHECK(nonzero == 0);
  std::uint32_t out = 0;
  CHECK(digest(&s, &out));
  CHECK(out == oracle::hash({}, 12345u));
  reset(&s, 0xFFFFFFFFu);  // seed at the top of the range: seed - P1 and seed + P1 + P2 wrap
  CHECK(s.v[0] == 0xFFFFFFFFu + 2654435761u + 2246822519u);
  CHECK(s.v[3] == 0xFFFFFFFFu - 2654435761u);
}

TEST_CASE("update: null and corrupt states, null data, len 0", "[xxh32][update]") {
  const std::uint8_t d[20] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
  CHECK_FALSE(update(nullptr, d, 4));
  CHECK_FALSE(update(nullptr, nullptr, 0));
  State s;
  reset(&s, 3);
  CHECK(update(&s, d, 5));
  const State before = s;
  CHECK_FALSE(update(&s, nullptr, 1));
  CHECK(same_state(s, before));
  CHECK(update(&s, nullptr, 0));  // len 0 with a null pointer is fine and changes nothing
  CHECK(same_state(s, before));
  CHECK(update(&s, d, 0));
  CHECK(same_state(s, before));
  const std::uint32_t bad[] = {16u, 17u, 255u, 0xFFFFFFFFu};
  for (const std::uint32_t b : bad) {
    State c = forged_state(5, 3, 3);
    c.buf_len = b;
    const State saved = c;
    CHECK_FALSE(update(&c, d, 4));
    CHECK_FALSE(update(&c, d, 0));  // corrupt is checked before len 0
    CHECK_FALSE(update(&c, nullptr, 0));
    CHECK(same_state(c, saved));
    std::uint32_t out = kSentinel;
    CHECK_FALSE(digest(&c, &out));
    CHECK(out == kSentinel);
  }
  State edge = forged_state(5, 3, 15);  // buf_len 15 is the largest valid value
  CHECK(update(&edge, d, 1));
  CHECK(edge.buf_len == 0u);
  CHECK(edge.total_len == 6u);
}

TEST_CASE("update: the buffer length after each call", "[xxh32][update]") {
  const std::vector<std::uint8_t> d = gen_data(200, 1);
  State s;
  reset(&s, 0);
  CHECK(update(&s, d.data(), 16));
  CHECK(s.buf_len == 0u);  // a whole stripe is consumed at once
  CHECK(update(&s, d.data(), 15));
  CHECK(s.buf_len == 15u);
  CHECK(update(&s, d.data(), 1));
  CHECK(s.buf_len == 0u);  // completed exactly at the end of the call: consumed, not 16
  CHECK(update(&s, d.data(), 10));
  CHECK(update(&s, d.data(), 22));
  CHECK(s.buf_len == 0u);  // 10 + 22 = 32
  CHECK(update(&s, d.data(), 10));
  CHECK(update(&s, d.data(), 25));
  CHECK(s.buf_len == 3u);  // 10 + 25 = 35
  CHECK(update(&s, d.data(), 13));
  CHECK(s.buf_len == 0u);
  CHECK(update(&s, d.data(), 1));
  CHECK(update(&s, d.data(), 1));
  CHECK(s.buf_len == 2u);
  CHECK(s.total_len == 16u + 15u + 1u + 32u + 35u + 13u + 2u);
  reset(&s, 0);
  CHECK(update(&s, d.data(), 7));
  CHECK(s.buf_len == 7u);
  CHECK(s.total_len == 7u);
}

TEST_CASE("update: the 32-bit stream length limit", "[xxh32][update]") {
  const std::vector<std::uint8_t> d = gen_data(32, 2);
  State s = forged_state(0xFFFFFFF0u, 4, 0);
  CHECK(update(&s, d.data(), 15));
  CHECK(s.total_len == 0xFFFFFFFFu);  // exactly kMaxStreamLength is accepted
  CHECK(s.buf_len == 15u);
  const State at_max = s;
  CHECK_FALSE(update(&s, d.data(), 1));  // one more is refused and nothing changes
  CHECK(same_state(s, at_max));
  CHECK(update(&s, d.data(), 0));
  CHECK(same_state(s, at_max));
  State t = forged_state(0xFFFFFFF0u, 4, 0);
  const State t0 = t;
  CHECK_FALSE(update(&t, d.data(), 17));  // 0xFFFFFFF0 + 17 = 2^32 + 1: refused, not wrapped
  CHECK(same_state(t, t0));
  CHECK_FALSE(update(&t, d.data(), 0xFFFFFFFFu));
  CHECK(same_state(t, t0));
  State u = forged_state(1, 4, 1);
  const State u0 = u;
  CHECK_FALSE(update(&u, d.data(), 0xFFFFFFFFu));  // 1 + 0xFFFFFFFF
  CHECK(same_state(u, u0));
  State v = forged_state(0, 4, 0);
  CHECK_FALSE(update(&v, nullptr, 0xFFFFFFFFu));  // null data is checked too
  // a state already at the limit still hashes, with the length 0xFFFFFFFF
  std::uint32_t out = 0;
  CHECK(digest(&at_max, &out));
  CHECK(out == oracle::finish(std::vector<std::uint64_t>{at_max.v[0], at_max.v[1], at_max.v[2], at_max.v[3]}.data(), 0xFFFFFFFFull, 4,
                              std::vector<std::uint8_t>(at_max.buf, at_max.buf + 15)));
}

TEST_CASE("digest: null arguments, corrupt state, no change, repeat and continue", "[xxh32][digest]") {
  const std::vector<std::uint8_t> d = gen_data(100, 3);
  State s;
  reset(&s, 77);
  std::uint32_t out = kSentinel;
  CHECK_FALSE(digest(nullptr, &out));
  CHECK_FALSE(digest(&s, nullptr));
  CHECK_FALSE(digest(nullptr, nullptr));
  CHECK(out == kSentinel);
  REQUIRE(update(&s, d.data(), 37));
  const State before = s;
  std::uint32_t a = 0, b = 0;
  REQUIRE(digest(&s, &a));
  REQUIRE(digest(&s, &b));
  CHECK(a == b);
  CHECK(same_state(s, before));
  CHECK(a == oracle::hash(std::vector<std::uint8_t>(d.begin(), d.begin() + 37), 77));
  REQUIRE(update(&s, d.data() + 37, 63));  // the stream continues after a digest
  REQUIRE(digest(&s, &a));
  CHECK(a == oracle::hash(d, 77));
  std::uint32_t one = 0;
  REQUIRE(hash(d.data(), 100, 77, &one));
  CHECK(a == one);
}

TEST_CASE("hash, whole stream, byte by byte and every single split agree with the reference", "[xxh32][sweep]") {
  Fnv f;
  std::size_t cases = 0, bad = 0;
  for (const std::uint32_t seed : kRefSeeds)
    for (const std::uint32_t n : kRefLengths) {
      const std::vector<std::uint8_t> d = gen_data(n, seed & 255u);
      std::uint32_t one = kSentinel;
      if (!hash(d.data(), n, seed, &one)) ++bad;
      if (one != oracle::hash(d, seed)) ++bad;
      f.u32(one);
      ++cases;
      bool ok = true;
      if (stream_hash(d, seed, {n}, &ok) != one) ++bad;
      if (stream_hash(d, seed, std::vector<std::uint32_t>(n, 1u), &ok) != one) ++bad;
      if (n <= 80)
        for (std::uint32_t cut = 0; cut <= n; ++cut)
          if (stream_hash(d, seed, {cut, n - cut}, &ok) != one) ++bad;
      if (!ok) ++bad;
    }
  CHECK(bad == 0);
  CHECK(cases == kRefCases_table);
  CHECK(f.h == kRefDigest_table);
}

TEST_CASE("random chunking agrees with the reference", "[xxh32][sweep]") {
  Lcg g(71);
  Lcg chunker(72);
  Fnv f;
  std::size_t bad = 0;
  for (std::size_t c = 0; c < kRefRandom; ++c) {
    const std::uint32_t kind = g.u32() % 4u;
    std::uint32_t n;
    if (kind == 0) {
      n = g.u32() % 40u;
    } else if (kind < 3) {
      n = g.u32() % 300u;
    } else {
      const std::uint32_t stripes = g.u32() % 20u;
      n = 16u * stripes + g.u32() % 3u;
    }
    std::uint32_t seed = 0;
    if (g.u32() % 2u != 0) seed = g.u32();
    const std::uint32_t base = g.u32();
    std::vector<std::uint8_t> d(n);
    for (std::uint32_t i = 0; i < n; ++i) d[i] = static_cast<std::uint8_t>((base + i * 7u) & 255u);
    std::vector<std::uint32_t> chunks;
    std::uint32_t left = n;
    while (left > 0) {
      std::uint32_t k = 1u + chunker.u32() % 40u;
      if (k > left) k = left;
      chunks.push_back(k);
      left -= k;
    }
    bool ok = true;
    const std::uint32_t got = stream_hash(d, seed, chunks, &ok);
    std::uint32_t one = 0;
    if (!hash(d.data(), n, seed, &one) || !ok || got != one) ++bad;
    f.u32(one);
  }
  CHECK(bad == 0);
  CHECK(f.h == kRefDigest_random);
}

TEST_CASE("a 1 MiB input, one shot and streamed", "[xxh32][big]") {
  const std::vector<std::uint8_t> d = gen_data(kRefBigLen, 0x33u);
  std::uint32_t one = 0;
  REQUIRE(hash(d.data(), kRefBigLen, kRefBigSeed, &one));
  CHECK(one == kRefBig);
  CHECK(oracle::hash(d, kRefBigSeed) == kRefBig);
  State s;
  reset(&s, kRefBigSeed);
  std::size_t pos = 0;
  bool ok = true;
  while (pos < d.size()) {
    const std::size_t k = d.size() - pos < 4097 ? d.size() - pos : 4097;
    if (!update(&s, d.data() + pos, static_cast<std::uint32_t>(k))) ok = false;
    pos += k;
  }
  CHECK(ok);
  CHECK(s.total_len == kRefBigLen);
  std::uint32_t out = 0;
  REQUIRE(digest(&s, &out));
  CHECK(out == kRefBig);
}

TEST_CASE("digest of forged states is literal", "[xxh32][forged]") {
  Fnv f;
  std::size_t cases = 0, bad = 0;
  for (const std::uint32_t total : kRefForgedTotals)
    for (const std::uint32_t seed : kRefForgedSeeds)
      for (std::uint32_t buf_len = 0; buf_len < 16; ++buf_len) {
        const State s = forged_state(total, seed, buf_len);
        const State saved = s;
        std::uint32_t out = kSentinel;
        if (!digest(&s, &out)) ++bad;
        const std::uint64_t v[4] = {s.v[0], s.v[1], s.v[2], s.v[3]};
        if (out != oracle::finish(v, total, seed, forged_tail(buf_len))) ++bad;
        if (!same_state(s, saved)) ++bad;
        f.u32(out);
        ++cases;
      }
  CHECK(bad == 0);
  CHECK(cases == kRefCases_forged);
  CHECK(f.h == kRefDigest_forged);
}
