// Tests for core/ratapprox. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/ratapprox_ref.py, an independent Python implementation
//      (fractions.Fraction, a brute force over every denominator and a continued fraction method that the script cross-checks against each other).
//      The tests re-run every scenario against the real functions and recompute the 64-bit digest and the counts.
//  (2) ORACLE: naive in-test oracles that share no code with core/ratapprox/ratapprox.cpp: (a) a sign-magnitude big integer (6 limbs of 32 bits,
//      schoolbook multiply, binary gcd; no 128-bit type, no compiler builtin) that checks every returned fraction against the DEFINITION: lowest
//      terms, 1 <= den <= max_den, lo <= x <= hi, no candidate between (determinant 1 and den sum above max_den), x a candidate gives lo = hi = x,
//      and the nearest by an exact distance comparison with the tie rule; (b) a plain int64 brute force over every denominator 1..max_den, used when
//      the reduced x and max_den are small. The oracles run on EVERY case of the grids below.
//  (3) CONSTANTS: hand-picked rows (extremes, ties with large neighbours, the success rows next to the Overflow family) were computed once with
//      Python Fractions and written in as constants; the null, check-order, BadBound and Overflow rows are written by hand from the header text.
// After every failing call the outputs are compared byte for byte with the sentinels that were passed (the header promises "untouched").
// What is NOT done: no assertion on anything the header leaves open; no timing test (every loop in the implementation has a fixed bound, argued in
// the docs); Windows is CI-only; the random, Fibonacci and hand scenarios are NOT exhaustive.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "ratapprox.h"

using namespace lumora::ratapprox;

