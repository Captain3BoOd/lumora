// Tests for core/bitio. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/bitio_ref.py, an independent Python implementation (a list of bytes
// with every field moved one bit at a time). Regenerate it with: python3 tools/bitio_ref.py
// The tests also carry a naive oracle that shares no code with core/bitio/bitio.cpp: a std::vector<bool> of bits, one bit per step.
// Buffers are allocated with exactly cap bytes on the heap, so AddressSanitizer sees any access past byte cap - 1.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "bitio.h"

using namespace lumora::bitio;

namespace {

// BEGIN GENERATED REFERENCE (tools/bitio_ref.py)
const std::uint32_t kRefCaps[] = {0u, 1u, 2u, 5u, 9u};
const std::uint32_t kRefValues[] = {0u, 1u, 4294967295u, 2863311530u, 305419896u};
const std::uint32_t kRefLimitCaps[] = {0u, 1u, 2u, 3u, 268435455u, 268435456u, 268435457u, 4294967295u};
const std::uint32_t kRefLimitPos[] = {0u, 1u, 7u, 8u, 9u, 31u, 32u, 33u, 63u, 64u, 65u, 2147483615u, 2147483616u, 2147483647u, 2147483648u, 2147483649u, 4294967295u};
const std::uint32_t kRefLimitNbits[] = {0u, 1u, 2u, 31u, 32u, 33u, 4294967295u};
const std::uint64_t kRefDigest_table = 0xD750599FB7114456ull;
const std::size_t kRefCases_table = 98301;
const std::uint64_t kRefDigest_limit = 0x84727F966D398619ull;
const std::size_t kRefCases_limit = 2236;
const std::uint64_t kRefDigest_random = 0xD16EA4EE1581F04Dull;
const std::size_t kRefStat_write_ok = 43575;
const std::size_t kRefStat_write_fail = 37965;
const std::size_t kRefStat_read_ok = 8715;
const std::size_t kRefStat_read_fail = 7593;
const std::size_t kRefStat_rnd_write_ok = 713;
const std::size_t kRefStat_rnd_write_fail = 442;
const std::size_t kRefStat_rnd_read_ok = 709;
const std::size_t kRefStat_rnd_read_fail = 453;
const std::size_t kRefStat_rnd_align_ok = 560;
const std::size_t kRefStat_rnd_align_fail = 15;
// END GENERATED REFERENCE

constexpr std::uint64_t kLcgMul = 6364136223846793005ull;
constexpr std::uint64_t kLcgAdd = 1442695040888963407ull;
constexpr std::uint32_t kNoOut = 0xDEADBEEFu;
constexpr std::uint32_t kBigNbits = 0xFFFFFFFFu;
constexpr std::size_t kDummy = 16;

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

std::uint32_t checksum(const std::uint8_t* p, std::size_t n) {
  std::uint32_t s = 0;
  for (std::size_t i = 0; i < n; ++i) s = s * 31u + p[i] + 1u;
  return s;
}

// A heap buffer of exactly n bytes (n may be 0; the pointer is still non-null)
struct Buf {
  std::unique_ptr<std::uint8_t[]> mem;
  std::size_t n;
  explicit Buf(std::size_t size) : mem(new std::uint8_t[size]), n(size) {}
  std::uint8_t* data() { return mem.get(); }
  const std::uint8_t* data() const { return mem.get(); }
  std::uint8_t& operator[](std::size_t i) { return mem[i]; }
  const std::uint8_t& operator[](std::size_t i) const { return mem[i]; }
  std::uint32_t sum() const { return checksum(mem.get(), n); }
};

Buf background(std::size_t n, std::size_t kind) {
  Buf b(n);
  for (std::size_t i = 0; i < n; ++i) b[i] = kind == 0 ? 0 : (kind == 1 ? 255 : static_cast<std::uint8_t>((i * 37u + 11u) & 255u));
  return b;
}

bool same_bytes(const Buf& a, const std::vector<std::uint8_t>& want) {
  if (a.n != want.size()) return false;
  for (std::size_t i = 0; i < a.n; ++i)
    if (a[i] != want[i]) return false;
  return true;
}

// ---- naive oracle: one bit per step ---------------------------------------------------------------------------------------------
struct BitOracle {
  std::vector<bool> bits;
  explicit BitOracle(const Buf& b) : bits(b.n * 8) {
    for (std::size_t i = 0; i < b.n * 8; ++i) bits[i] = ((b[i / 8] >> (i % 8)) & 1) != 0;
  }
  static bool ok(std::uint64_t cap, std::uint64_t pos, std::uint64_t nbits) {
    return cap <= 268435456ull && pos <= 8 * cap && nbits <= 32 && pos + nbits <= 8 * cap;
  }
  bool write(std::uint64_t cap, std::uint32_t* pos, std::uint32_t value, std::uint32_t nbits) {
    if (!ok(cap, *pos, nbits)) return false;
    for (std::uint32_t i = 0; i < nbits; ++i) bits[*pos + i] = ((value >> i) & 1u) != 0;
    *pos += nbits;
    return true;
  }
  bool read(std::uint64_t cap, std::uint32_t* pos, std::uint32_t nbits, std::uint32_t* out) const {
    if (!ok(cap, *pos, nbits)) return false;
    std::uint32_t v = 0;
    for (std::uint32_t i = 0; i < nbits; ++i)
      if (bits[*pos + i]) v |= 1u << i;
    *out = v;
    *pos += nbits;
    return true;
  }
  bool same(const Buf& b) const {
    for (std::size_t i = 0; i < b.n * 8; ++i)
      if (bits[i] != (((b[i / 8] >> (i % 8)) & 1) != 0)) return false;
    return true;
  }
};

std::uint32_t nbits_at(std::size_t i) { return i < 35 ? static_cast<std::uint32_t>(i) : kBigNbits; }

}  // namespace

