// Tests for core/modarith. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/modarith_ref.py, an independent Python implementation (Python integers
// for mulmod, powmod and modinv; its own Miller-Rabin to a DIFFERENT base set from the C++ code, cross-checked there against a second
// Miller-Rabin, a sieve and trial division). Regenerate it with: python3 tools/modarith_ref.py
// The tests carry a naive oracle that shares no code with core/modarith/modarith.cpp and uses plain 64-bit arithmetic only: mulmod, powmod and
// modinv for moduli below 2^16 (plain multiplication, repeated multiplication, a brute-force search) and primality by a sieve below 2^22.
// No 128-bit type, builtin or intrinsic is used anywhere in these tests. Large answers come from the Python digests and tables.
// Primality is exact for every 64-bit n in the C++ code because of a published result (see docs/modarith.md); these tests only check the sets
// listed in the docs. Everything from 2^32 up is NOT exhaustive.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "modarith.h"

using namespace lumora::modarith;

namespace {

struct KV {
  std::uint64_t a, b, m;
  std::uint32_t code;
  std::uint64_t out;
};

// BEGIN GENERATED REFERENCE (tools/modarith_ref.py)
const std::uint64_t kRefBoundary[] = {0x0000000000000000ull, 0x0000000000000001ull, 0x0000000000000002ull, 0x0000000000000003ull, 0x0000000000000004ull, 0x0000000000000005ull, 0x0000000000000007ull, 0x000000000000000Aull, 0x000000000000FFFFull, 0x0000000000010000ull, 0x0000000000010001ull, 0x000000003B9ACA07ull, 0x000000007FFFFFFFull, 0x0000000080000000ull, 0x0000000080000001ull, 0x00000000FFFFFFFBull, 0x00000000FFFFFFFFull, 0x0000000100000000ull, 0x0000000100000001ull, 0x000000010000000Full, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0x8000000000000001ull, 0xFFFFFFFFFFFFFFC5ull, 0xFFFFFFFFFFFFFFFDull, 0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFFFFFFFFFFull};
const std::uint32_t kRefRandSeeds = 400, kRefSmallSieve = 1048576, kRefPspLimit = 4194304;
const std::uint64_t kRefDigest_grid = 0xAEF317ECB6CAC653ull;
const std::size_t kRefCases_grid = 40095, kRefOps_grid = 40095;
const std::uint64_t kRefDigest_small = 0xA34597512C353835ull;
const std::size_t kRefCases_small = 6864, kRefOps_small = 6864;
const std::uint64_t kRefDigest_rand = 0x351E838F2BD3FDD6ull;
const std::size_t kRefCases_rand = 1200, kRefOps_rand = 1200;
const std::uint64_t kRefDigest_prime_small = 0xCED31AEE2EA2589Dull;
const std::size_t kRefCases_prime_small = 1048576, kRefOps_prime_small = 1048576;
const std::uint64_t kRefDigest_prime_win = 0xE4C3601D471E554Dull;
const std::size_t kRefCases_prime_win = 3072, kRefOps_prime_win = 3072;
const std::uint64_t kRefDigest_psp = 0xECFA6E0F8A03DFE2ull;
const std::size_t kRefCount_spsp2 = 104, kRefCount_carmichael = 69;
const KV kRefTable[] = {
    {0xFFFFFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0xFFFFFFF600000019ull, 0, 0, 0, 0},
    {0xFFFFFFEA00000055ull, 0, 0, 0, 0},
    {0x00000000FFE200E1ull, 0, 0, 0, 0},
    {0x0000000100020001ull, 0, 0, 0, 0},
    {0x0000FFF0FFFB004Bull, 0, 0, 0, 0},
    {0x00000002FFFFFFF1ull, 0, 0, 0, 0},
    {0x0000000000000003ull, 0, 0, 1, 0},
    {0x0000000000000007ull, 0, 0, 1, 0},
    {0x000000000000000Full, 0, 0, 0, 0},
    {0x000000000000001Full, 0, 0, 1, 0},
    {0x000000000000003Full, 0, 0, 0, 0},
    {0x000000000000007Full, 0, 0, 1, 0},
    {0x00000000000000FFull, 0, 0, 0, 0},
    {0x00000000000001FFull, 0, 0, 0, 0},
    {0x00000000000003FFull, 0, 0, 0, 0},
    {0x00000000000007FFull, 0, 0, 0, 0},
    {0x0000000000000FFFull, 0, 0, 0, 0},
    {0x0000000000001FFFull, 0, 0, 1, 0},
    {0x0000000000003FFFull, 0, 0, 0, 0},
    {0x0000000000007FFFull, 0, 0, 0, 0},
    {0x000000000000FFFFull, 0, 0, 0, 0},
    {0x000000000001FFFFull, 0, 0, 1, 0},
    {0x000000000003FFFFull, 0, 0, 0, 0},
    {0x000000000007FFFFull, 0, 0, 1, 0},
    {0x00000000000FFFFFull, 0, 0, 0, 0},
    {0x00000000001FFFFFull, 0, 0, 0, 0},
    {0x00000000003FFFFFull, 0, 0, 0, 0},
    {0x00000000007FFFFFull, 0, 0, 0, 0},
    {0x0000000000FFFFFFull, 0, 0, 0, 0},
    {0x0000000001FFFFFFull, 0, 0, 0, 0},
    {0x0000000003FFFFFFull, 0, 0, 0, 0},
    {0x0000000007FFFFFFull, 0, 0, 0, 0},
    {0x000000000FFFFFFFull, 0, 0, 0, 0},
    {0x000000001FFFFFFFull, 0, 0, 0, 0},
    {0x000000003FFFFFFFull, 0, 0, 0, 0},
    {0x000000007FFFFFFFull, 0, 0, 1, 0},
    {0x00000000FFFFFFFFull, 0, 0, 0, 0},
    {0x00000001FFFFFFFFull, 0, 0, 0, 0},
    {0x00000003FFFFFFFFull, 0, 0, 0, 0},
    {0x00000007FFFFFFFFull, 0, 0, 0, 0},
    {0x0000000FFFFFFFFFull, 0, 0, 0, 0},
    {0x0000001FFFFFFFFFull, 0, 0, 0, 0},
    {0x0000003FFFFFFFFFull, 0, 0, 0, 0},
    {0x0000007FFFFFFFFFull, 0, 0, 0, 0},
    {0x000000FFFFFFFFFFull, 0, 0, 0, 0},
    {0x000001FFFFFFFFFFull, 0, 0, 0, 0},
    {0x000003FFFFFFFFFFull, 0, 0, 0, 0},
    {0x000007FFFFFFFFFFull, 0, 0, 0, 0},
    {0x00000FFFFFFFFFFFull, 0, 0, 0, 0},
    {0x00001FFFFFFFFFFFull, 0, 0, 0, 0},
    {0x00003FFFFFFFFFFFull, 0, 0, 0, 0},
    {0x00007FFFFFFFFFFFull, 0, 0, 0, 0},
    {0x0000FFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x0001FFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x0003FFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x0007FFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x000FFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x001FFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x003FFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x007FFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x00FFFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x01FFFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x03FFFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x07FFFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x0FFFFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x1FFFFFFFFFFFFFFFull, 0, 0, 1, 0},
    {0x3FFFFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0x7FFFFFFFFFFFFFFFull, 0, 0, 0, 0},
    {0xFFFFFFFFFFFFFFC5ull, 0, 0, 1, 0},
    {0x0000000000000005ull, 0, 0, 1, 0},
    {0x0000000000000011ull, 0, 0, 1, 0},
    {0x0000000000000101ull, 0, 0, 1, 0},
    {0x0000000000010001ull, 0, 0, 1, 0},
    {0x0000000100000001ull, 0, 0, 0, 0},
    {0x000000000014F5D5ull, 0, 0, 0, 0},
    {0x00000000018271B1ull, 0, 0, 0, 0},
    {0x00000000BFA17DC7ull, 0, 0, 0, 0},
    {0x000001F51F3FEE3Bull, 0, 0, 0, 0},
    {0x0000032907381CDFull, 0, 0, 0, 0},
    {0x000136A352B2C8C1ull, 0, 0, 0, 0},
    {0x351591274F9AF9FBull, 0, 0, 0, 0},
    {0xFFFFFFFFFFFFFFADull, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFFA1ull, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFF4Dull, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFF43ull, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFEFFull, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFEE9ull, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFEBDull, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFE9Full, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFE95ull, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFE57ull, 0, 0, 1, 0},
    {0xFFFFFFFFFFFFFE3Bull, 0, 0, 1, 0},
    {0x00000000FFFFFFFBull, 0, 0, 1, 0},
    {0x000000010000000Full, 0, 0, 1, 0},
    {0x7FFFFFFFFFFFFFE7ull, 0, 0, 1, 0},
    {0x000000003B9ACA07ull, 0, 0, 1, 0},
    {0x000000003B800001ull, 0, 0, 1, 0},
    {0x0DE0B6B3A7640003ull, 0, 0, 1, 0},
    {0x0000000000000231ull, 0, 0, 0, 0},
    {0x000000000000A051ull, 0, 0, 0, 0},
    {0x00000000000C97B1ull, 0, 0, 0, 0},
    {0x0000000013251481ull, 0, 0, 0, 0},
    {0x00000001418E8631ull, 0, 0, 0, 0},
    {0x00000036133736D1ull, 0, 0, 0, 0},
    {0x000008DD3F98C901ull, 0, 0, 0, 0},
    {0x00051AAB47E249D1ull, 0, 0, 0, 0},
    {0x00D8A300793EEF31ull, 0, 0, 0, 0},
};
const KV kRefMul[] = {
    {0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull, 0, 0x0000000000000000ull},
    {0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFEull, 0, 0x0000000000000001ull},
    {0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFFFFFFFFFFull, 0, 0x0000000000000001ull},
    {0x8000000000000000ull, 0x0000000000000002ull, 0x8000000000000001ull, 0, 0x7FFFFFFFFFFFFFFFull},
    {0x8000000000000001ull, 0x8000000000000001ull, 0xFFFFFFFFFFFFFFFFull, 0, 0x4000000000000002ull},
    {0xFFFFFFFFFFFFFFFFull, 0x0000000000000002ull, 0x8000000000000000ull, 0, 0x7FFFFFFFFFFFFFFEull},
    {0x01B69B4BA630F34Eull, 0x0DB4DA5F49F8B478ull, 0xFFFFFFFFFFFFFFC5ull, 0, 0xA0132BE36F723646ull},
    {0x0000000100000000ull, 0x0000000100000000ull, 0x0000000100000001ull, 0, 0x0000000000000001ull},
    {0x00000000FFFFFFFFull, 0x00000000FFFFFFFFull, 0x0000000100000000ull, 0, 0x0000000000000001ull},
    {0x00000000FFFFFFFFull, 0x00000000FFFFFFFFull, 0x0000000100000001ull, 0, 0x0000000000000004ull},
    {0x0000000000000005ull, 0x0000000000000007ull, 0x0000000000000001ull, 0, 0x0000000000000000ull},
    {0x0000000000000000ull, 0x0000000000000005ull, 0x0000000000000009ull, 0, 0x0000000000000000ull},
    {0x0000000000000005ull, 0x0000000000000007ull, 0x0000000000000000ull, 2, 0x0000000000000000ull},
    {0xFFFFFFFFFFFFFFFFull, 0x0000000000000001ull, 0x000000000000000Aull, 0, 0x0000000000000005ull},
    {0x0000000000000003ull, 0x0000000000000005ull, 0x0000000000000007ull, 0, 0x0000000000000001ull},
    {0x0000000100000000ull, 0x0000000100000000ull, 0xFFFFFFFFFFFFFFFFull, 0, 0x0000000000000001ull},
    {0x0000000100000000ull, 0x0000000100000000ull, 0xFFFFFFFFFFFFFFFBull, 0, 0x0000000000000005ull},
};
const KV kRefPow[] = {
    {0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000005ull, 0, 0x0000000000000001ull},
    {0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000001ull, 0, 0x0000000000000000ull},
    {0x0000000000000005ull, 0x0000000000000000ull, 0x0000000000000001ull, 0, 0x0000000000000000ull},
    {0x0000000000000007ull, 0x0000000000000000ull, 0x000000000000000Dull, 0, 0x0000000000000001ull},
    {0x0000000000000000ull, 0x0000000000000005ull, 0x000000000000000Dull, 0, 0x0000000000000000ull},
    {0x0000000000000002ull, 0x000000000000000Aull, 0x00000000000003E8ull, 0, 0x0000000000000018ull},
    {0x0000000000000002ull, 0x0000000000000040ull, 0xFFFFFFFFFFFFFFFFull, 0, 0x0000000000000001ull},
    {0x0000000000000003ull, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull, 0, 0x83B58E3F0A83554Full},
    {0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFEull, 0, 0x0000000000000001ull},
    {0x0000000000000002ull, 0x1FFFFFFFFFFFFFFEull, 0x1FFFFFFFFFFFFFFFull, 0, 0x0000000000000001ull},
    {0x0000000000000007ull, 0x8000000000000000ull, 0x8000000000000001ull, 0, 0x56F730833B8A68EBull},
    {0x000000000000000Aull, 0x0000000000000012ull, 0x000000003B9ACA07ull, 0, 0x0000000000000031ull},
    {0xFFFFFFFFFFFFFFFDull, 0xAB54A98CEB1F0AD2ull, 0xFFFFFFFFFFFFFFC5ull, 0, 0x1FF3A22E14210C01ull},
    {0x0000000000000005ull, 0x0000000000000005ull, 0x0000000000000000ull, 2, 0x0000000000000000ull},
    {0x00000000075BCD15ull, 0x000000003B9ACA06ull, 0x000000003B9ACA07ull, 0, 0x0000000000000001ull},
    {0x0000000000000002ull, 0xFFFFFFFFFFFFFFFFull, 0x0000000100000001ull, 0, 0x0000000080000001ull},
};
const KV kRefInv[] = {
    {0x0000000000000003ull, 0, 0x0000000000000007ull, 0, 0x0000000000000005ull},
    {0x0000000000000000ull, 0, 0x0000000000000007ull, 3, 0x0000000000000000ull},
    {0x0000000000000007ull, 0, 0x0000000000000007ull, 3, 0x0000000000000000ull},
    {0x000000000000000Eull, 0, 0x0000000000000007ull, 3, 0x0000000000000000ull},
    {0x0000000000000002ull, 0, 0x0000000000000004ull, 3, 0x0000000000000000ull},
    {0x0000000000000005ull, 0, 0x0000000000000001ull, 0, 0x0000000000000000ull},
    {0x0000000000000000ull, 0, 0x0000000000000001ull, 0, 0x0000000000000000ull},
    {0x0000000000000005ull, 0, 0x0000000000000000ull, 2, 0x0000000000000000ull},
    {0x0000000000000001ull, 0, 0xFFFFFFFFFFFFFFFFull, 0, 0x0000000000000001ull},
    {0xFFFFFFFFFFFFFFFEull, 0, 0xFFFFFFFFFFFFFFFFull, 0, 0xFFFFFFFFFFFFFFFEull},
    {0x0000000000000002ull, 0, 0xFFFFFFFFFFFFFFFFull, 0, 0x8000000000000000ull},
    {0xFFFFFFFFFFFFFFFFull, 0, 0xFFFFFFFFFFFFFFFFull, 3, 0x0000000000000000ull},
    {0x8000000000000000ull, 0, 0xFFFFFFFFFFFFFFFFull, 0, 0x0000000000000002ull},
    {0x0000000000000003ull, 0, 0xFFFFFFFFFFFFFFC5ull, 0, 0x5555555555555542ull},
    {0x002BDC545D6B4B87ull, 0, 0xFFFFFFFFFFFFFFC5ull, 0, 0x7326886C603CFD89ull},
    {0x0000000000000006ull, 0, 0x0000000000000009ull, 3, 0x0000000000000000ull},
    {0x0000000000000023ull, 0, 0x000000000000000Cull, 0, 0x000000000000000Bull},
    {0x0000000000010001ull, 0, 0xFFFFFFFFFFFFFFC5ull, 0, 0xAC2353DCAC2353B5ull},
    {0x000000000000000Aull, 0, 0x8000000000000000ull, 3, 0x0000000000000000ull},
    {0xFFFFFFFFFFFFFFFFull, 0, 0x8000000000000000ull, 0, 0x7FFFFFFFFFFFFFFFull},
    {0x00000000075BCD15ull, 0, 0x000000003B9ACA07ull, 0, 0x00000000011C5344ull},
};
// END GENERATED REFERENCE

constexpr std::uint64_t kSentinel = 0xDEADBEEFCAFEF00Dull;
constexpr std::uint64_t kMax = 0xFFFFFFFFFFFFFFFFull;

std::uint32_t code(ModResult r) { return static_cast<std::uint32_t>(r); }

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
};