namespace {

constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();

// BEGIN GENERATED REFERENCE (tools/ratapprox_ref.py)
const std::uint32_t kRefRandSeeds = 400, kRefRandPerSeed = 10;
const std::int64_t kRefNi[] = {kMin, -9223372036854775807ll, -9223372036854775806ll, -4611686018427387905ll, -4611686018427387904ll, -4611686018427387903ll, -4294967297ll, -4294967296ll, -4294967295ll, -2147483649ll, -2147483648ll, -2147483647ll, -20ll, -19ll, -18ll, -17ll, -16ll, -15ll, -14ll, -13ll, -12ll, -11ll, -10ll, -9ll, -8ll, -7ll, -6ll, -5ll, -4ll, -3ll, -2ll, -1ll, 0ll, 1ll, 2ll, 3ll, 4ll, 5ll, 6ll, 7ll, 8ll, 9ll, 10ll, 11ll, 12ll, 13ll, 14ll, 15ll, 16ll, 17ll, 18ll, 19ll, 20ll, 2147483647ll, 2147483648ll, 2147483649ll, 4294967295ll, 4294967296ll, 4294967297ll, 4611686018427387903ll, 4611686018427387904ll, 4611686018427387905ll, 9223372036854775805ll, 9223372036854775806ll, 9223372036854775807ll};
const std::int64_t kRefDi[] = {kMin, -9223372036854775807ll, -9223372036854775806ll, -4611686018427387905ll, -4611686018427387904ll, -4611686018427387903ll, -4294967297ll, -4294967296ll, -4294967295ll, -2147483649ll, -2147483648ll, -2147483647ll, -20ll, -19ll, -18ll, -17ll, -16ll, -15ll, -14ll, -13ll, -12ll, -11ll, -10ll, -9ll, -8ll, -7ll, -6ll, -5ll, -4ll, -3ll, -2ll, -1ll, 1ll, 2ll, 3ll, 4ll, 5ll, 6ll, 7ll, 8ll, 9ll, 10ll, 11ll, 12ll, 13ll, 14ll, 15ll, 16ll, 17ll, 18ll, 19ll, 20ll, 2147483647ll, 2147483648ll, 2147483649ll, 4294967295ll, 4294967296ll, 4294967297ll, 4611686018427387903ll, 4611686018427387904ll, 4611686018427387905ll, 9223372036854775805ll, 9223372036854775806ll, 9223372036854775807ll};
const std::uint64_t kRefNb[] = {1ull, 2ull, 3ull, 4ull, 5ull, 6ull, 7ull, 8ull, 9ull, 10ull, 11ull, 12ull, 15ull, 16ull, 17ull, 100ull, 255ull, 256ull, 257ull, 65535ull, 65536ull, 65537ull, 2147483647ull, 2147483648ull, 4294967295ull, 4294967296ull, 4294967297ull, 4611686018427387904ull, 4611686018427387905ull, 9223372036854775806ull, 9223372036854775807ull};
const std::int64_t kRefOvfD[] = {1ll, 3ll, 5ll, 7ll, 9ll, 11ll, 13ll, 15ll, 17ll, 19ll, 2147483647ll, 2147483649ll, 4294967295ll, 4294967297ll, 4611686018427387903ll, 4611686018427387905ll, 9223372036854775805ll, 9223372036854775807ll};
const std::size_t kRefSizeNi = 65, kRefSizeDi = 64, kRefSizeNb = 31, kRefSizeOvfD = 18, kRefGridASkipped = 273, kRefGridBX = 1959;
const std::uint64_t kRefDigest_gridA = 0xDC7517164531F851ull;
const std::size_t kRefCases_gridA = 132023, kRefOps_gridA = 132023;
const std::uint64_t kRefDigest_gridB = 0xFAA7D72461F3DC1Dull;
const std::size_t kRefCases_gridB = 250752, kRefOps_gridB = 250752;
const std::uint64_t kRefDigest_ties = 0xB84E2D30F97961BDull;
const std::size_t kRefCases_ties = 5964, kRefOps_ties = 5964;
const std::uint64_t kRefDigest_ovf = 0xF73C6608E4A6704Aull;
const std::size_t kRefCases_ovf = 102, kRefOps_ovf = 102;
const std::uint64_t kRefDigest_fib = 0x3A3882EC1D1C4F63ull;
const std::size_t kRefCases_fib = 5376, kRefOps_fib = 5376;
const std::uint64_t kRefDigest_rand = 0xA47F85E2BE6966F0ull;
const std::size_t kRefCases_rand = 4000, kRefOps_rand = 4000;
// END GENERATED REFERENCE

struct HandRow {
  std::int64_t num;
  std::int64_t den;
  std::uint64_t n;
  std::uint32_t code;
  Frac lo;
  Frac hi;
  Frac near;
};
const HandRow kHandRows[] = {
    {kMin, -2ll, 1ull, 0, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}},
    {kMin, -2ll, 2ull, 0, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}},
    {kMin, -2ll, 3ull, 0, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}},
    {kMin, -2ll, 4ull, 0, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}},
    {kMin, -2ll, 5ull, 0, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}},
    {kMin, -2ll, 6ull, 0, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}},
    {kMin, -2ll, 9223372036854775807ull, 0, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}, {4611686018427387904ll, 1ll}},
    {kMin, -4ll, 1ull, 0, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}},
    {kMin, -4ll, 2ull, 0, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}},
    {kMin, -4ll, 3ull, 0, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}},
    {kMin, -4ll, 4ull, 0, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}},
    {kMin, -4ll, 5ull, 0, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}},
    {kMin, -4ll, 6ull, 0, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}},
    {kMin, -4ll, 9223372036854775807ull, 0, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}, {2305843009213693952ll, 1ll}},
    {kMin, -6ll, 1ull, 0, {1537228672809129301ll, 1ll}, {1537228672809129302ll, 1ll}, {1537228672809129301ll, 1ll}},
    {kMin, -6ll, 2ull, 0, {1537228672809129301ll, 1ll}, {3074457345618258603ll, 2ll}, {3074457345618258603ll, 2ll}},
    {kMin, -6ll, 3ull, 0, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}},
    {kMin, -6ll, 4ull, 0, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}},
    {kMin, -6ll, 5ull, 0, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}},
    {kMin, -6ll, 6ull, 0, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}},
    {kMin, -6ll, 9223372036854775807ull, 0, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}},
    {kMin, 1ll, 1ull, 0, {kMin, 1ll}, {kMin, 1ll}, {kMin, 1ll}},
    {kMin, 1ll, 2ull, 0, {kMin, 1ll}, {kMin, 1ll}, {kMin, 1ll}},
    {kMin, 1ll, 9223372036854775807ull, 0, {kMin, 1ll}, {kMin, 1ll}, {kMin, 1ll}},
    {kMin, 3ll, 1ull, 0, {-3074457345618258603ll, 1ll}, {-3074457345618258602ll, 1ll}, {-3074457345618258603ll, 1ll}},
    {kMin, 3ll, 2ull, 0, {-3074457345618258603ll, 1ll}, {-6148914691236517205ll, 2ll}, {-6148914691236517205ll, 2ll}},
    {kMin, 3ll, 3ull, 0, {kMin, 3ll}, {kMin, 3ll}, {kMin, 3ll}},
    {kMin, 3ll, 4ull, 0, {kMin, 3ll}, {kMin, 3ll}, {kMin, 3ll}},
    {kMin, 3ll, 9223372036854775807ull, 0, {kMin, 3ll}, {kMin, 3ll}, {kMin, 3ll}},
    {kMin, -1ll, 1ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -1ll, 2ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -1ll, 9223372036854775807ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -3ll, 1ull, 0, {3074457345618258602ll, 1ll}, {3074457345618258603ll, 1ll}, {3074457345618258603ll, 1ll}},
    {kMin, -3ll, 2ull, 0, {6148914691236517205ll, 2ll}, {3074457345618258603ll, 1ll}, {6148914691236517205ll, 2ll}},
    {kMin, -3ll, 3ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -3ll, 4ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -3ll, 9223372036854775807ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -5ll, 4ull, 0, {3689348814741910323ll, 2ll}, {5534023222112865485ll, 3ll}, {5534023222112865485ll, 3ll}},
    {kMin, -5ll, 5ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -5ll, 6ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -9223372036854775807ll, 9223372036854775806ull, 0, {1ll, 1ll}, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775807ll, 9223372036854775806ll}},
    {kMin, -9223372036854775807ll, 9223372036854775807ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -9223372036854775805ll, 9223372036854775804ull, 0, {3074457345618258603ll, 3074457345618258602ll}, {6148914691236517205ll, 6148914691236517203ll}, {6148914691236517205ll, 6148914691236517203ll}},
    {kMin, -9223372036854775805ll, 9223372036854775805ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {kMin, -9223372036854775805ll, 9223372036854775807ull, 4, {0, 0}, {0, 0}, {0, 0}},
    {9223372036854775807ll, 1ll, 1ull, 0, {9223372036854775807ll, 1ll}, {9223372036854775807ll, 1ll}, {9223372036854775807ll, 1ll}},
    {9223372036854775807ll, 1ll, 2ull, 0, {9223372036854775807ll, 1ll}, {9223372036854775807ll, 1ll}, {9223372036854775807ll, 1ll}},
    {9223372036854775807ll, 1ll, 9223372036854775807ull, 0, {9223372036854775807ll, 1ll}, {9223372036854775807ll, 1ll}, {9223372036854775807ll, 1ll}},
    {9223372036854775807ll, 2ll, 1ull, 0, {4611686018427387903ll, 1ll}, {4611686018427387904ll, 1ll}, {4611686018427387903ll, 1ll}},
    {9223372036854775807ll, 2ll, 2ull, 0, {9223372036854775807ll, 2ll}, {9223372036854775807ll, 2ll}, {9223372036854775807ll, 2ll}},
    {9223372036854775807ll, 2ll, 3ull, 0, {9223372036854775807ll, 2ll}, {9223372036854775807ll, 2ll}, {9223372036854775807ll, 2ll}},
    {9223372036854775807ll, 2ll, 9223372036854775807ull, 0, {9223372036854775807ll, 2ll}, {9223372036854775807ll, 2ll}, {9223372036854775807ll, 2ll}},
    {9223372036854775807ll, 9223372036854775807ll, 1ull, 0, {1ll, 1ll}, {1ll, 1ll}, {1ll, 1ll}},
    {9223372036854775807ll, 9223372036854775807ll, 9223372036854775807ull, 0, {1ll, 1ll}, {1ll, 1ll}, {1ll, 1ll}},
    {9223372036854775807ll, -1ll, 1ull, 0, {-9223372036854775807ll, 1ll}, {-9223372036854775807ll, 1ll}, {-9223372036854775807ll, 1ll}},
    {9223372036854775807ll, -1ll, 9223372036854775807ull, 0, {-9223372036854775807ll, 1ll}, {-9223372036854775807ll, 1ll}, {-9223372036854775807ll, 1ll}},
    {9223372036854775807ll, kMin, 1ull, 0, {-1ll, 1ll}, {0ll, 1ll}, {-1ll, 1ll}},
    {9223372036854775807ll, kMin, 2ull, 0, {-1ll, 1ll}, {-1ll, 2ll}, {-1ll, 1ll}},
    {9223372036854775807ll, kMin, 9223372036854775807ull, 0, {-1ll, 1ll}, {-9223372036854775806ll, 9223372036854775807ll}, {-9223372036854775806ll, 9223372036854775807ll}},
    {kMin, kMin, 1ull, 0, {1ll, 1ll}, {1ll, 1ll}, {1ll, 1ll}},
    {kMin, kMin, 9223372036854775807ull, 0, {1ll, 1ll}, {1ll, 1ll}, {1ll, 1ll}},
    {kMin, 9223372036854775807ll, 1ull, 0, {-2ll, 1ll}, {-1ll, 1ll}, {-1ll, 1ll}},
    {kMin, 9223372036854775807ll, 2ull, 0, {-3ll, 2ll}, {-1ll, 1ll}, {-1ll, 1ll}},
    {kMin, 9223372036854775807ll, 3ull, 0, {-4ll, 3ll}, {-1ll, 1ll}, {-1ll, 1ll}},
    {kMin, 9223372036854775807ll, 9223372036854775806ull, 0, {-9223372036854775807ll, 9223372036854775806ll}, {-1ll, 1ll}, {-9223372036854775807ll, 9223372036854775806ll}},
    {kMin, 9223372036854775807ll, 9223372036854775807ull, 0, {kMin, 9223372036854775807ll}, {kMin, 9223372036854775807ll}, {kMin, 9223372036854775807ll}},
    {-9223372036854775807ll, kMin, 1ull, 0, {0ll, 1ll}, {1ll, 1ll}, {1ll, 1ll}},
    {-9223372036854775807ll, kMin, 9223372036854775807ull, 0, {9223372036854775806ll, 9223372036854775807ll}, {1ll, 1ll}, {9223372036854775806ll, 9223372036854775807ll}},
    {1ll, kMin, 1ull, 0, {-1ll, 1ll}, {0ll, 1ll}, {0ll, 1ll}},
    {1ll, kMin, 9223372036854775807ull, 0, {-1ll, 9223372036854775807ll}, {0ll, 1ll}, {-1ll, 9223372036854775807ll}},
    {-1ll, kMin, 1ull, 0, {0ll, 1ll}, {1ll, 1ll}, {0ll, 1ll}},
    {-1ll, kMin, 9223372036854775807ull, 0, {0ll, 1ll}, {1ll, 9223372036854775807ll}, {1ll, 9223372036854775807ll}},
    {3ll, kMin, 1ull, 0, {-1ll, 1ll}, {0ll, 1ll}, {0ll, 1ll}},
    {3ll, kMin, 2ull, 0, {-1ll, 2ll}, {0ll, 1ll}, {0ll, 1ll}},
    {3ll, kMin, 9223372036854775807ull, 0, {-2ll, 6148914691236517205ll}, {-1ll, 3074457345618258603ll}, {-2ll, 6148914691236517205ll}},
    {kMin, -9223372036854775807ll, 1ull, 0, {1ll, 1ll}, {2ll, 1ll}, {1ll, 1ll}},
    {kMin, -9223372036854775807ll, 2ull, 0, {1ll, 1ll}, {3ll, 2ll}, {1ll, 1ll}},
    {kMin, -9223372036854775806ll, 1ull, 0, {1ll, 1ll}, {2ll, 1ll}, {1ll, 1ll}},
    {kMin, -9223372036854775806ll, 9223372036854775807ull, 0, {4611686018427387904ll, 4611686018427387903ll}, {4611686018427387904ll, 4611686018427387903ll}, {4611686018427387904ll, 4611686018427387903ll}},
    {-9223372036854775806ll, kMin, 2ull, 0, {1ll, 2ll}, {1ll, 1ll}, {1ll, 1ll}},
    {-9223372036854775806ll, kMin, 9223372036854775807ull, 0, {4611686018427387903ll, 4611686018427387904ll}, {4611686018427387903ll, 4611686018427387904ll}, {4611686018427387903ll, 4611686018427387904ll}},
    {1ll, 9223372036854775807ll, 1ull, 0, {0ll, 1ll}, {1ll, 1ll}, {0ll, 1ll}},
    {1ll, 9223372036854775807ll, 2ull, 0, {0ll, 1ll}, {1ll, 2ll}, {0ll, 1ll}},
    {1ll, 9223372036854775807ll, 9223372036854775806ull, 0, {0ll, 1ll}, {1ll, 9223372036854775806ll}, {1ll, 9223372036854775806ll}},
    {1ll, 9223372036854775807ll, 9223372036854775807ull, 0, {1ll, 9223372036854775807ll}, {1ll, 9223372036854775807ll}, {1ll, 9223372036854775807ll}},
    {-1ll, 9223372036854775807ll, 1ull, 0, {-1ll, 1ll}, {0ll, 1ll}, {0ll, 1ll}},
    {-1ll, 9223372036854775807ll, 2ull, 0, {-1ll, 2ll}, {0ll, 1ll}, {0ll, 1ll}},
    {-1ll, 9223372036854775807ll, 9223372036854775807ull, 0, {-1ll, 9223372036854775807ll}, {-1ll, 9223372036854775807ll}, {-1ll, 9223372036854775807ll}},
    {9223372036854775806ll, 9223372036854775807ll, 1ull, 0, {0ll, 1ll}, {1ll, 1ll}, {1ll, 1ll}},
    {9223372036854775806ll, 9223372036854775807ll, 2ull, 0, {1ll, 2ll}, {1ll, 1ll}, {1ll, 1ll}},
    {9223372036854775806ll, 9223372036854775807ll, 9223372036854775806ull, 0, {9223372036854775805ll, 9223372036854775806ll}, {1ll, 1ll}, {9223372036854775805ll, 9223372036854775806ll}},
    {9223372036854775806ll, 9223372036854775807ll, 9223372036854775807ull, 0, {9223372036854775806ll, 9223372036854775807ll}, {9223372036854775806ll, 9223372036854775807ll}, {9223372036854775806ll, 9223372036854775807ll}},
    {9223372036854775807ll, 9223372036854775806ll, 1ull, 0, {1ll, 1ll}, {2ll, 1ll}, {1ll, 1ll}},
    {9223372036854775807ll, 9223372036854775806ll, 2ull, 0, {1ll, 1ll}, {3ll, 2ll}, {1ll, 1ll}},
    {9223372036854775807ll, 9223372036854775806ll, 9223372036854775805ull, 0, {1ll, 1ll}, {9223372036854775806ll, 9223372036854775805ll}, {9223372036854775806ll, 9223372036854775805ll}},
    {9223372036854775807ll, 9223372036854775806ll, 9223372036854775806ull, 0, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775807ll, 9223372036854775806ll}},
    {9223372036854775807ll, 9223372036854775806ll, 9223372036854775807ull, 0, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775807ll, 9223372036854775806ll}, {9223372036854775807ll, 9223372036854775806ll}},
    {9223372036854775806ll, 9223372036854775805ll, 1ull, 0, {1ll, 1ll}, {2ll, 1ll}, {1ll, 1ll}},
    {9223372036854775806ll, 9223372036854775805ll, 9223372036854775807ull, 0, {9223372036854775806ll, 9223372036854775805ll}, {9223372036854775806ll, 9223372036854775805ll}, {9223372036854775806ll, 9223372036854775805ll}},
    {9223372036854775807ll, 9223372036854775805ll, 1ull, 0, {1ll, 1ll}, {2ll, 1ll}, {1ll, 1ll}},
    {9223372036854775807ll, 9223372036854775805ll, 9223372036854775804ull, 0, {4611686018427387904ll, 4611686018427387903ll}, {4611686018427387903ll, 4611686018427387902ll}, {4611686018427387904ll, 4611686018427387903ll}},
    {9223372036854775807ll, 9223372036854775805ll, 9223372036854775807ull, 0, {9223372036854775807ll, 9223372036854775805ll}, {9223372036854775807ll, 9223372036854775805ll}, {9223372036854775807ll, 9223372036854775805ll}},
    {4611686018427387904ll, 3ll, 1ull, 0, {1537228672809129301ll, 1ll}, {1537228672809129302ll, 1ll}, {1537228672809129301ll, 1ll}},
    {4611686018427387904ll, 3ll, 2ull, 0, {1537228672809129301ll, 1ll}, {3074457345618258603ll, 2ll}, {3074457345618258603ll, 2ll}},
    {4611686018427387904ll, 3ll, 3ull, 0, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}, {4611686018427387904ll, 3ll}},
    {-4611686018427387904ll, 3ll, 1ull, 0, {-1537228672809129302ll, 1ll}, {-1537228672809129301ll, 1ll}, {-1537228672809129301ll, 1ll}},
    {-4611686018427387904ll, 3ll, 2ull, 0, {-3074457345618258603ll, 2ll}, {-1537228672809129301ll, 1ll}, {-3074457345618258603ll, 2ll}},
    {-4611686018427387904ll, 3ll, 3ull, 0, {-4611686018427387904ll, 3ll}, {-4611686018427387904ll, 3ll}, {-4611686018427387904ll, 3ll}},
    {4611686018427387904ll, -3ll, 1ull, 0, {-1537228672809129302ll, 1ll}, {-1537228672809129301ll, 1ll}, {-1537228672809129301ll, 1ll}},
    {4611686018427387904ll, -3ll, 2ull, 0, {-3074457345618258603ll, 2ll}, {-1537228672809129301ll, 1ll}, {-3074457345618258603ll, 2ll}},
    {4611686018427387904ll, -3ll, 3ull, 0, {-4611686018427387904ll, 3ll}, {-4611686018427387904ll, 3ll}, {-4611686018427387904ll, 3ll}},
    {4611686018427387905ll, 4611686018427387904ll, 1ull, 0, {1ll, 1ll}, {2ll, 1ll}, {1ll, 1ll}},
    {4611686018427387905ll, 4611686018427387904ll, 2ull, 0, {1ll, 1ll}, {3ll, 2ll}, {1ll, 1ll}},
    {4611686018427387905ll, 4611686018427387904ll, 4611686018427387904ull, 0, {4611686018427387905ll, 4611686018427387904ll}, {4611686018427387905ll, 4611686018427387904ll}, {4611686018427387905ll, 4611686018427387904ll}},
    {4611686018427387904ll, 4611686018427387905ll, 4611686018427387904ull, 0, {4611686018427387903ll, 4611686018427387904ll}, {1ll, 1ll}, {4611686018427387903ll, 4611686018427387904ll}},
    {4611686018427387904ll, 4611686018427387905ll, 4611686018427387905ull, 0, {4611686018427387904ll, 4611686018427387905ll}, {4611686018427387904ll, 4611686018427387905ll}, {4611686018427387904ll, 4611686018427387905ll}},
    {1ll, 4611686018427387904ll, 1ull, 0, {0ll, 1ll}, {1ll, 1ll}, {0ll, 1ll}},
    {1ll, 4611686018427387904ll, 4611686018427387903ull, 0, {0ll, 1ll}, {1ll, 4611686018427387903ll}, {1ll, 4611686018427387903ll}},
    {1ll, 4611686018427387904ll, 4611686018427387904ull, 0, {1ll, 4611686018427387904ll}, {1ll, 4611686018427387904ll}, {1ll, 4611686018427387904ll}},
    {1ll, 4611686018427387904ll, 4611686018427387905ull, 0, {1ll, 4611686018427387904ll}, {1ll, 4611686018427387904ll}, {1ll, 4611686018427387904ll}},
    {5ll, 12ll, 3ull, 0, {1ll, 3ll}, {1ll, 2ll}, {1ll, 2ll}},
    {-5ll, 12ll, 3ull, 0, {-1ll, 2ll}, {-1ll, 3ll}, {-1ll, 2ll}},
    {5ll, -12ll, 3ull, 0, {-1ll, 2ll}, {-1ll, 3ll}, {-1ll, 2ll}},
    {33ll, 544ll, 17ull, 0, {1ll, 17ll}, {1ll, 16ll}, {1ll, 16ll}},
    {-33ll, 544ll, 17ull, 0, {-1ll, 16ll}, {-1ll, 17ll}, {-1ll, 16ll}},
    {33ll, -544ll, 17ull, 0, {-1ll, 16ll}, {-1ll, 17ll}, {-1ll, 16ll}},
    {2147483649ll, 2305843011361177600ll, 1073741825ull, 0, {1ll, 1073741825ll}, {1ll, 1073741824ll}, {1ll, 1073741824ll}},
    {-2147483649ll, 2305843011361177600ll, 1073741825ull, 0, {-1ll, 1073741824ll}, {-1ll, 1073741825ll}, {-1ll, 1073741824ll}},
    {2147483649ll, -2305843011361177600ll, 1073741825ull, 0, {-1ll, 1073741824ll}, {-1ll, 1073741825ll}, {-1ll, 1073741824ll}},
    {4294967293ll, 9223372023969873924ll, 2147483647ull, 0, {1ll, 2147483647ll}, {1ll, 2147483646ll}, {1ll, 2147483646ll}},
    {-4294967293ll, 9223372023969873924ll, 2147483647ull, 0, {-1ll, 2147483646ll}, {-1ll, 2147483647ll}, {-1ll, 2147483646ll}},
    {4294967293ll, -9223372023969873924ll, 2147483647ull, 0, {-1ll, 2147483646ll}, {-1ll, 2147483647ll}, {-1ll, 2147483646ll}},
    {2199023255551ll, 2ll, 1ull, 0, {1099511627775ll, 1ll}, {1099511627776ll, 1ll}, {1099511627775ll, 1ll}},
    {2199023255551ll, 2ll, 2ull, 0, {2199023255551ll, 2ll}, {2199023255551ll, 2ll}, {2199023255551ll, 2ll}},
    {-9223372036854775807ll, 2ll, 1ull, 0, {-4611686018427387904ll, 1ll}, {-4611686018427387903ll, 1ll}, {-4611686018427387904ll, 1ll}},
    {-9223372036854775807ll, -2ll, 1ull, 0, {4611686018427387903ll, 1ll}, {4611686018427387904ll, 1ll}, {4611686018427387903ll, 1ll}},
    {-1ll, 2ll, 1ull, 0, {-1ll, 1ll}, {0ll, 1ll}, {-1ll, 1ll}},
    {1ll, 2ll, 1ull, 0, {0ll, 1ll}, {1ll, 1ll}, {0ll, 1ll}},
    {3ll, 2ll, 1ull, 0, {1ll, 1ll}, {2ll, 1ll}, {1ll, 1ll}},
    {-3ll, 2ll, 1ull, 0, {-2ll, 1ll}, {-1ll, 1ll}, {-2ll, 1ll}},
    {5ll, 4ll, 2ull, 0, {1ll, 1ll}, {3ll, 2ll}, {1ll, 1ll}},
    {-5ll, 4ll, 2ull, 0, {-3ll, 2ll}, {-1ll, 1ll}, {-1ll, 1ll}},
    {7ll, 5ll, 2ull, 0, {1ll, 1ll}, {3ll, 2ll}, {3ll, 2ll}},
    {-7ll, 5ll, 2ull, 0, {-3ll, 2ll}, {-1ll, 1ll}, {-3ll, 2ll}},
};