TEST_CASE("constants", "[bitio]") { CHECK(kMaxBytes == 268435456u); }

TEST_CASE("null buffer, null position, null out", "[bitio][null]") {
  Buf b = background(4, 2);
  std::uint32_t pos = 0, out = kNoOut;
  CHECK_FALSE(write_bits(nullptr, 4, &pos, 1, 3));
  CHECK_FALSE(write_bits(nullptr, 4, &pos, 1, 0));  // a null buffer fails even with nbits 0
  CHECK_FALSE(write_bits(b.data(), 4, nullptr, 1, 3));
  CHECK_FALSE(write_bits(b.data(), 4, nullptr, 1, 0));
  CHECK_FALSE(read_bits(nullptr, 4, &pos, 3, &out));
  CHECK_FALSE(read_bits(nullptr, 4, &pos, 0, &out));
  CHECK_FALSE(read_bits(b.data(), 4, nullptr, 3, &out));
  CHECK_FALSE(read_bits(b.data(), 4, &pos, 3, nullptr));
  CHECK_FALSE(read_bits(b.data(), 4, &pos, 0, nullptr));
  CHECK_FALSE(align_to_byte(4, nullptr));
  CHECK(bits_left(4, nullptr) == 0u);
  CHECK(pos == 0u);
  CHECK(out == kNoOut);
  CHECK(b.sum() == background(4, 2).sum());
}

TEST_CASE("LSB-first layout", "[bitio][write]") {
  Buf b = background(8, 0);
  std::uint32_t pos = 0;
  REQUIRE(write_bits(b.data(), 8, &pos, 5u, 3));
  CHECK(pos == 3u);
  CHECK(b[0] == 0x05);
  REQUIRE(write_bits(b.data(), 8, &pos, 0x1Fu, 5));
  CHECK(pos == 8u);
  CHECK(b[0] == 0xFD);  // 101 then 11111 above it
  Buf c = background(8, 0);
  pos = 0;
  REQUIRE(write_bits(c.data(), 8, &pos, 0x12345678u, 32));
  CHECK(same_bytes(c, {0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0}));
  Buf d = background(6, 0);
  pos = 7;  // a 32-bit field that starts at bit 7 spans five bytes
  REQUIRE(write_bits(d.data(), 6, &pos, 0xFFFFFFFFu, 32));
  CHECK(pos == 39u);
  CHECK(same_bytes(d, {0x80, 0xFF, 0xFF, 0xFF, 0x7F, 0x00}));
}