// ---- oracle (moduli below 2^16 only, so that a plain product of two residues cannot overflow) ----
std::uint64_t o_mul(std::uint64_t a, std::uint64_t b, std::uint64_t m) { return (a % m) * (b % m) % m; }

std::uint64_t o_pow(std::uint64_t base, std::uint64_t exp, std::uint64_t m) {
  std::uint64_t r = 1 % m;
  for (std::uint64_t i = 0; i < exp; ++i) r = r * (base % m) % m;
  return r;
}

// Returns the code and the inverse, found by trying every residue.
ModResult o_inv(std::uint64_t a, std::uint64_t m, std::uint64_t* out) {
  if (m == 1u) {
    *out = 0;
    return ModResult::Ok;
  }
  for (std::uint64_t x = 0; x < m; ++x) {
    if ((a % m) * x % m == 1u) {
      *out = x;
      return ModResult::Ok;
    }
  }
  return ModResult::NotInvertible;
}

// Sieve of smallest prime factors below `limit`.
std::vector<std::uint32_t> make_spf(std::uint32_t limit) {
  std::vector<std::uint32_t> spf(limit);
  for (std::uint32_t i = 0; i < limit; ++i) spf[i] = i;
  for (std::uint32_t i = 2; static_cast<std::uint64_t>(i) * i < limit; ++i) {
    if (spf[i] != i) continue;
    for (std::uint32_t j = i * i; j < limit; j += i) {
      if (spf[j] == j) spf[j] = i;
    }
  }
  return spf;
}