constexpr std::uint64_t kGold = 0x9E3779B97F4A7C15ull;
const Frac kS1 = {0x1234567890ABCDEll, 0x0FEDCBA987654321ll};
const Frac kS2 = {0x2222333344445555ll, 0x0AAAABBBBCCCCDDDll};

std::uint32_t code(ApproxResult r) { return static_cast<std::uint32_t>(r); }
bool same(const Frac& a, const Frac& b) { return std::memcmp(&a, &b, sizeof(Frac)) == 0; }

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
// Oracle (a): sign-magnitude big integer, 6 limbs of 32 bits (192 bits; the largest value formed below is under 2^190). Not shared with ratapprox.cpp.
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
bool lowest(const Frac& f) { return f.den > 0 && ogcd64(mag64(f.num), static_cast<std::uint64_t>(f.den)) == 1ull; }

// The exact value x = sx * P / Q in lowest terms (Q above 0).
struct Xv {
  bool neg;
  std::uint64_t p;
  std::uint64_t q;
};
Xv reduce(std::int64_t num, std::int64_t den) {  // den != 0
  const std::uint64_t a = mag64(num), b = mag64(den);
  const std::uint64_t g = ogcd64(a, b);
  Xv x;
  x.neg = num != 0 && ((num < 0) != (den < 0));
  x.p = a / g;
  x.q = b / g;
  return x;
}
// The header's Overflow family, stated from the reduced value: x positive, reduced numerator above INT64_MAX (so exactly 2^63) and x a candidate.
bool oracle_overflow(const Xv& x, std::uint64_t n) { return !x.neg && x.p > static_cast<std::uint64_t>(kMax) && x.q <= n; }

