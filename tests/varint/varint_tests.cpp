// Tests for core/varint. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/varint_ref.py, an independent Python
// implementation (7-bit groups from the binary string, a scan for the first byte without the continuation bit). Regenerate:
// python3 tools/varint_ref.py
// The tests also carry a naive oracle (division and multiplication by 128, complement for zigzag) that shares no code with
// core/varint/varint.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "varint.h"

using namespace lumora::varint;

namespace {

// BEGIN GENERATED REFERENCE (tools/varint_ref.py)
const std::uint64_t kRefV[] = {0ull, 1ull, 2ull, 126ull, 127ull, 128ull, 129ull, 255ull, 256ull, 16383ull, 16384ull, 16385ull, 2097151ull, 2097152ull, 2097153ull, 268435455ull, 268435456ull, 268435457ull, 34359738367ull, 34359738368ull, 34359738369ull, 4398046511103ull, 4398046511104ull, 4398046511105ull, 562949953421311ull, 562949953421312ull, 562949953421313ull, 72057594037927935ull, 72057594037927936ull, 72057594037927937ull, 9223372036854775807ull, 9223372036854775808ull, 9223372036854775809ull, 4294967295ull, 4294967296ull, 4294967297ull, 9223372036854775807ull, 9223372036854775808ull, 9223372036854775809ull, 18446744073709551614ull, 18446744073709551615ull};
const std::int64_t kRefZ[] = {0ll, 1ll, -1ll, 2ll, -2ll, 63ll, -63ll, 64ll, -64ll, 65ll, -65ll, INT64_MIN, -9223372036854775807ll, 9223372036854775807ll, 9223372036854775806ll, 128ll, 127ll, 129ll, -128ll, -129ll, -127ll, 16384ll, 16383ll, 16385ll, -16384ll, -16385ll, -16383ll, 2097152ll, 2097151ll, 2097153ll, -2097152ll, -2097153ll, -2097151ll, 268435456ll, 268435455ll, 268435457ll, -268435456ll, -268435457ll, -268435455ll, 34359738368ll, 34359738367ll, 34359738369ll, -34359738368ll, -34359738369ll, -34359738367ll, 4398046511104ll, 4398046511103ll, 4398046511105ll, -4398046511104ll, -4398046511105ll, -4398046511103ll, 562949953421312ll, 562949953421311ll, 562949953421313ll, -562949953421312ll, -562949953421313ll, -562949953421311ll, 72057594037927936ll, 72057594037927935ll, 72057594037927937ll, -72057594037927936ll, -72057594037927937ll, -72057594037927935ll};
const std::uint8_t kRefAlpha[] = {0x00, 0x01, 0x7F, 0x80, 0x81, 0xFF};
const std::uint32_t kRefLongN[] = {9u, 10u, 11u, 12u};
const std::uint8_t kRefLongP[] = {0x80, 0xFF, 0xA5};
const std::uint8_t kRefLongT[] = {0x00, 0x01, 0x02, 0x7F, 0x80, 0xFF};
const std::size_t kRefRandN = 20000;
const std::uint64_t kRefDigest_encode = 0xA1C6FF3DD8EB0793ull;
const std::size_t kRefCases_encode = 22042;
const std::uint64_t kRefDigest_decode = 0xCE66955AC0871CBCull;
const std::size_t kRefCases_decode = 66210;
const std::uint64_t kRefDigest_decode_rt = 0x9F40B54C4370749Bull;
const std::size_t kRefCases_decode_rt = 22042;
const std::uint64_t kRefDigest_zigzag = 0x69EAB1499871B4EEull;
const std::size_t kRefCases_zigzag = 46106;
const std::uint64_t kRefDigest_signed = 0x07F9557340658085ull;
const std::size_t kRefCases_signed = 24064;
// END GENERATED REFERENCE

constexpr std::uint64_t kLcgMul = 6364136223846793005ull;
constexpr std::uint64_t kLcgAdd = 1442695040888963407ull;

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void u64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint64_t next() {
    x = x * kLcgMul + kLcgAdd;
    return x;
  }
  std::uint64_t u64() {
    const std::uint64_t r = next();
    return r >> ((r >> 1) & 63u);
  }
  std::int64_t s64() {
    const std::uint64_t r = next();
    const std::uint64_t mag = r >> ((r >> 1) & 63u);
    const bool neg = ((r >> 9) & 1u) != 0;
    return static_cast<std::int64_t>(neg ? std::uint64_t{0} - mag : mag);
  }
};

