// Tests for core/ratio. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/ratio_ref.py, an independent Python implementation (fractions.Fraction
//      and plain integers). The tests re-run every scenario against the real functions and recompute the 64-bit digest and the counts.
//  (2) ORACLE: a naive in-test oracle that shares no code with core/ratio/ratio.cpp: a sign-magnitude big integer (6 limbs of 32 bits, schoolbook multiply,
//      binary gcd), no 128-bit type and no compiler builtin. It forms the exact unreduced result N / D, finds g = gcd(N, D) and decides the expected
//      outcome (Overflow when N / g or D / g does not fit), then checks the returned Ratio with N = num * g and D = den * g. Floor and ceil are
//      checked against their defining inequalities. The oracle is run on EVERY pair of the exhaustive grids (see the section markers below).
//  (3) CONSTANTS: the hand-picked rows (extreme values, products near 2^126) were computed once with Python Fractions and written in as constants; the
//      check-order, null and BadRatio matrices are written by hand from the header text.
// After every failing call the output is compared byte for byte with the sentinel that was passed (the header promises "untouched").
// What is NOT done: no assertion on anything the header leaves open; no timing test (the running time has no data-dependent loop: every loop in the
// implementation has a fixed bound, argued in the docs); Windows is CI-only; the random scenarios are NOT exhaustive.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "ratio.h"

using namespace lumora::ratio;

