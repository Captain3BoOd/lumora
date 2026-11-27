// Tests for core/utf16. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/utf16_ref.py, an independent Python implementation (a scan over unit
// classes; the codecs module is not used). Regenerate it with: python3 tools/utf16_ref.py
// The tests also carry a naive oracle that shares no code with core/utf16/utf16.cpp: it recognises surrogates by bit masks and decodes a
// pair from the two 10-bit halves. Units are host-order uint16_t values; there is no byte order handling.
// Inputs are heap buffers of exactly n units, so AddressSanitizer sees any read past the end.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

#include "utf16.h"

using namespace lumora::utf16;

namespace {

// BEGIN GENERATED REFERENCE (tools/utf16_ref.py)
const std::uint32_t kRefAlphabet[] = {0x0000u, 0x0001u, 0x0041u, 0x007Fu, 0x0080u, 0x07FFu, 0x0800u, 0x8000u, 0xD7FEu, 0xD7FFu, 0xD800u, 0xD801u, 0xDBFEu, 0xDBFFu, 0xDC00u, 0xDC01u, 0xDFFEu, 0xDFFFu, 0xE000u, 0xE001u, 0xFEFFu, 0xFFFDu, 0xFFFEu, 0xFFFFu};
const std::uint32_t kRefHighs24[] = {0xD800u, 0xD801u, 0xD802u, 0xD83Du, 0xD87Fu, 0xD880u, 0xD8FFu, 0xD900u, 0xD93Fu, 0xD940u, 0xD9FFu, 0xDA00u, 0xDA3Fu, 0xDA40u, 0xDABFu, 0xDB00u, 0xDB3Fu, 0xDB40u, 0xDB7Fu, 0xDB80u, 0xDBBFu, 0xDBC0u, 0xDBFEu, 0xDBFFu};
const std::uint32_t kRefExtraCps[] = {0x110000u, 0x110001u, 0x1FFFFFu, 0x80000000u, 0xFFFFFFFFu};
const std::uint32_t kRefCapCps[] = {0x0u, 0x41u, 0xD7FFu, 0xD800u, 0xDFFFu, 0xE000u, 0xFFFFu, 0x10000u, 0x10FFFFu, 0x110000u, 0xFFFFFFFFu};
const std::uint32_t kRefCaps[] = {0u, 1u, 2u, 3u};
const std::uint32_t kRefStrMax = 4;
const std::uint64_t kRefDigest_l1 = 0xE16ADDD80F8C4325ull;
const std::size_t kRefCases_l1 = 65536;
const std::uint64_t kRefDigest_hl = 0x5F155325A3E22325ull;
const std::size_t kRefCases_hl = 1048576;
const std::uint64_t kRefDigest_pair = 0x9AC5E345700BA325ull;
const std::size_t kRefCases_pair = 3145728;
const std::uint64_t kRefDigest_h24 = 0x15CA419A344CA325ull;
const std::size_t kRefCases_h24 = 1572864;
const std::uint64_t kRefDigest_str = 0x57A31DBDD59C165Full;
const std::size_t kRefCases_str = 346201;
const std::uint64_t kRefDigest_enc = 0xD1CC0F4DBEAA5D33ull;
const std::size_t kRefCases_enc = 1114117;
const std::uint64_t kRefDigest_cap = 0xCC1E5CB7B2FFC34Bull;
const std::size_t kRefCases_cap = 55;
const std::uint64_t kRefDigest_vec = 0x1576F1A62199F8BBull;
const std::size_t kRefCases_vec = 149;
struct RefVector { std::uint32_t n; std::uint16_t units[16]; std::uint32_t offset; std::uint32_t count; };
const RefVector kRefVectors[] = {
    {0u, {0}, 0u, 0u},
    {1u, {0x0000}, 1u, 1u},
    {1u, {0x0041}, 1u, 1u},
    {1u, {0xD7FF}, 1u, 1u},
    {1u, {0xE000}, 1u, 1u},
    {1u, {0xFFFF}, 1u, 1u},
    {1u, {0xFEFF}, 1u, 1u},
    {1u, {0xFFFE}, 1u, 1u},
    {2u, {0xD800, 0xDC00}, 2u, 1u},
    {2u, {0xDBFF, 0xDFFF}, 2u, 1u},
    {2u, {0xD83D, 0xDE00}, 2u, 1u},
    {2u, {0xDBFF, 0xDC00}, 2u, 1u},
    {2u, {0xD800, 0xDFFF}, 2u, 1u},
    {4u, {0x0041, 0xD83D, 0xDE00, 0x005A}, 4u, 3u},
    {1u, {0xDC00}, 0u, 0u},
    {2u, {0xDC00, 0x005A}, 0u, 0u},
    {4u, {0xDC00, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {2u, {0x0041, 0xDC00}, 1u, 0u},
    {3u, {0x0041, 0xDC00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {4u, {0x0041, 0xD83D, 0xDE00, 0xDC00}, 3u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xDC00, 0x005A}, 3u, 0u},
    {7u, {0x0041, 0xD83D, 0xDE00, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {1u, {0xDFFF}, 0u, 0u},
    {2u, {0xDFFF, 0x005A}, 0u, 0u},
    {4u, {0xDFFF, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {2u, {0x0041, 0xDFFF}, 1u, 0u},
    {3u, {0x0041, 0xDFFF, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xDFFF, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {4u, {0x0041, 0xD83D, 0xDE00, 0xDFFF}, 3u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xDFFF, 0x005A}, 3u, 0u},
    {7u, {0x0041, 0xD83D, 0xDE00, 0xDFFF, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {1u, {0xD800}, 0u, 0u},
    {2u, {0xD800, 0x005A}, 0u, 0u},
    {4u, {0xD800, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {2u, {0x0041, 0xD800}, 1u, 0u},
    {3u, {0x0041, 0xD800, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD800, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {4u, {0x0041, 0xD83D, 0xDE00, 0xD800}, 3u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0x005A}, 3u, 0u},
    {7u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {1u, {0xDBFF}, 0u, 0u},
    {2u, {0xDBFF, 0x005A}, 0u, 0u},
    {4u, {0xDBFF, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {2u, {0x0041, 0xDBFF}, 1u, 0u},
    {3u, {0x0041, 0xDBFF, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xDBFF, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {4u, {0x0041, 0xD83D, 0xDE00, 0xDBFF}, 3u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0x005A}, 3u, 0u},
    {7u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {2u, {0xD800, 0x0041}, 0u, 0u},
    {3u, {0xD800, 0x0041, 0x005A}, 0u, 0u},
    {5u, {0xD800, 0x0041, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {3u, {0x0041, 0xD800, 0x0041}, 1u, 0u},
    {4u, {0x0041, 0xD800, 0x0041, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xD800, 0x0041, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0x0041}, 3u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0x0041, 0x005A}, 3u, 0u},
    {8u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0x0041, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {2u, {0xD800, 0xE000}, 0u, 0u},
    {3u, {0xD800, 0xE000, 0x005A}, 0u, 0u},
    {5u, {0xD800, 0xE000, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {3u, {0x0041, 0xD800, 0xE000}, 1u, 0u},
    {4u, {0x0041, 0xD800, 0xE000, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xD800, 0xE000, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xE000}, 3u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xE000, 0x005A}, 3u, 0u},
    {8u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xE000, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {2u, {0xD800, 0xD7FF}, 0u, 0u},
    {3u, {0xD800, 0xD7FF, 0x005A}, 0u, 0u},
    {5u, {0xD800, 0xD7FF, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {3u, {0x0041, 0xD800, 0xD7FF}, 1u, 0u},
    {4u, {0x0041, 0xD800, 0xD7FF, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xD800, 0xD7FF, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD7FF}, 3u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD7FF, 0x005A}, 3u, 0u},
    {8u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD7FF, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {2u, {0xDBFF, 0xFFFF}, 0u, 0u},
    {3u, {0xDBFF, 0xFFFF, 0x005A}, 0u, 0u},
    {5u, {0xDBFF, 0xFFFF, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {3u, {0x0041, 0xDBFF, 0xFFFF}, 1u, 0u},
    {4u, {0x0041, 0xDBFF, 0xFFFF, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xDBFF, 0xFFFF, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0xFFFF}, 3u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0xFFFF, 0x005A}, 3u, 0u},
    {8u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0xFFFF, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {2u, {0xD800, 0xD800}, 0u, 0u},
    {3u, {0xD800, 0xD800, 0x005A}, 0u, 0u},
    {5u, {0xD800, 0xD800, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {3u, {0x0041, 0xD800, 0xD800}, 1u, 0u},
    {4u, {0x0041, 0xD800, 0xD800, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xD800, 0xD800, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD800}, 3u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD800, 0x005A}, 3u, 0u},
    {8u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD800, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {3u, {0xD800, 0xD800, 0xDC00}, 0u, 0u},
    {4u, {0xD800, 0xD800, 0xDC00, 0x005A}, 0u, 0u},
    {6u, {0xD800, 0xD800, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {4u, {0x0041, 0xD800, 0xD800, 0xDC00}, 1u, 0u},
    {5u, {0x0041, 0xD800, 0xD800, 0xDC00, 0x005A}, 1u, 0u},
    {7u, {0x0041, 0xD800, 0xD800, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD800, 0xDC00}, 3u, 0u},
    {7u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD800, 0xDC00, 0x005A}, 3u, 0u},
    {9u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0xD800, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {2u, {0xDC00, 0xDC00}, 0u, 0u},
    {3u, {0xDC00, 0xDC00, 0x005A}, 0u, 0u},
    {5u, {0xDC00, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {3u, {0x0041, 0xDC00, 0xDC00}, 1u, 0u},
    {4u, {0x0041, 0xDC00, 0xDC00, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xDC00, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xDC00, 0xDC00}, 3u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xDC00, 0xDC00, 0x005A}, 3u, 0u},
    {8u, {0x0041, 0xD83D, 0xDE00, 0xDC00, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {2u, {0xDBFF, 0xDBFF}, 0u, 0u},
    {3u, {0xDBFF, 0xDBFF, 0x005A}, 0u, 0u},
    {5u, {0xDBFF, 0xDBFF, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {3u, {0x0041, 0xDBFF, 0xDBFF}, 1u, 0u},
    {4u, {0x0041, 0xDBFF, 0xDBFF, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xDBFF, 0xDBFF, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0xDBFF}, 3u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0xDBFF, 0x005A}, 3u, 0u},
    {8u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0xDBFF, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {3u, {0xD800, 0x0041, 0xDC00}, 0u, 0u},
    {4u, {0xD800, 0x0041, 0xDC00, 0x005A}, 0u, 0u},
    {6u, {0xD800, 0x0041, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {4u, {0x0041, 0xD800, 0x0041, 0xDC00}, 1u, 0u},
    {5u, {0x0041, 0xD800, 0x0041, 0xDC00, 0x005A}, 1u, 0u},
    {7u, {0x0041, 0xD800, 0x0041, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0x0041, 0xDC00}, 3u, 0u},
    {7u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0x0041, 0xDC00, 0x005A}, 3u, 0u},
    {9u, {0x0041, 0xD83D, 0xDE00, 0xD800, 0x0041, 0xDC00, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {2u, {0xDFFF, 0xD800}, 0u, 0u},
    {3u, {0xDFFF, 0xD800, 0x005A}, 0u, 0u},
    {5u, {0xDFFF, 0xD800, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {3u, {0x0041, 0xDFFF, 0xD800}, 1u, 0u},
    {4u, {0x0041, 0xDFFF, 0xD800, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xDFFF, 0xD800, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xDFFF, 0xD800}, 3u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xDFFF, 0xD800, 0x005A}, 3u, 0u},
    {8u, {0x0041, 0xD83D, 0xDE00, 0xDFFF, 0xD800, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
    {2u, {0xDBFF, 0x0000}, 0u, 0u},
    {3u, {0xDBFF, 0x0000, 0x005A}, 0u, 0u},
    {5u, {0xDBFF, 0x0000, 0xD83D, 0xDE00, 0x005A}, 0u, 0u},
    {3u, {0x0041, 0xDBFF, 0x0000}, 1u, 0u},
    {4u, {0x0041, 0xDBFF, 0x0000, 0x005A}, 1u, 0u},
    {6u, {0x0041, 0xDBFF, 0x0000, 0xD83D, 0xDE00, 0x005A}, 1u, 0u},
    {5u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0x0000}, 3u, 0u},
    {6u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0x0000, 0x005A}, 3u, 0u},
    {8u, {0x0041, 0xD83D, 0xDE00, 0xDBFF, 0x0000, 0xD83D, 0xDE00, 0x005A}, 3u, 0u},
};
// END GENERATED REFERENCE

constexpr std::uint32_t kSentinel = 0xDEADBEEFu;

std::uint32_t code(Utf16Result r) { return static_cast<std::uint32_t>(r); }

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t n = 0;
  void add(std::uint64_t v) {
    ++n;
    h = (h ^ v) * 0x100000001B3ull;
  }
};

struct Outcome {
  std::uint32_t vs = 0, off = 0, cs = 0, count = 0, ds = 0, cp = 0, ln = 0;
  bool sentinel_ok = true;
  bool operator==(const Outcome& o) const {
    return vs == o.vs && off == o.off && cs == o.cs && count == o.count && ds == o.ds && cp == o.cp && ln == o.ln;
  }
  std::uint64_t pack() const {
    return static_cast<std::uint64_t>(vs) | (static_cast<std::uint64_t>(off) << 4) | (static_cast<std::uint64_t>(ds) << 12) |
           (static_cast<std::uint64_t>(ln) << 16) | (static_cast<std::uint64_t>(cp) << 20) | (static_cast<std::uint64_t>(count) << 41);
  }
};

Outcome run_impl(const std::uint16_t* p, std::uint32_t n) {
  Outcome o;
  std::uint32_t off = kSentinel;
  o.vs = code(validate(p, n, &off));
  o.off = off;
  std::uint32_t cnt = kSentinel;
  o.cs = code(count_code_points(p, n, &cnt));
  if (o.cs == 0) {
    o.count = cnt;
  } else {
    o.sentinel_ok = cnt == kSentinel;
  }
  std::uint32_t cp = kSentinel, ln = kSentinel;
  o.ds = code(decode_one(p, n, &cp, &ln));
  if (o.ds == 0) {
    o.cp = cp;
    o.ln = ln;
  } else {
    o.sentinel_ok = o.sentinel_ok && cp == kSentinel && ln == kSentinel;
  }
  return o;
}

bool oracle_decode(const std::uint16_t* p, std::size_t avail, std::uint32_t& cp, std::uint32_t& ln) {
  if (avail == 0) return false;
  const std::uint32_t u = p[0];
  if ((u & 0xF800u) != 0xD800u) {  // not a surrogate
    cp = u;
    ln = 1;
    return true;
  }
  if ((u & 0xFC00u) != 0xD800u) return false;  // low surrogate first
  if (avail < 2 || (static_cast<std::uint32_t>(p[1]) & 0xFC00u) != 0xDC00u) return false;
  cp = 0x10000u + (((u & 0x3FFu) << 10) | (static_cast<std::uint32_t>(p[1]) & 0x3FFu));
  ln = 2;
  return true;
}

Outcome run_oracle(const std::uint16_t* p, std::uint32_t n) {
  Outcome o;
  std::uint32_t i = 0, count = 0;
  o.vs = 0;
  o.off = n;
  while (i < n) {
    std::uint32_t cp = 0, ln = 0;
    if (!oracle_decode(p + i, n - i, cp, ln)) {
      o.vs = 4;
      o.off = i;
      break;
    }
    i += ln;
    ++count;
  }
  o.cs = o.vs;
  o.count = o.vs == 0 ? count : 0;
  std::uint32_t cp = 0, ln = 0;
  if (oracle_decode(p, n, cp, ln)) {
    o.ds = 0;
    o.cp = cp;
    o.ln = ln;
  } else {
    o.ds = 4;
  }
  return o;
}

std::vector<std::uint16_t> oracle_encode(std::uint32_t cp) {
  std::vector<std::uint16_t> out;
  if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) return out;
  if (cp < 0x10000u) {
    out.push_back(static_cast<std::uint16_t>(cp));
  } else {
    const std::uint32_t v = cp - 0x10000u;
    out.push_back(static_cast<std::uint16_t>(0xD800u | (v >> 10)));
    out.push_back(static_cast<std::uint16_t>(0xDC00u | (v & 0x3FFu)));
  }
  return out;
}

std::size_t check_input(const std::uint16_t* p, std::uint32_t n, Fold* f) {
  const Outcome a = run_impl(p, n);
  const Outcome b = run_oracle(p, n);
  std::size_t bad = 0;
  if (!(a == b) || !a.sentinel_ok) ++bad;
  if (a.ds == 0) {
    std::uint16_t out[2] = {0xAAAA, 0xAAAA};
    std::uint32_t ln = 0;
    if (encode_one(a.cp, out, 2, &ln) != Utf16Result::Ok || ln != a.ln) {
      ++bad;
    } else {
      for (std::uint32_t k = 0; k < ln; ++k)
        if (out[k] != p[k]) ++bad;
    }
  }
  if (f != nullptr) f->add(a.pack());
  return bad;
}

std::unique_ptr<std::uint16_t[]> heap(std::size_t n) { return std::unique_ptr<std::uint16_t[]>(new std::uint16_t[n == 0 ? 1 : n]); }

}  // namespace

TEST_CASE("header anchors", "[utf16]") {
  // U+0000, U+FEFF, U+FFFE and U+FFFF are valid single units; U+10000 is D800 DC00; U+10FFFF is DBFF DFFF
  std::uint32_t cp = 0, ln = 0, off = 99, cnt = 99;
  for (std::uint32_t u : {0x0000u, 0xFEFFu, 0xFFFEu, 0xFFFFu}) {
    const std::uint16_t s[1] = {static_cast<std::uint16_t>(u)};
    CHECK(decode_one(s, 1, &cp, &ln) == Utf16Result::Ok);
    CHECK(cp == u);
    CHECK(ln == 1u);
    CHECK(validate(s, 1, &off) == Utf16Result::Ok);
    CHECK(off == 1u);
  }
  const std::uint16_t lo[2] = {0xD800, 0xDC00};
  CHECK(decode_one(lo, 2, &cp, &ln) == Utf16Result::Ok);
  CHECK(cp == 0x10000u);
  CHECK(ln == 2u);
  const std::uint16_t hi[2] = {0xDBFF, 0xDFFF};
  CHECK(decode_one(hi, 2, &cp, &ln) == Utf16Result::Ok);
  CHECK(cp == 0x10FFFFu);
  CHECK(count_code_points(hi, 2, &cnt) == Utf16Result::Ok);
  CHECK(cnt == 1u);
  const std::uint16_t smile[2] = {0xD83D, 0xDE00};  // 0x10000 + ((0xD83D - 0xD800) << 10) + (0xDE00 - 0xDC00) = 0x1F600
  CHECK(decode_one(smile, 2, &cp, &ln) == Utf16Result::Ok);
  CHECK(cp == 0x1F600u);
  CHECK(kMaxUnits == (1u << 27));
  CHECK(kMaxCodePoint == 0x10FFFFu);
  std::uint16_t out[2] = {};
  CHECK(encode_one(0x1F600, out, 2, &ln) == Utf16Result::Ok);
  CHECK(ln == 2u);
  CHECK(out[0] == 0xD83D);
  CHECK(out[1] == 0xDE00);
  CHECK(encode_one(0x10000, out, 2, &ln) == Utf16Result::Ok);
  CHECK(out[0] == 0xD800);
  CHECK(out[1] == 0xDC00);
}

TEST_CASE("validate and count: argument order and outputs", "[utf16]") {
  std::uint32_t off = kSentinel, cnt = kSentinel;
  const std::uint16_t one[1] = {0x0041};
  CHECK(validate(nullptr, 1, &off) == Utf16Result::NullArgument);
  CHECK(validate(nullptr, kMaxUnits + 1u, &off) == Utf16Result::NullArgument);  // null is checked before the size
  CHECK(validate(nullptr, 0xFFFFFFFFu, &off) == Utf16Result::NullArgument);
  CHECK(off == kSentinel);
  CHECK(validate(one, kMaxUnits + 1u, &off) == Utf16Result::TooLarge);  // nothing is read: the buffer has one unit
  CHECK(validate(one, 0xFFFFFFFFu, &off) == Utf16Result::TooLarge);
  CHECK(off == kSentinel);
  CHECK(validate(nullptr, 0, &off) == Utf16Result::Ok);
  CHECK(off == 0u);
  off = kSentinel;
  CHECK(validate(one, 0, &off) == Utf16Result::Ok);
  CHECK(off == 0u);
  CHECK(validate(one, 1, &off) == Utf16Result::Ok);
  CHECK(off == 1u);
  const std::uint16_t bad[2] = {0x0041, 0xDC00};
  CHECK(validate(nullptr, 1, nullptr) == Utf16Result::NullArgument);
  CHECK(validate(one, kMaxUnits + 1u, nullptr) == Utf16Result::TooLarge);
  CHECK(validate(one, 1, nullptr) == Utf16Result::Ok);
  CHECK(validate(nullptr, 0, nullptr) == Utf16Result::Ok);
  CHECK(validate(bad, 2, nullptr) == Utf16Result::Malformed);
  CHECK(validate(bad, 2, &off) == Utf16Result::Malformed);
  CHECK(off == 1u);
  // count: out null is NullArgument at every n, also with a null text
  CHECK(count_code_points(one, 1, nullptr) == Utf16Result::NullArgument);
  CHECK(count_code_points(nullptr, 0, nullptr) == Utf16Result::NullArgument);
  CHECK(count_code_points(one, 0, nullptr) == Utf16Result::NullArgument);
  CHECK(count_code_points(nullptr, 1, &cnt) == Utf16Result::NullArgument);
  CHECK(count_code_points(nullptr, kMaxUnits + 1u, &cnt) == Utf16Result::NullArgument);
  CHECK(cnt == kSentinel);
  CHECK(count_code_points(one, kMaxUnits + 1u, &cnt) == Utf16Result::TooLarge);
  CHECK(cnt == kSentinel);
  CHECK(count_code_points(bad, 2, &cnt) == Utf16Result::Malformed);
  CHECK(cnt == kSentinel);
  CHECK(count_code_points(nullptr, 0, &cnt) == Utf16Result::Ok);  // empty text gives 0
  CHECK(cnt == 0u);
  cnt = kSentinel;
  CHECK(count_code_points(one, 0, &cnt) == Utf16Result::Ok);
  CHECK(cnt == 0u);
  CHECK(count_code_points(one, 1, &cnt) == Utf16Result::Ok);
  CHECK(cnt == 1u);
  const std::uint16_t mix[4] = {0x0041, 0xD83D, 0xDE00, 0x005A};
  CHECK(count_code_points(mix, 4, &cnt) == Utf16Result::Ok);
  CHECK(cnt == 3u);
}

TEST_CASE("decode_one: argument order and untouched outputs", "[utf16]") {
  const std::uint16_t a[1] = {0x0041};
  std::uint32_t cp = kSentinel, ln = kSentinel;
  CHECK(decode_one(nullptr, 1, &cp, &ln) == Utf16Result::NullArgument);
  CHECK(decode_one(a, 1, nullptr, &ln) == Utf16Result::NullArgument);
  CHECK(decode_one(a, 1, &cp, nullptr) == Utf16Result::NullArgument);
  CHECK(decode_one(nullptr, 0, &cp, &ln) == Utf16Result::NullArgument);  // null arguments come before n == 0
  CHECK(decode_one(a, 0, nullptr, &ln) == Utf16Result::NullArgument);
  CHECK(decode_one(a, 0, &cp, nullptr) == Utf16Result::NullArgument);
  CHECK(decode_one(a, 0, &cp, &ln) == Utf16Result::Malformed);
  CHECK(cp == kSentinel);
  CHECK(ln == kSentinel);
  const std::uint16_t low[1] = {0xDC00};
  CHECK(decode_one(low, 1, &cp, &ln) == Utf16Result::Malformed);
  CHECK(cp == kSentinel);
  CHECK(ln == kSentinel);
  auto p = heap(1);  // a high surrogate with n == 1 reads one unit only
  p[0] = 0xD800;
  CHECK(decode_one(p.get(), 1, &cp, &ln) == Utf16Result::Malformed);
  CHECK(cp == kSentinel);
  CHECK(ln == kSentinel);
  const std::uint16_t hh[2] = {0xD800, 0xD800};
  CHECK(decode_one(hh, 2, &cp, &ln) == Utf16Result::Malformed);
  CHECK(cp == kSentinel);
  const std::uint16_t extra[3] = {0x0041, 0xDC00, 0xDC00};  // units after the first sequence are ignored
  CHECK(decode_one(extra, 3, &cp, &ln) == Utf16Result::Ok);
  CHECK(ln == 1u);
  const std::uint16_t pair_then[3] = {0xD800, 0xDC00, 0xDC00};
  CHECK(decode_one(pair_then, 3, &cp, &ln) == Utf16Result::Ok);
  CHECK(ln == 2u);
}

TEST_CASE("encode_one: argument order, capacity and untouched outputs", "[utf16]") {
  std::uint16_t buf[4];
  for (auto& b : buf) b = 0xAAAA;
  std::uint32_t ln = kSentinel;
  CHECK(encode_one(0x41, nullptr, 1, &ln) == Utf16Result::NullArgument);
  CHECK(encode_one(0x41, buf, 4, nullptr) == Utf16Result::NullArgument);
  CHECK(encode_one(0x110000, nullptr, 4, &ln) == Utf16Result::NullArgument);  // null comes before the value check
  CHECK(encode_one(0x110000, buf, 4, &ln) == Utf16Result::Malformed);
  CHECK(encode_one(0xD800, buf, 0, &ln) == Utf16Result::Malformed);  // the value check comes before the capacity check
  CHECK(encode_one(0x41, nullptr, 0, &ln) == Utf16Result::BufferTooSmall);  // dst null with dst_cap 0 never succeeds
  CHECK(encode_one(0x41, buf, 0, &ln) == Utf16Result::BufferTooSmall);
  CHECK(encode_one(0x10000, buf, 1, &ln) == Utf16Result::BufferTooSmall);
  CHECK(ln == kSentinel);
  for (auto b : buf) CHECK(b == 0xAAAA);
  CHECK(encode_one(0x10000, buf, 2, &ln) == Utf16Result::Ok);
  CHECK(ln == 2u);
  CHECK(buf[2] == 0xAAAA);  // nothing past the sequence is written
  CHECK(encode_one(0xFFFF, buf, 1, &ln) == Utf16Result::Ok);
  CHECK(ln == 1u);
  CHECK(buf[0] == 0xFFFF);
  CHECK(buf[1] == 0xDC00);  // the second unit of the earlier call is not touched
}

TEST_CASE("ill-formed classes and valid vectors from the reference, with offsets and counts", "[utf16]") {
  std::size_t bad = 0;
  Fold f;
  for (const RefVector& v : kRefVectors) {
    auto p = heap(v.n);
    for (std::uint32_t i = 0; i < v.n; ++i) p[i] = v.units[i];
    std::uint32_t off = kSentinel, cnt = kSentinel;
    const Utf16Result r = validate(p.get(), v.n, &off);
    if (off != v.offset) ++bad;
    if ((r == Utf16Result::Ok) != (v.offset == v.n)) ++bad;
    if (r == Utf16Result::Ok) {
      if (count_code_points(p.get(), v.n, &cnt) != Utf16Result::Ok || cnt != v.count) ++bad;
    }
    bad += check_input(p.get(), v.n, &f);
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_vec);
  CHECK(f.h == kRefDigest_vec);
}

TEST_CASE("every 1-unit input", "[utf16][sweep]") {
  Fold f;
  std::size_t bad = 0;
  auto p = heap(1);
  for (std::uint32_t x = 0; x < 65536; ++x) {
    p[0] = static_cast<std::uint16_t>(x);
    bad += check_input(p.get(), 1, &f);
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_l1);
  CHECK(f.h == kRefDigest_l1);
}

TEST_CASE("every (high, low) pair", "[utf16][sweep]") {
  Fold f;
  std::size_t bad = 0;
  auto p = heap(2);
  for (std::uint32_t h = 0xD800; h <= 0xDBFF; ++h)
    for (std::uint32_t l = 0xDC00; l <= 0xDFFF; ++l) {
      p[0] = static_cast<std::uint16_t>(h);
      p[1] = static_cast<std::uint16_t>(l);
      bad += check_input(p.get(), 2, &f);
    }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_hl);
  CHECK(f.h == kRefDigest_hl);
}

TEST_CASE("every pair with one unit from the alphabet A and the other any unit", "[utf16][sweep]") {
  Fold f;
  std::size_t bad = 0;
  auto p = heap(2);
  for (std::uint32_t x = 0; x < 65536; ++x)
    for (std::uint32_t a : kRefAlphabet) {
      p[0] = static_cast<std::uint16_t>(x);
      p[1] = static_cast<std::uint16_t>(a);
      bad += check_input(p.get(), 2, &f);
    }
  for (std::uint32_t a : kRefAlphabet)
    for (std::uint32_t x = 0; x < 65536; ++x) {
      p[0] = static_cast<std::uint16_t>(a);
      p[1] = static_cast<std::uint16_t>(x);
      bad += check_input(p.get(), 2, &f);
    }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_pair);
  CHECK(f.h == kRefDigest_pair);
}

TEST_CASE("24 highs followed by every unit (high-by-any-unit is not exhaustive)", "[utf16][sweep]") {
  Fold f;
  std::size_t bad = 0;
  auto p = heap(2);
  for (std::uint32_t h : kRefHighs24)
    for (std::uint32_t x = 0; x < 65536; ++x) {
      p[0] = static_cast<std::uint16_t>(h);
      p[1] = static_cast<std::uint16_t>(x);
      bad += check_input(p.get(), 2, &f);
    }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_h24);
  CHECK(f.h == kRefDigest_h24);
}

TEST_CASE("every string of length 0 to 4 over the alphabet A", "[utf16][sweep]") {
  Fold f;
  std::size_t bad = 0;
  const std::uint32_t na = sizeof kRefAlphabet / sizeof kRefAlphabet[0];
  for (std::uint32_t len = 0; len <= kRefStrMax; ++len) {
    auto p = heap(len);
    std::uint32_t total = 1;
    for (std::uint32_t i = 0; i < len; ++i) total *= na;
    for (std::uint32_t idx = 0; idx < total; ++idx) {
      std::uint32_t r = idx;
      for (std::uint32_t k = len; k-- > 0;) {  // the last unit changes fastest, as in the reference
        p[k] = static_cast<std::uint16_t>(kRefAlphabet[r % na]);
        r /= na;
      }
      bad += check_input(p.get(), len, &f);
    }
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_str);
  CHECK(f.h == kRefDigest_str);
}

TEST_CASE("every scalar value: encode, round trip and reference digest", "[utf16][sweep]") {
  Fold f;
  std::size_t bad = 0;
  std::size_t scalars = 0;
  auto dst = heap(2);
  auto exact = heap(2);
  auto build = [&](std::uint32_t cp) {
    dst[0] = dst[1] = 0xAAAA;
    std::uint32_t ln = kSentinel;
    const Utf16Result r = encode_one(cp, dst.get(), 2, &ln);
    const auto want = oracle_encode(cp);
    std::uint64_t packed = code(r);
    if (r == Utf16Result::Ok) {
      if (want.empty() || ln != want.size()) {
        ++bad;
        return;
      }
      for (std::uint32_t k = 0; k < ln; ++k)
        if (dst[k] != want[k]) ++bad;
      for (std::uint32_t k = ln; k < 2; ++k)
        if (dst[k] != 0xAAAA) ++bad;  // nothing past len is written
      packed |= (static_cast<std::uint64_t>(ln) << 4) | (static_cast<std::uint64_t>(dst[0]) << 8) |
                (static_cast<std::uint64_t>(ln > 1 ? dst[1] : 0) << 24);
      ++scalars;
      for (std::uint32_t k = 0; k < ln; ++k) exact[k] = dst[k];
      std::uint32_t c = 0, l = 0, off = 0, cnt = 0;
      if (decode_one(exact.get(), ln, &c, &l) != Utf16Result::Ok || c != cp || l != ln) ++bad;
      if (validate(exact.get(), ln, &off) != Utf16Result::Ok || off != ln) ++bad;
      if (count_code_points(exact.get(), ln, &cnt) != Utf16Result::Ok || cnt != 1u) ++bad;
    } else {
      if (!want.empty() || r != Utf16Result::Malformed || ln != kSentinel) ++bad;
      if (dst[0] != 0xAAAA || dst[1] != 0xAAAA) ++bad;
    }
    f.add(packed);
  };
  for (std::uint32_t cp = 0; cp <= kMaxCodePoint; ++cp) build(cp);
  for (std::uint32_t cp : kRefExtraCps) build(cp);
  CHECK(bad == 0);
  CHECK(scalars == 1112064u);
  CHECK(f.n == kRefCases_enc);
  CHECK(f.h == kRefDigest_enc);
}

TEST_CASE("encode_one capacity table", "[utf16][sweep]") {
  Fold f;
  std::size_t bad = 0;
  std::uint16_t dummy = 0xAAAA;
  for (std::uint32_t cp : kRefCapCps) {
    for (std::uint32_t cap : kRefCaps) {
      for (int dn = 0; dn < (cap == 0 ? 2 : 1); ++dn) {
        std::vector<std::uint16_t> buf(cap, 0xAAAA);
        std::uint16_t* dst = dn != 0 ? nullptr : (cap == 0 ? &dummy : buf.data());
        std::uint32_t ln = kSentinel;
        const Utf16Result r = encode_one(cp, dst, cap, &ln);
        std::uint64_t v = code(r) | (static_cast<std::uint64_t>(dn) << 8) | (static_cast<std::uint64_t>(cap) << 12);
        const auto want = oracle_encode(cp);
        if (r == Utf16Result::Ok) {
          v |= static_cast<std::uint64_t>(ln) << 4;
          if (want.empty() || ln != want.size() || ln > cap) ++bad;
          for (std::uint32_t k = 0; k < cap; ++k)
            if (buf[k] != (k < ln && k < want.size() ? want[k] : 0xAAAA)) ++bad;
        } else {
          if (ln != kSentinel) ++bad;
          for (auto b : buf)
            if (b != 0xAAAA) ++bad;
          const Utf16Result expect = want.empty() ? Utf16Result::Malformed : Utf16Result::BufferTooSmall;
          if (r != expect) ++bad;
        }
        f.add(v);
      }
    }
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_cap);
  CHECK(f.h == kRefDigest_cap);
}

TEST_CASE("long text: first bad offset far from the start", "[utf16]") {
  const std::uint32_t n = 100000;
  auto p = heap(n);
  for (std::uint32_t i = 0; i < n; ++i) p[i] = static_cast<std::uint16_t>(0x41 + i % 26);
  std::uint32_t off = 0, cnt = 0;
  CHECK(validate(p.get(), n, &off) == Utf16Result::Ok);
  CHECK(off == n);
  CHECK(count_code_points(p.get(), n, &cnt) == Utf16Result::Ok);
  CHECK(cnt == n);
  p[n - 1] = 0xD800;  // a high surrogate at the very end
  CHECK(validate(p.get(), n, &off) == Utf16Result::Malformed);
  CHECK(off == n - 1u);
  p[n - 1] = 0x41;
  p[54321] = 0xDC00;
  p[77777] = 0xD800;
  CHECK(validate(p.get(), n, &off) == Utf16Result::Malformed);
  CHECK(off == 54321u);
  cnt = kSentinel;
  CHECK(count_code_points(p.get(), n, &cnt) == Utf16Result::Malformed);
  CHECK(cnt == kSentinel);
}

TEST_CASE("n equal to kMaxUnits is accepted, and units beyond n are never used", "[utf16]") {
  // n == kMaxUnits is not TooLarge: a lone low surrogate first stops the scan after one unit, so a 1-unit buffer is enough
  auto p = heap(1);
  for (std::uint32_t u : {0xDC00u, 0xDFFFu}) {
    p[0] = static_cast<std::uint16_t>(u);
    std::uint32_t off = kSentinel, cnt = kSentinel;
    CHECK(validate(p.get(), kMaxUnits, &off) == Utf16Result::Malformed);
    CHECK(off == 0u);
    CHECK(count_code_points(p.get(), kMaxUnits, &cnt) == Utf16Result::Malformed);
    CHECK(cnt == kSentinel);
  }
  // a pair cut off by n is ill-formed even when its low surrogate is in the buffer
  const std::uint16_t buf[4] = {0x0041, 0xD83D, 0xDE00, 0x0042};
  std::uint32_t off = kSentinel, cnt = kSentinel, c = kSentinel, l = kSentinel;
  CHECK(validate(buf, 2, &off) == Utf16Result::Malformed);
  CHECK(off == 1u);
  CHECK(count_code_points(buf, 2, &cnt) == Utf16Result::Malformed);
  CHECK(cnt == kSentinel);
  CHECK(decode_one(buf + 1, 1, &c, &l) == Utf16Result::Malformed);
  CHECK(c == kSentinel);
  CHECK(validate(buf, 3, &off) == Utf16Result::Ok);
  CHECK(off == 3u);
}
