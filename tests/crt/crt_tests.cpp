// Tests for core/crt. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/crt_ref.py, an independent Python implementation (arbitrary
//      precision integers; a brute force over x and exact merging with math.lcm and pow(x, -1, m), cross-checked by the script). The tests re-run every
//      scenario against the real function and recompute the 64-bit digest and the counts.
//  (2) ORACLE: a naive in-test oracle that shares no code with core/crt/crt.cpp: the exact lcm is formed in a 6-limb big integer (schoolbook multiply,
//      remainder by shift-subtract, binary gcd; no 128-bit type, no compiler builtin), NoSolution is the pairwise test r[i] = r[j] (mod gcd) with a
//      binary gcd, Overflow is "consistent and the exact lcm is above 2^64 - 1", and a success is checked as: out_m equals the exact lcm, out_r is below
//      out_m, out_r = r[i] (mod m[i]) for every i. When the lcm is at most 5000 a brute force over x = 0 .. lcm-1 must give the same out_r.
//  (3) CONSTANTS: the hand rows were computed once with Python integers and written in as constants; the null, TooLarge, ZeroModulus and check-order
//      rows are written by hand from the header text.
// After every failure the outputs are compared byte for byte with the sentinels that were passed (the header promises "untouched").
// What is NOT done: no assertion on anything the header leaves open (overlapping arrays); no timing test (every loop in the implementation has a fixed
// bound, argued in the docs); Windows is CI-only; the fib, random and hand scenarios are NOT exhaustive over all systems.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "crt.h"

using namespace lumora::crt;