TEST_CASE("write: high bits of value are ignored, other bits keep their value", "[bitio][write]") {
  Buf b = background(4, 0);
  std::uint32_t pos = 0;
  REQUIRE(write_bits(b.data(), 4, &pos, 0xFFFFFFFFu, 4));
  CHECK(same_bytes(b, {0x0F, 0, 0, 0}));
  Buf f = background(4, 1);
  pos = 2;
  REQUIRE(write_bits(f.data(), 4, &pos, 0u, 3));
  CHECK(same_bytes(f, {0xE3, 0xFF, 0xFF, 0xFF}));
  pos = 9;  // replace old bits with new ones
  REQUIRE(write_bits(f.data(), 4, &pos, 0xA5u, 8));
  CHECK(pos == 17u);
  CHECK(same_bytes(f, {0xE3, 0x4B, 0xFF, 0xFF}));  // 0xFF with bits 9..16 replaced by 0xA5: byte 1 = 0x4B, byte 2 bit 0 = 1 (0xFF kept)
  Buf g = background(3, 1);
  pos = 4;
  REQUIRE(write_bits(g.data(), 3, &pos, 0u, 16));
  CHECK(same_bytes(g, {0x0F, 0x00, 0xF0}));
}

TEST_CASE("read: order, masking and position", "[bitio][read]") {
  Buf b = background(1, 0);
  b[0] = 0xA5;
  std::uint32_t pos = 0, out = kNoOut;
  REQUIRE(read_bits(b.data(), 1, &pos, 4, &out));
  CHECK(out == 5u);
  CHECK(pos == 4u);
  REQUIRE(read_bits(b.data(), 1, &pos, 4, &out));
  CHECK(out == 0xAu);
  CHECK(pos == 8u);
  CHECK_FALSE(read_bits(b.data(), 1, &pos, 1, &out));
  CHECK(out == 0xAu);
  CHECK(pos == 8u);
  Buf f = background(6, 1);
  pos = 3;
  REQUIRE(read_bits(f.data(), 6, &pos, 3, &out));
  CHECK(out == 7u);  // only the low nbits are set
  pos = 5;
  REQUIRE(read_bits(f.data(), 6, &pos, 32, &out));
  CHECK(out == 0xFFFFFFFFu);
  CHECK(pos == 37u);
}

TEST_CASE("nbits 0 succeeds and changes nothing", "[bitio][zero]") {
  Buf b = background(2, 2);
  const std::uint32_t before = b.sum();
  const std::uint32_t starts[] = {0u, 5u, 8u, 16u};  // 16 is 8 * cap
  for (const std::uint32_t s : starts) {
    std::uint32_t pos = s, out = kNoOut;
    CHECK(write_bits(b.data(), 2, &pos, 0xFFFFFFFFu, 0));
    CHECK(pos == s);
    CHECK(read_bits(b.data(), 2, &pos, 0, &out));
    CHECK(out == 0u);
    CHECK(pos == s);
  }
  CHECK(b.sum() == before);
  Buf empty = background(0, 0);  // cap 0 is valid: only nbits 0 at position 0 works
  std::uint32_t pos = 0, out = kNoOut;
  CHECK(write_bits(empty.data(), 0, &pos, 1, 0));
  CHECK(read_bits(empty.data(), 0, &pos, 0, &out));
  CHECK(out == 0u);
  CHECK_FALSE(write_bits(empty.data(), 0, &pos, 1, 1));
  CHECK_FALSE(read_bits(empty.data(), 0, &pos, 1, &out));
  CHECK(bits_left(0, &pos) == 0u);
}

