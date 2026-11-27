// Tests for core/checksum. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/checksum_ref.py, an independent Python implementation (bitwise
// CRC without a table, per-byte-modulo Adler; zlib is not used). Regenerate it with: python3 tools/checksum_ref.py
// The tests also carry a naive oracle that shares no code or table with core/checksum/checksum.cpp. The values named "header anchors"
// are the ones written in checksum.h; every other expected value comes from the reference and the oracle.
// Inputs are heap vectors of exactly n bytes, so AddressSanitizer sees any read past the end.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "checksum.h"

using namespace lumora::checksum;

namespace {

// BEGIN GENERATED REFERENCE (tools/checksum_ref.py)
const std::uint32_t kRefKinds = 4;
const std::uint32_t kRefLens[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u, 41u, 42u, 43u, 44u, 45u, 46u, 47u, 48u, 49u, 50u, 51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u, 59u, 60u, 61u, 62u, 63u, 64u, 65u, 66u, 67u, 68u, 69u, 70u, 71u, 72u, 73u, 74u, 75u, 76u, 77u, 78u, 79u, 80u, 127u, 128u, 129u, 255u, 256u, 257u, 1000u, 4096u, 5550u, 5551u, 5552u, 5553u, 5554u, 11103u, 11104u, 11105u, 16656u};
const std::uint32_t kRefLongLens[] = {65521u, 65536u, 70000u};
const std::uint32_t kRefCrcStarts[] = {0x00000000u, 0xFFFFFFFFu, 0x12345678u};
const std::uint32_t kRefAdlerStarts[] = {0x00000001u, 0x00000000u, 0xFFFFFFFFu, 0xFFF0FFF0u, 0x0000FFF1u, 0xFFF10000u};
const std::uint32_t kRefChLens[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u, 100u, 5551u, 5552u, 5553u, 11104u, 11105u, 20000u};
const std::uint32_t kRefChChunks[] = {1u, 2u, 3u, 7u, 64u, 5552u};
const std::uint64_t kRefDigest_crc_single = 0x9C89F12D985316C5ull;
const std::size_t kRefCases_crc_single = 256;
const std::uint64_t kRefDigest_crc_main = 0x4023F9E1F7B83C46ull;
const std::size_t kRefCases_crc_main = 1182;
const std::uint64_t kRefDigest_crc_chunk = 0xBDE3A47BE021CA29ull;
const std::size_t kRefCases_crc_chunk = 576;
const std::uint64_t kRefDigest_adler_single = 0x5C5CA533A82EFA2Dull;
const std::size_t kRefCases_adler_single = 256;
const std::uint64_t kRefDigest_adler_main = 0x93099B3B9A3B6F86ull;
const std::size_t kRefCases_adler_main = 2358;
const std::uint64_t kRefDigest_adler_chunk = 0xD531438BDC2816C9ull;
const std::size_t kRefCases_adler_chunk = 576;
struct RefVector { const char* text; std::uint32_t crc; std::uint32_t adler; };
const RefVector kRefVectors[] = {
    {"123456789", 0xCBF43926u, 0x091E01DEu},
    {"Wikipedia", 0xADAAC02Eu, 0x11E60398u},
    {"The quick brown fox jumps over the lazy dog", 0x414FA339u, 0x5BDC0FDAu},
    {"a", 0xE8B7BE43u, 0x00620062u},
    {"abc", 0x352441C2u, 0x024D0127u},
};
// END GENERATED REFERENCE

struct Fnv {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t n = 0;
  void u32(std::uint32_t v) {
    ++n;
    for (int i = 0; i < 4; ++i) h = (h ^ ((v >> (8 * i)) & 0xFFu)) * 0x100000001B3ull;
  }
};

std::vector<std::uint8_t> make_data(std::uint32_t kind, std::uint32_t n) {
  std::vector<std::uint8_t> d(n);
  if (kind == 0) {
    for (auto& b : d) b = 0xFF;
  } else if (kind == 2) {
    std::uint64_t x = static_cast<std::uint64_t>(n) * 2654435761ull + 7ull;
    for (auto& b : d) {
      x = x * 6364136223846793005ull + 1442695040888963407ull;
      b = static_cast<std::uint8_t>((x >> 32) & 0xFFu);
    }
  } else if (kind == 3) {
    for (std::uint32_t i = 0; i < n; ++i) d[i] = static_cast<std::uint8_t>((i * 131u + n) & 0xFFu);
  }
  return d;
}

// naive oracle: bit-at-a-time CRC, per-byte modulo Adler in 64 bits
std::uint32_t oracle_crc(std::uint32_t crc, const std::vector<std::uint8_t>& d, std::size_t from, std::size_t to) {
  std::uint32_t c = ~crc;
  for (std::size_t i = from; i < to; ++i) {
    c ^= d[i];
    for (int k = 0; k < 8; ++k) {
      if ((c & 1u) != 0u) {
        c = (c >> 1) ^ 0xEDB88320u;
      } else {
        c >>= 1;
      }
    }
  }
  return ~c;
}

std::uint32_t oracle_adler(std::uint32_t adler, const std::vector<std::uint8_t>& d, std::size_t from, std::size_t to) {
  std::uint64_t s1 = (adler & 0xFFFFu) % 65521u;
  std::uint64_t s2 = (adler >> 16) % 65521u;
  for (std::size_t i = from; i < to; ++i) {
    s1 = (s1 + d[i]) % 65521u;
    s2 = (s2 + s1) % 65521u;
  }
  return static_cast<std::uint32_t>((s2 << 16) | s1);
}

std::vector<std::uint8_t> bytes_of(const char* s) {
  std::vector<std::uint8_t> v;
  for (; *s != '\0'; ++s) v.push_back(static_cast<std::uint8_t>(*s));
  return v;
}

const std::uint8_t* ptr(const std::vector<std::uint8_t>& v, std::size_t off) { return v.empty() ? nullptr : v.data() + off; }

std::uint32_t crc_chain(std::uint32_t start, const std::vector<std::uint8_t>& d, std::size_t chunk) {
  std::uint32_t v = start;
  for (std::size_t i = 0; i < d.size(); i += chunk) {
    const std::size_t m = d.size() - i < chunk ? d.size() - i : chunk;
    v = crc32_update(v, d.data() + i, static_cast<std::uint32_t>(m));
  }
  return v;
}

std::uint32_t adler_chain(std::uint32_t start, const std::vector<std::uint8_t>& d, std::size_t chunk) {
  std::uint32_t v = start;
  for (std::size_t i = 0; i < d.size(); i += chunk) {
    const std::size_t m = d.size() - i < chunk ? d.size() - i : chunk;
    v = adler32_update(v, d.data() + i, static_cast<std::uint32_t>(m));
  }
  return v;
}

bool valid_adler(std::uint32_t a) { return (a & 0xFFFFu) < 65521u && (a >> 16) < 65521u; }

}  // namespace