// ---- naive oracle -------------------------------------------------------------------------------------------------
std::vector<std::uint8_t> o_encode(std::uint64_t v) {
  std::vector<std::uint8_t> out;
  for (;;) {
    const std::uint8_t group = static_cast<std::uint8_t>(v % 128u);
    v /= 128u;
    if (v == 0) {
      out.push_back(group);
      return out;
    }
    out.push_back(static_cast<std::uint8_t>(group + 128u));
  }
}

struct Dec {
  bool ok = false;
  std::uint64_t value = 0;
  std::uint32_t used = 0;
};

Dec o_decode(const std::uint8_t* in, std::uint32_t length) {
  Dec d;
  std::uint32_t end = 0;
  bool found = false;
  for (std::uint32_t i = 0; i < length && i < 10; ++i) {
    if (in[i] < 128) {
      end = i;
      found = true;
      break;
    }
  }
  if (!found) return d;
  if (end == 9 && in[9] > 1) return d;
  if (end > 0 && in[end] == 0) return d;
  std::uint64_t mult = 1;
  std::uint64_t value = 0;
  for (std::uint32_t i = 0; i <= end; ++i) {
    value += static_cast<std::uint64_t>(in[i] % 128u) * mult;
    if (i < 9) mult *= 128u;
  }
  d.ok = true;
  d.value = value;
  d.used = end + 1;
  return d;
}

std::uint64_t o_zz(std::int64_t v) {
  const std::uint64_t u = static_cast<std::uint64_t>(v);
  return v >= 0 ? u + u : static_cast<std::uint64_t>(~v) * 2u + 1u;
}

std::int64_t o_unzz(std::uint64_t u) {
  const std::int64_t h = static_cast<std::int64_t>(u / 2u);
  return (u % 2u == 0) ? h : ~h;
}

std::vector<std::uint64_t> all_values() {
  std::vector<std::uint64_t> vals(std::begin(kRefV), std::end(kRefV));
  for (std::uint64_t v = 0; v <= 2000; ++v) vals.push_back(v);
  Lcg g(21);
  for (std::size_t i = 0; i < kRefRandN; ++i) vals.push_back(g.u64());
  return vals;
}

std::vector<std::int64_t> all_signed() {
  std::vector<std::int64_t> zs(std::begin(kRefZ), std::end(kRefZ));
  for (std::int64_t z = -2000; z <= 2000; ++z) zs.push_back(z);
  Lcg g(22);
  for (std::size_t i = 0; i < kRefRandN; ++i) zs.push_back(g.s64());
  return zs;
}

// Runs decode and compares with the oracle; feeds the digest the way the reference does. Returns false on a mismatch.
bool decode_case(Fnv& f, const std::uint8_t* data, std::uint32_t length) {
  std::uint64_t value = 0x1111222233334444ull;
  std::uint32_t consumed = 0x5555u;
  const bool ok = decode(data, length, &value, &consumed);
  const Dec want = o_decode(data, length);
  bool same = ok == want.ok;
  if (ok) {
    same = same && value == want.value && consumed == want.used;
    f.byte(1);
    f.byte(static_cast<std::uint8_t>(consumed));
    f.u64(value);
    // canonical: re-encoding gives the same bytes
    const std::vector<std::uint8_t> re = o_encode(value);
    same = same && re.size() == consumed;
    for (std::size_t i = 0; same && i < re.size(); ++i) same = re[i] == data[i];
  } else {
    same = same && value == 0x1111222233334444ull && consumed == 0x5555u;
    f.byte(0);
  }
  return same;
}

}  // namespace

TEST_CASE("constants", "[varint]") { CHECK(kMaxBytes == 10); }