TEST_CASE("exact end succeeds and one bit more fails (small caps)", "[bitio][bounds]") {
  const std::uint32_t caps[] = {1u, 2u, 4u, 5u, 9u};
  for (const std::uint32_t cap : caps) {
    for (std::uint32_t n = 1; n <= 32; ++n) {
      if (n > 8u * cap) continue;
      const std::uint32_t start = 8u * cap - n;
      Buf b = background(cap, 0);
      std::uint32_t pos = start, out = kNoOut;
      REQUIRE(write_bits(b.data(), cap, &pos, 0xFFFFFFFFu, n));
      CHECK(pos == 8u * cap);
      CHECK(bits_left(cap, &pos) == 0u);
      pos = start;
      REQUIRE(read_bits(b.data(), cap, &pos, n, &out));
      CHECK(out == (n == 32 ? 0xFFFFFFFFu : (1u << n) - 1u));
      CHECK(pos == 8u * cap);
      pos = start + 1;  // one bit past the end
      const std::uint32_t before = b.sum();
      CHECK_FALSE(write_bits(b.data(), cap, &pos, 1u, n));
      CHECK_FALSE(read_bits(b.data(), cap, &pos, n, &out));
      CHECK(pos == start + 1);
      CHECK(b.sum() == before);
    }
  }
}

TEST_CASE("failures change nothing", "[bitio][fail]") {
  Buf b = background(4, 2);
  const std::uint32_t before = b.sum();
  std::uint32_t pos = 0, out = kNoOut;
  CHECK_FALSE(write_bits(b.data(), 4, &pos, 1u, 33));  // nbits above 32
  CHECK_FALSE(write_bits(b.data(), 4, &pos, 1u, kBigNbits));
  CHECK_FALSE(read_bits(b.data(), 4, &pos, 33, &out));
  CHECK_FALSE(read_bits(b.data(), 4, &pos, kBigNbits, &out));
  pos = 33;  // above 8 * cap
  CHECK_FALSE(write_bits(b.data(), 4, &pos, 1u, 0));
  CHECK_FALSE(write_bits(b.data(), 4, &pos, 1u, 1));
  CHECK_FALSE(read_bits(b.data(), 4, &pos, 0, &out));
  CHECK_FALSE(align_to_byte(4, &pos));
  CHECK(bits_left(4, &pos) == 0u);
  CHECK(pos == 33u);
  pos = kBigNbits;
  CHECK_FALSE(write_bits(b.data(), 4, &pos, 1u, 1));
  CHECK_FALSE(read_bits(b.data(), 4, &pos, 1, &out));
  CHECK_FALSE(align_to_byte(4, &pos));
  CHECK(bits_left(4, &pos) == 0u);
  CHECK(pos == kBigNbits);
  pos = 32;  // at the end, one bit more
  CHECK_FALSE(write_bits(b.data(), 4, &pos, 1u, 1));
  pos = 1;  // 1 + 32 > 32
  CHECK_FALSE(write_bits(b.data(), 4, &pos, 1u, 32));
  pos = 31;
  CHECK_FALSE(read_bits(b.data(), 4, &pos, 2, &out));
  CHECK(out == kNoOut);
  CHECK(b.sum() == before);
}

TEST_CASE("align_to_byte and bits_left", "[bitio][align]") {
  const std::uint32_t from[] = {0, 1, 7, 8, 9, 15, 16, 17, 31, 32};
  const std::uint32_t to[] = {0, 8, 8, 8, 16, 16, 16, 24, 32, 32};
  for (std::size_t i = 0; i < 10; ++i) {
    std::uint32_t pos = from[i];
    CHECK(align_to_byte(4, &pos));
    CHECK(pos == to[i]);
    CHECK(bits_left(4, &from[i]) == 32u - from[i]);
  }
  std::uint32_t pos = 33;
  CHECK_FALSE(align_to_byte(4, &pos));
  CHECK(pos == 33u);
  pos = 0;
  CHECK_FALSE(align_to_byte(kMaxBytes + 1, &pos));
  CHECK(bits_left(kMaxBytes + 1, &pos) == 0u);
  CHECK(bits_left(0, &pos) == 0u);
  pos = 3;
  CHECK(align_to_byte(0, &pos) == false);  // 3 is above 8 * 0
  Buf b = background(2, 1);  // the skipped bits are not touched: align only moves the position
  pos = 1;
  CHECK(align_to_byte(2, &pos));
  CHECK(b.sum() == background(2, 1).sum());
}