namespace {

constexpr std::uint64_t kU64 = std::numeric_limits<std::uint64_t>::max();

// BEGIN GENERATED REFERENCE (tools/crt_ref.py)
const std::uint32_t kRefRandSeeds = 300, kRefRandPerSeed = 10;
const std::uint64_t kRefBm[] = {1ull, 2ull, 3ull, 4294967295ull, 4294967296ull, 4294967297ull, 0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL, 0x8000000000000001ULL, 0xFFFFFFFFFFFFFFFDULL, 0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFFULL};
const std::uint64_t kRefBt[] = {2ull, 3ull, 4294967295ull, 4294967296ull, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL, 0x4000000000000001ULL};
const std::uint64_t kRefXs[] = {0ull, 1ull, 0x2BDC545D6B4B87ULL, 0xFFFFFFFFFFFFFFFFULL};
const std::uint64_t kRefGs[] = {1048576ull, 2147483648ull, 4294967295ull, 4294967296ull, 0x10000000001ULL, 0x1000000000000ULL};
const std::uint64_t kRefAs[] = {1ull, 3ull, 5ull, 7ull, 15ull, 17ull, 255ull, 65537ull, 2147483647ull, 4294967295ull};
const std::uint64_t kRefF7[] = {3ull, 5ull, 17ull, 257ull, 641ull, 65537ull, 6700417ull};
const std::size_t kRefSizeBm = 12, kRefSizeBt = 8, kRefSizeXs = 4, kRefSizeGs = 6, kRefSizeAs = 10, kRefSizeF7 = 7;
const std::uint64_t kRefDigest_n1 = 0x35427EC84B844205ULL;
const std::size_t kRefCases_n1 = 4160;
const std::uint64_t kRefDigest_n2 = 0x5846DFE02D28AF47ULL;
const std::size_t kRefCases_n2 = 90000;
const std::uint64_t kRefDigest_n3 = 0x02EB1DBEA9F6E04DULL;
const std::size_t kRefCases_n3 = 46656;
const std::uint64_t kRefDigest_n4 = 0x2DCF10CC0F6F2B66ULL;
const std::size_t kRefCases_n4 = 65536;
const std::uint64_t kRefDigest_resid = 0xF29D1C3F225E8924ULL;
const std::size_t kRefCases_resid = 24336;
const std::uint64_t kRefDigest_bigpairs = 0x156255B8B02965D9ULL;
const std::size_t kRefCases_bigpairs = 11628;
const std::uint64_t kRefDigest_bigtrip = 0x912141C438F92CF3ULL;
const std::size_t kRefCases_bigtrip = 4096;
const std::uint64_t kRefDigest_gcdheavy = 0xEA8B19203C2A7649ULL;
const std::size_t kRefCases_gcdheavy = 2052;
const std::uint64_t kRefDigest_fib = 0x3F8A5755DE78EFF2ULL;
const std::size_t kRefCases_fib = 8190;
const std::uint64_t kRefDigest_big64 = 0x3D9C874E9ED51D5CULL;
const std::size_t kRefCases_big64 = 87;
const std::uint64_t kRefDigest_rand = 0x6F8AADF19AD64918ULL;
const std::size_t kRefCases_rand = 3000;
// END GENERATED REFERENCE

struct HandSys {
  std::uint32_t n;
  std::uint64_t r[8];
  std::uint64_t m[8];
  std::uint32_t code;
  std::uint64_t out_r;
  std::uint64_t out_m;
};
const HandSys kHandSys[] = {
    {1, {0ull, 0, 0, 0, 0, 0, 0, 0}, {0xFFFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0, 0, 0}, 0, 0ull, 0xFFFFFFFFFFFFFFFFULL},
    {1, {5ull, 0, 0, 0, 0, 0, 0, 0}, {0xFFFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0, 0, 0}, 0, 5ull, 0xFFFFFFFFFFFFFFFFULL},
    {1, {0xFFFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0, 0, 0}, {0xFFFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0, 0, 0}, 0, 0ull, 0xFFFFFFFFFFFFFFFFULL},
    {1, {0xFFFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0, 0, 0}, {1ull, 0, 0, 0, 0, 0, 0, 0}, 0, 0ull, 1ull},
    {3, {0ull, 0ull, 0ull, 0, 0, 0, 0, 0}, {1ull, 1ull, 1ull, 0, 0, 0, 0, 0}, 0, 0ull, 1ull},
    {7, {0ull, 1ull, 7ull, 56ull, 454ull, 53251ull, 4783873ull, 0}, {3ull, 5ull, 17ull, 257ull, 641ull, 65537ull, 6700417ull, 0}, 0, 0xDEADBEEFCAFEBABEULL, 0xFFFFFFFFFFFFFFFFULL},
    {8, {0ull, 1ull, 7ull, 56ull, 454ull, 53251ull, 4783873ull, 0ull}, {3ull, 5ull, 17ull, 257ull, 641ull, 65537ull, 6700417ull, 2ull}, 5, 0, 0},
    {8, {0ull, 1ull, 7ull, 56ull, 454ull, 53251ull, 4783873ull, 1ull}, {3ull, 5ull, 17ull, 257ull, 641ull, 65537ull, 6700417ull, 2ull}, 5, 0, 0},
    {2, {0ull, 0ull, 0, 0, 0, 0, 0, 0}, {0x8000000000000000ULL, 3ull, 0, 0, 0, 0, 0, 0}, 5, 0, 0},
    {2, {1ull, 2ull, 0, 0, 0, 0, 0, 0}, {0x8000000000000000ULL, 3ull, 0, 0, 0, 0, 0, 0}, 5, 0, 0},
    {2, {1ull, 2ull, 0, 0, 0, 0, 0, 0}, {3ull, 0x8000000000000000ULL, 0, 0, 0, 0, 0, 0}, 5, 0, 0},
    {2, {5ull, 7ull, 0, 0, 0, 0, 0, 0}, {4294967296ull, 4294967295ull, 0, 0, 0, 0, 0, 0}, 0, 8589934597ull, 0xFFFFFFFF00000000ULL},
    {2, {1ull, 2ull, 0, 0, 0, 0, 0, 0}, {4294967295ull, 4294967297ull, 0, 0, 0, 0, 0, 0}, 0, 0x7FFFFFFF80000001ULL, 0xFFFFFFFFFFFFFFFFULL},
    {3, {1ull, 2ull, 3ull, 0, 0, 0, 0, 0}, {4294967297ull, 4294967295ull, 4294967296ull, 0, 0, 0, 0, 0}, 5, 0, 0},
    {2, {1ull, 2ull, 0, 0, 0, 0, 0, 0}, {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL, 0, 0, 0, 0, 0, 0}, 5, 0, 0},
    {3, {0ull, 1ull, 1ull, 0, 0, 0, 0, 0}, {0x8000000000000000ULL, 3ull, 2ull, 0, 0, 0, 0, 0}, 4, 0, 0},
    {3, {0ull, 0ull, 1ull, 0, 0, 0, 0, 0}, {0x8000000000000000ULL, 3ull, 2ull, 0, 0, 0, 0, 0}, 4, 0, 0},
    {2, {5ull, 5ull, 0, 0, 0, 0, 0, 0}, {0x8000000000000000ULL, 0x8000000000000000ULL, 0, 0, 0, 0, 0, 0}, 0, 5ull, 0x8000000000000000ULL},
    {2, {5ull, 6ull, 0, 0, 0, 0, 0, 0}, {0x8000000000000000ULL, 0x8000000000000000ULL, 0, 0, 0, 0, 0, 0}, 4, 0, 0},
    {2, {0x4000000000000005ULL, 5ull, 0, 0, 0, 0, 0, 0}, {0x8000000000000000ULL, 0x4000000000000000ULL, 0, 0, 0, 0, 0, 0}, 0, 0x4000000000000005ULL, 0x8000000000000000ULL},
    {2, {5ull, 0x4000000000000005ULL, 0, 0, 0, 0, 0, 0}, {0x4000000000000000ULL, 0x8000000000000000ULL, 0, 0, 0, 0, 0, 0}, 0, 0x4000000000000005ULL, 0x8000000000000000ULL},
    {2, {7ull, 7ull, 0, 0, 0, 0, 0, 0}, {0x30000000000ULL, 0x50000000000ULL, 0, 0, 0, 0, 0, 0}, 0, 7ull, 0xF0000000000ULL},
    {2, {7ull, 8ull, 0, 0, 0, 0, 0, 0}, {0x30000000000ULL, 0x50000000000ULL, 0, 0, 0, 0, 0, 0}, 4, 0, 0},
    {2, {7ull, 0x10000000007ULL, 0, 0, 0, 0, 0, 0}, {0x30000000000ULL, 0x50000000000ULL, 0, 0, 0, 0, 0, 0}, 0, 0x60000000007ULL, 0xF0000000000ULL},
    {2, {3ull, 10ull, 0, 0, 0, 0, 0, 0}, {7ull, 7ull, 0, 0, 0, 0, 0, 0}, 0, 3ull, 7ull},
    {3, {1ull, 1ull, 1ull, 0, 0, 0, 0, 0}, {6ull, 10ull, 15ull, 0, 0, 0, 0, 0}, 0, 1ull, 30ull},
    {2, {1ull, 2ull, 0, 0, 0, 0, 0, 0}, {6ull, 10ull, 0, 0, 0, 0, 0, 0}, 4, 0, 0},
    {2, {5ull, 11ull, 0, 0, 0, 0, 0, 0}, {12ull, 18ull, 0, 0, 0, 0, 0, 0}, 0, 29ull, 36ull},
    {2, {11ull, 5ull, 0, 0, 0, 0, 0, 0}, {18ull, 12ull, 0, 0, 0, 0, 0, 0}, 0, 29ull, 36ull},
    {2, {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0, 0}, {3ull, 5ull, 0, 0, 0, 0, 0, 0}, 0, 0ull, 15ull},
    {2, {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL, 0, 0, 0, 0, 0, 0}, {0x8000000000000000ULL, 0x7FFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0, 0}, 5, 0, 0},
    {2, {1ull, 2ull, 0, 0, 0, 0, 0, 0}, {0x8000000000000001ULL, 0x7FFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0, 0}, 5, 0, 0},
    {3, {1ull, 2ull, 3ull, 0, 0, 0, 0, 0}, {3ull, 0ull, 6ull, 0, 0, 0, 0, 0}, 3, 0, 0},
    {1, {0ull, 0, 0, 0, 0, 0, 0, 0}, {0ull, 0, 0, 0, 0, 0, 0, 0}, 3, 0, 0},
    {2, {0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFEULL, 0, 0, 0, 0, 0, 0}, {0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFEULL, 0, 0, 0, 0, 0, 0}, 0, 0ull, 0xFFFFFFFFFFFFFFFEULL},
    {2, {0xFFFFFFFFFFFFFFFDULL, 0ull, 0, 0, 0, 0, 0, 0}, {0xFFFFFFFFFFFFFFFEULL, 2ull, 0, 0, 0, 0, 0, 0}, 4, 0, 0},
    {2, {0ull, 0ull, 0, 0, 0, 0, 0, 0}, {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL, 0, 0, 0, 0, 0, 0}, 5, 0, 0},
    {2, {0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFEULL, 0, 0, 0, 0, 0, 0}, {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL, 0, 0, 0, 0, 0, 0}, 5, 0, 0},
    {2, {1ull, 1ull, 0, 0, 0, 0, 0, 0}, {4294967296ull, 4294967296ull, 0, 0, 0, 0, 0, 0}, 0, 1ull, 4294967296ull},
    {2, {1ull, 4294967297ull, 0, 0, 0, 0, 0, 0}, {8589934592ull, 4294967296ull, 0, 0, 0, 0, 0, 0}, 0, 1ull, 8589934592ull},
    {2, {4294967295ull, 4294967295ull, 0, 0, 0, 0, 0, 0}, {4294967295ull, 4294967296ull, 0, 0, 0, 0, 0, 0}, 0, 4294967295ull, 0xFFFFFFFF00000000ULL},
    {2, {3ull, 3ull, 0, 0, 0, 0, 0, 0}, {0x8000000000000000ULL, 0xC000000000000000ULL, 0, 0, 0, 0, 0, 0}, 5, 0, 0},
};


constexpr std::uint64_t kGold = 0x9E3779B97F4A7C15ull;
constexpr std::uint64_t kS1 = 0x1234567890ABCDEFull;
constexpr std::uint64_t kS2 = 0x0FEDCBA987654321ull;

std::uint32_t code(CrtResult r) { return static_cast<std::uint32_t>(r); }

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
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
// Oracle: unsigned big integer, 6 limbs of 32 bits. Not shared with crt.cpp.
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
// Remainder of a big integer by a nonzero uint64_t, bit by bit from the top.
std::uint64_t big_mod(const Big& a, std::uint64_t m) {
  std::uint64_t r = 0;
  for (int i = kL * 32 - 1; i >= 0; --i) {
    const bool carry = (r >> 63) != 0;
    r = (r << 1) | ((a.w[i / 32] >> (i % 32)) & 1u);
    if (carry || r >= m) r -= m;
  }
  return r;
}
bool big_fits_u64(const Big& a) {
  for (int i = 2; i < kL; ++i)
    if (a.w[i] != 0) return false;
  return true;
}
std::uint64_t big_low(const Big& a) { return (static_cast<std::uint64_t>(a.w[1]) << 32) | a.w[0]; }

// Binary gcd on uint64_t (a different method from Euclid's remainder loop in the implementation); gcd(0, x) = x.
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

struct Tally {
  std::size_t bad = 0;
  std::string first;
  void fail(const std::string& what) {
    if (bad == 0) first = what;
    ++bad;
  }
};

std::string describe(const std::uint64_t* r, const std::uint64_t* m, std::uint32_t n) {
  std::string s = "system n=" + std::to_string(n) + ":";
  for (std::uint32_t i = 0; i < n && i < 8; ++i) s += " (" + std::to_string(r[i]) + " mod " + std::to_string(m[i]) + ")";
  return s + ": ";
}

// One system: calls the implementation, checks it with the oracle, folds the digest (a failure code folds alone, as in the reference).
void do_case(const std::uint64_t* r, const std::uint64_t* m, std::uint32_t n, Fold& f, Tally& t) {
  ++f.cases;
  std::uint64_t o1 = kS1, o2 = kS2;
  const CrtResult res = crt_solve(n, r, m, &o1, &o2);
  f.add(code(res));
  std::uint32_t want = 0;
  bool zero = false;
  for (std::uint32_t i = 0; i < n; ++i)
    if (m[i] == 0) zero = true;
  Big big = big_u64(1);
  if (zero) {
    want = 3;
  } else {
    bool ok = true;
    for (std::uint32_t i = 0; i < n && ok; ++i)
      for (std::uint32_t j = i + 1; j < n; ++j) {
        const std::uint64_t g = ogcd64(m[i], m[j]);
        if (r[i] % g != r[j] % g) {
          ok = false;
          break;
        }
      }
    if (!ok) {
      want = 4;
    } else {
      for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint64_t g = ogcd64(m[i], big_mod(big, m[i]));
        big = big_mul(big, big_u64(m[i] / g));
      }
      if (!big_fits_u64(big)) want = 5;
    }
  }
  std::string err;
  if (code(res) != want) {
    err = "code " + std::to_string(code(res)) + " expected " + std::to_string(want);
  } else if (want != 0) {
    if (o1 != kS1 || o2 != kS2) err = "failure changed an output";
  } else {
    f.add(o1);
    f.add(o2);
    if (o2 != big_low(big)) err = "out_m is not the exact lcm";
    else if (o1 >= o2) err = "out_r not below out_m";
    for (std::uint32_t i = 0; i < n && err.empty(); ++i)
      if (o1 % m[i] != r[i] % m[i]) err = "out_r violates a congruence";
    if (err.empty() && o2 <= 5000ull) {
      std::uint64_t x = 0;
      for (; x < o2; ++x) {
        bool all = true;
        for (std::uint32_t i = 0; i < n; ++i)
          if (x % m[i] != r[i] % m[i]) all = false;
        if (all) break;
      }
      if (x != o1) err = "brute force found another smallest solution";
    }
  }
  if (!err.empty()) t.fail(describe(r, m, n) + err);
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.h == digest);
}