SBig xnum(const Xv& x) { return sb_norm(SBig{x.neg, big_u64(x.p)}); }
SBig fnum(const Frac& f) { return sb_from(f.num); }
Big fden(const Frac& f) { return big_u64(static_cast<std::uint64_t>(f.den)); }
int cmp_frac(const SBig& an, const Big& ad, const SBig& bn, const Big& bd) { return sb_cmp(sb_mul(an, SBig{false, bd}), sb_mul(bn, SBig{false, ad})); }

// Oracle (a) on a successful call. Returns an empty string when lo, hi and near are exactly what the definition requires.
std::string check_def(const Xv& x, std::uint64_t n, const Frac& lo, const Frac& hi, const Frac& near) {
  if (!lowest(lo) || !lowest(hi) || !lowest(near)) return "not lowest terms or den not above 0";
  if (static_cast<std::uint64_t>(lo.den) > n || static_cast<std::uint64_t>(hi.den) > n || static_cast<std::uint64_t>(near.den) > n) return "den above max_den";
  const SBig xn = xnum(x);
  const Big xd = big_u64(x.q);
  if (x.q <= n) {  // x is a candidate: all three are x itself
    const Frac w = {static_cast<std::int64_t>(x.neg ? (~x.p + 1ull) : x.p), static_cast<std::int64_t>(x.q)};
    if (!same(lo, w) || !same(hi, w) || !same(near, w)) return "x is a candidate but a result differs from x";
    return "";
  }
  if (cmp_frac(fnum(lo), fden(lo), xn, xd) >= 0) return "lo not below x";
  if (cmp_frac(fnum(hi), fden(hi), xn, xd) <= 0) return "hi not above x";
  const SBig det = sb_add(sb_mul(fnum(hi), SBig{false, fden(lo)}), sb_neg(sb_mul(fnum(lo), SBig{false, fden(hi)})));
  if (sb_cmp(det, sb_from(1)) != 0) return "hi.num * lo.den - lo.num * hi.den is not 1";
  if (static_cast<std::uint64_t>(lo.den) + static_cast<std::uint64_t>(hi.den) <= n) return "lo.den + hi.den not above max_den";
  // distances: (x - lo) = dl / (q * lo.den), (hi - x) = dh / (q * hi.den); compare dl * hi.den with dh * lo.den
  const SBig dl = sb_add(sb_mul(xn, SBig{false, fden(lo)}), sb_neg(sb_mul(fnum(lo), SBig{false, xd})));
  const SBig dh = sb_add(sb_mul(fnum(hi), SBig{false, xd}), sb_neg(sb_mul(xn, SBig{false, fden(hi)})));
  const int c = sb_cmp(sb_mul(dl, SBig{false, fden(hi)}), sb_mul(dh, SBig{false, fden(lo)}));
  const bool pick_lo = c < 0 || (c == 0 && lo.den <= hi.den);
  if (!same(near, pick_lo ? lo : hi)) return "nearest wrong";
  return "";
}