// ---- harness: calls with a preset *out, checks the code and that a failure leaves *out untouched ----
struct Got {
  ModResult r;
  std::uint64_t out;
  bool touched;
};

Got do_mul(std::uint64_t a, std::uint64_t b, std::uint64_t m) {
  std::uint64_t o = kSentinel;
  const ModResult r = mulmod_u64(a, b, m, &o);
  return {r, o, o != kSentinel};
}
Got do_pow(std::uint64_t a, std::uint64_t e, std::uint64_t m) {
  std::uint64_t o = kSentinel;
  const ModResult r = powmod_u64(a, e, m, &o);
  return {r, o, o != kSentinel};
}
Got do_inv(std::uint64_t a, std::uint64_t m) {
  std::uint64_t o = kSentinel;
  const ModResult r = modinv_u64(a, m, &o);
  return {r, o, o != kSentinel};
}

void fold_got(Fold* f, const Got& g, std::size_t* bad) {
  ++f->cases;
  ++f->ops;
  f->add(code(g.r));
  if (g.r == ModResult::Ok) {
    f->add(g.out);
  } else if (g.touched) {
    ++*bad;  // a failure leaves *out untouched
  }
}

bool prime(std::uint64_t n, bool preset = false) {
  bool o = preset;
  CHECK(is_prime_u64(n, &o) == ModResult::Ok);
  return o;
}

}  // namespace