namespace {

constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();

// BEGIN GENERATED REFERENCE (tools/ratio_ref.py)
const std::uint32_t kRefRandSeeds = 400, kRefRandPerSeed = 10;
const std::int64_t kRefNs[] = {kMin, -9223372036854775807ll, -9223372036854775806ll, -4611686018427387905ll, -4611686018427387904ll, -4611686018427387903ll, -4294967297ll, -4294967296ll, -4294967295ll, -2147483649ll, -2147483648ll, -2147483647ll, -8ll, -7ll, -6ll, -5ll, -4ll, -3ll, -2ll, -1ll, 0ll, 1ll, 2ll, 3ll, 4ll, 5ll, 6ll, 7ll, 8ll, 2147483647ll, 2147483648ll, 2147483649ll, 4294967295ll, 4294967296ll, 4294967297ll, 4611686018427387903ll, 4611686018427387904ll, 4611686018427387905ll, 9223372036854775805ll, 9223372036854775806ll, 9223372036854775807ll};
const std::int64_t kRefDs[] = {1ll, 2ll, 3ll, 4ll, 5ll, 6ll, 7ll, 8ll, 2147483647ll, 2147483648ll, 2147483649ll, 4294967295ll, 4294967296ll, 4294967297ll, 4611686018427387903ll, 4611686018427387904ll, 4611686018427387905ll, 9223372036854775805ll, 9223372036854775806ll, 9223372036854775807ll};
const std::int64_t kRefDm[] = {kMin, -9223372036854775807ll, -9223372036854775806ll, -9223372036854775805ll, -4611686018427387905ll, -4611686018427387904ll, -4611686018427387903ll, -4294967297ll, -4294967296ll, -4294967295ll, -2147483649ll, -2147483648ll, -2147483647ll, -8ll, -7ll, -6ll, -5ll, -4ll, -3ll, -2ll, -1ll, 0ll, 1ll, 2ll, 3ll, 4ll, 5ll, 6ll, 7ll, 8ll, 2147483647ll, 2147483648ll, 2147483649ll, 4294967295ll, 4294967296ll, 4294967297ll, 4611686018427387903ll, 4611686018427387904ll, 4611686018427387905ll, 9223372036854775805ll, 9223372036854775806ll, 9223372036854775807ll};
const Ratio kRefPartners[] = {{0ll, 1ll}, {1ll, 1ll}, {-1ll, 1ll}, {2ll, 3ll}, {-3ll, 2ll}, {kMin, 1ll}, {9223372036854775807ll, 1ll}, {1ll, 9223372036854775807ll}, {-1ll, 9223372036854775807ll}, {kMin, 3ll}, {9223372036854775807ll, 2ll}, {1ll, 2ll}};
const std::size_t kRefSizeNs = 41, kRefSizeDs = 20, kRefSizeDm = 42, kRefSizeG = 555, kRefSizeB = 1167, kRefSizeD12 = 183;
const std::uint64_t kRefDigest_make = 0x05717370AB54490Full;
const std::size_t kRefCases_make = 1722, kRefOps_make = 1722;
const std::uint64_t kRefDigest_grid = 0xEEFA11E7DAE26CC4ull;
const std::size_t kRefCases_grid = 308025, kRefOps_grid = 1540125;
const std::uint64_t kRefDigest_fc = 0xC53FE711CC2F5ED3ull;
const std::size_t kRefCases_fc = 555, kRefOps_fc = 555;
const std::uint64_t kRefDigest_bad = 0x87A57BEFC2391AA5ull;
const std::size_t kRefCases_bad = 15171, kRefOps_bad = 141207;
const std::uint64_t kRefDigest_d12 = 0x3B3F530D04E07D91ull;
const std::size_t kRefCases_d12 = 33489, kRefOps_d12 = 167445;
const std::uint64_t kRefDigest_randp = 0x524188A892A02887ull;
const std::size_t kRefCases_randp = 4000, kRefOps_randp = 20000;
const std::uint64_t kRefDigest_randm = 0xBB0C24E39E8FAA82ull;
const std::size_t kRefCases_randm = 4000, kRefOps_randm = 4000;
// END GENERATED REFERENCE

struct BinRow {
  int op;
  Ratio a, b;
  std::uint32_t code;
  std::int64_t rnum, rden;
};
struct MakeRow {
  std::int64_t num, den;
  std::uint32_t code;
  std::int64_t rnum, rden;
};
struct CmpRow {
  Ratio a, b;
  std::int32_t want;
};
struct FcRow {
  Ratio a;
  std::int64_t floor, ceil;
};
const BinRow kBinRows[] = {
    {0, {kMin, 1ll}, {9223372036854775807ll, 1ll}, 0, -1ll, 1ll},  // MIN add MAX
    {0, {kMin, 1ll}, {1ll, 1ll}, 0, -9223372036854775807ll, 1ll},  // MIN add o
    {0, {9223372036854775807ll, 1ll}, {1ll, 1ll}, 4, 0ll, 0ll},  // MAX add o
    {0, {kMin, 1ll}, {-1ll, 1ll}, 4, 0ll, 0ll},  // MIN add no
    {0, {9223372036854775807ll, 1ll}, {9223372036854775807ll, 1ll}, 4, 0ll, 0ll},  // MAX add MAX
    {0, {kMin, 1ll}, {kMin, 1ll}, 4, 0ll, 0ll},  // MIN add MIN
    {0, {1ll, 9223372036854775807ll}, {-1ll, 9223372036854775807ll}, 0, 0ll, 1ll},  // iMAX add niMAX
    {0, {1ll, 9223372036854775807ll}, {1ll, 9223372036854775807ll}, 0, 2ll, 9223372036854775807ll},  // iMAX add iMAX
    {0, {9223372036854775806ll, 9223372036854775805ll}, {-1ll, 9223372036854775807ll}, 4, 0ll, 0ll},  // MAXq add niMAX
    {0, {kMin, 9223372036854775805ll}, {kMin, 9223372036854775807ll}, 4, 0ll, 0ll},  // MINd add MINm
    {0, {1ll, 2ll}, {-1ll, 2ll}, 0, 0ll, 1ll},  // h add nh
    {0, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}, 4, 0ll, 0ll},  // K add K
    {0, {-4611686018427387904ll, 1ll}, {-4611686018427387904ll, 1ll}, 0, kMin, 1ll},  // nK add nK
    {0, {1ll, 4611686018427387904ll}, {1ll, 4611686018427387904ll}, 0, 1ll, 2305843009213693952ll},  // iK add iK
    {0, {kMin, 3ll}, {9223372036854775807ll, 3ll}, 0, -1ll, 3ll},  // MIN3 add MAX3
    {0, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775806ll, 9223372036854775805ll}, 4, 0ll, 0ll},  // MAXr add MAXq
    {1, {kMin, 1ll}, {kMin, 1ll}, 0, 0ll, 1ll},  // MIN sub MIN
    {1, {9223372036854775807ll, 1ll}, {kMin, 1ll}, 4, 0ll, 0ll},  // MAX sub MIN
    {1, {kMin, 1ll}, {1ll, 1ll}, 4, 0ll, 0ll},  // MIN sub o
    {1, {kMin, 1ll}, {-1ll, 1ll}, 0, -9223372036854775807ll, 1ll},  // MIN sub no
    {1, {0ll, 1ll}, {kMin, 1ll}, 4, 0ll, 0ll},  // z sub MIN
    {1, {1ll, 1ll}, {kMin, 1ll}, 4, 0ll, 0ll},  // o sub MIN
    {1, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775806ll, 9223372036854775805ll}, 4, 0ll, 0ll},  // MAXr sub MAXq
    {1, {kMin, 9223372036854775807ll}, {kMin, 9223372036854775805ll}, 4, 0ll, 0ll},  // MINm sub MINd
    {1, {-4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}, 0, kMin, 1ll},  // nK sub K
    {1, {1ll, 2ll}, {1ll, 2ll}, 0, 0ll, 1ll},  // h sub h
    {1, {1ll, 9223372036854775807ll}, {1ll, 9223372036854775807ll}, 0, 0ll, 1ll},  // iMAX sub iMAX
    {2, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775806ll, 9223372036854775805ll}, 0, 9223372036854775807ll, 9223372036854775805ll},  // MAXr mul MAXq
    {2, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775806ll, 9223372036854775807ll}, 0, 1ll, 1ll},  // MAXr mul MAXm
    {2, {9223372036854775806ll, 9223372036854775805ll}, {9223372036854775807ll, 9223372036854775806ll}, 0, 9223372036854775807ll, 9223372036854775805ll},  // MAXq mul MAXr
    {2, {kMin, 1ll}, {-1ll, 1ll}, 4, 0ll, 0ll},  // MIN mul no
    {2, {kMin, 1ll}, {1ll, 1ll}, 0, kMin, 1ll},  // MIN mul o
    {2, {kMin, 1ll}, {1ll, 2ll}, 0, -4611686018427387904ll, 1ll},  // MIN mul h
    {2, {kMin, 1ll}, {-1ll, 2ll}, 0, 4611686018427387904ll, 1ll},  // MIN mul nh
    {2, {9223372036854775807ll, 1ll}, {1ll, 9223372036854775807ll}, 0, 1ll, 1ll},  // MAX mul iMAX
    {2, {kMin, 1ll}, {1ll, 9223372036854775807ll}, 0, kMin, 9223372036854775807ll},  // MIN mul iMAX
    {2, {9223372036854775807ll, 1ll}, {9223372036854775807ll, 1ll}, 4, 0ll, 0ll},  // MAX mul MAX
    {2, {1ll, 9223372036854775807ll}, {1ll, 9223372036854775807ll}, 4, 0ll, 0ll},  // iMAX mul iMAX
    {2, {4611686018427387904ll, 1ll}, {2ll, 1ll}, 4, 0ll, 0ll},  // K mul t
    {2, {-4611686018427387904ll, 1ll}, {2ll, 1ll}, 0, kMin, 1ll},  // nK mul t
    {2, {1ll, 4611686018427387904ll}, {1ll, 4611686018427387904ll}, 4, 0ll, 0ll},  // iK mul iK
    {2, {kMin, 9223372036854775807ll}, {9223372036854775806ll, 9223372036854775807ll}, 4, 0ll, 0ll},  // MINm mul MAXm
    {2, {kMin, 3ll}, {9223372036854775807ll, 3ll}, 4, 0ll, 0ll},  // MIN3 mul MAX3
    {2, {0ll, 1ll}, {kMin, 1ll}, 0, 0ll, 1ll},  // z mul MIN
    {2, {kMin, 9223372036854775805ll}, {9223372036854775807ll, 9223372036854775806ll}, 4, 0ll, 0ll},  // MINd mul MAXr
    {3, {kMin, 1ll}, {kMin, 1ll}, 0, 1ll, 1ll},  // MIN div MIN
    {3, {9223372036854775807ll, 1ll}, {9223372036854775807ll, 1ll}, 0, 1ll, 1ll},  // MAX div MAX
    {3, {1ll, 1ll}, {kMin, 1ll}, 4, 0ll, 0ll},  // o div MIN
    {3, {kMin, 1ll}, {-1ll, 1ll}, 4, 0ll, 0ll},  // MIN div no
    {3, {kMin, 1ll}, {2ll, 1ll}, 0, -4611686018427387904ll, 1ll},  // MIN div t
    {3, {kMin, 1ll}, {1ll, 2ll}, 4, 0ll, 0ll},  // MIN div h
    {3, {kMin, 1ll}, {-1ll, 2ll}, 4, 0ll, 0ll},  // MIN div nh
    {3, {9223372036854775807ll, 1ll}, {-1ll, 2ll}, 4, 0ll, 0ll},  // MAX div nh
    {3, {1ll, 1ll}, {1ll, 9223372036854775807ll}, 0, 9223372036854775807ll, 1ll},  // o div iMAX
    {3, {1ll, 9223372036854775807ll}, {1ll, 1ll}, 0, 1ll, 9223372036854775807ll},  // iMAX div o
    {3, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775806ll, 9223372036854775805ll}, 4, 0ll, 0ll},  // MAXr div MAXq
    {3, {9223372036854775806ll, 9223372036854775805ll}, {9223372036854775807ll, 9223372036854775806ll}, 4, 0ll, 0ll},  // MAXq div MAXr
    {3, {kMin, 3ll}, {kMin, 1ll}, 0, 1ll, 3ll},  // MIN3 div MIN
    {3, {9223372036854775807ll, 1ll}, {0ll, 1ll}, 3, 0ll, 0ll},  // MAX div z
    {3, {0ll, 1ll}, {kMin, 1ll}, 0, 0ll, 1ll},  // z div MIN
    {3, {kMin, 1ll}, {1ll, 9223372036854775807ll}, 4, 0ll, 0ll},  // MIN div iMAX
    {3, {1ll, 9223372036854775807ll}, {9223372036854775807ll, 1ll}, 4, 0ll, 0ll},  // iMAX div MAX
    {3, {kMin, 9223372036854775807ll}, {kMin, 9223372036854775805ll}, 0, 9223372036854775805ll, 9223372036854775807ll},  // MINm div MINd
    {3, {1ll, 4611686018427387904ll}, {kMin, 1ll}, 4, 0ll, 0ll},  // iK div MIN
};
const MakeRow kMakeRows[] = {
    {kMin, kMin, 0, 1ll, 1ll},
    {-4611686018427387904ll, kMin, 0, 1ll, 2ll},
    {kMin, 2ll, 0, -4611686018427387904ll, 1ll},
    {kMin, 1ll, 0, kMin, 1ll},
    {-9223372036854775807ll, -1ll, 0, 9223372036854775807ll, 1ll},
    {0ll, kMin, 0, 0ll, 1ll},
    {kMin, -1ll, 4, 0ll, 0ll},
    {1ll, kMin, 4, 0ll, 0ll},
    {-1ll, kMin, 4, 0ll, 0ll},
    {kMin, 3ll, 0, kMin, 3ll},
    {6ll, -9ll, 0, -2ll, 3ll},
    {9223372036854775807ll, 9223372036854775807ll, 0, 1ll, 1ll},
    {9223372036854775807ll, -1ll, 0, -9223372036854775807ll, 1ll},
    {kMin, -3ll, 4, 0ll, 0ll},
    {3ll, kMin, 4, 0ll, 0ll},
    {4611686018427387904ll, kMin, 0, -1ll, 2ll},
    {0ll, 5ll, 0, 0ll, 1ll},
    {0ll, -5ll, 0, 0ll, 1ll},
    {5ll, 0ll, 3, 0ll, 0ll},
    {0ll, 0ll, 3, 0ll, 0ll},
    {kMin, 0ll, 3, 0ll, 0ll},
    {-6ll, -8ll, 0, 3ll, 4ll},
    {9223372036854775807ll, 9223372036854775807ll, 0, 1ll, 1ll},
    {kMin, 9223372036854775807ll, 0, kMin, 9223372036854775807ll},
    {9223372036854775807ll, kMin, 4, 0ll, 0ll},
    {-9223372036854775807ll, kMin, 4, 0ll, 0ll},
    {kMin, -9223372036854775807ll, 4, 0ll, 0ll},
    {kMin, -9223372036854775806ll, 0, 4611686018427387904ll, 4611686018427387903ll},
    {-9223372036854775806ll, kMin, 0, 4611686018427387903ll, 4611686018427387904ll},
    {4ll, kMin, 0, -1ll, 2305843009213693952ll},
    {kMin, 4ll, 0, -2305843009213693952ll, 1ll},
    {kMin, -2ll, 0, 4611686018427387904ll, 1ll},
    {kMin, -4ll, 0, 2305843009213693952ll, 1ll},
    {7ll, -21ll, 0, -1ll, 3ll},
};
const CmpRow kCmpRows[] = {
    {{9223372036854775807ll, 9223372036854775806ll}, {9223372036854775806ll, 9223372036854775805ll}, -1},  // MAXr ? MAXq
    {{9223372036854775806ll, 9223372036854775805ll}, {9223372036854775807ll, 9223372036854775806ll}, 1},  // MAXq ? MAXr
    {{kMin, 9223372036854775807ll}, {kMin, 9223372036854775805ll}, 1},  // MINm ? MINd
    {{kMin, 9223372036854775805ll}, {kMin, 9223372036854775807ll}, -1},  // MINd ? MINm
    {{kMin, 1ll}, {kMin, 1ll}, 0},  // MIN ? MIN
    {{kMin, 1ll}, {9223372036854775807ll, 1ll}, -1},  // MIN ? MAX
    {{9223372036854775807ll, 1ll}, {kMin, 1ll}, 1},  // MAX ? MIN
    {{0ll, 1ll}, {kMin, 1ll}, 1},  // z ? MIN
    {{kMin, 1ll}, {0ll, 1ll}, -1},  // MIN ? z
    {{1ll, 9223372036854775807ll}, {0ll, 1ll}, 1},  // iMAX ? z
    {{-1ll, 9223372036854775807ll}, {0ll, 1ll}, -1},  // niMAX ? z
    {{-1ll, 9223372036854775807ll}, {1ll, 9223372036854775807ll}, -1},  // niMAX ? iMAX
    {{1ll, 4611686018427387904ll}, {1ll, 9223372036854775807ll}, 1},  // iK ? iMAX
    {{kMin, 3ll}, {kMin, 1ll}, 1},  // MIN3 ? MIN
    {{1ll, 2ll}, {-1ll, 2ll}, 1},  // h ? nh
    {{9223372036854775806ll, 9223372036854775807ll}, {9223372036854775807ll, 9223372036854775806ll}, -1},  // MAXm ? MAXr
    {{9223372036854775807ll, 9223372036854775806ll}, {9223372036854775806ll, 9223372036854775807ll}, 1},  // MAXr ? MAXm
    {{-4611686018427387904ll, 1ll}, {kMin, 1ll}, 1},  // nK ? MIN
    {{kMin, 1ll}, {kMin, 3ll}, -1},  // MIN ? MIN3
    {{4611686018427387904ll, 1ll}, {9223372036854775807ll, 1ll}, -1},  // K ? MAX
};
const FcRow kFcRows[] = {
    {{kMin, 1ll}, kMin, kMin},
    {{9223372036854775807ll, 1ll}, 9223372036854775807ll, 9223372036854775807ll},
    {{kMin, 3ll}, -3074457345618258603ll, -3074457345618258602ll},
    {{9223372036854775807ll, 2ll}, 4611686018427387903ll, 4611686018427387904ll},
    {{-1ll, 2ll}, -1ll, 0ll},
    {{1ll, 2ll}, 0ll, 1ll},
    {{0ll, 1ll}, 0ll, 0ll},
    {{-7ll, 2ll}, -4ll, -3ll},
    {{7ll, 2ll}, 3ll, 4ll},
    {{-7ll, 3ll}, -3ll, -2ll},
    {{7ll, 3ll}, 2ll, 3ll},
    {{kMin, 9223372036854775807ll}, -2ll, -1ll},
    {{9223372036854775807ll, 9223372036854775806ll}, 1ll, 2ll},
    {{-9223372036854775807ll, 9223372036854775806ll}, -2ll, -1ll},
    {{-1ll, 9223372036854775807ll}, -1ll, 0ll},
    {{1ll, 9223372036854775807ll}, 0ll, 1ll},
    {{4611686018427387904ll, 1ll}, 4611686018427387904ll, 4611686018427387904ll},
    {{-4611686018427387904ll, 3ll}, -1537228672809129302ll, -1537228672809129301ll},
    {{kMin, 9223372036854775805ll}, -2ll, -1ll},
    {{9223372036854775806ll, 9223372036854775807ll}, 0ll, 1ll},
};