TEST_CASE("encoded_size boundaries", "[varint][size]") {
  CHECK(encoded_size(0) == 1);
  CHECK(encoded_size(1) == 1);
  CHECK(encoded_size(127) == 1);
  CHECK(encoded_size(128) == 2);
  CHECK(encoded_size(16383) == 2);
  CHECK(encoded_size(16384) == 3);
  CHECK(encoded_size(UINT64_MAX) == 10);
  CHECK(encoded_size(1ull << 63) == 10);
  CHECK(encoded_size((1ull << 63) - 1) == 9);
  for (std::uint32_t k = 1; k <= 9; ++k) {
    CHECK(encoded_size((1ull << (7 * k)) - 1) == k);
    CHECK(encoded_size(1ull << (7 * k)) == k + 1);
  }
}

TEST_CASE("encode known bytes", "[varint][encode]") {
  struct Case {
    std::uint64_t v;
    std::vector<std::uint8_t> bytes;
  };
  const Case cases[] = {
      {0, {0x00}},
      {1, {0x01}},
      {127, {0x7F}},
      {128, {0x80, 0x01}},
      {300, {0xAC, 0x02}},
      {16383, {0xFF, 0x7F}},
      {16384, {0x80, 0x80, 0x01}},
      {UINT64_MAX, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}},
      {1ull << 63, {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}},
      {(1ull << 63) - 1, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}},
  };
  for (const Case& c : cases) {
    std::uint8_t buf[16];
    for (std::uint8_t& b : buf) b = 0xEE;
    std::uint32_t written = 0;
    REQUIRE(encode(c.v, buf, 16, &written));
    CHECK(written == c.bytes.size());
    for (std::size_t i = 0; i < c.bytes.size(); ++i) CHECK(buf[i] == c.bytes[i]);
    for (std::size_t i = c.bytes.size(); i < 16; ++i) CHECK(buf[i] == 0xEE);  // beyond the encoding: untouched
  }
}

TEST_CASE("encode failures write nothing and keep *written", "[varint][encode]") {
  std::uint8_t buf[12];
  std::uint32_t written = 77;
  for (std::uint8_t& b : buf) b = 0xAB;
  CHECK_FALSE(encode(5, nullptr, 12, &written));
  CHECK_FALSE(encode(5, buf, 12, nullptr));
  CHECK_FALSE(encode(5, nullptr, 12, nullptr));
  CHECK_FALSE(encode(5, buf, 0, &written));
  CHECK_FALSE(encode(128, buf, 1, &written));
  CHECK_FALSE(encode(UINT64_MAX, buf, 9, &written));
  CHECK_FALSE(encode(1ull << 63, buf, 9, &written));
  CHECK_FALSE(encode(16384, buf, 2, &written));
  CHECK(written == 77);
  for (std::uint8_t b : buf) CHECK(b == 0xAB);
  // exact capacity succeeds
  CHECK(encode(UINT64_MAX, buf, 10, &written));
  CHECK(written == 10);
  CHECK(buf[10] == 0xAB);
  CHECK(encode(0, buf, 1, &written));
  CHECK(written == 1);
  CHECK(buf[1] == 0xFF);  // byte 1 was written by the 10-byte encoding above, 0xFF: untouched by the 1-byte one
}