std::vector<std::uint64_t> rs(std::uint64_t m) {
  std::vector<std::uint64_t> v = {0ull, 1ull, m - 1ull, 1ull << 63, kU64, 12345ull, 0ull - m};  // 0 - m wraps to 2^64 - m
  if (m <= kU64 - 1ull) v.push_back(m + 1ull);
  v.push_back(m);
  if (m <= (kU64 - 1ull) / 2ull) v.push_back(2ull * m + 1ull);
  std::sort(v.begin(), v.end());
  v.erase(std::unique(v.begin(), v.end()), v.end());
  return v;
}

std::vector<std::uint64_t> first_primes(std::size_t k) {
  std::vector<std::uint64_t> p;
  for (std::uint64_t c = 2; p.size() < k; ++c) {
    bool prime = true;
    for (const std::uint64_t q : p)
      if (c % q == 0) prime = false;
    if (prime) p.push_back(c);
  }
  return p;
}

}  // namespace

// =============================================================================================================================================
// Grid definitions: the sizes of the lists are recounted here.
// =============================================================================================================================================
TEST_CASE("list sizes match the reference", "[crt]") {
  CHECK(sizeof kRefBm / sizeof kRefBm[0] == kRefSizeBm);
  CHECK(sizeof kRefBt / sizeof kRefBt[0] == kRefSizeBt);
  CHECK(sizeof kRefXs / sizeof kRefXs[0] == kRefSizeXs);
  CHECK(sizeof kRefGs / sizeof kRefGs[0] == kRefSizeGs);
  CHECK(sizeof kRefAs / sizeof kRefAs[0] == kRefSizeAs);
  CHECK(sizeof kRefF7 / sizeof kRefF7[0] == kRefSizeF7);
}