constexpr std::uint64_t kGold = 0x9E3779B97F4A7C15ull;
const Ratio kSentinel = {0x1234567890ABCDEll, 0x0FEDCBA987654321ll};

std::uint32_t code(RatioResult r) { return static_cast<std::uint32_t>(r); }
bool same(const Ratio& a, const Ratio& b) { return std::memcmp(&a, &b, sizeof(Ratio)) == 0; }

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  std::size_t ops = 0;
  void add(std::uint64_t v) { h = (h ^ v) * 0x100000001B3ull; }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  }
  std::uint64_t next64() {
    const std::uint64_t hi = next();
    const std::uint64_t lo = next();
    return (hi << 32) | lo;
  }
};

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Oracle: sign-magnitude big integer, 6 limbs of 32 bits (192 bits; the largest value formed is below 2^190). Not shared with ratio.cpp.
// ---------------------------------------------------------------------------------------------------------------------------------------------
constexpr int kL = 6;
struct Big {
  std::uint32_t w[kL];
};

Big big_u64(std::uint64_t v) {
  Big b;
  for (int i = 0; i < kL; ++i) b.w[i] = 0;
  b.w[0] = static_cast<std::uint32_t>(v & 0xFFFFFFFFull);
  b.w[1] = static_cast<std::uint32_t>(v >> 32);
  return b;
}
bool big_zero(const Big& a) {
  for (int i = 0; i < kL; ++i)
    if (a.w[i] != 0) return false;
  return true;
}
int big_cmp(const Big& a, const Big& b) {
  for (int i = kL - 1; i >= 0; --i) {
    if (a.w[i] != b.w[i]) return a.w[i] < b.w[i] ? -1 : 1;
  }
  return 0;
}
Big big_mul(const Big& a, const Big& b) {
  Big r = big_u64(0);
  for (int i = 0; i < kL; ++i) {
    std::uint64_t carry = 0;
    for (int j = 0; i + j < kL; ++j) {
      const std::uint64_t cur = static_cast<std::uint64_t>(r.w[i + j]) + static_cast<std::uint64_t>(a.w[i]) * b.w[j] + carry;
      r.w[i + j] = static_cast<std::uint32_t>(cur & 0xFFFFFFFFull);
      carry = cur >> 32;
    }
  }
  return r;
}
Big big_add(const Big& a, const Big& b) {
  Big r;
  std::uint64_t carry = 0;
  for (int i = 0; i < kL; ++i) {
    const std::uint64_t cur = static_cast<std::uint64_t>(a.w[i]) + b.w[i] + carry;
    r.w[i] = static_cast<std::uint32_t>(cur & 0xFFFFFFFFull);
    carry = cur >> 32;
  }
  return r;
}
Big big_sub(const Big& a, const Big& b) {  // a >= b
  Big r;
  std::int64_t borrow = 0;
  for (int i = 0; i < kL; ++i) {
    std::int64_t cur = static_cast<std::int64_t>(a.w[i]) - static_cast<std::int64_t>(b.w[i]) - borrow;
    borrow = cur < 0 ? 1 : 0;
    if (cur < 0) cur += 0x100000000ll;
    r.w[i] = static_cast<std::uint32_t>(cur);
  }
  return r;
}
void big_shr1(Big& a) {
  for (int i = 0; i < kL; ++i) {
    a.w[i] >>= 1;
    if (i + 1 < kL) a.w[i] |= (a.w[i + 1] & 1u) << 31;
  }
}
void big_shl1(Big& a) {
  for (int i = kL - 1; i >= 0; --i) {
    a.w[i] <<= 1;
    if (i > 0) a.w[i] |= a.w[i - 1] >> 31;
  }
}
bool big_even(const Big& a) { return (a.w[0] & 1u) == 0; }
// Binary gcd; gcd(0, x) = x.
Big big_gcd(Big a, Big b) {
  if (big_zero(a)) return b;
  if (big_zero(b)) return a;
  int shift = 0;
  while (big_even(a) && big_even(b)) {
    big_shr1(a);
    big_shr1(b);
    ++shift;
  }
  while (big_even(a)) big_shr1(a);
  while (!big_zero(b)) {
    while (big_even(b)) big_shr1(b);
    if (big_cmp(a, b) > 0) {
      const Big t = a;
      a = b;
      b = t;
    }
    b = big_sub(b, a);
  }
  while (shift-- > 0) big_shl1(a);
  return a;
}