TEST_CASE("header anchors", "[modarith]") {
  CHECK(code(ModResult::Ok) == 0u);
  CHECK(code(ModResult::NullArgument) == 1u);
  CHECK(code(ModResult::ZeroModulus) == 2u);
  CHECK(code(ModResult::NotInvertible) == 3u);
}

TEST_CASE("boundary grid: mulmod, powmod over every (a, b, m) and modinv over every (a, m)", "[sweep][modarith]") {
  const std::size_t n = sizeof(kRefBoundary) / sizeof(kRefBoundary[0]);
  Fold f;
  std::size_t bad = 0;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      for (std::size_t k = 0; k < n; ++k) {
        fold_got(&f, do_mul(kRefBoundary[i], kRefBoundary[j], kRefBoundary[k]), &bad);
        fold_got(&f, do_pow(kRefBoundary[i], kRefBoundary[j], kRefBoundary[k]), &bad);
      }
    }
  }
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t k = 0; k < n; ++k) {
      const Got g = do_inv(kRefBoundary[i], kRefBoundary[k]);
      fold_got(&f, g, &bad);
      if (g.r == ModResult::Ok && kRefBoundary[k] > 1u && do_mul(kRefBoundary[i], g.out, kRefBoundary[k]).out != 1u) ++bad;
      if (g.r == ModResult::Ok && g.out >= kRefBoundary[k] && kRefBoundary[k] != 1u) ++bad;
    }
  }
  CHECK(bad == 0u);
  CHECK(n == 27u);
  CHECK(f.cases == kRefCases_grid);
  CHECK(f.ops == kRefOps_grid);
  CHECK(f.h == kRefDigest_grid);
}