// EXHAUSTIVE: n = 1, m in 1..64, r in 0..2m-1.
TEST_CASE("n = 1: exhaustive", "[crt]") {
  Fold f;
  Tally t;
  for (std::uint64_t m = 1; m <= 64; ++m)
    for (std::uint64_t r = 0; r < 2 * m; ++r) do_case(&r, &m, 1, f, t);
  expect_digest(f, t, kRefCases_n1, kRefDigest_n1);
}

// EXHAUSTIVE: n = 2, m1, m2 in 1..24, every residue in range (ordered pairs).
TEST_CASE("n = 2: exhaustive, moduli 1..24", "[crt]") {
  Fold f;
  Tally t;
  for (std::uint64_t m1 = 1; m1 <= 24; ++m1)
    for (std::uint64_t m2 = 1; m2 <= 24; ++m2)
      for (std::uint64_t r1 = 0; r1 < m1; ++r1)
        for (std::uint64_t r2 = 0; r2 < m2; ++r2) {
          const std::uint64_t r[2] = {r1, r2}, m[2] = {m1, m2};
          do_case(r, m, 2, f, t);
        }
  expect_digest(f, t, kRefCases_n2, kRefDigest_n2);
}

// EXHAUSTIVE: n = 3, m1..m3 in 1..8, every residue in range. The loops run over ORDERED triples, so every ordering of every system is included.
TEST_CASE("n = 3: exhaustive, moduli 1..8, every ordering", "[crt]") {
  Fold f;
  Tally t;
  for (std::uint64_t m1 = 1; m1 <= 8; ++m1)
    for (std::uint64_t m2 = 1; m2 <= 8; ++m2)
      for (std::uint64_t m3 = 1; m3 <= 8; ++m3)
        for (std::uint64_t r1 = 0; r1 < m1; ++r1)
          for (std::uint64_t r2 = 0; r2 < m2; ++r2)
            for (std::uint64_t r3 = 0; r3 < m3; ++r3) {
              const std::uint64_t r[3] = {r1, r2, r3}, m[3] = {m1, m2, m3};
              do_case(r, m, 3, f, t);
            }
  expect_digest(f, t, kRefCases_n3, kRefDigest_n3);
}