TEST_CASE("header anchors", "[checksum]") {
  const auto nine = bytes_of("123456789");
  CHECK(crc32(nine.data(), 9) == 0xCBF43926u);
  CHECK(crc32(nullptr, 0) == 0u);
  CHECK(adler32(nullptr, 0) == 1u);
  const auto wiki = bytes_of("Wikipedia");
  CHECK(adler32(wiki.data(), 9) == 0x11E60398u);
}

TEST_CASE("reference vectors (from tools/checksum_ref.py, not from the header)", "[checksum]") {
  std::size_t bad = 0;
  for (const RefVector& v : kRefVectors) {
    const auto d = bytes_of(v.text);
    const auto n = static_cast<std::uint32_t>(d.size());
    if (crc32(d.data(), n) != v.crc || adler32(d.data(), n) != v.adler) ++bad;
    if (oracle_crc(0, d, 0, d.size()) != v.crc || oracle_adler(1, d, 0, d.size()) != v.adler) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("null data and zero length", "[checksum]") {
  const std::uint32_t starts[] = {0u, 1u, 0xCBF43926u, 0xFFFFFFFFu};
  const std::uint8_t one[1] = {0x55};
  for (std::uint32_t s : starts) {
    CHECK(crc32_update(s, nullptr, 5) == s);
    CHECK(crc32_update(s, nullptr, 0xFFFFFFFFu) == s);
    CHECK(crc32_update(s, nullptr, 0) == s);
    CHECK(crc32_update(s, one, 0) == s);
  }
  CHECK(crc32(nullptr, 5) == 0u);
  CHECK(crc32(one, 0) == 0u);
  CHECK(adler32(nullptr, 5) == 1u);
  CHECK(adler32(nullptr, 0xFFFFFFFFu) == 1u);
  CHECK(adler32(one, 0) == 1u);
  CHECK(adler32_update(0x11E60398u, nullptr, 7) == 0x11E60398u);
  CHECK(adler32_update(0x11E60398u, one, 0) == 0x11E60398u);
}

TEST_CASE("one-shot functions equal update from 0 and 1", "[checksum]") {
  const auto d = make_data(2, 300);
  CHECK(crc32(d.data(), 300) == crc32_update(0, d.data(), 300));
  CHECK(adler32(d.data(), 300) == adler32_update(1, d.data(), 300));
}

TEST_CASE("adler start values with a half of 65521 or more are reduced", "[checksum]") {
  // hand-written: 65535 mod 65521 = 14; 65521 mod 65521 = 0; 65520 stays
  CHECK(adler32_update(0xFFFFFFFFu, nullptr, 0) == 0x000E000Eu);
  CHECK(adler32_update(0xFFFFFFFFu, nullptr, 9) == 0x000E000Eu);
  CHECK(adler32_update(65521u, nullptr, 0) == 0u);
  CHECK(adler32_update(65521u << 16, nullptr, 0) == 0u);
  CHECK(adler32_update(65520u | (65520u << 16), nullptr, 0) == (65520u | (65520u << 16)));
  CHECK(adler32_update(0u, nullptr, 0) == 0u);
  // reduction applies before the bytes: start 65521 behaves like start 0, one 0x01 byte gives s1 = 1, s2 = 1
  const std::uint8_t b = 1;
  CHECK(adler32_update(65521u, &b, 1) == 0x00010001u);
  CHECK(adler32_update(65526u, &b, 1) == 0x00060006u);  // low half 65526 mod 65521 = 5, then + 1
}

TEST_CASE("crc start values are any uint32 and are evaluated literally", "[checksum]") {
  const auto d = make_data(2, 50);
  std::size_t bad = 0;
  const std::uint32_t starts[] = {0u, 1u, 0x80000000u, 0xFFFFFFFFu, 0xDEADBEEFu};
  for (std::uint32_t s : starts) {
    if (crc32_update(s, d.data(), 50) != oracle_crc(s, d, 0, 50)) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("single bytes cover every table entry", "[checksum][sweep]") {
  Fnv fc, fa;
  std::size_t bad = 0;
  for (std::uint32_t b = 0; b < 256; ++b) {
    const std::vector<std::uint8_t> d(1, static_cast<std::uint8_t>(b));
    const std::uint32_t c = crc32_update(0, d.data(), 1);
    const std::uint32_t a = adler32_update(1, d.data(), 1);
    if (c != oracle_crc(0, d, 0, 1) || a != oracle_adler(1, d, 0, 1)) ++bad;
    fc.u32(c);
    fa.u32(a);
  }
  CHECK(bad == 0);
  CHECK(fc.n == kRefCases_crc_single);
  CHECK(fc.h == kRefDigest_crc_single);
  CHECK(fa.n == kRefCases_adler_single);
  CHECK(fa.h == kRefDigest_adler_single);
}

TEST_CASE("lengths, kinds and start values match the oracle and the reference digest", "[checksum][sweep]") {
  Fnv fc, fa;
  std::size_t bad = 0;
  auto one = [&](std::uint32_t kind, std::uint32_t n, bool all_starts) {
    const auto d = make_data(kind, n);
    const std::size_t nc = all_starts ? sizeof kRefCrcStarts / sizeof kRefCrcStarts[0] : 1;
    const std::size_t na = all_starts ? sizeof kRefAdlerStarts / sizeof kRefAdlerStarts[0] : 1;
    for (std::size_t i = 0; i < nc; ++i) {
      const std::uint32_t c = crc32_update(kRefCrcStarts[i], ptr(d, 0), n);
      if (c != oracle_crc(kRefCrcStarts[i], d, 0, d.size())) ++bad;
      fc.u32(c);
    }
    for (std::size_t i = 0; i < na; ++i) {
      const std::uint32_t a = adler32_update(kRefAdlerStarts[i], ptr(d, 0), n);
      if (a != oracle_adler(kRefAdlerStarts[i], d, 0, d.size()) || !valid_adler(a)) ++bad;
      fa.u32(a);
    }
  };
  for (std::uint32_t kind = 0; kind < kRefKinds; ++kind)
    for (std::uint32_t n : kRefLens) one(kind, n, true);
  for (std::uint32_t kind : {0u, 2u})
    for (std::uint32_t n : kRefLongLens) one(kind, n, false);
  CHECK(bad == 0);
  CHECK(fc.n == kRefCases_crc_main);
  CHECK(fc.h == kRefDigest_crc_main);
  CHECK(fa.n == kRefCases_adler_main);
  CHECK(fa.h == kRefDigest_adler_main);
}

TEST_CASE("deferred modulo at the block boundary: 0xFF data, worst-case start", "[checksum][sweep]") {
  // lengths around 5552 and its multiples, all-0xFF bytes, both sums starting at 65520 (the largest valid value)
  const std::uint32_t lens[] = {5551u, 5552u, 5553u, 11103u, 11104u, 11105u, 16655u, 16656u, 16657u, 22207u, 22208u, 22209u};
  std::size_t bad = 0;
  for (std::uint32_t n : lens) {
    const auto d = make_data(0, n);
    const std::uint32_t starts[] = {1u, 65520u | (65520u << 16), 65520u, 65520u << 16};
    for (std::uint32_t s : starts) {
      if (adler32_update(s, d.data(), n) != oracle_adler(s, d, 0, d.size())) ++bad;
    }
    if (crc32(d.data(), n) != oracle_crc(0, d, 0, d.size())) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("every split point gives the one-shot value", "[checksum][sweep]") {
  std::size_t bad = 0;
  for (std::uint32_t kind : {2u, 0u, 3u}) {
    for (std::uint32_t n = 0; n <= 80; ++n) {
      const auto d = make_data(kind, n);
      const std::uint32_t c = crc32(ptr(d, 0), n);
      const std::uint32_t a = adler32(ptr(d, 0), n);
      for (std::uint32_t k = 0; k <= n; ++k) {
        const std::uint32_t c1 = crc32_update(0, ptr(d, 0), k);
        const std::uint32_t c2 = crc32_update(c1, ptr(d, k), n - k);
        const std::uint32_t a1 = adler32_update(1, ptr(d, 0), k);
        const std::uint32_t a2 = adler32_update(a1, ptr(d, k), n - k);
        if (c2 != c || a2 != a) ++bad;
      }
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("split at the block size", "[checksum][sweep]") {
  const auto d = make_data(0, 11106);
  const std::uint32_t splits[] = {0u, 1u, 5551u, 5552u, 5553u, 11103u, 11104u, 11105u, 11106u};
  std::size_t bad = 0;
  const std::uint32_t a = adler32(d.data(), 11106);
  const std::uint32_t c = crc32(d.data(), 11106);
  for (std::uint32_t k : splits) {
    const std::uint32_t a2 = adler32_update(adler32_update(1, ptr(d, 0), k), ptr(d, k), 11106 - k);
    const std::uint32_t c2 = crc32_update(crc32_update(0, ptr(d, 0), k), ptr(d, k), 11106 - k);
    if (a2 != a || c2 != c) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("many parts, empty parts and null zero-length parts", "[checksum][sweep]") {
  std::uint64_t x = 99;
  auto rnd = [&x]() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  };
  std::size_t bad = 0;
  for (int t = 0; t < 400; ++t) {
    const std::uint32_t n = rnd() % 400u;
    const auto d = make_data(2, n);
    std::uint32_t c = 0, a = 1;
    std::uint32_t pos = 0;
    while (pos < n) {
      const std::uint32_t r = rnd() % 5u;
      if (r == 0) {  // empty part with a real pointer
        c = crc32_update(c, ptr(d, pos), 0);
        a = adler32_update(a, ptr(d, pos), 0);
        continue;
      }
      if (r == 1) {  // empty part with a null pointer
        c = crc32_update(c, nullptr, 0);
        a = adler32_update(a, nullptr, 0);
        continue;
      }
      std::uint32_t m = 1u + rnd() % 60u;
      if (m > n - pos) m = n - pos;
      c = crc32_update(c, ptr(d, pos), m);
      a = adler32_update(a, ptr(d, pos), m);
      pos += m;
    }
    if (c != crc32(ptr(d, 0), n) || a != adler32(ptr(d, 0), n)) ++bad;
    if (c != oracle_crc(0, d, 0, d.size()) || a != oracle_adler(1, d, 0, d.size())) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("chunked streaming matches the reference digest", "[checksum][sweep]") {
  Fnv fc, fa;
  std::size_t bad = 0;
  for (std::uint32_t kind : {2u, 0u}) {
    for (std::uint32_t n : kRefChLens) {
      const auto d = make_data(kind, n);
      const std::uint32_t whole_c = crc32(ptr(d, 0), n);
      const std::uint32_t whole_a = adler32(ptr(d, 0), n);
      for (std::uint32_t chunk : kRefChChunks) {
        const std::uint32_t c = crc_chain(0, d, chunk);
        const std::uint32_t a = adler_chain(1, d, chunk);
        if (c != whole_c || a != whole_a) ++bad;
        fc.u32(c);
        fa.u32(a);
      }
    }
  }
  CHECK(bad == 0);
  CHECK(fc.n == kRefCases_crc_chunk);
  CHECK(fc.h == kRefDigest_crc_chunk);
  CHECK(fa.n == kRefCases_adler_chunk);
  CHECK(fa.h == kRefDigest_adler_chunk);
}

TEST_CASE("adler results are always valid values", "[checksum][sweep]") {
  std::size_t bad = 0;
  const std::uint32_t starts[] = {0u, 1u, 65520u, 65521u, 65522u, 0xFFFFu, 0x10000u, 0xFFFF0000u, 0xFFFFFFFFu, 0x7FFFFFFFu};
  const auto d = make_data(0, 6000);
  for (std::uint32_t s : starts) {
    for (std::uint32_t n : {0u, 1u, 5552u, 6000u}) {
      if (!valid_adler(adler32_update(s, ptr(d, 0), n))) ++bad;
    }
    if (!valid_adler(adler32_update(s, nullptr, 3))) ++bad;
  }
  CHECK(bad == 0);
}