// Oracle (b): brute force over every denominator 1..n with plain int64 (reduced |x| numerator and q at most 2^20, n at most 300).
Frac red64(std::int64_t p, std::int64_t q) {
  const std::int64_t g = static_cast<std::int64_t>(ogcd64(mag64(p), static_cast<std::uint64_t>(q)));
  return Frac{p / g, q / g};
}
bool brute_ok(const Xv& x, std::uint64_t n) { return x.p <= (1ull << 20) && x.q <= (1ull << 20) && n <= 300ull; }
// a / b < c / d for b, d above 0
bool lt(const Frac& a, const Frac& c) { return a.num * c.den < c.num * a.den; }
std::string check_brute(const Xv& x, std::uint64_t n, const Frac& lo, const Frac& hi, const Frac& near) {
  const std::int64_t sx = x.neg ? -static_cast<std::int64_t>(x.p) : static_cast<std::int64_t>(x.p);
  const std::int64_t q = static_cast<std::int64_t>(x.q);
  bool have_lo = false, have_hi = false, have_n = false;
  Frac blo = {0, 1}, bhi = {0, 1}, bn = {0, 1};
  std::int64_t bd = 0, bq = 1;  // the best distance so far is bd / (q * bq)
  for (std::int64_t d = 1; d <= static_cast<std::int64_t>(n); ++d) {
    const std::int64_t t = sx * d;
    std::int64_t f = t / q;
    if (t % q != 0 && t < 0) --f;  // floor
    for (std::int64_t p = f; p <= f + 1; ++p) {
      const Frac c = red64(p, d);
      const std::int64_t e = sx * d - p * q;  // sign of (x - p / d)
      const std::int64_t dist = e < 0 ? -e : e;
      if (e >= 0 && (!have_lo || lt(blo, c))) {
        blo = c;
        have_lo = true;
      }
      if (e <= 0 && (!have_hi || lt(c, bhi))) {
        bhi = c;
        have_hi = true;
      }
      bool better = !have_n;
      if (have_n) {
        const std::int64_t l = dist * bq, r = bd * d;  // dist / d against bd / bq (the factor 1 / q is common)
        if (l != r) better = l < r;
        else if (c.den != bn.den) better = c.den < bn.den;
        else better = lt(c, bn);
      }
      if (better) {
        bn = c;
        bd = dist;
        bq = d;
        have_n = true;
      }
    }
  }
  if (!same(lo, blo)) return "brute lo differs";
  if (!same(hi, bhi)) return "brute hi differs";
  if (!same(near, bn)) return "brute nearest differs";
  return "";
}

struct Tally {
  std::size_t bad = 0;
  std::string first;
  void fail(const std::string& what) {
    if (bad == 0) first = what;
    ++bad;
  }
};

std::string describe(std::int64_t num, std::int64_t den, std::uint64_t n) {
  char buf[200];
  std::snprintf(buf, sizeof buf, "x = (%lld / %lld), max_den %llu: ", static_cast<long long>(num), static_cast<long long>(den),
                static_cast<unsigned long long>(n));
  return buf;
}