struct SBig {
  bool neg;
  Big m;
};
SBig sb_from(std::int64_t v) {
  const std::uint64_t u = static_cast<std::uint64_t>(v);
  return SBig{v < 0, big_u64(v < 0 ? (~u + 1ull) : u)};
}
SBig sb_norm(SBig a) {
  if (big_zero(a.m)) a.neg = false;
  return a;
}
SBig sb_mul(const SBig& a, const SBig& b) { return sb_norm(SBig{a.neg != b.neg, big_mul(a.m, b.m)}); }
SBig sb_neg(SBig a) {
  a.neg = !a.neg;
  return sb_norm(a);
}
SBig sb_add(const SBig& a, const SBig& b) {
  if (a.neg == b.neg) return sb_norm(SBig{a.neg, big_add(a.m, b.m)});
  if (big_cmp(a.m, b.m) >= 0) return sb_norm(SBig{a.neg, big_sub(a.m, b.m)});
  return sb_norm(SBig{b.neg, big_sub(b.m, a.m)});
}
int sb_cmp(const SBig& a, const SBig& b) {
  if (a.neg != b.neg) return a.neg ? -1 : 1;
  const int c = big_cmp(a.m, b.m);
  return a.neg ? -c : c;
}

// Plain binary gcd on uint64_t (a different method from Euclid's remainder loop in the implementation).
std::uint64_t ogcd64(std::uint64_t a, std::uint64_t b) {
  if (a == 0) return b;
  if (b == 0) return a;
  int shift = 0;
  while (((a | b) & 1ull) == 0) {
    a >>= 1;
    b >>= 1;
    ++shift;
  }
  while ((a & 1ull) == 0) a >>= 1;
  while (b != 0) {
    while ((b & 1ull) == 0) b >>= 1;
    if (a > b) {
      const std::uint64_t t = a;
      a = b;
      b = t;
    }
    b -= a;
  }
  return a << shift;
}
std::uint64_t mag64(std::int64_t v) {
  const std::uint64_t u = static_cast<std::uint64_t>(v);
  return v < 0 ? (~u + 1ull) : u;
}
bool ovalid(const Ratio& r) { return r.den > 0 && ogcd64(mag64(r.num), static_cast<std::uint64_t>(r.den)) == 1ull; }

// Checks a result against the exact unreduced value N / D (D above 0). Returns an empty string when fine.
std::string check_exact(const SBig& n, const Big& d, RatioResult got, const Ratio& out, const Ratio& before) {
  const Big g = big_gcd(n.m, d);  // N = 0 gives g = D
  const Big maxnum = big_u64(n.neg ? 0x8000000000000000ull : 0x7FFFFFFFFFFFFFFFull);
  const Big maxden = big_u64(0x7FFFFFFFFFFFFFFFull);
  const bool fits = big_cmp(n.m, big_mul(maxnum, g)) <= 0 && big_cmp(d, big_mul(maxden, g)) <= 0;
  if (!fits) {
    if (got != RatioResult::Overflow) return "expected Overflow, got code " + std::to_string(code(got));
    if (!same(out, before)) return "Overflow changed the output";
    return "";
  }
  if (got != RatioResult::Ok) return "expected Ok, got code " + std::to_string(code(got));
  if (out.den <= 0) return "den not above 0";
  if (big_cmp(big_mul(big_u64(static_cast<std::uint64_t>(out.den)), g), d) != 0) return "den times g differs from D";
  if (big_cmp(big_mul(big_u64(mag64(out.num)), g), n.m) != 0) return "num times g differs from N";
  if ((out.num < 0) != n.neg) return "sign";
  return "";
}

enum Op { kAdd = 0, kSub = 1, kMul = 2, kDiv = 3 };
using BinFn = RatioResult (*)(const Ratio*, const Ratio*, Ratio*);
const BinFn kFns[4] = {ratio_add, ratio_sub, ratio_mul, ratio_div};

struct Tally {
  std::size_t bad = 0;
  std::string first;
  void fail(const std::string& what) {
    if (bad == 0) first = what;
    ++bad;
  }
};

std::string describe(const char* op, const Ratio& a, const Ratio& b) {
  char buf[200];
  std::snprintf(buf, sizeof buf, "%s (%lld / %lld) and (%lld / %lld): ", op, static_cast<long long>(a.num), static_cast<long long>(a.den),
                static_cast<long long>(b.num), static_cast<long long>(b.den));
  return buf;
}