TEST_CASE("decode known values", "[varint][decode]") {
  std::uint64_t value = 9;
  std::uint32_t consumed = 9;
  const std::uint8_t a[] = {0x00};
  CHECK(decode(a, 1, &value, &consumed));
  CHECK(value == 0);
  CHECK(consumed == 1);
  const std::uint8_t b[] = {0xAC, 0x02, 0x55};
  CHECK(decode(b, 3, &value, &consumed));  // trailing byte is not read into the value
  CHECK(value == 300);
  CHECK(consumed == 2);
  const std::uint8_t c[] = {0x80, 0x01};  // zero payload byte in the middle is canonical
  CHECK(decode(c, 2, &value, &consumed));
  CHECK(value == 128);
  const std::uint8_t d[] = {0x80, 0x80, 0x01};
  CHECK(decode(d, 3, &value, &consumed));
  CHECK(value == 16384);
  CHECK(consumed == 3);
  const std::uint8_t e[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  CHECK(decode(e, 10, &value, &consumed));
  CHECK(value == UINT64_MAX);
  CHECK(consumed == 10);
  const std::uint8_t f[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01, 0x80, 0x80};
  CHECK(decode(f, 12, &value, &consumed));  // after the 10th byte nothing is read
  CHECK(value == (1ull << 63));
  CHECK(consumed == 10);
}

TEST_CASE("decode failures leave value and consumed alone", "[varint][decode]") {
  struct Bad {
    std::vector<std::uint8_t> bytes;
    std::uint32_t length;
  };
  const std::vector<Bad> bad = {
      {{0x00}, 0},                                                           // length 0
      {{0x80}, 1},                                                           // truncated
      {{0xFF, 0xFF}, 2},                                                     // truncated
      {{0x80, 0x00}, 2},                                                     // padding
      {{0xFF, 0x00}, 2},                                                     // padding
      {{0x80, 0x80, 0x00}, 3},                                               // padding
      {{0x80, 0x01}, 1},                                                     // complete only past the length
      {{0xAC, 0x02}, 1},                                                     // complete only past the length
      {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}, 9},    // 10th byte past the length
      {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02}, 10},   // bits beyond 64
      {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}, 10},   // bits beyond 64
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 10},   // 10th byte 0: padding
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}, 10},   // ten continuation bytes, length 10
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 11},  // longer than 10
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 11},
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 4000000000u},
      {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x81, 0x01}, 11},
  };
  std::size_t wrong = 0;
  for (const Bad& c : bad) {
    std::uint64_t value = 0x0123456789ABCDEFull;
    std::uint32_t consumed = 0xDEADu;
    std::vector<std::uint8_t> buf = c.bytes;  // 11 bytes at most: the decoder reads ten
    if (decode(buf.data(), c.length, &value, &consumed)) ++wrong;  // a huge length still reads at most ten bytes
    if (value != 0x0123456789ABCDEFull || consumed != 0xDEADu) ++wrong;
  }
  CHECK(wrong == 0);
  std::uint64_t value = 5;
  std::uint32_t consumed = 6;
  const std::uint8_t one[] = {0x05};
  CHECK_FALSE(decode(nullptr, 1, &value, &consumed));
  CHECK_FALSE(decode(one, 1, nullptr, &consumed));
  CHECK_FALSE(decode(one, 1, &value, nullptr));
  CHECK_FALSE(decode(nullptr, 0, nullptr, nullptr));
  CHECK(value == 5);
  CHECK(consumed == 6);
}

TEST_CASE("decode never reads past length or past ten bytes", "[varint][decode]") {
  // A heap buffer of exactly `length` bytes: a read past it is an AddressSanitizer error, and the bytes after the
  // window below would complete the encoding if they were read.
  std::size_t wrong = 0;
  for (std::uint32_t n = 1; n <= 12; ++n) {
    std::vector<std::uint8_t> buf(n, 0x80);  // all continuation: no encoding ends inside
    std::uint64_t value = 1;
    std::uint32_t consumed = 2;
    if (decode(buf.data(), n, &value, &consumed)) ++wrong;
    if (value != 1 || consumed != 2) ++wrong;
  }
  CHECK(wrong == 0);
  // bytes after `length` that would end the encoding: not used
  const std::uint8_t tail[] = {0x80, 0x80, 0x01, 0x01};
  for (std::uint32_t len = 1; len <= 2; ++len) {
    std::uint64_t value = 1;
    std::uint32_t consumed = 2;
    CHECK_FALSE(decode(tail, len, &value, &consumed));
  }
  std::uint64_t value = 1;
  std::uint32_t consumed = 2;
  CHECK(decode(tail, 3, &value, &consumed));
  CHECK(value == 16384);
  // A valid 10-byte encoding followed by more bytes: only ten bytes are used.
  std::vector<std::uint8_t> ten(10, 0xFF);
  ten[9] = 0x01;
  CHECK(decode(ten.data(), 10, &value, &consumed));
  CHECK(consumed == 10);
}