TEST_CASE("cap at the top of the range: rejections with a small buffer that must not be touched", "[bitio][limit]") {
  Buf b = background(8, 2);
  const std::uint32_t before = b.sum();
  std::uint32_t out = kNoOut;
  std::uint32_t pos = 2147483648u - 31u;  // a 32-bit field would end at 2^31 + 1
  CHECK_FALSE(write_bits(b.data(), kMaxBytes, &pos, 0xFFFFFFFFu, 32));
  CHECK_FALSE(read_bits(b.data(), kMaxBytes, &pos, 32, &out));
  CHECK(pos == 2147483648u - 31u);
  pos = 2147483648u;  // exactly 8 * cap
  CHECK_FALSE(write_bits(b.data(), kMaxBytes, &pos, 1u, 1));
  CHECK_FALSE(read_bits(b.data(), kMaxBytes, &pos, 1, &out));
  pos = 2147483649u;  // 2^31 + 1: invalid
  CHECK_FALSE(write_bits(b.data(), kMaxBytes, &pos, 1u, 0));
  CHECK_FALSE(read_bits(b.data(), kMaxBytes, &pos, 0, &out));
  CHECK_FALSE(align_to_byte(kMaxBytes, &pos));
  CHECK(bits_left(kMaxBytes, &pos) == 0u);
  CHECK(pos == 2147483649u);
  pos = 0;  // cap 2^28 + 1 is invalid for everything
  CHECK_FALSE(write_bits(b.data(), kMaxBytes + 1u, &pos, 1u, 1));
  CHECK_FALSE(write_bits(b.data(), kMaxBytes + 1u, &pos, 1u, 0));
  CHECK_FALSE(read_bits(b.data(), kMaxBytes + 1u, &pos, 1, &out));
  CHECK_FALSE(read_bits(b.data(), kMaxBytes + 1u, &pos, 0, &out));
  CHECK_FALSE(align_to_byte(kMaxBytes + 1u, &pos));
  CHECK(bits_left(kMaxBytes + 1u, &pos) == 0u);
  CHECK_FALSE(write_bits(b.data(), 0xFFFFFFFFu, &pos, 1u, 1));
  CHECK(bits_left(0xFFFFFFFFu, &pos) == 0u);
  CHECK(pos == 0u);
  CHECK(out == kNoOut);
  CHECK(b.sum() == before);
}

TEST_CASE("cap at the top of the range: bits_left, align and zero-bit operations at the maximum position", "[bitio][limit]") {
  Buf b = background(8, 2);
  const std::uint32_t before = b.sum();
  std::uint32_t pos = 0, out = kNoOut;
  CHECK(bits_left(kMaxBytes, &pos) == 2147483648u);
  pos = 2147483648u;  // exactly 8 * cap
  CHECK(bits_left(kMaxBytes, &pos) == 0u);
  CHECK(align_to_byte(kMaxBytes, &pos));
  CHECK(pos == 2147483648u);
  CHECK(write_bits(b.data(), kMaxBytes, &pos, 1u, 0));
  CHECK(read_bits(b.data(), kMaxBytes, &pos, 0, &out));
  CHECK(out == 0u);
  CHECK(pos == 2147483648u);
  pos = 2147483647u;  // the last bit: align moves to 2^31
  CHECK(bits_left(kMaxBytes, &pos) == 1u);
  CHECK(align_to_byte(kMaxBytes, &pos));
  CHECK(pos == 2147483648u);
  pos = 2147483641u;
  CHECK(align_to_byte(kMaxBytes, &pos));
  CHECK(pos == 2147483648u);
  CHECK(b.sum() == before);
  // a successful nonzero field that ends exactly at 2^31 bits was NOT run: it needs a 256 MiB buffer
}

TEST_CASE("cap at the top of the range: successful fields at small positions with a buffer that covers only the field bytes", "[bitio][limit]") {
  Buf b = background(8, 0);
  std::uint32_t pos = 5, out = kNoOut;
  REQUIRE(write_bits(b.data(), kMaxBytes, &pos, 0xFFFFFFFFu, 32));  // bits 5..36: bytes 0 to 4
  CHECK(pos == 37u);
  CHECK(same_bytes(b, {0xE0, 0xFF, 0xFF, 0xFF, 0x1F, 0, 0, 0}));
  pos = 5;
  REQUIRE(read_bits(b.data(), kMaxBytes, &pos, 32, &out));
  CHECK(out == 0xFFFFFFFFu);
  CHECK(bits_left(kMaxBytes, &pos) == 2147483648u - 37u);
}