void fold_result(Fold& f, RatioResult r, const Ratio& out) {
  ++f.ops;
  f.add(code(r));
  if (r == RatioResult::Ok) {
    f.add(static_cast<std::uint64_t>(out.num));
    f.add(static_cast<std::uint64_t>(out.den));
  }
}

// One binary operation: calls the implementation, checks it with the oracle, folds the digest.
void do_bin(Op op, const Ratio& a, const Ratio& b, Fold& f, Tally& t) {
  static const char* const kNames[4] = {"add", "sub", "mul", "div"};
  Ratio out = kSentinel;
  const RatioResult r = kFns[op](&a, &b, &out);
  fold_result(f, r, out);
  std::string err;
  if (!ovalid(a) || !ovalid(b)) {
    if (r != RatioResult::BadRatio) err = "expected BadRatio";
    else if (!same(out, kSentinel)) err = "BadRatio changed the output";
  } else if (op == kDiv && b.num == 0) {
    if (r != RatioResult::ZeroDivision) err = "expected ZeroDivision";
    else if (!same(out, kSentinel)) err = "ZeroDivision changed the output";
  } else {
    const SBig an = sb_from(a.num), bn = sb_from(b.num);
    const Big ad = big_u64(static_cast<std::uint64_t>(a.den)), bd = big_u64(static_cast<std::uint64_t>(b.den));
    SBig n;
    Big d;
    if (op == kAdd || op == kSub) {
      n = sb_add(sb_mul(an, SBig{false, bd}), sb_mul(op == kSub ? sb_neg(bn) : bn, SBig{false, ad}));
      d = big_mul(ad, bd);
    } else if (op == kMul) {
      n = sb_mul(an, bn);
      d = big_mul(ad, bd);
    } else {
      n = sb_mul(an, SBig{false, bd});
      d = big_mul(ad, bn.m);
      if (bn.neg) n = sb_neg(n);
    }
    err = check_exact(n, d, r, out, kSentinel);
  }
  if (!err.empty()) t.fail(describe(kNames[op], a, b) + err);
}

void do_cmp(const Ratio& a, const Ratio& b, Fold& f, Tally& t) {
  std::int32_t out = 77;
  const RatioResult r = ratio_compare(&a, &b, &out);
  ++f.ops;
  f.add(code(r));
  if (r == RatioResult::Ok) f.add(static_cast<std::uint64_t>(static_cast<std::int64_t>(out)));
  std::string err;
  if (!ovalid(a) || !ovalid(b)) {
    if (r != RatioResult::BadRatio) err = "expected BadRatio";
    else if (out != 77) err = "BadRatio changed the output";
  } else if (r != RatioResult::Ok) {
    err = "expected Ok";
  } else {
    const int want = sb_cmp(sb_mul(sb_from(a.num), SBig{false, big_u64(static_cast<std::uint64_t>(b.den))}),
                            sb_mul(sb_from(b.num), SBig{false, big_u64(static_cast<std::uint64_t>(a.den))}));
    if (out != want) err = "compare value " + std::to_string(out) + " expected " + std::to_string(want);
  }
  if (!err.empty()) t.fail(describe("compare", a, b) + err);
}

void do_five(const Ratio& a, const Ratio& b, Fold& f, Tally& t) {
  for (int o = 0; o < 4; ++o) do_bin(static_cast<Op>(o), a, b, f, t);
  do_cmp(a, b, f, t);
}

// floor: q * den <= num < (q + 1) * den; ceil: (q - 1) * den < num <= q * den.
void do_fc(const Ratio& a, Fold& f, Tally& t) {
  const std::int64_t s0 = 0x5151515151515151ll;
  std::int64_t fl = s0, ce = s0;
  const RatioResult rf = ratio_floor(&a, &fl);
  const RatioResult rc = ratio_ceil(&a, &ce);
  ++f.ops;
  f.add(code(rf));
  if (rf == RatioResult::Ok) f.add(static_cast<std::uint64_t>(fl));
  f.add(code(rc));
  if (rc == RatioResult::Ok) f.add(static_cast<std::uint64_t>(ce));
  std::string err;
  if (!ovalid(a)) {
    if (rf != RatioResult::BadRatio || rc != RatioResult::BadRatio) err = "expected BadRatio";
    else if (fl != s0 || ce != s0) err = "BadRatio changed the output";
  } else if (rf != RatioResult::Ok || rc != RatioResult::Ok) {
    err = "expected Ok";
  } else {
    const SBig num = sb_from(a.num), den = SBig{false, big_u64(static_cast<std::uint64_t>(a.den))}, one = sb_from(1), mone = sb_from(-1);
    const SBig qf = sb_from(fl), qc = sb_from(ce);
    if (!(sb_cmp(sb_mul(qf, den), num) <= 0 && sb_cmp(num, sb_mul(sb_add(qf, one), den)) < 0)) err = "floor";
    if (!(sb_cmp(sb_mul(sb_add(qc, mone), den), num) < 0 && sb_cmp(num, sb_mul(qc, den)) <= 0)) err += "ceil";
  }
  if (!err.empty()) {
    char buf[120];
    std::snprintf(buf, sizeof buf, "floor/ceil (%lld / %lld): ", static_cast<long long>(a.num), static_cast<long long>(a.den));
    t.fail(buf + err);
  }
}

void do_make(std::int64_t n, std::int64_t d, Fold& f, Tally& t) {
  Ratio out = kSentinel;
  const RatioResult r = ratio_make(n, d, &out);
  fold_result(f, r, out);
  std::string err;
  if (d == 0) {
    if (r != RatioResult::ZeroDivision) err = "expected ZeroDivision";
    else if (!same(out, kSentinel)) err = "ZeroDivision changed the output";
  } else {
    SBig nn = sb_from(n);
    if (d < 0) nn = sb_neg(nn);
    err = check_exact(nn, big_u64(mag64(d)), r, out, kSentinel);
  }
  if (!err.empty()) {
    char buf[120];
    std::snprintf(buf, sizeof buf, "make (%lld, %lld): ", static_cast<long long>(n), static_cast<long long>(d));
    t.fail(buf + err);
  }
}

std::vector<Ratio> grid_g() {
  std::vector<Ratio> g;
  for (std::size_t i = 0; i < kRefSizeNs; ++i)
    for (std::size_t j = 0; j < kRefSizeDs; ++j) {
      const Ratio r = {kRefNs[i], kRefDs[j]};
      if (ovalid(r)) g.push_back(r);
    }
  return g;
}
std::vector<Ratio> grid_b() {
  std::vector<Ratio> b;
  for (std::size_t i = 0; i < kRefSizeNs; ++i)
    for (std::size_t j = 0; j < kRefSizeDm; ++j) {
      const Ratio r = {kRefNs[i], kRefDm[j]};
      if (!ovalid(r)) b.push_back(r);
    }
  return b;
}
std::vector<Ratio> grid_d12() {
  std::vector<Ratio> v;
  for (std::int64_t n = -12; n <= 12; ++n)
    for (std::int64_t d = 1; d <= 12; ++d) {
      const Ratio r = {n, d};
      if (ovalid(r)) v.push_back(r);
    }
  return v;
}

