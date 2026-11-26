// Tests for core/fixed. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/fixed_ref.py, an independent Python implementation
// (exact fractions for the rounding, math.isqrt for the square root). Regenerate it with:
// python3 tools/fixed_ref.py
// The tests also carry a naive oracle (sign and magnitude with a remainder test, a binary search for the square root)
// that shares no code with core/fixed/fixed.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "fixed.h"

namespace fx = lumora::fixed;
using namespace lumora::fixed;

namespace {

// BEGIN GENERATED REFERENCE (tools/fixed_ref.py)
const std::int32_t kRefE[] = {INT32_MIN, -2147483647, -2147418112, -65537, -65536, -65535, -32769, -32768, -3, -2, -1, 0, 1, 2, 3, 32767, 32768, 32769, 65535, 65536, 65537, 98304, 131072, 218453, 1073741824, 2147418111, 2147483646, 2147483647, 46340, 46341, 11862016, 2147450879, 2147450880, -2147450880, 12345678, -12345678, 7, -7, 21845, 43691};
const std::uint64_t kRefK[] = {1ull, 2ull, 3ull, 4ull, 5ull, 255ull, 256ull, 257ull, 65535ull, 65536ull, 65537ull, 1048575ull, 1048576ull, 16777216ull, 1073741823ull, 1073741824ull, 2147483647ull, 2147483648ull, 2147483649ull, 3037000499ull, 3037000500ull, 4294967295ull, 4294967294ull, 4294967293ull, 4294901760ull};
const std::size_t kRefRandN = 20000;
const std::uint64_t kRefDigest_mul = 0xBAEA02B125D4D60Eull;
const std::size_t kRefCases_mul = 21600;
const std::uint64_t kRefDigest_div = 0xC018C2764885B290ull;
const std::size_t kRefCases_div = 21600;
const std::uint64_t kRefDigest_lerp = 0xA02238029207943Bull;
const std::size_t kRefCases_lerp = 84000;
const std::uint64_t kRefDigest_floor = 0x9605A3E813C4FE3Cull;
const std::size_t kRefCases_floor = 420041;
const std::uint64_t kRefDigest_from_int = 0x39BEA01ED8E04D71ull;
const std::size_t kRefCases_from_int = 220041;
const std::uint64_t kRefDigest_sqrt = 0x43115CA3BAA7EF14ull;
const std::size_t kRefCases_sqrt = 90041;
const std::uint64_t kRefDigest_isqrt = 0xE218745DA0448C06ull;
const std::size_t kRefCases_isqrt = 22082;
// END GENERATED REFERENCE

constexpr std::uint64_t kLcgMul = 6364136223846793005ull;
constexpr std::uint64_t kLcgAdd = 1442695040888963407ull;

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  std::size_t n = 0;
  void word(std::uint32_t w) {
    for (int i = 0; i < 4; ++i) {
      h ^= (w >> (8 * i)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
    ++n;
  }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint64_t next() {
    x = x * kLcgMul + kLcgAdd;
    return x;
  }
  std::int32_t q() {
    const std::uint64_t r = next();
    const std::uint32_t mag = static_cast<std::uint32_t>((r >> 32) >> ((r >> 8) & 31u));
    const bool neg = ((r >> 7) & 1u) != 0;
    return static_cast<std::int32_t>(neg ? std::uint32_t{0} - mag : mag);
  }
  std::uint64_t u64() {
    const std::uint64_t r = next();
    return r >> ((r >> 1) & 63u);
  }
};

constexpr std::size_t kE = sizeof(kRefE) / sizeof(kRefE[0]);

// ---- naive oracle -------------------------------------------------------------------------------------------------
std::uint64_t mag_of(std::int64_t v) {
  return v < 0 ? static_cast<std::uint64_t>(-(v + 1)) + 1u : static_cast<std::uint64_t>(v);
}

// Nearest integer to mag / den, an exact half rounds up (away from zero for a magnitude).
std::uint64_t round_div(std::uint64_t mag, std::uint64_t den) {
  const std::uint64_t q = mag / den;
  const std::uint64_t r = mag % den;
  return r >= den - r ? q + 1u : q;
}

std::int32_t clamp_signed(bool neg, std::uint64_t mag) {
  if (neg) return mag >= 2147483648ull ? INT32_MIN : static_cast<std::int32_t>(-static_cast<std::int64_t>(mag));
  return mag > 2147483647ull ? INT32_MAX : static_cast<std::int32_t>(mag);
}

std::int32_t o_mul(std::int32_t a, std::int32_t b) {
  const std::uint64_t m = mag_of(a) * mag_of(b);
  return clamp_signed((a < 0) != (b < 0), round_div(m, 65536u));
}

std::int32_t o_div(std::int32_t a, std::int32_t b) {
  if (a == 0) return 0;
  if (b == 0) return a > 0 ? INT32_MAX : INT32_MIN;
  return clamp_signed((a < 0) != (b < 0), round_div(mag_of(a) * 65536u, mag_of(b)));
}

std::int32_t o_lerp(std::int32_t a, std::int32_t b, std::int32_t t) {
  const std::int64_t d = static_cast<std::int64_t>(b) - a;
  const std::uint64_t tm = round_div(mag_of(d) * mag_of(t), 65536u);
  const std::int64_t term = ((d < 0) != (t < 0)) ? -static_cast<std::int64_t>(tm) : static_cast<std::int64_t>(tm);
  const std::int64_t sum = a + term;
  if (sum > INT32_MAX) return INT32_MAX;
  if (sum < INT32_MIN) return INT32_MIN;
  return static_cast<std::int32_t>(sum);
}

std::int32_t o_floor(std::int32_t a) {
  if (a >= 0) return a / 65536;
  const std::int64_t m = -static_cast<std::int64_t>(a);
  return static_cast<std::int32_t>(-(m / 65536) - ((m % 65536) != 0 ? 1 : 0));
}

std::int32_t o_from_int(std::int32_t n) {
  const std::int64_t v = static_cast<std::int64_t>(n) * 65536;
  return v > INT32_MAX ? INT32_MAX : (v < INT32_MIN ? INT32_MIN : static_cast<std::int32_t>(v));
}

std::uint32_t o_isqrt(std::uint64_t v) {
  std::uint64_t lo = 0, hi = 0xFFFFFFFFull;
  while (lo < hi) {
    const std::uint64_t mid = lo + (hi - lo + 1u) / 2u;
    if (mid * mid <= v) lo = mid;
    else hi = mid - 1u;
  }
  return static_cast<std::uint32_t>(lo);
}

std::int32_t o_sqrt(std::int32_t a) {
  return a <= 0 ? 0 : static_cast<std::int32_t>(o_isqrt(static_cast<std::uint64_t>(a) * 65536u));
}

std::uint32_t u(std::int32_t v) { return static_cast<std::uint32_t>(v); }

}  // namespace

TEST_CASE("constants", "[fixed]") {
  CHECK(kOne == 65536);
  CHECK(kMax == INT32_MAX);
  CHECK(kMin == INT32_MIN);
}

TEST_CASE("from_int", "[fixed][from_int]") {
  CHECK(from_int(0) == 0);
  CHECK(from_int(1) == 65536);
  CHECK(from_int(-1) == -65536);
  CHECK(from_int(32767) == 2147418112);
  CHECK(from_int(-32768) == INT32_MIN);  // exact, not a clamp
  CHECK(from_int(32768) == INT32_MAX);
  CHECK(from_int(-32769) == INT32_MIN);
  CHECK(from_int(INT32_MAX) == INT32_MAX);
  CHECK(from_int(INT32_MIN) == INT32_MIN);
  std::size_t bad = 0;
  for (std::int32_t n = -32768; n <= 32767; ++n)
    if (from_int(n) != n * 65536 || to_int_floor(from_int(n)) != n) ++bad;
  CHECK(bad == 0);
}

TEST_CASE("to_int_floor", "[fixed][floor]") {
  CHECK(to_int_floor(0) == 0);
  CHECK(to_int_floor(-1) == -1);
  CHECK(to_int_floor(1) == 0);
  CHECK(to_int_floor(kOne - 1) == 0);
  CHECK(to_int_floor(kOne) == 1);
  CHECK(to_int_floor(-kOne) == -1);
  CHECK(to_int_floor(-kOne - 1) == -2);
  CHECK(to_int_floor(-kOne + 1) == -1);
  CHECK(to_int_floor(kMax) == 32767);
  CHECK(to_int_floor(kMin) == -32768);
  CHECK(to_int_floor(kMin + 1) == -32768);
  std::size_t bad = 0;
  for (std::int64_t a = -200000; a <= 200000; ++a) {
    const std::int32_t f = to_int_floor(static_cast<std::int32_t>(a));
    if (static_cast<std::int64_t>(f) * 65536 > a || a >= (static_cast<std::int64_t>(f) + 1) * 65536) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("mul known values and rounding", "[fixed][mul]") {
  CHECK(mul(kOne, kOne) == kOne);
  CHECK(mul(3 * kOne, -2 * kOne) == -6 * kOne);
  CHECK(mul(32768, kOne) == 32768);
  CHECK(mul(1, 32768) == 1);        // 0.5 ulp: tie goes away from zero
  CHECK(mul(-1, 32768) == -1);
  CHECK(mul(1, -32768) == -1);
  CHECK(mul(-1, -32768) == 1);
  CHECK(mul(1, 32767) == 0);        // just below the tie
  CHECK(mul(-1, 32767) == 0);
  CHECK(mul(3, 32768) == 2);        // 1.5 ulp rounds up
  CHECK(mul(-3, 32768) == -2);
  CHECK(mul(5, 32768) == 3);        // 2.5 ulp rounds up
  CHECK(mul(7, 9362) == 1);         // 65534 / 65536
  CHECK(mul(0, kMax) == 0);
  CHECK(mul(kMin, 0) == 0);
  CHECK(mul(kMin, kOne) == kMin);
  CHECK(mul(kMax, kOne) == kMax);
  CHECK(mul(kMin, -kOne) == kMax);  // +2^31 saturates
  CHECK(mul(kMin, kMin) == kMax);
  CHECK(mul(kMax, kMax) == kMax);
  CHECK(mul(kMin, kMax) == kMin);
  CHECK(mul(from_int(32767), from_int(32767)) == kMax);
  CHECK(mul(kMax, 2 * kOne) == kMax);
  CHECK(mul(kMin, 2 * kOne) == kMin);
  CHECK(mul(kMin, 32768) == -1073741824);
}

TEST_CASE("mul saturation boundary", "[fixed][mul]") {
  // a * b / 65536 == 2^31 - 0.5 rounds to 2^31 (kMax after the clamp); 2^31 - 1 stays kMax; -(2^31) is kMin exactly.
  CHECK(mul(65536, kMax) == kMax);
  CHECK(mul(-65536, kMax) == -kMax);
  CHECK(mul(65536, kMin) == kMin);
  CHECK(mul(-65536, kMin) == kMax);
  CHECK(mul(-65535, kMin) == 2147450880);
  CHECK(mul(65535, kMin) == -2147450880);
  CHECK(mul(kMax, 65537) == kMax);
  CHECK(mul(kMax, 65535) == 2147450879);
}

TEST_CASE("div known values and rounding", "[fixed][div]") {
  CHECK(fx::div(kOne, kOne) == kOne);
  CHECK(fx::div(kOne, 2 * kOne) == 32768);
  CHECK(fx::div(-kOne, 2 * kOne) == -32768);
  CHECK(fx::div(kOne, -2 * kOne) == -32768);
  CHECK(fx::div(-kOne, -2 * kOne) == 32768);
  CHECK(fx::div(1, 3) == 21845);        // 21845.33
  CHECK(fx::div(2, 3) == 43691);        // 43690.67
  CHECK(fx::div(-1, 3) == -21845);
  CHECK(fx::div(-2, 3) == -43691);
  CHECK(fx::div(1, 131072) == 1);       // 0.5 exactly: away from zero
  CHECK(fx::div(-1, 131072) == -1);
  CHECK(fx::div(1, -131072) == -1);
  CHECK(fx::div(1, 131073) == 0);       // just below the half
  CHECK(fx::div(1, 131071) == 1);
  CHECK(fx::div(3, 131072) == 2);       // 1.5
  CHECK(fx::div(5, 131072) == 3);       // 2.5
  CHECK(fx::div(0, 5) == 0);
  CHECK(fx::div(0, -5) == 0);
  CHECK(fx::div(0, 0) == 0);
  CHECK(fx::div(1, 0) == kMax);
  CHECK(fx::div(kMax, 0) == kMax);
  CHECK(fx::div(-1, 0) == kMin);
  CHECK(fx::div(kMin, 0) == kMin);
  CHECK(fx::div(kMax, 1) == kMax);
  CHECK(fx::div(kMin, 1) == kMin);
  CHECK(fx::div(kMin, -1) == kMax);
  CHECK(fx::div(kMin, kOne) == kMin);   // exact, no clamp needed
  CHECK(fx::div(kMax, kOne) == kMax);
  CHECK(fx::div(kMin, kMin) == kOne);
  CHECK(fx::div(kMax, kMax) == kOne);
  CHECK(fx::div(kMax, kMin) == -kOne);
  CHECK(fx::div(kMax, 32768) == kMax);  // saturates
  CHECK(fx::div(1073741824, 2 * kOne) == 536870912);
  CHECK(fx::div(kMin, 3 * kOne) == -715827883);  // 2^31 / 3 = 715827882.67
}

TEST_CASE("lerp known values and exactness", "[fixed][lerp]") {
  CHECK(lerp(0, 10 * kOne, 32768) == 5 * kOne);
  CHECK(lerp(0, 10 * kOne, 0) == 0);
  CHECK(lerp(0, 10 * kOne, kOne) == 10 * kOne);
  CHECK(lerp(10 * kOne, 0, 32768) == 5 * kOne);
  CHECK(lerp(5, 6, 32768) == 6);    // 5 + round(0.5): tie away
  CHECK(lerp(5, 4, 32768) == 4);    // 5 + round(-0.5) = 5 - 1
  CHECK(lerp(5, 5, kMax) == 5);
  CHECK(lerp(0, 3, 21845) == 1);    // 0.99998 rounds to 1
  CHECK(lerp(0, kOne, 2 * kOne) == 2 * kOne);  // extrapolates
  CHECK(lerp(0, kOne, -kOne) == -kOne);
  CHECK(lerp(kMin, kMax, 2 * kOne) == kMax);
  CHECK(lerp(kMax, kMin, 2 * kOne) == kMin);
  CHECK(lerp(kMin, kMax, kMin) == kMin);
  CHECK(lerp(kMin, kMax, kMax) == kMax);
  CHECK(lerp(kMin, kMax, kOne) == kMax);
  CHECK(lerp(kMax, kMin, kOne) == kMin);
  CHECK(lerp(kMin, kMax, 0) == kMin);
  CHECK(lerp(kMin, kMax, 32768) == 0);  // -2^31 + round(4294967295 / 2) = -2^31 + 2147483648
  CHECK(lerp(kMax, kMin, 32768) == -1);  // 2^31 - 1 + round(-2147483647.5) = 2^31 - 1 - 2147483648
}

TEST_CASE("isqrt_u64 known values", "[fixed][isqrt]") {
  CHECK(isqrt_u64(0) == 0);
  CHECK(isqrt_u64(1) == 1);
  CHECK(isqrt_u64(2) == 1);
  CHECK(isqrt_u64(3) == 1);
  CHECK(isqrt_u64(4) == 2);
  CHECK(isqrt_u64(8) == 2);
  CHECK(isqrt_u64(9) == 3);
  CHECK(isqrt_u64(15) == 3);
  CHECK(isqrt_u64(16) == 4);
  CHECK(isqrt_u64(UINT64_MAX) == 4294967295u);
  CHECK(isqrt_u64(UINT64_MAX - 1) == 4294967295u);
  CHECK(isqrt_u64(4294967295ull * 4294967295ull) == 4294967295u);
  CHECK(isqrt_u64(4294967295ull * 4294967295ull - 1) == 4294967294u);
  CHECK(isqrt_u64(1ull << 63) == 3037000499u);
  CHECK(isqrt_u64(1ull << 62) == 2147483648u);
  CHECK(isqrt_u64((1ull << 62) - 1) == 2147483647u);
}

TEST_CASE("isqrt_u64 exact around every square", "[fixed][isqrt][exhaustive]") {
  std::size_t bad = 0;
  for (std::uint64_t k = 0; k <= 70000; ++k) {
    const std::uint64_t sq = k * k;
    if (isqrt_u64(sq) != k) ++bad;
    if (sq > 0 && isqrt_u64(sq - 1) != k - 1) ++bad;
    if (isqrt_u64(sq + 2 * k) != k) ++bad;       // largest value with root k
    if (isqrt_u64(sq + 2 * k + 1) != k + 1) ++bad;
  }
  CHECK(bad == 0);
  // Property for random 64-bit values: r * r <= v < (r + 1)^2, written without overflow.
  Lcg g(11);
  for (int i = 0; i < 20000; ++i) {
    const std::uint64_t v = g.u64();
    const std::uint64_t r = isqrt_u64(v);
    if (r * r > v) ++bad;
    if (v - r * r > 2 * r) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("sqrt known values", "[fixed][sqrt]") {
  CHECK(fx::sqrt(0) == 0);
  CHECK(fx::sqrt(-1) == 0);
  CHECK(fx::sqrt(kMin) == 0);
  CHECK(fx::sqrt(-kOne) == 0);
  CHECK(fx::sqrt(1) == 256);
  CHECK(fx::sqrt(kOne) == kOne);
  CHECK(fx::sqrt(4 * kOne) == 2 * kOne);
  CHECK(fx::sqrt(kOne / 4) == kOne / 2);
  CHECK(fx::sqrt(2 * kOne) == 92681);   // floor(1.41421356 * 65536) = 92681
  CHECK(fx::sqrt(kMax) == 11863283);
  CHECK(fx::sqrt(kMax - 65535) == 11863283 - 181);
}

TEST_CASE("algebraic properties on the edge list", "[fixed][props]") {
  std::size_t bad = 0;
  for (std::size_t i = 0; i < kE; ++i) {
    const std::int32_t a = kRefE[i];
    if (mul(a, kOne) != a) ++bad;
    if (mul(kOne, a) != a) ++bad;
    if (mul(a, 0) != 0) ++bad;
    if (fx::div(a, kOne) != a) ++bad;
    if (a != 0 && fx::div(a, a) != kOne) ++bad;
    if (lerp(a, a, kMax) != a) ++bad;
    for (std::size_t j = 0; j < kE; ++j) {
      const std::int32_t b = kRefE[j];
      if (mul(a, b) != mul(b, a)) ++bad;
      if (lerp(a, b, 0) != a) ++bad;
      if (lerp(a, b, kOne) != b) ++bad;
      if (mul(a, b) == INT32_MIN && ((a < 0) == (b < 0)) && a != 0 && b != 0) ++bad;  // a positive product never gives kMin
    }
  }
  CHECK(bad == 0);
  // sqrt is monotone and fx::sqrt(a)^2 <= a * 65536 < (fx::sqrt(a) + 1)^2
  std::int32_t prev = 0;
  for (std::int32_t a = 0; a < 100000; ++a) {
    const std::int32_t r = fx::sqrt(a);
    const std::uint64_t rr = static_cast<std::uint64_t>(r);
    if (r < prev) ++bad;
    if (rr * rr > static_cast<std::uint64_t>(a) * 65536u) ++bad;
    if ((rr + 1) * (rr + 1) <= static_cast<std::uint64_t>(a) * 65536u) ++bad;
    prev = r;
  }
  CHECK(bad == 0);
}

TEST_CASE("mul matches the oracle on the edge list and random pairs", "[fixed][mul][sweep]") {
  Fnv f;
  std::size_t bad = 0;
  for (std::size_t i = 0; i < kE; ++i)
    for (std::size_t j = 0; j < kE; ++j) {
      const std::int32_t r = mul(kRefE[i], kRefE[j]);
      if (r != o_mul(kRefE[i], kRefE[j])) ++bad;
      f.word(u(r));
    }
  Lcg g(1);
  for (std::size_t i = 0; i < kRefRandN; ++i) {
    const std::int32_t a = g.q();
    const std::int32_t b = g.q();
    const std::int32_t r = mul(a, b);
    if (r != o_mul(a, b)) ++bad;
    f.word(u(r));
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_mul);
  CHECK(f.h == kRefDigest_mul);
}

TEST_CASE("div matches the oracle on the edge list and random pairs", "[fixed][div][sweep]") {
  Fnv f;
  std::size_t bad = 0;
  for (std::size_t i = 0; i < kE; ++i)
    for (std::size_t j = 0; j < kE; ++j) {
      const std::int32_t r = fx::div(kRefE[i], kRefE[j]);
      if (r != o_div(kRefE[i], kRefE[j])) ++bad;
      f.word(u(r));
    }
  Lcg g(2);
  for (std::size_t i = 0; i < kRefRandN; ++i) {
    const std::int32_t a = g.q();
    const std::int32_t b = g.q();
    const std::int32_t r = fx::div(a, b);
    if (r != o_div(a, b)) ++bad;
    f.word(u(r));
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_div);
  CHECK(f.h == kRefDigest_div);
}

TEST_CASE("lerp matches the oracle on the edge list and random triples", "[fixed][lerp][sweep]") {
  Fnv f;
  std::size_t bad = 0;
  for (std::size_t i = 0; i < kE; ++i)
    for (std::size_t j = 0; j < kE; ++j)
      for (std::size_t k = 0; k < kE; ++k) {
        const std::int32_t r = lerp(kRefE[i], kRefE[j], kRefE[k]);
        if (r != o_lerp(kRefE[i], kRefE[j], kRefE[k])) ++bad;
        f.word(u(r));
      }
  Lcg g(3);
  for (std::size_t i = 0; i < kRefRandN; ++i) {
    const std::int32_t a = g.q();
    const std::int32_t b = g.q();
    const std::int32_t t = g.q();
    const std::int32_t r = lerp(a, b, t);
    if (r != o_lerp(a, b, t)) ++bad;
    f.word(u(r));
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_lerp);
  CHECK(f.h == kRefDigest_lerp);
}

TEST_CASE("to_int_floor digest", "[fixed][floor][sweep]") {
  Fnv f;
  std::size_t bad = 0;
  for (std::size_t i = 0; i < kE; ++i) {
    const std::int32_t r = to_int_floor(kRefE[i]);
    if (r != o_floor(kRefE[i])) ++bad;
    f.word(u(r));
  }
  for (std::int32_t a = -200000; a <= 200000; ++a) {
    const std::int32_t r = to_int_floor(a);
    if (r != o_floor(a)) ++bad;
    f.word(u(r));
  }
  Lcg g(6);
  for (std::size_t i = 0; i < kRefRandN; ++i) {
    const std::int32_t a = g.q();
    const std::int32_t r = to_int_floor(a);
    if (r != o_floor(a)) ++bad;
    f.word(u(r));
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_floor);
  CHECK(f.h == kRefDigest_floor);
}

TEST_CASE("from_int digest", "[fixed][from_int][sweep]") {
  Fnv f;
  std::size_t bad = 0;
  for (std::int32_t n = -100000; n <= 100000; ++n) {
    const std::int32_t r = from_int(n);
    if (r != o_from_int(n)) ++bad;
    f.word(u(r));
  }
  for (std::size_t i = 0; i < kE; ++i) {
    const std::int32_t r = from_int(kRefE[i]);
    if (r != o_from_int(kRefE[i])) ++bad;
    f.word(u(r));
  }
  Lcg g(7);
  for (std::size_t i = 0; i < kRefRandN; ++i) {
    const std::int32_t n = g.q();
    const std::int32_t r = from_int(n);
    if (r != o_from_int(n)) ++bad;
    f.word(u(r));
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_from_int);
  CHECK(f.h == kRefDigest_from_int);
}

TEST_CASE("sqrt digest", "[fixed][sqrt][sweep]") {
  Fnv f;
  std::size_t bad = 0;
  for (std::size_t i = 0; i < kE; ++i) {
    const std::int32_t r = fx::sqrt(kRefE[i]);
    if (r != o_sqrt(kRefE[i])) ++bad;
    f.word(u(r));
  }
  for (std::int32_t a = 0; a <= 70000; ++a) {
    const std::int32_t r = fx::sqrt(a);
    if (r != o_sqrt(a)) ++bad;
    f.word(u(r));
  }
  Lcg g(4);
  for (std::size_t i = 0; i < kRefRandN; ++i) {
    const std::int32_t a = g.q();
    const std::int32_t r = fx::sqrt(a);
    if (r != o_sqrt(a)) ++bad;
    f.word(u(r));
  }
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_sqrt);
  CHECK(f.h == kRefDigest_sqrt);
}

TEST_CASE("isqrt_u64 digest", "[fixed][isqrt][sweep]") {
  Fnv f;
  std::size_t bad = 0;
  auto feed = [&](std::uint64_t v) {
    const std::uint32_t r = isqrt_u64(v);
    if (r != o_isqrt(v)) ++bad;
    f.word(r);
  };
  for (std::uint64_t v = 0; v <= 2000; ++v) feed(v);
  for (std::uint64_t k : kRefK) {
    const std::uint64_t sq = k * k;
    feed(sq - 1u);
    feed(sq);
    if (sq + 1u != 0u) feed(sq + 1u);
  }
  feed(UINT64_MAX);
  feed(UINT64_MAX - 1u);
  feed(1ull << 63);
  feed((1ull << 63) - 1u);
  feed(1ull << 62);
  feed((1ull << 62) - 1u);
  Lcg g(5);
  for (std::size_t i = 0; i < kRefRandN; ++i) feed(g.u64());
  CHECK(bad == 0);
  CHECK(f.n == kRefCases_isqrt);
  CHECK(f.h == kRefDigest_isqrt);
}