TEST_CASE("small values: mulmod, powmod, modinv for a, b, exp in 0..15 and m in 0..12 against the oracle", "[sweep][modarith]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint64_t m = 0; m <= 12; ++m) {
    for (std::uint64_t a = 0; a <= 15; ++a) {
      for (std::uint64_t b = 0; b <= 15; ++b) {
        const Got gm = do_mul(a, b, m);
        const Got gp = do_pow(a, b, m);
        if (m == 0u) {
          if (gm.r != ModResult::ZeroModulus || gp.r != ModResult::ZeroModulus) ++bad;
        } else {
          if (gm.r != ModResult::Ok || gm.out != o_mul(a, b, m)) ++bad;
          if (gp.r != ModResult::Ok || gp.out != o_pow(a, b, m)) ++bad;
        }
        fold_got(&f, gm, &bad);
        fold_got(&f, gp, &bad);
      }
      const Got gi = do_inv(a, m);
      if (m == 0u) {
        if (gi.r != ModResult::ZeroModulus) ++bad;
      } else {
        std::uint64_t want = 0;
        const ModResult wr = o_inv(a, m, &want);
        if (gi.r != wr || (wr == ModResult::Ok && gi.out != want)) ++bad;
      }
      fold_got(&f, gi, &bad);
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_small);
  CHECK(f.ops == kRefOps_small);
  CHECK(f.h == kRefDigest_small);
}