TEST_CASE("every position and width on small buffers matches the oracle and the reference digest", "[bitio][sweep]") {
  Fnv f;
  std::size_t cases = 0, bad = 0, wok = 0, wfail = 0, rok = 0, rfail = 0;
  for (const std::uint32_t cap : kRefCaps)
    for (std::size_t bg = 0; bg < 3; ++bg) {
      const std::uint32_t limit = std::min<std::uint32_t>(8u * cap + 3u, 75u);
      for (std::uint32_t p0 = 0; p0 < limit; ++p0) {
        for (std::size_t ni = 0; ni < 36; ++ni) {
          const std::uint32_t nbits = nbits_at(ni);
          for (const std::uint32_t value : kRefValues) {
            Buf b = background(cap, bg);
            BitOracle o(b);
            std::uint32_t pos = p0, opos = p0;
            const bool got = write_bits(b.data(), cap, &pos, value, nbits);
            const bool want = o.write(cap, &opos, value, nbits);
            if (got != want || pos != opos || !o.same(b)) ++bad;
            f.u32(got ? 1u : 0u);
            f.u32(pos);
            f.u32(b.sum());
            ++cases;
            if (got) ++wok;
            else ++wfail;
          }
          Buf b = background(cap, bg);
          const BitOracle o(b);
          std::uint32_t pos = p0, opos = p0, out = kNoOut, oout = kNoOut;
          const bool got = read_bits(b.data(), cap, &pos, nbits, &out);
          const bool want = o.read(cap, &opos, nbits, &oout);
          if (got != want || pos != opos || out != oout || !o.same(b)) ++bad;
          f.u32(got ? 1u : 0u);
          f.u32(pos);
          f.u32(out);
          ++cases;
          if (got) ++rok;
          else ++rfail;
        }
        std::uint32_t pos = p0;
        const bool got = align_to_byte(cap, &pos);
        const std::uint32_t left = bits_left(cap, &p0);
        const bool inside = p0 <= 8u * cap;
        if (got != inside || (inside && (pos != ((p0 + 7u) / 8u) * 8u || left != 8u * cap - p0)) || (!inside && (pos != p0 || left != 0u))) ++bad;
        f.u32(got ? 1u : 0u);
        f.u32(pos);
        f.u32(left);
        ++cases;
      }
    }
  CHECK(bad == 0);
  CHECK(cases == kRefCases_table);
  CHECK(wok == kRefStat_write_ok);
  CHECK(wfail == kRefStat_write_fail);
  CHECK(rok == kRefStat_read_ok);
  CHECK(rfail == kRefStat_read_fail);
  CHECK(f.h == kRefDigest_table);
}

TEST_CASE("huge caps with a small dummy buffer match the reference digest", "[bitio][sweep][limit]") {
  Fnv f;
  std::size_t cases = 0, bad = 0;
  for (const std::uint32_t cap : kRefLimitCaps) {
    std::vector<std::uint32_t> ps(std::begin(kRefLimitPos), std::end(kRefLimitPos));
    const std::int64_t end = 8 * static_cast<std::int64_t>(cap);
    const std::int64_t deltas[] = {-33, -32, -1, 0, 1};
    for (const std::int64_t d : deltas) {
      const std::int64_t v = end + d;
      if (v >= 0 && v < 4294967296ll) ps.push_back(static_cast<std::uint32_t>(v));
    }
    std::sort(ps.begin(), ps.end());
    ps.erase(std::unique(ps.begin(), ps.end()), ps.end());
    for (const std::uint32_t p0 : ps) {
      std::uint32_t pos = p0;
      const bool got = align_to_byte(cap, &pos);
      f.u32(got ? 1u : 0u);
      f.u32(pos);
      f.u32(bits_left(cap, &p0));
      ++cases;
      for (const std::uint32_t nbits : kRefLimitNbits) {
        const bool fails = cap > kMaxBytes || p0 > 8ull * cap || nbits > 32 || p0 + static_cast<std::uint64_t>(nbits) > 8ull * cap;
        if (!(fails || nbits == 0 || p0 + static_cast<std::uint64_t>(nbits) <= 8ull * kDummy)) continue;  // would reach past the dummy buffer
        Buf b = background(kDummy, 2);
        pos = p0;
        const bool w = write_bits(b.data(), cap, &pos, 0xFFFFFFFFu, nbits);
        if (w == fails) ++bad;
        f.u32(w ? 1u : 0u);
        f.u32(pos);
        f.u32(b.sum());
        Buf c = background(kDummy, 2);
        pos = p0;
        std::uint32_t out = kNoOut;
        const bool r = read_bits(c.data(), cap, &pos, nbits, &out);
        if (r == fails) ++bad;
        if (fails && (pos != p0 || out != kNoOut || c.sum() != background(kDummy, 2).sum())) ++bad;
        f.u32(r ? 1u : 0u);
        f.u32(pos);
        f.u32(out);
        cases += 2;
      }
    }
  }
  CHECK(bad == 0);
  CHECK(cases == kRefCases_limit);
  CHECK(f.h == kRefDigest_limit);
}