// EXHAUSTIVE: n = 4, moduli from {1, 2, 3, 4, 6} (ordered, so every ordering is included), every residue in range.
TEST_CASE("n = 4: exhaustive, moduli in {1,2,3,4,6}, every ordering", "[crt]") {
  Fold f;
  Tally t;
  const std::uint64_t s4[5] = {1, 2, 3, 4, 6};
  for (const std::uint64_t m1 : s4)
    for (const std::uint64_t m2 : s4)
      for (const std::uint64_t m3 : s4)
        for (const std::uint64_t m4 : s4)
          for (std::uint64_t r1 = 0; r1 < m1; ++r1)
            for (std::uint64_t r2 = 0; r2 < m2; ++r2)
              for (std::uint64_t r3 = 0; r3 < m3; ++r3)
                for (std::uint64_t r4 = 0; r4 < m4; ++r4) {
                  const std::uint64_t r[4] = {r1, r2, r3, r4}, m[4] = {m1, m2, m3, m4};
                  do_case(r, m, 4, f, t);
                }
  expect_digest(f, t, kRefCases_n4, kRefDigest_n4);
}

// EXHAUSTIVE: residues above the modulus (including UINT64_MAX): m1, m2 in 1..16, r in RS(m).
TEST_CASE("residues above the modulus: exhaustive over RS(m)", "[crt]") {
  Fold f;
  Tally t;
  for (std::uint64_t m1 = 1; m1 <= 16; ++m1)
    for (std::uint64_t m2 = 1; m2 <= 16; ++m2)
      for (const std::uint64_t r1 : rs(m1))
        for (const std::uint64_t r2 : rs(m2)) {
          const std::uint64_t r[2] = {r1, r2}, m[2] = {m1, m2};
          do_case(r, m, 2, f, t);
        }
  expect_digest(f, t, kRefCases_resid, kRefDigest_resid);
}

// EXHAUSTIVE over BM x BM (moduli near 2^32, 2^63 and 2^64-1) and RS residues, plus the consistent variant (r1, r1 mod g).
TEST_CASE("big moduli pairs: exhaustive over BM x BM", "[crt]") {
  Fold f;
  Tally t;
  for (std::size_t i = 0; i < kRefSizeBm; ++i)
    for (std::size_t j = 0; j < kRefSizeBm; ++j) {
      const std::uint64_t m[2] = {kRefBm[i], kRefBm[j]};
      for (const std::uint64_t r1 : rs(m[0]))
        for (const std::uint64_t r2 : rs(m[1])) {
          const std::uint64_t r[2] = {r1, r2};
          do_case(r, m, 2, f, t);
        }
      const std::uint64_t g = ogcd64(m[0], m[1]);
      for (const std::uint64_t r1 : rs(m[0])) {
        const std::uint64_t r[2] = {r1, r1 % g};
        do_case(r, m, 2, f, t);
      }
    }
  expect_digest(f, t, kRefCases_bigpairs, kRefDigest_bigpairs);
}

// EXHAUSTIVE over BT^3 (ordered) with residues X mod m (consistent) and with the last residue shifted by one.
TEST_CASE("big moduli triples: exhaustive over BT^3", "[crt]") {
  Fold f;
  Tally t;
  for (std::size_t a = 0; a < kRefSizeBt; ++a)
    for (std::size_t b = 0; b < kRefSizeBt; ++b)
      for (std::size_t c = 0; c < kRefSizeBt; ++c) {
        const std::uint64_t m[3] = {kRefBt[a], kRefBt[b], kRefBt[c]};
        for (std::size_t k = 0; k < kRefSizeXs; ++k) {
          const std::uint64_t x = kRefXs[k];
          const std::uint64_t r[3] = {x % m[0], x % m[1], x % m[2]};
          do_case(r, m, 3, f, t);
          const std::uint64_t r2[3] = {x % m[0], x % m[1], (x % m[2] + 1ull) % m[2]};
          do_case(r2, m, 3, f, t);
        }
      }
  expect_digest(f, t, kRefCases_bigtrip, kRefDigest_bigtrip);
}

// EXHAUSTIVE: gcd-heavy pairs m1 = g*a, m2 = g*b whose product exceeds 2^64 but whose lcm usually fits; consistent and inconsistent residues.
TEST_CASE("gcd-heavy pairs: exhaustive over GS x AS x AS", "[crt]") {
  Fold f;
  Tally t;
  for (std::size_t gi = 0; gi < kRefSizeGs; ++gi)
    for (std::size_t ai = 0; ai < kRefSizeAs; ++ai)
      for (std::size_t bi = 0; bi < kRefSizeAs; ++bi) {
        const std::uint64_t g = kRefGs[gi], a = kRefAs[ai], b = kRefAs[bi];
        if (a > kU64 / g || b > kU64 / g) continue;
        const std::uint64_t m[2] = {g * a, g * b};
        const std::uint64_t cs[2] = {0ull, 7ull};
        for (const std::uint64_t c : cs) {
          const std::uint64_t r[2] = {c + g, c + 2ull * g};
          do_case(r, m, 2, f, t);
          const std::uint64_t r2[2] = {c + g, c + 1ull + 2ull * g};
          do_case(r2, m, 2, f, t);
        }
      }
  expect_digest(f, t, kRefCases_gcdheavy, kRefDigest_gcdheavy);
}