TEST_CASE("round trip and canonical form", "[varint][roundtrip]") {
  std::size_t bad = 0;
  for (std::uint64_t v : all_values()) {
    std::uint8_t buf[10];
    std::uint32_t written = 0;
    if (!encode(v, buf, 10, &written)) ++bad;
    const std::vector<std::uint8_t> want = o_encode(v);
    if (written != want.size() || written != encoded_size(v)) ++bad;
    for (std::uint32_t i = 0; i < written; ++i)
      if (buf[i] != want[i]) ++bad;
    std::uint64_t value = 0;
    std::uint32_t consumed = 0;
    if (!decode(buf, written, &value, &consumed) || value != v || consumed != written) ++bad;
    // every shorter length is a truncation
    for (std::uint32_t len = 0; len < written; ++len)
      if (decode(buf, len, &value, &consumed)) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("encode digest", "[varint][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint64_t v : all_values()) {
    std::uint8_t buf[10];
    std::uint32_t written = 0;
    if (!encode(v, buf, 10, &written)) ++bad;
    f.byte(static_cast<std::uint8_t>(written));
    for (std::uint32_t i = 0; i < written; ++i) f.byte(buf[i]);
    ++n;
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_encode);
  CHECK(f.h == kRefDigest_encode);
}

TEST_CASE("decode digest over every short input and long patterns", "[varint][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint32_t b0 = 0; b0 < 256; ++b0) {
    const std::uint8_t d[1] = {static_cast<std::uint8_t>(b0)};
    if (!decode_case(f, d, 1)) ++bad;
    ++n;
  }
  for (std::uint32_t b0 = 0; b0 < 256; ++b0)
    for (std::uint32_t b1 = 0; b1 < 256; ++b1) {
      const std::uint8_t d[2] = {static_cast<std::uint8_t>(b0), static_cast<std::uint8_t>(b1)};
      if (!decode_case(f, d, 2)) ++bad;
      ++n;
    }
  for (std::uint8_t a : kRefAlpha)
    for (std::uint8_t b : kRefAlpha)
      for (std::uint8_t c : kRefAlpha) {
        const std::uint8_t d[3] = {a, b, c};
        if (!decode_case(f, d, 3)) ++bad;
        ++n;
      }
  for (std::uint32_t ln : kRefLongN)
    for (std::uint8_t p : kRefLongP)
      for (std::uint8_t t : kRefLongT) {
        std::vector<std::uint8_t> data(ln, p);
        data[ln - 1] = t;
        for (std::uint32_t length : {ln, 10u, 9u}) {
          if (length > ln) continue;  // never ask for more bytes than the buffer holds
          if (!decode_case(f, data.data(), length)) ++bad;
          ++n;
        }
      }
  const std::uint8_t three[3] = {0x80, 0x01, 0x05};
  for (std::uint32_t length = 0; length < 4; ++length) {
    // length 0 passes a valid pointer: the buffer is never read
    if (!decode_case(f, three, length > 3 ? 3 : length)) ++bad;
    ++n;
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_decode);
  CHECK(f.h == kRefDigest_decode);
}

TEST_CASE("decode digest of encodings followed by other bytes", "[varint][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint64_t v : all_values()) {
    std::vector<std::uint8_t> e = o_encode(v);
    e.push_back(0x80);
    e.push_back(0x80);
    e.push_back(0x03);
    if (!decode_case(f, e.data(), static_cast<std::uint32_t>(e.size()))) ++bad;
    ++n;
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_decode_rt);
  CHECK(f.h == kRefDigest_decode_rt);
}

TEST_CASE("zigzag known values", "[varint][zigzag]") {
  CHECK(zigzag_encode(0) == 0);
  CHECK(zigzag_encode(-1) == 1);
  CHECK(zigzag_encode(1) == 2);
  CHECK(zigzag_encode(-2) == 3);
  CHECK(zigzag_encode(2) == 4);
  CHECK(zigzag_encode(INT64_MAX) == UINT64_MAX - 1);
  CHECK(zigzag_encode(INT64_MIN) == UINT64_MAX);
  CHECK(zigzag_encode(INT64_MIN + 1) == UINT64_MAX - 2);
  CHECK(zigzag_encode(INT64_MAX - 1) == UINT64_MAX - 3);
  CHECK(zigzag_decode(0) == 0);
  CHECK(zigzag_decode(1) == -1);
  CHECK(zigzag_decode(2) == 1);
  CHECK(zigzag_decode(3) == -2);
  CHECK(zigzag_decode(4) == 2);
  CHECK(zigzag_decode(UINT64_MAX) == INT64_MIN);
  CHECK(zigzag_decode(UINT64_MAX - 1) == INT64_MAX);
  CHECK(zigzag_decode(UINT64_MAX - 2) == INT64_MIN + 1);
  CHECK(zigzag_decode(1ull << 63) == (INT64_MAX / 2 + 1));  // even: u / 2
  CHECK(zigzag_decode((1ull << 63) + 1) == -(INT64_MAX / 2 + 1) - 1);
}