TEST_CASE("scripted random sequences match the oracle and the reference digest", "[bitio][sweep]") {
  Lcg g(81);
  Fnv f;
  std::size_t bad = 0, wok = 0, wfail = 0, rok = 0, rfail = 0, aok = 0, afail = 0;
  for (std::size_t s = 0; s < 100; ++s) {
    const std::uint32_t cap = g.u32() % 41u;
    Buf b(cap);
    for (std::uint32_t i = 0; i < cap; ++i) b[i] = static_cast<std::uint8_t>(g.u32() & 255u);
    BitOracle o(b);
    std::uint32_t pos = 0;
    for (std::size_t step = 0; step < 40; ++step) {
      const std::uint32_t sel = g.u32() % 7u;
      std::uint32_t res = 0, out = 0;
      if (sel <= 1) {
        const std::uint32_t nbits = g.u32() % 34u;
        const std::uint32_t value = g.u32();
        std::uint32_t opos = pos;
        const bool want = o.write(cap, &opos, value, nbits);
        const bool got = write_bits(b.data(), cap, &pos, value, nbits);
        if (got != want || pos != opos) ++bad;
        res = got ? 1u : 0u;
        out = value;
        if (got) ++wok;
        else ++wfail;
      } else if (sel <= 3) {
        const std::uint32_t nbits = g.u32() % 34u;
        std::uint32_t opos = pos, oout = kNoOut;
        out = kNoOut;
        const bool want = o.read(cap, &opos, nbits, &oout);
        const bool got = read_bits(b.data(), cap, &pos, nbits, &out);
        if (got != want || pos != opos || out != oout) ++bad;
        res = got ? 1u : 0u;
        if (got) ++rok;
        else ++rfail;
      } else if (sel == 4) {
        const std::uint32_t before = pos;
        const bool got = align_to_byte(cap, &pos);
        if (got != (before <= 8u * cap) || (got && pos != (before + 7u) / 8u * 8u) || (!got && pos != before)) ++bad;
        res = got ? 1u : 0u;
        if (got) ++aok;
        else ++afail;
      } else if (sel == 5) {
        out = bits_left(cap, &pos);
        if (out != (pos <= 8u * cap ? 8u * cap - pos : 0u)) ++bad;
      } else {
        pos = g.u32() % (8u * cap + 3u);
      }
      if (!o.same(b)) ++bad;
      f.byte(static_cast<std::uint8_t>(sel));
      f.u32(res);
      f.u32(out);
      f.u32(pos);
      f.u32(b.sum());
    }
  }
  CHECK(bad == 0);
  CHECK(wok == kRefStat_rnd_write_ok);
  CHECK(wfail == kRefStat_rnd_write_fail);
  CHECK(rok == kRefStat_rnd_read_ok);
  CHECK(rfail == kRefStat_rnd_read_fail);
  CHECK(aok == kRefStat_rnd_align_ok);
  CHECK(afail == kRefStat_rnd_align_fail);
  CHECK(f.h == kRefDigest_random);
}