// Fibonacci moduli F(a), F(b), 3 <= a < b <= 93 (F(93) is the largest Fibonacci number below 2^64): every pair, consistent and shifted residues.
// gcd(F(a), F(b)) = F(gcd(a, b)), and consecutive pairs give the longest Euclid chains.
TEST_CASE("Fibonacci moduli: every pair", "[crt]") {
  std::uint64_t fib[95];
  fib[0] = 0;
  fib[1] = 1;
  for (int i = 2; i < 95; ++i) fib[i] = fib[i - 1] + fib[i - 2];
  CHECK(fib[93] < kU64);
  CHECK(fib[93] > kU64 / 2);
  Fold f;
  Tally t;
  const std::uint64_t x = 987654321987ull;
  for (int a = 3; a <= 93; ++a)
    for (int b = a + 1; b <= 93; ++b) {
      const std::uint64_t m[2] = {fib[a], fib[b]};
      const std::uint64_t r[2] = {x % m[0], x % m[1]};
      do_case(r, m, 2, f, t);
      const std::uint64_t r2[2] = {x % m[0], (x + 1ull) % m[1]};
      do_case(r2, m, 2, f, t);
    }
  expect_digest(f, t, kRefCases_fib, kRefDigest_fib);
}

// 64-equation systems (the largest allowed n).
TEST_CASE("64 equations", "[crt]") {
  Fold f;
  Tally t;
  const std::uint64_t x = 0xDEADBEEFCAFEBABEull;
  const std::vector<std::uint64_t> p = first_primes(64);
  {
    std::uint64_t r[64], m[64];
    for (std::size_t i = 0; i < 64; ++i) {
      m[i] = p[i];
      r[i] = kU64 % p[i];
    }
    do_case(r, m, 64, f, t);  // first 64 primes: consistent, lcm far above 2^64: Overflow
  }
  {
    std::uint64_t r[64], m[64];
    for (std::uint32_t k = 0; k < 64; ++k) {
      m[k] = 1ull << k;
      r[k] = x % m[k];
    }
    do_case(r, m, 64, f, t);  // 64 divisors of 2^63: Ok, lcm 2^63
  }
  for (std::size_t j = 0; j < 63; ++j) {
    std::uint64_t r[64], m[64];
    for (std::size_t i = 0; i < 63; ++i) {
      m[i] = p[i];
      r[i] = x % p[i];
    }
    m[63] = p[j];
    r[63] = (x % p[j] + 1ull) % p[j];
    do_case(r, m, 64, f, t);  // one conflicting pair: NoSolution although the lcm would overflow
  }
  const std::uint64_t eqm[4] = {1ull, 7ull, kU64, 1ull << 63};
  for (const std::uint64_t mv : eqm) {
    std::uint64_t r[64], m[64];
    for (std::size_t i = 0; i < 64; ++i) {
      m[i] = mv;
      r[i] = x % mv;
    }
    do_case(r, m, 64, f, t);
    r[63] = (x + 1ull) % mv;
    do_case(r, m, 64, f, t);
  }
  for (std::size_t rot = 0; rot < 7; ++rot) {
    std::uint64_t r[64], m[64];
    for (std::size_t i = 0; i < 64; ++i) {
      m[i] = kRefF7[(i + rot) % 7];
      r[i] = x % m[i];
    }
    do_case(r, m, 64, f, t);  // the seven coprime factors of 2^64 - 1: lcm exactly 2^64 - 1, Ok
    m[63] = 2;
    r[63] = x % 2ull;
    do_case(r, m, 64, f, t);  // plus a modulus 2: Overflow
  }
  expect_digest(f, t, kRefCases_big64, kRefDigest_big64);
}

// NOT exhaustive: 300 seeds x 10 random systems (up to 6 equations; moduli random or sharing a factor; residues consistent or random).
TEST_CASE("random systems (sampled, not exhaustive)", "[crt]") {
  Fold f;
  Tally t;
  for (std::uint32_t s = 0; s < kRefRandSeeds; ++s) {
    Lcg q(static_cast<std::uint64_t>(s + 1u) * kGold);
    for (std::uint32_t k = 0; k < kRefRandPerSeed; ++k) {
      const std::uint32_t n = q.next() % 7u;
      const std::uint32_t cons = q.next() & 1u;
      const std::uint64_t x = q.next64();
      const std::uint32_t gb = q.next() % 20u + 1u;
      std::uint64_t gg = q.next64() >> (64u - gb);
      if (gg == 0) gg = 1;
      std::uint64_t r[6], m[6];
      for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t fl = q.next() & 1u;
        const std::uint32_t bits = q.next() % 64u + 1u;
        const std::uint64_t m64 = q.next64(), r64 = q.next64();
        std::uint64_t mi = fl != 0 ? ((m64 >> 52) + 1ull) * gg : (m64 >> (64u - bits));
        if (mi == 0) mi = 1;
        m[i] = mi;
        r[i] = cons != 0 ? x % mi : r64;
      }
      do_case(r, m, n, f, t);
    }
  }
  expect_digest(f, t, kRefCases_rand, kRefDigest_rand);
}