TEST_CASE("oracle sweep: moduli 1..64, residues past the modulus, exponents 0..40 (not hashed)", "[sweep][modarith]") {
  std::size_t bad = 0;
  std::size_t cases = 0;
  for (std::uint64_t m = 1; m <= 64; ++m) {
    for (std::uint64_t a = 0; a <= m + 3; ++a) {
      for (std::uint64_t b = 0; b <= m + 3; ++b) {
        const Got g = do_mul(a, b, m);
        if (g.r != ModResult::Ok || g.out != o_mul(a, b, m)) ++bad;
        ++cases;
      }
      for (std::uint64_t e = 0; e <= 40; ++e) {
        const Got g = do_pow(a, e, m);
        if (g.r != ModResult::Ok || g.out != o_pow(a, e, m)) ++bad;
        ++cases;
      }
      const Got gi = do_inv(a, m);
      std::uint64_t want = 0;
      const ModResult wr = o_inv(a, m, &want);
      if (gi.r != wr || (wr == ModResult::Ok && gi.out != want)) ++bad;
      ++cases;
    }
  }
  CHECK(bad == 0u);
  CHECK(cases == 205216u);
}

TEST_CASE("pseudo-random 64-bit values of random bit length (not exhaustive)", "[sweep][modarith]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t seed = 1; seed <= kRefRandSeeds; ++seed) {
    Lcg g(static_cast<std::uint64_t>(seed) * 104729u + 7u);
    std::uint64_t v[4];
    for (int i = 0; i < 4; ++i) {
      const std::uint32_t shift = g.next() % 64u;
      const std::uint64_t hi = g.next();
      const std::uint64_t lo = g.next();
      v[i] = ((hi << 32) | lo) >> shift;
    }
    fold_got(&f, do_mul(v[0], v[1], v[2]), &bad);
    fold_got(&f, do_pow(v[0], v[3], v[2]), &bad);
    const Got gi = do_inv(v[0], v[2]);
    fold_got(&f, gi, &bad);
    if (gi.r == ModResult::Ok && v[2] > 1u && do_mul(v[0], gi.out, v[2]).out != 1u) ++bad;
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_rand);
  CHECK(f.ops == kRefOps_rand);
  CHECK(f.h == kRefDigest_rand);
}