Ratio rand_ratio(Lcg& r) {
  const std::uint32_t bn = r.next() % 64u;
  const std::uint32_t bd = r.next() % 63u + 1u;
  const std::uint64_t nm = bn != 0 ? (r.next64() >> (64u - bn)) : (static_cast<void>(r.next64()), 0ull);
  std::uint64_t dm = r.next64() >> (64u - bd);
  const std::uint32_t sg = r.next() & 1u;
  if (dm == 0) dm = 1;
  const std::uint64_t g = ogcd64(nm, dm);
  const std::int64_t n = static_cast<std::int64_t>(nm / g);
  return Ratio{sg != 0 ? -n : n, static_cast<std::int64_t>(dm / g)};
}
void rand_wide(Lcg& r, std::int64_t* n, std::int64_t* d) {
  const std::uint32_t bn = r.next() % 65u;
  const std::uint32_t bd = r.next() % 65u;
  const std::uint64_t n64 = r.next64();
  const std::uint64_t d64 = r.next64();
  *n = bn != 0 ? static_cast<std::int64_t>(n64 >> (64u - bn)) : 0;
  *d = bd != 0 ? static_cast<std::int64_t>(d64 >> (64u - bd)) : 0;
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::size_t ops, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.ops == ops);
  CHECK(f.h == digest);
}

}  // namespace

// =============================================================================================================================================
// Grid sizes are the ones the reference printed (they come from the same definitions, recomputed here with a different gcd).
// =============================================================================================================================================
TEST_CASE("grid definitions match the reference", "[ratio]") {
  CHECK(grid_g().size() == kRefSizeG);
  CHECK(grid_b().size() == kRefSizeB);
  CHECK(grid_d12().size() == kRefSizeD12);
}

// =============================================================================================================================================
// EXHAUSTIVE: ratio_make over every (num in Ns, den in Dm). Digest + oracle.
// =============================================================================================================================================
TEST_CASE("make: exhaustive over Ns x Dm", "[ratio]") {
  Fold f;
  Tally t;
  for (std::size_t i = 0; i < kRefSizeNs; ++i)
    for (std::size_t j = 0; j < kRefSizeDm; ++j) {
      ++f.cases;
      do_make(kRefNs[i], kRefDm[j], f, t);
    }
  expect_digest(f, t, kRefCases_make, kRefOps_make, kRefDigest_make);
}

// CONSTANTS: hand-picked make rows (computed with Python Fractions).
TEST_CASE("make: extreme rows", "[ratio]") {
  for (const MakeRow& row : kMakeRows) {
    Ratio out = kSentinel;
    const RatioResult r = ratio_make(row.num, row.den, &out);
    INFO("make " << row.num << " / " << row.den);
    CHECK(code(r) == row.code);
    if (row.code == 0) {
      CHECK(out.num == row.rnum);
      CHECK(out.den == row.rden);
    } else {
      CHECK(same(out, kSentinel));
    }
  }
}

// =============================================================================================================================================
// EXHAUSTIVE: add, sub, mul, div, compare over every ordered pair of G (555 ratios, 308025 pairs). Digest + oracle.
// =============================================================================================================================================
TEST_CASE("binary operations: exhaustive over G x G", "[ratio]") {
  const std::vector<Ratio> g = grid_g();
  Fold f;
  Tally t;
  for (const Ratio& a : g)
    for (const Ratio& b : g) {
      ++f.cases;
      do_five(a, b, f, t);
    }
  expect_digest(f, t, kRefCases_grid, kRefOps_grid, kRefDigest_grid);
}

TEST_CASE("floor and ceil: exhaustive over G", "[ratio]") {
  const std::vector<Ratio> g = grid_g();
  Fold f;
  Tally t;
  for (const Ratio& a : g) {
    ++f.cases;
    do_fc(a, f, t);
  }
  expect_digest(f, t, kRefCases_fc, kRefOps_fc, kRefDigest_fc);
}

// =============================================================================================================================================
// EXHAUSTIVE: every invalid (num, den) of Ns x Dm (B) against the 12 partners, as the first and as the second argument, for all five two-argument
// functions, and floor / ceil of every element of B. Digest + oracle (BadRatio, output untouched).
// =============================================================================================================================================
TEST_CASE("BadRatio: exhaustive over B x partners", "[ratio]") {
  const std::vector<Ratio> b = grid_b();
  Fold f;
  Tally t;
  for (const Ratio& x : b) {
    for (const Ratio& p : kRefPartners) {
      ++f.cases;
      do_five(x, p, f, t);
      do_five(p, x, f, t);
    }
    ++f.cases;
    do_fc(x, f, t);
  }
  expect_digest(f, t, kRefCases_bad, kRefOps_bad, kRefDigest_bad);
}

// =============================================================================================================================================
// EXHAUSTIVE: every ordered pair of the small grid D12 (n in -12..12, d in 1..12, valid only). Digest + oracle.
// =============================================================================================================================================
TEST_CASE("binary operations: exhaustive over D12 x D12", "[ratio]") {
  const std::vector<Ratio> v = grid_d12();
  Fold f;
  Tally t;
  for (const Ratio& a : v)
    for (const Ratio& b : v) {
      ++f.cases;
      do_five(a, b, f, t);
    }
  expect_digest(f, t, kRefCases_d12, kRefOps_d12, kRefDigest_d12);
}

// =============================================================================================================================================
// NOT EXHAUSTIVE: 400 seeded LCG scenarios of 10 random pairs (valid ratios, bit lengths up to 63) and 10 random make calls (bit lengths up to 64).
// =============================================================================================================================================
TEST_CASE("random pairs (sampled, not exhaustive)", "[ratio]") {
  Fold f;
  Tally t;
  for (std::uint32_t s = 0; s < kRefRandSeeds; ++s) {
    Lcg r(static_cast<std::uint64_t>(s + 1u) * kGold);
    for (std::uint32_t k = 0; k < kRefRandPerSeed; ++k) {
      const Ratio a = rand_ratio(r);
      const Ratio b = rand_ratio(r);
      ++f.cases;
      do_five(a, b, f, t);
    }
  }
  expect_digest(f, t, kRefCases_randp, kRefOps_randp, kRefDigest_randp);
}

TEST_CASE("random make (sampled, not exhaustive)", "[ratio]") {
  Fold f;
  Tally t;
  for (std::uint32_t s = 0; s < kRefRandSeeds; ++s) {
    Lcg r(static_cast<std::uint64_t>(s + 1u) * kGold);
    for (std::uint32_t k = 0; k < kRefRandPerSeed; ++k) {
      std::int64_t n = 0, d = 0;
      rand_wide(r, &n, &d);
      ++f.cases;
      do_make(n, d, f, t);
    }
  }
  expect_digest(f, t, kRefCases_randm, kRefOps_randm, kRefDigest_randm);
}

// =============================================================================================================================================
// CONSTANTS: hand-picked rows with extreme values (products near 2^126, sums whose intermediates overflow, results needing den above INT64_MAX).
// =============================================================================================================================================
TEST_CASE("extreme rows: add sub mul div", "[ratio]") {
  for (const BinRow& row : kBinRows) {
    Ratio out = kSentinel;
    const RatioResult r = kFns[row.op](&row.a, &row.b, &out);
    INFO("op " << row.op << " a " << row.a.num << " / " << row.a.den << " b " << row.b.num << " / " << row.b.den);
    CHECK(code(r) == row.code);
    if (row.code == 0) {
      CHECK(out.num == row.rnum);
      CHECK(out.den == row.rden);
    } else {
      CHECK(same(out, kSentinel));
    }
  }
}