// One case: both functions, checked by the oracles, folded into the digest (error codes fold alone, as in the reference).
void do_case(std::int64_t num, std::int64_t den, std::uint64_t n, Fold& f, Tally& t) {
  ++f.cases;
  Frac lo = kS1, hi = kS2, nr = kS1;
  const ApproxResult rb = ratapprox_bounds(num, den, n, &lo, &hi);
  const ApproxResult rn = ratapprox_nearest(num, den, n, &nr);
  ++f.ops;
  f.add(code(rb));
  std::string err;
  std::uint32_t want = 0;
  Xv x = {false, 0, 1};
  if (den == 0) {
    want = 2;
  } else if (n == 0 || n > static_cast<std::uint64_t>(kMax)) {
    want = 3;
  } else {
    x = reduce(num, den);
    if (oracle_overflow(x, n)) want = 4;
  }
  if (code(rb) != want || code(rn) != want) {
    err = "code " + std::to_string(code(rb)) + " / " + std::to_string(code(rn)) + " expected " + std::to_string(want);
  } else if (want != 0) {
    if (!same(lo, kS1) || !same(hi, kS2) || !same(nr, kS1)) err = "failure changed an output";
  } else {
    f.add(static_cast<std::uint64_t>(lo.num));
    f.add(static_cast<std::uint64_t>(lo.den));
    f.add(static_cast<std::uint64_t>(hi.num));
    f.add(static_cast<std::uint64_t>(hi.den));
    f.add(code(rn));
    f.add(static_cast<std::uint64_t>(nr.num));
    f.add(static_cast<std::uint64_t>(nr.den));
    err = check_def(x, n, lo, hi, nr);
    if (err.empty() && brute_ok(x, n)) err = check_brute(x, n, lo, hi, nr);
  }
  if (!err.empty()) t.fail(describe(num, den, n) + err);
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::size_t ops, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.ops == ops);
  CHECK(f.h == digest);
}

bool sorted_unique(const std::int64_t* a, std::size_t n) {
  for (std::size_t i = 1; i < n; ++i)
    if (!(a[i - 1] < a[i])) return false;
  return true;
}

constexpr std::size_t count_of(std::size_t bytes, std::size_t each) { return bytes / each; }

}  // namespace

// =============================================================================================================================================
// The grids: the reference printed their sizes (unique counts); they are recounted here from the arrays, which must be strictly increasing.
// =============================================================================================================================================
TEST_CASE("grid definitions: unique counts of NI, DI, NB", "[ratapprox]") {
  CHECK(count_of(sizeof kRefNi, sizeof kRefNi[0]) == kRefSizeNi);
  CHECK(count_of(sizeof kRefDi, sizeof kRefDi[0]) == kRefSizeDi);
  CHECK(count_of(sizeof kRefNb, sizeof kRefNb[0]) == kRefSizeNb);
  CHECK(count_of(sizeof kRefOvfD, sizeof kRefOvfD[0]) == kRefSizeOvfD);
  CHECK(sorted_unique(kRefNi, kRefSizeNi));
  CHECK(sorted_unique(kRefDi, kRefSizeDi));
  for (std::size_t i = 1; i < kRefSizeNb; ++i) CHECK(kRefNb[i - 1] < kRefNb[i]);
  CHECK(kRefSizeNi == 65u);
  CHECK(kRefSizeDi == 64u);
  CHECK(kRefSizeNb == 31u);
}

// =============================================================================================================================================
// EXHAUSTIVE: every (num in NI, den in DI) at every bound of NB plus the bounds d-1, d, d+1 around its reduced denominator d (where in
// 1..INT64_MAX). Overflow inputs are skipped here (counted, and covered by the ovf scenario and the Overflow rows). Digest + both oracles.
// =============================================================================================================================================
TEST_CASE("grid A: exhaustive over NI x DI x (NB and d-1, d, d+1)", "[ratapprox]") {
  Fold f;
  Tally t;
  std::size_t skipped = 0;
  for (std::size_t i = 0; i < kRefSizeNi; ++i)
    for (std::size_t j = 0; j < kRefSizeDi; ++j) {
      const Xv x = reduce(kRefNi[i], kRefDi[j]);
      std::vector<std::uint64_t> bs(kRefNb, kRefNb + kRefSizeNb);
      for (int k = -1; k <= 1; ++k) {
        const std::uint64_t v = x.q + static_cast<std::uint64_t>(static_cast<std::int64_t>(k));
        if (v >= 1ull && v <= static_cast<std::uint64_t>(kMax)) bs.push_back(v);
      }
      std::sort(bs.begin(), bs.end());
      bs.erase(std::unique(bs.begin(), bs.end()), bs.end());
      for (const std::uint64_t n : bs) {
        if (oracle_overflow(x, n)) {
          ++skipped;
          continue;
        }
        do_case(kRefNi[i], kRefDi[j], n, f, t);
      }
    }
  CHECK(skipped == kRefGridASkipped);
  expect_digest(f, t, kRefCases_gridA, kRefOps_gridA, kRefDigest_gridA);
}

// =============================================================================================================================================
// EXHAUSTIVE: x = 0 and +-p / q for 1 <= p, q <= 40 in lowest terms (1959 values), each in two forms (n, d) and (-3n, -3d), every bound 1..64.
// =============================================================================================================================================
TEST_CASE("grid B: exhaustive small fractions at every bound 1..64", "[ratapprox]") {
  Fold f;
  Tally t;
  // x order must match the reference: 0 first, then for q = 1..40, p = 1..40 (lowest terms): +p / q then -p / q
  struct V {
    std::int64_t n, d;
  };
  std::vector<V> v;
  v.push_back(V{0, 1});
  for (std::int64_t q = 1; q <= 40; ++q)
    for (std::int64_t p = 1; p <= 40; ++p)
      if (ogcd64(static_cast<std::uint64_t>(p), static_cast<std::uint64_t>(q)) == 1ull) {
        v.push_back(V{p, q});
        v.push_back(V{-p, q});
      }
  CHECK(v.size() == kRefGridBX);
  for (const V& a : v)
    for (std::uint64_t n = 1; n <= 64; ++n) {
      do_case(a.n, a.d, n, f, t);
      do_case(-3 * a.n, -3 * a.d, n, f, t);
    }
  expect_digest(f, t, kRefCases_gridB, kRefOps_gridB, kRefDigest_gridB);
}

// =============================================================================================================================================
// EXHAUSTIVE: exact midpoint ties. For N = 1..16 every pair of consecutive Farey terms of order N in [0, 1]: the midpoint (an exact tie between
// the two neighbours) plus j (0..2), both signs, in two forms, at bound N.
// =============================================================================================================================================
TEST_CASE("ties: midpoints of consecutive Farey terms, N = 1..16", "[ratapprox]") {
  Fold f;
  Tally t;
  for (std::int64_t n = 1; n <= 16; ++n) {
    std::vector<Frac> fr;
    for (std::int64_t b = 1; b <= n; ++b)
      for (std::int64_t a = 0; a <= b; ++a)
        if (ogcd64(static_cast<std::uint64_t>(a), static_cast<std::uint64_t>(b)) == 1ull) fr.push_back(Frac{a, b});
    std::sort(fr.begin(), fr.end(), [](const Frac& a, const Frac& b) { return lt(a, b); });
    for (std::size_t i = 0; i + 1 < fr.size(); ++i) {
      const Frac m = red64(fr[i].num * fr[i + 1].den + fr[i + 1].num * fr[i].den, 2 * fr[i].den * fr[i + 1].den);
      for (std::int64_t j = 0; j < 3; ++j)
        for (int sg = 0; sg < 2; ++sg) {
          const std::int64_t yn = (m.num + j * m.den) * (sg == 0 ? 1 : -1);
          do_case(yn, m.den, static_cast<std::uint64_t>(n), f, t);
          do_case(-2 * yn, -2 * m.den, static_cast<std::uint64_t>(n), f, t);
        }
    }
  }
  expect_digest(f, t, kRefCases_ties, kRefOps_ties, kRefDigest_ties);
}