TEST_CASE("known answers computed with Python integers", "[modarith]") {
  std::size_t bad = 0;
  for (const KV& k : kRefMul) {
    const Got g = do_mul(k.a, k.b, k.m);
    if (code(g.r) != k.code || (k.code == 0u && g.out != k.out) || (k.code != 0u && g.touched)) ++bad;
  }
  for (const KV& k : kRefPow) {
    const Got g = do_pow(k.a, k.b, k.m);
    if (code(g.r) != k.code || (k.code == 0u && g.out != k.out) || (k.code != 0u && g.touched)) ++bad;
  }
  for (const KV& k : kRefInv) {
    const Got g = do_inv(k.a, k.m);
    if (code(g.r) != k.code || (k.code == 0u && g.out != k.out) || (k.code != 0u && g.touched)) ++bad;
  }
  CHECK(bad == 0u);
  CHECK(sizeof(kRefMul) / sizeof(KV) == 17u);
  CHECK(sizeof(kRefPow) / sizeof(KV) == 16u);
  CHECK(sizeof(kRefInv) / sizeof(KV) == 21u);
}

TEST_CASE("null rules and check order: Null, ZeroModulus, then the function failure", "[args][modarith]") {
  std::uint64_t o = kSentinel;
  CHECK(mulmod_u64(1, 2, 3, nullptr) == ModResult::NullArgument);
  CHECK(powmod_u64(1, 2, 3, nullptr) == ModResult::NullArgument);
  CHECK(modinv_u64(1, 3, nullptr) == ModResult::NullArgument);
  CHECK(is_prime_u64(7, nullptr) == ModResult::NullArgument);
  // Null beats ZeroModulus
  CHECK(mulmod_u64(1, 2, 0, nullptr) == ModResult::NullArgument);
  CHECK(powmod_u64(1, 2, 0, nullptr) == ModResult::NullArgument);
  CHECK(modinv_u64(1, 0, nullptr) == ModResult::NullArgument);
  // ZeroModulus, and nothing written
  CHECK(mulmod_u64(5, 7, 0, &o) == ModResult::ZeroModulus);
  CHECK(powmod_u64(5, 7, 0, &o) == ModResult::ZeroModulus);
  CHECK(modinv_u64(5, 0, &o) == ModResult::ZeroModulus);
  CHECK(modinv_u64(0, 0, &o) == ModResult::ZeroModulus);
  CHECK(o == kSentinel);
  // NotInvertible, and nothing written (a multiple of m, including 0, for every m above 1; a shared factor)
  CHECK(modinv_u64(0, 7, &o) == ModResult::NotInvertible);
  CHECK(modinv_u64(7, 7, &o) == ModResult::NotInvertible);
  CHECK(modinv_u64(14, 7, &o) == ModResult::NotInvertible);
  CHECK(modinv_u64(6, 9, &o) == ModResult::NotInvertible);
  CHECK(modinv_u64(kMax, kMax, &o) == ModResult::NotInvertible);
  CHECK(modinv_u64(0, kMax, &o) == ModResult::NotInvertible);
  CHECK(o == kSentinel);
  // m == 1: every residue is 0, the result is Ok and *out = 0, also for a == 0
  o = kSentinel;
  CHECK(modinv_u64(0, 1, &o) == ModResult::Ok);
  CHECK(o == 0u);
  o = kSentinel;
  CHECK(modinv_u64(12345, 1, &o) == ModResult::Ok);
  CHECK(o == 0u);
  // exp 0 gives 1 mod m, 0 when m == 1; 0 ^ 0 is 1 for m above 1
  o = kSentinel;
  CHECK(powmod_u64(0, 0, 2, &o) == ModResult::Ok);
  CHECK(o == 1u);
  CHECK(powmod_u64(0, 0, 1, &o) == ModResult::Ok);
  CHECK(o == 0u);
  CHECK(powmod_u64(kMax, 0, kMax, &o) == ModResult::Ok);
  CHECK(o == 1u);
  CHECK(mulmod_u64(kMax, kMax, 1, &o) == ModResult::Ok);
  CHECK(o == 0u);
  // an is_prime failure cannot happen with a non-null out; *out is written for every n
  bool b = true;
  CHECK(is_prime_u64(0, &b) == ModResult::Ok);
  CHECK(b == false);
  CHECK(is_prime_u64(2, &b) == ModResult::Ok);
  CHECK(b == true);
}