TEST_CASE("extreme rows: compare", "[ratio]") {
  for (const CmpRow& row : kCmpRows) {
    std::int32_t out = 77;
    INFO("a " << row.a.num << " / " << row.a.den << " b " << row.b.num << " / " << row.b.den);
    CHECK(ratio_compare(&row.a, &row.b, &out) == RatioResult::Ok);
    CHECK(out == row.want);
    // swapped arguments give the opposite sign
    CHECK(ratio_compare(&row.b, &row.a, &out) == RatioResult::Ok);
    CHECK(out == -row.want);
  }
}

TEST_CASE("extreme rows: floor and ceil", "[ratio]") {
  for (const FcRow& row : kFcRows) {
    std::int64_t fl = 0, ce = 0;
    INFO("a " << row.a.num << " / " << row.a.den);
    CHECK(ratio_floor(&row.a, &fl) == RatioResult::Ok);
    CHECK(ratio_ceil(&row.a, &ce) == RatioResult::Ok);
    CHECK(fl == row.floor);
    CHECK(ce == row.ceil);
  }
}

// =============================================================================================================================================
// Validity, check order, nulls: written by hand from the header text.
// =============================================================================================================================================
namespace {
const Ratio kBadVals[] = {{1, 0},      {1, -1},    {kMin, kMin}, {1, kMin},   {3, kMin},   {kMin, 2},  {kMin, -1}, {2, 4},   {0, 2},
                          {0, 0},      {0, -1},    {kMax, kMax}, {-4, 6},     {6, 9},      {kMin, 4},  {-6, 8},    {0, kMax}};
const Ratio kGoodVals[] = {{0, 1},    {1, 1},     {-1, 1},      {kMin, 1},  {kMax, 1},  {kMin, 3},  {kMin, kMax}, {kMin, kMax - 2},
                           {1, kMax}, {-1, kMax}, {kMin + 1, 1}, {1, 2},     {-1, 2},    {kMax, kMax - 1}};
}  // namespace

TEST_CASE("validity: INT64_MIN as a numerator is valid with an odd denominator only", "[ratio]") {
  for (const Ratio& g : kGoodVals) {
    std::int32_t c = 77;
    INFO("good " << g.num << " / " << g.den);
    CHECK(ratio_compare(&g, &g, &c) == RatioResult::Ok);
    CHECK(c == 0);
  }
  for (const Ratio& b : kBadVals) {
    std::int32_t c = 77;
    INFO("bad " << b.num << " / " << b.den);
    CHECK(ratio_compare(&b, &kGoodVals[1], &c) == RatioResult::BadRatio);
    CHECK(ratio_compare(&kGoodVals[1], &b, &c) == RatioResult::BadRatio);
    CHECK(c == 77);
  }
}

TEST_CASE("BadRatio on every function, first and second argument, output untouched", "[ratio]") {
  for (const Ratio& b : kBadVals) {
    for (const Ratio& g : kGoodVals) {
      for (int o = 0; o < 4; ++o) {
        Ratio out = kSentinel;
        INFO("op " << o << " bad " << b.num << " / " << b.den << " good " << g.num << " / " << g.den);
        CHECK(kFns[o](&b, &g, &out) == RatioResult::BadRatio);
        CHECK(same(out, kSentinel));
        CHECK(kFns[o](&g, &b, &out) == RatioResult::BadRatio);
        CHECK(same(out, kSentinel));
      }
      std::int64_t fc = 5;
      CHECK(ratio_floor(&b, &fc) == RatioResult::BadRatio);
      CHECK(ratio_ceil(&b, &fc) == RatioResult::BadRatio);
      CHECK(fc == 5);
    }
  }
}