// CONSTANTS: hand rows (Python integers). Item rows: modulus 2^64 - 1 alone, the seven coprime factors of 2^64 - 1 (Ok) and the same plus a modulus
// 2 (Overflow), 2^63 with 3 (Overflow), 2^32 with 2^32 - 1 (fits), (2^32 - 1, 2^32 + 1) (lcm exactly 2^64 - 1), conflicts whose lcm would overflow
// (NoSolution), products above 2^64 with a fitting lcm, zero moduli, equal moduli.
TEST_CASE("hand rows (constants), not exhaustive", "[crt]") {
  Fold f;
  Tally t;
  for (const HandSys& row : kHandSys) {
    std::uint64_t o1 = kS1, o2 = kS2;
    const CrtResult res = crt_solve(row.n, row.r, row.m, &o1, &o2);
    INFO(describe(row.r, row.m, row.n));
    CHECK(code(res) == row.code);
    if (row.code == 0) {
      CHECK(o1 == row.out_r);
      CHECK(o2 == row.out_m);
    } else {
      CHECK(o1 == kS1);
      CHECK(o2 == kS2);
    }
    do_case(row.r, row.m, row.n, f, t);
  }
  INFO(t.first);
  CHECK(t.bad == 0);
}

// =============================================================================================================================================
// Check order, nulls, TooLarge, ZeroModulus (written by hand from the header text).
// =============================================================================================================================================
TEST_CASE("empty system and the null rules", "[crt]") {
  std::uint64_t o1 = kS1, o2 = kS2;
  // n = 0: r and m may be null; the result is (0, 1)
  CHECK(crt_solve(0, nullptr, nullptr, &o1, &o2) == CrtResult::Ok);
  CHECK(o1 == 0ull);
  CHECK(o2 == 1ull);
  const std::uint64_t r[2] = {1, 2}, m[2] = {3, 5};
  o1 = kS1;
  o2 = kS2;
  CHECK(crt_solve(0, r, m, &o1, &o2) == CrtResult::Ok);
  CHECK(o1 == 0ull);
  CHECK(o2 == 1ull);
  o1 = kS1;
  o2 = kS2;
  // an output pointer null is NullArgument at every n, including 0
  CHECK(crt_solve(0, nullptr, nullptr, nullptr, &o2) == CrtResult::NullArgument);
  CHECK(crt_solve(0, nullptr, nullptr, &o1, nullptr) == CrtResult::NullArgument);
  CHECK(crt_solve(0, nullptr, nullptr, nullptr, nullptr) == CrtResult::NullArgument);
  CHECK(crt_solve(2, r, m, nullptr, &o2) == CrtResult::NullArgument);
  CHECK(crt_solve(2, r, m, &o1, nullptr) == CrtResult::NullArgument);
  // r or m null with n above 0
  CHECK(crt_solve(2, nullptr, m, &o1, &o2) == CrtResult::NullArgument);
  CHECK(crt_solve(2, r, nullptr, &o1, &o2) == CrtResult::NullArgument);
  CHECK(crt_solve(2, nullptr, nullptr, &o1, &o2) == CrtResult::NullArgument);
  CHECK(crt_solve(1, nullptr, m, &o1, &o2) == CrtResult::NullArgument);
  // the given n is used even when it is above the limit: null beats TooLarge
  CHECK(crt_solve(65, nullptr, m, &o1, &o2) == CrtResult::NullArgument);
  CHECK(crt_solve(65, r, nullptr, &o1, &o2) == CrtResult::NullArgument);
  CHECK(crt_solve(0xFFFFFFFFu, nullptr, nullptr, &o1, &o2) == CrtResult::NullArgument);
  CHECK(crt_solve(65, r, m, nullptr, &o2) == CrtResult::NullArgument);
  CHECK(o1 == kS1);
  CHECK(o2 == kS2);
}

TEST_CASE("TooLarge: n above 64, the arrays are not read", "[crt]") {
  std::uint64_t o1 = kS1, o2 = kS2;
  const std::uint64_t r[2] = {1, 2}, m[2] = {0, 5};  // a zero modulus in an array that is too long must not be reported
  CHECK(crt_solve(65, r, m, &o1, &o2) == CrtResult::TooLarge);   // would read past the 2-element arrays if it looked (ASan)
  CHECK(crt_solve(66, r, m, &o1, &o2) == CrtResult::TooLarge);
  CHECK(crt_solve(1000, r, m, &o1, &o2) == CrtResult::TooLarge);
  CHECK(crt_solve(0xFFFFFFFFu, r, m, &o1, &o2) == CrtResult::TooLarge);
  CHECK(crt_solve(0x80000000u, r, m, &o1, &o2) == CrtResult::TooLarge);
  CHECK(o1 == kS1);
  CHECK(o2 == kS2);
  // 64 is allowed, 65 is not (real 65-element arrays)
  std::uint64_t r65[65], m65[65];
  for (std::size_t i = 0; i < 65; ++i) {
    r65[i] = 0;
    m65[i] = 1;
  }
  CHECK(crt_solve(64, r65, m65, &o1, &o2) == CrtResult::Ok);
  CHECK(o1 == 0ull);
  CHECK(o2 == 1ull);
  o1 = kS1;
  o2 = kS2;
  CHECK(crt_solve(65, r65, m65, &o1, &o2) == CrtResult::TooLarge);
  CHECK(o1 == kS1);
  CHECK(o2 == kS2);
  CHECK(kMaxEquations == 64u);
}