TEST_CASE("primality: every n below 2^20 against a sieve", "[sweep][modarith]") {
  const std::vector<std::uint32_t> spf = make_spf(kRefSmallSieve);
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t n = 0; n < kRefSmallSieve; ++n) {
    bool got = n % 2u == 0u;  // a preset that a missing write would keep for half the values
    if (is_prime_u64(n, &got) != ModResult::Ok) ++bad;
    const bool want = n >= 2u && spf[n] == n;
    if (got != want) ++bad;
    ++f.cases;
    ++f.ops;
    if (got) f.add(n);
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_prime_small);
  CHECK(f.ops == kRefOps_prime_small);
  CHECK(f.h == kRefDigest_prime_small);
}

TEST_CASE("primality: windows of every value near 2^32, 2^63 and 2^64 - 1 (not exhaustive)", "[sweep][modarith]") {
  Fold f;
  const std::uint64_t windows[3][2] = {{(1ull << 32) - 512u, (1ull << 32) + 512u}, {(1ull << 63) - 512u, (1ull << 63) + 512u}, {kMax - 1023u, kMax}};
  for (const auto& w : windows) {
    for (std::uint64_t n = w[0];; ++n) {
      const bool p = prime(n);
      ++f.cases;
      ++f.ops;
      f.add((n << 1) | (p ? 1u : 0u));
      if (n == w[1] - (w[1] == kMax ? 0u : 1u)) break;
    }
  }
  CHECK(f.cases == kRefCases_prime_win);
  CHECK(f.ops == kRefOps_prime_win);
  CHECK(f.h == kRefDigest_prime_win);
}

TEST_CASE("primality: table of known primes and composites (not exhaustive)", "[modarith]") {
  std::size_t bad = 0;
  std::size_t primes = 0;
  for (const KV& k : kRefTable) {
    if (prime(k.a) != (k.code == 1u)) ++bad;
    if (k.code == 1u) ++primes;
  }
  CHECK(bad == 0u);
  CHECK(primes == 31u);
  // worst paths (every base is tried): the largest 64-bit prime, UINT64_MAX, and the smallest 64-bit strong pseudoprime to the first 11 prime
  // bases, which only the base 37 rejects. Correctness only, no timing.
  CHECK(prime(18446744073709551557ull) == true);
  CHECK(prime(kMax) == false);
  CHECK(prime(3825123056546413051ull) == false);
  CHECK(prime(341550071728321ull) == false);
}

TEST_CASE("primality: every base-2 strong pseudoprime and every Carmichael number below 2^22", "[sweep][modarith]") {
  const std::vector<std::uint32_t> spf = make_spf(kRefPspLimit);
  Fold f;
  std::size_t bad = 0;
  std::size_t spsp = 0;
  std::size_t carm = 0;
  std::vector<std::uint32_t> carmichael;
  for (std::uint32_t n = 9; n < kRefPspLimit; n += 2u) {
    if (spf[n] == n) continue;
    // strong test to base 2 with plain 64-bit arithmetic (n < 2^22, so a product stays below 2^44)
    std::uint64_t d = n - 1u;
    std::uint32_t s = 0;
    while ((d & 1u) == 0u) {
      d >>= 1;
      ++s;
    }
    std::uint64_t x = 1;
    std::uint64_t base = 2;
    for (std::uint64_t e = d; e != 0u; e >>= 1) {
      if ((e & 1u) != 0u) x = x * base % n;
      base = base * base % n;
    }
    bool pass = x == 1u || x == n - 1u;
    for (std::uint32_t i = 1; i < s && !pass; ++i) {
      x = x * x % n;
      if (x == n - 1u) pass = true;
    }
    if (pass) {
      ++spsp;
      f.add((1ull << 40) | n);
      if (prime(n)) ++bad;
    }
    // Korselt: squarefree, and p - 1 divides n - 1 for every prime factor p (at least 2 of them, n composite and odd)
    std::uint32_t m = n;
    bool ok = true;
    while (m > 1u && ok) {
      const std::uint32_t p = spf[m];
      m /= p;
      if (m % p == 0u || (n - 1u) % (p - 1u) != 0u) ok = false;
    }
    if (ok) carmichael.push_back(n);
  }
  for (std::uint32_t n : carmichael) {
    ++carm;
    f.add((2ull << 40) | n);
    if (prime(n)) ++bad;
  }
  CHECK(bad == 0u);
  CHECK(spsp == kRefCount_spsp2);
  CHECK(carm == kRefCount_carmichael);
  CHECK(f.h == kRefDigest_psp);
}