TEST_CASE("check order: null, then BadRatio, then ZeroDivision, then Overflow", "[ratio]") {
  const Ratio one = {1, 1}, zero = {0, 1}, minv = {kMin, 1}, bad = {2, 4}, bad0 = {0, 5};
  Ratio out = kSentinel;
  std::int32_t c = 77;
  std::int64_t fc = 5;
  for (int o = 0; o < 4; ++o) {
    // null pointer beats everything
    CHECK(kFns[o](nullptr, &one, &out) == RatioResult::NullArgument);
    CHECK(kFns[o](&one, nullptr, &out) == RatioResult::NullArgument);
    CHECK(kFns[o](&one, &one, nullptr) == RatioResult::NullArgument);
    CHECK(kFns[o](nullptr, nullptr, nullptr) == RatioResult::NullArgument);
    CHECK(kFns[o](nullptr, &bad, &out) == RatioResult::NullArgument);
    CHECK(kFns[o](&bad, nullptr, &out) == RatioResult::NullArgument);
    CHECK(kFns[o](&bad, &bad, nullptr) == RatioResult::NullArgument);
    CHECK(kFns[o](&bad, &zero, nullptr) == RatioResult::NullArgument);
    CHECK(same(out, kSentinel));
  }
  // BadRatio beats ZeroDivision and Overflow
  CHECK(ratio_div(&bad, &zero, &out) == RatioResult::BadRatio);
  CHECK(ratio_div(&minv, &bad0, &out) == RatioResult::BadRatio);
  CHECK(ratio_div(&bad0, &zero, &out) == RatioResult::BadRatio);
  CHECK(ratio_div(&one, &bad0, &out) == RatioResult::BadRatio);
  CHECK(ratio_add(&minv, &bad, &out) == RatioResult::BadRatio);
  CHECK(ratio_sub(&bad, &minv, &out) == RatioResult::BadRatio);
  CHECK(same(out, kSentinel));
  // ZeroDivision beats Overflow
  CHECK(ratio_div(&minv, &zero, &out) == RatioResult::ZeroDivision);
  CHECK(ratio_div(&zero, &zero, &out) == RatioResult::ZeroDivision);
  CHECK(ratio_div(&one, &zero, &out) == RatioResult::ZeroDivision);
  CHECK(same(out, kSentinel));
  // only div has ZeroDivision: dividing zero by a value and multiplying by zero are fine
  CHECK(ratio_div(&zero, &minv, &out) == RatioResult::Ok);
  CHECK(out.num == 0);
  CHECK(out.den == 1);
  out = kSentinel;
  // Overflow is last and leaves the output untouched
  const Ratio maxv = {kMax, 1};
  CHECK(ratio_add(&maxv, &one, &out) == RatioResult::Overflow);
  CHECK(ratio_div(&one, &minv, &out) == RatioResult::Overflow);
  CHECK(ratio_mul(&minv, &minv, &out) == RatioResult::Overflow);
  CHECK(same(out, kSentinel));
  // compare, floor, ceil
  CHECK(ratio_compare(nullptr, &one, &c) == RatioResult::NullArgument);
  CHECK(ratio_compare(&one, nullptr, &c) == RatioResult::NullArgument);
  CHECK(ratio_compare(&one, &one, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_compare(nullptr, &bad, &c) == RatioResult::NullArgument);
  CHECK(ratio_compare(&bad, &bad, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_compare(&bad, &one, &c) == RatioResult::BadRatio);
  CHECK(c == 77);
  CHECK(ratio_floor(nullptr, &fc) == RatioResult::NullArgument);
  CHECK(ratio_floor(&one, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_floor(&bad, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_floor(nullptr, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_ceil(nullptr, &fc) == RatioResult::NullArgument);
  CHECK(ratio_ceil(&one, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_ceil(&bad, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_ceil(nullptr, nullptr) == RatioResult::NullArgument);
  CHECK(fc == 5);
  // make: null out beats den = 0 (and Overflow); den = 0 beats Overflow
  CHECK(ratio_make(1, 2, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_make(1, 0, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_make(kMin, -1, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_make(5, 0, &out) == RatioResult::ZeroDivision);
  CHECK(ratio_make(0, 0, &out) == RatioResult::ZeroDivision);
  CHECK(ratio_make(kMin, 0, &out) == RatioResult::ZeroDivision);
  CHECK(ratio_make(kMin, -1, &out) == RatioResult::Overflow);
  CHECK(same(out, kSentinel));
}

TEST_CASE("null matrix: each pointer alone for every function", "[ratio]") {
  const Ratio v = {3, 7};
  Ratio out = kSentinel;
  std::int32_t c = 77;
  std::int64_t fc = 5;
  for (int o = 0; o < 4; ++o) {
    CHECK(kFns[o](nullptr, &v, &out) == RatioResult::NullArgument);
    CHECK(kFns[o](&v, nullptr, &out) == RatioResult::NullArgument);
    CHECK(kFns[o](&v, &v, nullptr) == RatioResult::NullArgument);
  }
  CHECK(ratio_compare(nullptr, &v, &c) == RatioResult::NullArgument);
  CHECK(ratio_compare(&v, nullptr, &c) == RatioResult::NullArgument);
  CHECK(ratio_compare(&v, &v, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_floor(nullptr, &fc) == RatioResult::NullArgument);
  CHECK(ratio_floor(&v, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_ceil(nullptr, &fc) == RatioResult::NullArgument);
  CHECK(ratio_ceil(&v, nullptr) == RatioResult::NullArgument);
  CHECK(ratio_make(3, 7, nullptr) == RatioResult::NullArgument);
  CHECK(same(out, kSentinel));
  CHECK(c == 77);
  CHECK(fc == 5);
}

// =============================================================================================================================================
// Aliasing: out may equal a or b. Checked over D12 x D12 (every ordered pair) and over every 7th element of G squared. On success the aliased result
// equals the non-aliased one; on failure the aliased input is unchanged.
// =============================================================================================================================================
TEST_CASE("aliasing: out equal to a, to b, and a = b = out", "[ratio]") {
  std::vector<Ratio> v = grid_d12();
  const std::vector<Ratio> g = grid_g();
  for (std::size_t i = 0; i < g.size(); i += 7) v.push_back(g[i]);
  for (const Ratio& p : kRefPartners) v.push_back(p);
  std::size_t bad = 0, checked = 0;
  std::string first;
  for (const Ratio& a : v)
    for (const Ratio& b : v) {
      for (int o = 0; o < 4; ++o) {
        Ratio plain = kSentinel;
        const RatioResult rp = kFns[o](&a, &b, &plain);
        Ratio x = a;  // out == a
        const RatioResult r1 = kFns[o](&x, &b, &x);
        Ratio y = b;  // out == b
        const RatioResult r2 = kFns[o](&a, &y, &y);
        ++checked;
        bool ok = r1 == rp && r2 == rp;
        if (rp == RatioResult::Ok) {
          ok = ok && same(x, plain) && same(y, plain);
        } else {
          ok = ok && same(x, a) && same(y, b);
        }
        if (!ok) {
          if (bad == 0) first = describe("alias", a, b) + "op " + std::to_string(o);
          ++bad;
        }
      }
    }
  for (const Ratio& a : v) {
    for (int o = 0; o < 4; ++o) {
      Ratio plain = kSentinel;
      const RatioResult rp = kFns[o](&a, &a, &plain);
      Ratio x = a;  // a = b = out
      const RatioResult r = kFns[o](&x, &x, &x);
      ++checked;
      const bool ok = r == rp && (rp == RatioResult::Ok ? same(x, plain) : same(x, a));
      if (!ok) {
        if (bad == 0) first = describe("alias3", a, a) + "op " + std::to_string(o);
        ++bad;
      }
    }
  }
  INFO(first);
  CHECK(bad == 0);
  CHECK(checked > 100000u);
}

// =============================================================================================================================================
// Selected identities near the limits (hand-written; each also appears in the exhaustive grids where the values are in them).
// =============================================================================================================================================
TEST_CASE("cancellation and extremes", "[ratio]") {
  Ratio out = kSentinel;
  const Ratio a = {kMax, kMax - 1}, b = {kMax - 1, kMax};  // a * b = 1 although both products are near 2^126
  CHECK(ratio_mul(&a, &b, &out) == RatioResult::Ok);
  CHECK(out.num == 1);
  CHECK(out.den == 1);
  const Ratio minv = {kMin, 1}, maxv = {kMax, 1};
  CHECK(ratio_add(&minv, &maxv, &out) == RatioResult::Ok);
  CHECK(out.num == -1);
  CHECK(out.den == 1);
  CHECK(ratio_sub(&minv, &minv, &out) == RatioResult::Ok);
  CHECK(out.num == 0);
  CHECK(out.den == 1);
  CHECK(ratio_div(&minv, &minv, &out) == RatioResult::Ok);
  CHECK(out.num == 1);
  CHECK(out.den == 1);
  out = kSentinel;
  const Ratio one = {1, 1};
  CHECK(ratio_div(&one, &minv, &out) == RatioResult::Overflow);  // -1 / 2^63
  CHECK(same(out, kSentinel));
  Ratio r = {kMin, 1};
  CHECK(ratio_mul(&r, &one, &r) == RatioResult::Ok);
  CHECK(r.num == kMin);
  CHECK(r.den == 1);
}

// =============================================================================================================================================
// Long Euclid chains: consecutive Fibonacci numbers are the worst case for the gcd (about 90 steps for 64-bit values). Ratios F(k) / F(k+1), k = 60
// to 91 (F(92) is the largest Fibonacci number below 2^63), all ordered pairs, all five functions, checked by the oracle (not digest-backed).
// =============================================================================================================================================
TEST_CASE("long gcd chains: Fibonacci ratios", "[ratio]") {
  std::uint64_t fib[94];
  fib[0] = 0;
  fib[1] = 1;
  for (int i = 2; i < 94; ++i) fib[i] = fib[i - 1] + fib[i - 2];
  std::vector<Ratio> v;
  for (int k = 60; k <= 91; ++k) {
    v.push_back(Ratio{static_cast<std::int64_t>(fib[k]), static_cast<std::int64_t>(fib[k + 1])});
    v.push_back(Ratio{-static_cast<std::int64_t>(fib[k]), static_cast<std::int64_t>(fib[k + 1])});
  }
  v.push_back(Ratio{static_cast<std::int64_t>(fib[91]), static_cast<std::int64_t>(fib[92])});
  Fold f;
  Tally t;
  for (const Ratio& a : v) {
    CHECK(ovalid(a));
    for (const Ratio& b : v) do_five(a, b, f, t);
    do_fc(a, f, t);
  }
  // make with unreduced Fibonacci pairs: F(k) * m / (F(k+1) * m) for small m that fits
  for (int k = 60; k <= 90; ++k)
    for (std::int64_t m = 1; m <= 5; ++m) {
      const std::uint64_t n = fib[k] * static_cast<std::uint64_t>(m), d = fib[k + 1] * static_cast<std::uint64_t>(m);
      if (n <= static_cast<std::uint64_t>(kMax) && d <= static_cast<std::uint64_t>(kMax)) do_make(static_cast<std::int64_t>(n), static_cast<std::int64_t>(d), f, t);
    }
  INFO(t.first);
  CHECK(t.bad == 0);
}