TEST_CASE("ZeroModulus: every modulus is scanned, before NoSolution and Overflow", "[crt]") {
  std::uint64_t o1 = kS1, o2 = kS2;
  const std::uint64_t r1[1] = {0}, m1[1] = {0};
  CHECK(crt_solve(1, r1, m1, &o1, &o2) == CrtResult::ZeroModulus);
  // the zero is last, or in the middle, or the system is inconsistent / overflowing without it
  const std::uint64_t ra[3] = {1, 2, 3}, ma[3] = {4, 6, 0};  // 1 mod 4 and 2 mod 6 conflict (gcd 2), zero last
  CHECK(crt_solve(3, ra, ma, &o1, &o2) == CrtResult::ZeroModulus);
  const std::uint64_t rb[3] = {1, 2, 3}, mb[3] = {0, 4, 6};
  CHECK(crt_solve(3, rb, mb, &o1, &o2) == CrtResult::ZeroModulus);
  const std::uint64_t rc[3] = {1, 2, 3}, mc[3] = {kU64, kU64 - 1ull, 0};  // lcm of the first two would overflow, zero last
  CHECK(crt_solve(3, rc, mc, &o1, &o2) == CrtResult::ZeroModulus);
  const std::uint64_t rd[3] = {0, 0, 0}, md[3] = {0, kU64, kU64 - 1ull};
  CHECK(crt_solve(3, rd, md, &o1, &o2) == CrtResult::ZeroModulus);
  std::uint64_t r64[64], m64[64];
  for (std::size_t i = 0; i < 64; ++i) {
    r64[i] = 1;
    m64[i] = 1;
  }
  m64[63] = 0;
  CHECK(crt_solve(64, r64, m64, &o1, &o2) == CrtResult::ZeroModulus);
  m64[63] = 1;
  m64[0] = 0;
  CHECK(crt_solve(64, r64, m64, &o1, &o2) == CrtResult::ZeroModulus);
  CHECK(o1 == kS1);
  CHECK(o2 == kS2);
}

TEST_CASE("NoSolution beats Overflow, Overflow needs a consistent system", "[crt]") {
  std::uint64_t o1 = kS1, o2 = kS2;
  const std::uint64_t m[3] = {1ull << 63, 3, 2};
  const std::uint64_t rbad[3] = {0, 1, 1};  // 2^63 and 2 conflict; the lcm 3 * 2^63 would overflow
  CHECK(crt_solve(3, rbad, m, &o1, &o2) == CrtResult::NoSolution);
  const std::uint64_t rgood[3] = {0, 1, 0};
  CHECK(crt_solve(3, rgood, m, &o1, &o2) == CrtResult::Overflow);
  CHECK(o1 == kS1);
  CHECK(o2 == kS2);
  // the conflict is in the last pair (checked after the overflow would already have happened in a merge)
  const std::uint64_t m2[4] = {kU64, kU64 - 1ull, 6, 10};
  const std::uint64_t r2[4] = {1, 1, 1, 2};
  CHECK(crt_solve(4, r2, m2, &o1, &o2) == CrtResult::NoSolution);
  const std::uint64_t r3[4] = {1, 1, 1, 1};
  CHECK(crt_solve(4, r3, m2, &o1, &o2) == CrtResult::Overflow);
  CHECK(o1 == kS1);
  CHECK(o2 == kS2);
}

TEST_CASE("boundary rows: 2^64 - 1 is the largest lcm", "[crt]") {
  std::uint64_t o1 = kS1, o2 = kS2;
  const std::uint64_t one[1] = {kU64 - 5ull}, mone[1] = {kU64};
  CHECK(crt_solve(1, one, mone, &o1, &o2) == CrtResult::Ok);
  CHECK(o1 == kU64 - 5ull);
  CHECK(o2 == kU64);
  const std::uint64_t f7[7] = {3, 5, 17, 257, 641, 65537, 6700417};
  std::uint64_t r7[7];
  for (std::size_t i = 0; i < 7; ++i) r7[i] = 2ull % f7[i];
  CHECK(crt_solve(7, r7, f7, &o1, &o2) == CrtResult::Ok);
  CHECK(o2 == kU64);
  CHECK(o1 == 2ull);
  std::uint64_t r8[8], m8[8];
  for (std::size_t i = 0; i < 7; ++i) {
    r8[i] = r7[i];
    m8[i] = f7[i];
  }
  r8[7] = 0;
  m8[7] = 2;
  o1 = kS1;
  o2 = kS2;
  CHECK(crt_solve(8, r8, m8, &o1, &o2) == CrtResult::Overflow);
  const std::uint64_t r23[2] = {0, 0}, m23[2] = {1ull << 63, 3};
  CHECK(crt_solve(2, r23, m23, &o1, &o2) == CrtResult::Overflow);
  const std::uint64_t r32[2] = {0, 0}, m32[2] = {1ull << 32, 0xFFFFFFFFull};
  CHECK(crt_solve(2, r32, m32, &o1, &o2) == CrtResult::Ok);
  CHECK(o2 == 0xFFFFFFFF00000000ull);
  CHECK(o1 == 0ull);
  CHECK(o1 != kS1);
}