TEST_CASE("zigzag bijection and oracle", "[varint][zigzag]") {
  std::size_t bad = 0;
  for (std::int64_t z : all_signed()) {
    const std::uint64_t u = zigzag_encode(z);
    if (u != o_zz(z)) ++bad;
    if (zigzag_decode(u) != z) ++bad;
    if (z >= 0 && u != static_cast<std::uint64_t>(z) * 2u) ++bad;
    if (z < 0 && (u % 2u) != 1u) ++bad;
  }
  for (std::uint64_t u : all_values()) {
    const std::int64_t z = zigzag_decode(u);
    if (z != o_unzz(u)) ++bad;
    if (zigzag_encode(z) != u) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("zigzag digest", "[varint][sweep]") {
  Fnv f;
  std::size_t n = 0;
  for (std::int64_t z : all_signed()) {
    f.u64(zigzag_encode(z));
    ++n;
  }
  for (std::uint64_t u : all_values()) {
    f.u64(static_cast<std::uint64_t>(zigzag_decode(u)));
    ++n;
  }
  CHECK(n == kRefCases_zigzag);
  CHECK(f.h == kRefDigest_zigzag);
}

TEST_CASE("signed wrappers", "[varint][signed]") {
  std::uint8_t buf[10];
  std::uint32_t written = 99;
  CHECK(encode_signed(-1, buf, 10, &written));
  CHECK(written == 1);
  CHECK(buf[0] == 0x01);
  CHECK(encode_signed(64, buf, 10, &written));
  CHECK(written == 2);
  CHECK(buf[0] == 0x80);
  CHECK(buf[1] == 0x01);
  CHECK(encode_signed(INT64_MIN, buf, 10, &written));
  CHECK(written == 10);
  std::int64_t value = 7;
  std::uint32_t consumed = 7;
  CHECK(decode_signed(buf, 10, &value, &consumed));
  CHECK(value == INT64_MIN);
  CHECK(consumed == 10);
  // failure rules
  written = 99;
  CHECK_FALSE(encode_signed(INT64_MIN, buf, 9, &written));
  CHECK_FALSE(encode_signed(1, nullptr, 9, &written));
  CHECK_FALSE(encode_signed(1, buf, 9, nullptr));
  CHECK(written == 99);
  const std::uint8_t trunc[] = {0x80};
  const std::uint8_t pad[] = {0x80, 0x00};
  const std::uint8_t good[] = {0x03};
  value = 7;
  consumed = 7;
  CHECK_FALSE(decode_signed(trunc, 1, &value, &consumed));
  CHECK_FALSE(decode_signed(pad, 2, &value, &consumed));
  CHECK_FALSE(decode_signed(good, 0, &value, &consumed));
  CHECK_FALSE(decode_signed(nullptr, 1, &value, &consumed));
  CHECK_FALSE(decode_signed(good, 1, nullptr, &consumed));
  CHECK_FALSE(decode_signed(good, 1, &value, nullptr));
  CHECK(value == 7);
  CHECK(consumed == 7);
  CHECK(decode_signed(good, 1, &value, &consumed));
  CHECK(value == -2);
  CHECK(consumed == 1);
}

TEST_CASE("signed digest and round trip", "[varint][signed][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::int64_t z : all_signed()) {
    std::uint8_t buf[10];
    std::uint32_t written = 0;
    if (!encode_signed(z, buf, 10, &written)) ++bad;
    f.byte(static_cast<std::uint8_t>(written));
    for (std::uint32_t i = 0; i < written; ++i) f.byte(buf[i]);
    const std::vector<std::uint8_t> want = o_encode(o_zz(z));
    if (want.size() != written) ++bad;
    for (std::uint32_t i = 0; i < written && i < want.size(); ++i)
      if (buf[i] != want[i]) ++bad;
    std::int64_t value = 0;
    std::uint32_t consumed = 0;
    if (!decode_signed(buf, written, &value, &consumed) || value != z || consumed != written) ++bad;
    ++n;
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_signed);
  CHECK(f.h == kRefDigest_signed);
}