// The Overflow family at the bounds d-1, d, d+1, 1, 2, INT64_MAX for the odd d of the list (success when the bound is below d).
TEST_CASE("overflow family scenario: INT64_MIN over -d, d odd", "[ratapprox]") {
  Fold f;
  Tally t;
  for (std::size_t i = 0; i < kRefSizeOvfD; ++i) {
    const std::uint64_t d = static_cast<std::uint64_t>(kRefOvfD[i]);
    std::vector<std::uint64_t> bs = {d - 1ull, d, d + 1ull, 1ull, 2ull, static_cast<std::uint64_t>(kMax)};
    std::sort(bs.begin(), bs.end());
    bs.erase(std::unique(bs.begin(), bs.end()), bs.end());
    for (const std::uint64_t n : bs)
      if (n >= 1ull && n <= static_cast<std::uint64_t>(kMax)) do_case(kMin, -kRefOvfD[i], n, f, t);
  }
  expect_digest(f, t, kRefCases_ovf, kRefOps_ovf, kRefDigest_ovf);
}

// NOT exhaustive: +-F(k) / F(k+1) and +-F(k+1) / F(k), k = 60..91 (the longest Euclid chains), at the bounds F(j) - 1, F(j), F(j) + 1 for
// j = 20, 26, ..., 92 and at 1, 2, INT64_MAX.
TEST_CASE("Fibonacci chains (not exhaustive)", "[ratapprox]") {
  std::uint64_t fib[95];
  fib[0] = 0;
  fib[1] = 1;
  for (int i = 2; i < 95; ++i) fib[i] = fib[i - 1] + fib[i - 2];
  Fold f;
  Tally t;
  const std::uint64_t mx = static_cast<std::uint64_t>(kMax);
  for (int k = 60; k <= 91; ++k)
    for (int ord = 0; ord < 2; ++ord) {
      const std::uint64_t a = ord == 0 ? fib[k] : fib[k + 1], b = ord == 0 ? fib[k + 1] : fib[k];
      for (int sg = 0; sg < 2; ++sg)
        for (int j = 20; j <= 98; j += 6) {
          std::vector<std::uint64_t> bs;
          if (j <= 92) bs = {fib[j] - 1ull, fib[j], fib[j] + 1ull};
          else bs = {1ull, 2ull, mx};
          for (const std::uint64_t n : bs)
            if (n >= 1ull && n <= mx && a <= mx && b <= mx) do_case(sg == 0 ? static_cast<std::int64_t>(a) : -static_cast<std::int64_t>(a), static_cast<std::int64_t>(b), n, f, t);
        }
    }
  expect_digest(f, t, kRefCases_fib, kRefOps_fib, kRefDigest_fib);
}

// NOT exhaustive: 400 seeds x 10 random inputs (bit lengths up to 64 for num and den, den never 0) with a random bound of 1..63 bits.
TEST_CASE("random inputs (sampled, not exhaustive)", "[ratapprox]") {
  Fold f;
  Tally t;
  for (std::uint32_t s = 0; s < kRefRandSeeds; ++s) {
    Lcg r(static_cast<std::uint64_t>(s + 1u) * kGold);
    for (std::uint32_t k = 0; k < kRefRandPerSeed; ++k) {
      const std::uint32_t bn = r.next() % 65u;
      const std::uint32_t bd = r.next() % 65u;
      const std::uint32_t bb = r.next() % 63u + 1u;
      const std::uint64_t n64 = r.next64(), d64 = r.next64(), b64 = r.next64();
      const std::int64_t num = bn != 0 ? static_cast<std::int64_t>(n64 >> (64u - bn)) : 0;
      std::int64_t den = bd != 0 ? static_cast<std::int64_t>(d64 >> (64u - bd)) : 0;
      if (den == 0) den = 1;
      std::uint64_t n = b64 >> (64u - bb);
      if (n == 0) n = 1;
      do_case(num, den, n, f, t);
    }
  }
  expect_digest(f, t, kRefCases_rand, kRefOps_rand, kRefDigest_rand);
}

// =============================================================================================================================================
// CONSTANTS: hand rows (computed with Python Fractions). Includes the SUCCESS rows next to the Overflow family: INT64_MIN over -2, -4, -6, 1, 3,
// and over odd -d with a bound below d; midpoint ties with large neighbours; extremes in every position. Also run through the oracles.
// =============================================================================================================================================
TEST_CASE("hand rows (constants), both functions, not exhaustive", "[ratapprox]") {
  Fold f;
  Tally t;
  for (const HandRow& row : kHandRows) {
    Frac lo = kS1, hi = kS2, nr = kS1;
    const ApproxResult rb = ratapprox_bounds(row.num, row.den, row.n, &lo, &hi);
    const ApproxResult rn = ratapprox_nearest(row.num, row.den, row.n, &nr);
    INFO(describe(row.num, row.den, row.n));
    CHECK(code(rb) == row.code);
    CHECK(code(rn) == row.code);
    if (row.code == 0) {
      CHECK(same(lo, row.lo));
      CHECK(same(hi, row.hi));
      CHECK(same(nr, row.near));
    } else {
      CHECK(same(lo, kS1));
      CHECK(same(hi, kS2));
      CHECK(same(nr, kS1));
    }
    do_case(row.num, row.den, row.n, f, t);
  }
  INFO(t.first);
  CHECK(t.bad == 0);
}

// =============================================================================================================================================
// Overflow family written by hand from the header text: num = INT64_MIN, den = -d with d odd, Overflow exactly when d <= max_den, both functions,
// outputs untouched. d = 1, 3, 5, INT64_MAX; bounds below, equal to and above d, plus 1, 2, INT64_MAX. Even d, and a positive den, succeed.
// =============================================================================================================================================
TEST_CASE("overflow family by hand", "[ratapprox]") {
  const std::uint64_t mx = static_cast<std::uint64_t>(kMax);
  const std::uint64_t ds[] = {1ull, 3ull, 5ull, mx};
  for (const std::uint64_t d : ds) {
    const std::uint64_t bs[] = {d - 1ull, d, d + 1ull, 1ull, 2ull, mx};
    for (const std::uint64_t n : bs) {
      if (n == 0ull || n > mx) continue;
      Frac lo = kS1, hi = kS2, nr = kS1;
      const std::int64_t den = -static_cast<std::int64_t>(d);
      const ApproxResult rb = ratapprox_bounds(kMin, den, n, &lo, &hi);
      const ApproxResult rn = ratapprox_nearest(kMin, den, n, &nr);
      INFO(describe(kMin, den, n));
      if (d <= n) {
        CHECK(rb == ApproxResult::Overflow);
        CHECK(rn == ApproxResult::Overflow);
        CHECK(same(lo, kS1));
        CHECK(same(hi, kS2));
        CHECK(same(nr, kS1));
      } else {
        CHECK(rb == ApproxResult::Ok);
        CHECK(rn == ApproxResult::Ok);
        CHECK(lowest(lo));
        CHECK(lowest(hi));
        CHECK(lowest(nr));
      }
    }
  }
  const std::int64_t evens[] = {-2, -4, -6, -8, -1024, -(1ll << 62)};
  const std::uint64_t ns5[] = {1ull, 2ull, 3ull, 7ull, mx};
  for (const std::int64_t den : evens)
    for (const std::uint64_t n : ns5) {
      Frac lo = kS1, hi = kS2, nr = kS1;
      INFO(describe(kMin, den, n));
      CHECK(ratapprox_bounds(kMin, den, n, &lo, &hi) == ApproxResult::Ok);
      CHECK(ratapprox_nearest(kMin, den, n, &nr) == ApproxResult::Ok);
    }
  const std::int64_t pos[] = {1, 2, 3, 5, kMax};
  const std::uint64_t ns3[] = {1ull, 2ull, mx};
  for (const std::int64_t den : pos)
    for (const std::uint64_t n : ns3) {
      Frac lo = kS1, hi = kS2, nr = kS1;
      INFO(describe(kMin, den, n));
      CHECK(ratapprox_bounds(kMin, den, n, &lo, &hi) == ApproxResult::Ok);
      CHECK(ratapprox_nearest(kMin, den, n, &nr) == ApproxResult::Ok);
      CHECK(lo.num < 0);
    }
  // MIN / MIN is 1, MIN / -(MIN + 1) is a positive value just above 1 with an odd den: both fine
  Frac one = kS1;
  CHECK(ratapprox_nearest(kMin, kMin, 1ull, &one) == ApproxResult::Ok);
  CHECK(one.num == 1);
  CHECK(one.den == 1);
}

// =============================================================================================================================================
// Check order and nulls (by hand from the header): null, then ZeroDivision, then BadBound, then Overflow; failures leave the outputs untouched.
// =============================================================================================================================================
TEST_CASE("null matrix: each pointer alone, both functions", "[ratapprox]") {
  Frac lo = kS1, hi = kS2, nr = kS1;
  CHECK(ratapprox_bounds(1, 3, 2, nullptr, &hi) == ApproxResult::NullArgument);
  CHECK(ratapprox_bounds(1, 3, 2, &lo, nullptr) == ApproxResult::NullArgument);
  CHECK(ratapprox_bounds(1, 3, 2, nullptr, nullptr) == ApproxResult::NullArgument);
  CHECK(ratapprox_nearest(1, 3, 2, nullptr) == ApproxResult::NullArgument);
  CHECK(same(lo, kS1));
  CHECK(same(hi, kS2));
  // null beats ZeroDivision, BadBound and Overflow
  const std::uint64_t ns[] = {0ull, 1ull, 5ull, 0x8000000000000000ull, ~0ull};
  const std::int64_t ds[] = {0, 1, -1, -3};
  for (const std::uint64_t n : ns) {
    for (const std::int64_t den : ds) {
      CHECK(ratapprox_bounds(kMin, den, n, nullptr, &hi) == ApproxResult::NullArgument);
      CHECK(ratapprox_bounds(kMin, den, n, &lo, nullptr) == ApproxResult::NullArgument);
      CHECK(ratapprox_bounds(kMin, den, n, nullptr, nullptr) == ApproxResult::NullArgument);
      CHECK(ratapprox_nearest(kMin, den, n, nullptr) == ApproxResult::NullArgument);
    }
  }
  CHECK(same(lo, kS1));
  CHECK(same(hi, kS2));
  CHECK(same(nr, kS1));
}

TEST_CASE("check order: ZeroDivision before BadBound before Overflow", "[ratapprox]") {
  const std::uint64_t bad[] = {0ull, 0x8000000000000000ull, 0x8000000000000001ull, ~0ull, 0xFFFFFFFFFFFFFFFEull};
  const std::int64_t nums[] = {0, 1, -1, 7, kMin, kMax, kMin + 1};
  const std::int64_t dens6[] = {1, -1, 3, -3, kMin, kMax};
  for (const std::uint64_t n : bad)
    for (const std::int64_t num : nums) {
      Frac lo = kS1, hi = kS2, nr = kS1;
      INFO(describe(num, 0, n));
      CHECK(ratapprox_bounds(num, 0, n, &lo, &hi) == ApproxResult::ZeroDivision);  // den 0 beats a bad bound
      CHECK(ratapprox_nearest(num, 0, n, &nr) == ApproxResult::ZeroDivision);
      for (const std::int64_t den : dens6) {
        INFO(describe(num, den, n));
        CHECK(ratapprox_bounds(num, den, n, &lo, &hi) == ApproxResult::BadBound);  // bad bound beats Overflow
        CHECK(ratapprox_nearest(num, den, n, &nr) == ApproxResult::BadBound);
      }
      CHECK(same(lo, kS1));
      CHECK(same(hi, kS2));
      CHECK(same(nr, kS1));
    }
  // ZeroDivision with a good bound, and the Overflow input with a good bound is Overflow
  Frac lo = kS1, hi = kS2, nr = kS1;
  CHECK(ratapprox_bounds(kMin, 0, 1ull, &lo, &hi) == ApproxResult::ZeroDivision);
  CHECK(ratapprox_nearest(kMin, 0, 1ull, &nr) == ApproxResult::ZeroDivision);
  CHECK(ratapprox_bounds(kMin, -1, 1ull, &lo, &hi) == ApproxResult::Overflow);
  CHECK(ratapprox_nearest(kMin, -1, 1ull, &nr) == ApproxResult::Overflow);
  CHECK(ratapprox_bounds(kMin, -1, 0x7FFFFFFFFFFFFFFFull, &lo, &hi) == ApproxResult::Overflow);
  CHECK(ratapprox_nearest(kMin, -1, 0x7FFFFFFFFFFFFFFFull, &nr) == ApproxResult::Overflow);
  CHECK(same(lo, kS1));
  CHECK(same(hi, kS2));
  CHECK(same(nr, kS1));
  // the largest allowed bound is fine, one above it is BadBound
  CHECK(ratapprox_bounds(1, 3, 0x7FFFFFFFFFFFFFFFull, &lo, &hi) == ApproxResult::Ok);
  CHECK(ratapprox_bounds(1, 3, 0x8000000000000000ull, &lo, &hi) == ApproxResult::BadBound);
}

// The tie examples (smaller denominator first, then the smaller value in the original orientation).
TEST_CASE("tie examples", "[ratapprox]") {
  Frac r = kS1;
  CHECK(ratapprox_nearest(1, 2, 1ull, &r) == ApproxResult::Ok);
  CHECK((r.num == 0 && r.den == 1));
  CHECK(ratapprox_nearest(-1, 2, 1ull, &r) == ApproxResult::Ok);
  CHECK((r.num == -1 && r.den == 1));
  CHECK(ratapprox_nearest(3, 2, 1ull, &r) == ApproxResult::Ok);
  CHECK((r.num == 1 && r.den == 1));
  CHECK(ratapprox_nearest(-3, 2, 1ull, &r) == ApproxResult::Ok);
  CHECK((r.num == -2 && r.den == 1));
  CHECK(ratapprox_nearest(2, -4, 1ull, &r) == ApproxResult::Ok);  // -1/2 in a non-reduced form
  CHECK((r.num == -1 && r.den == 1));
  Frac lo = kS1, hi = kS2;
  CHECK(ratapprox_bounds(1, 2, 1ull, &lo, &hi) == ApproxResult::Ok);
  CHECK((lo.num == 0 && lo.den == 1 && hi.num == 1 && hi.den == 1));
  CHECK(ratapprox_bounds(5, 7, 4ull, &lo, &hi) == ApproxResult::Ok);
  CHECK((lo.num == 2 && lo.den == 3 && hi.num == 3 && hi.den == 4));
  CHECK(ratapprox_nearest(5, 7, 4ull, &r) == ApproxResult::Ok);
  CHECK((r.num == 3 && r.den == 4));
}
