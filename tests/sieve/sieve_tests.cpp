// Tests for core/sieve. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/sieve_ref.py, an independent Python implementation (trial
//      division only, no sieve; cross-checked by the script against gcd counting and the Moebius definition for n <= 3000). The tests re-run every
//      scenario against the real functions and recompute the 64-bit digest and the entry counts.
//  (2) ORACLE inside the test, sharing no code with core/sieve/sieve.cpp: per-entry trial division (smallest divisor, then the factorization by
//      repeated division). The full table for limit 2^20 is compared ENTRY BY ENTRY against it for all three functions; every other table is compared
//      with the matching prefix of that oracle (the entries of a table do not depend on the limit). The oracle itself is checked against the digests.
//  (3) CONSTANTS: the header values and the hand rows (prime powers, primorials, products of two large primes) come from the Python reference.
// Guard checks run on EVERY successful call: 8 sentinel entries before out[0] and 16 after out[limit] must be unchanged, and on every failing call the
// whole buffer must be unchanged. Pointers are naturally aligned.
// Extents: exhaustive ONLY for the stated limits (every limit 0..700, the listed limits, and the full table for 2^20), NOT all limits through 2^20.
// What is NOT done: no timing test (every loop has a bound that depends on the limit only, argued in the docs); Windows is CI-only.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "sieve.h"

using lumora::sieve::SieveResult;
using lumora::sieve::kMaxLimit;
using lumora::sieve::mu_table;
using lumora::sieve::phi_table;
using lumora::sieve::spf_table;

namespace {

constexpr std::uint32_t kSentinel32 = 0xC3C3C3C3u;
constexpr std::uint8_t kSentinel8 = 0xA5u;
constexpr std::size_t kGuardBefore = 8, kGuardAfter = 16;

// BEGIN GENERATED REFERENCE (tools/sieve_ref.py)
const std::uint64_t kRefDigest_mu_full = 0x5895F18A3A10AAD6ULL;
const std::size_t kRefCases_mu_full = 1048577;
const std::uint64_t kRefDigest_mu_list = 0xDD2C17C8DE965124ULL;
const std::size_t kRefCases_mu_list = 11485335;
const std::uint64_t kRefDigest_mu_small = 0x8C9E9636A4BEB36AULL;
const std::size_t kRefCases_mu_small = 246051;
const std::uint64_t kRefDigest_phi_full = 0x2C53BC06D757B6EDULL;
const std::size_t kRefCases_phi_full = 1048577;
const std::uint64_t kRefDigest_phi_list = 0xF45D9D6E5DD440F3ULL;
const std::size_t kRefCases_phi_list = 11485335;
const std::uint64_t kRefDigest_phi_small = 0xF7B5C2614AC0169EULL;
const std::size_t kRefCases_phi_small = 246051;
const std::uint64_t kRefDigest_spf_full = 0x4C0B18EE4E3C3D71ULL;
const std::size_t kRefCases_spf_full = 1048577;
const std::uint64_t kRefDigest_spf_list = 0xE9B1A0EAAC5ACE81ULL;
const std::size_t kRefCases_spf_list = 11485335;
const std::uint64_t kRefDigest_spf_small = 0x4FB0E045D17A6057ULL;
const std::size_t kRefCases_spf_small = 246051;
const std::size_t kRefNumLimits = 62;
const std::uint32_t kRefLimits[62] = {3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 511u, 512u, 513u, 1023u, 1024u, 1025u, 2047u, 2048u, 2049u, 4095u, 4096u, 4097u, 8191u, 8192u, 8193u, 16383u, 16384u, 16385u, 32767u, 32768u, 32769u, 65535u, 65536u, 65537u, 131071u, 131072u, 131073u, 262143u, 262144u, 262145u, 524287u, 524288u, 524289u, 1038360u, 1038361u, 1038362u, 1042440u, 1042441u, 1042442u, 1048575u, 1048576u};
const std::size_t kRefNumRows = 155;
const std::uint32_t kRefRows[155][4] = {
    {2, 2, 1, 255},
    {3, 3, 2, 255},
    {4, 2, 2, 0},
    {5, 5, 4, 255},
    {6, 2, 2, 1},
    {7, 7, 6, 255},
    {8, 2, 4, 0},
    {9, 3, 6, 0},
    {11, 11, 10, 255},
    {13, 13, 12, 255},
    {16, 2, 8, 0},
    {17, 17, 16, 255},
    {19, 19, 18, 255},
    {23, 23, 22, 255},
    {25, 5, 20, 0},
    {27, 3, 18, 0},
    {29, 29, 28, 255},
    {30, 2, 8, 255},
    {31, 31, 30, 255},
    {32, 2, 16, 0},
    {37, 37, 36, 255},
    {41, 41, 40, 255},
    {43, 43, 42, 255},
    {47, 47, 46, 255},
    {49, 7, 42, 0},
    {53, 53, 52, 255},
    {59, 59, 58, 255},
    {61, 61, 60, 255},
    {64, 2, 32, 0},
    {67, 67, 66, 255},
    {71, 71, 70, 255},
    {73, 73, 72, 255},
    {79, 79, 78, 255},
    {81, 3, 54, 0},
    {83, 83, 82, 255},
    {89, 89, 88, 255},
    {97, 97, 96, 255},
    {101, 101, 100, 255},
    {103, 103, 102, 255},
    {107, 107, 106, 255},
    {109, 109, 108, 255},
    {113, 113, 112, 255},
    {121, 11, 110, 0},
    {125, 5, 100, 0},
    {128, 2, 64, 0},
    {169, 13, 156, 0},
    {210, 2, 48, 1},
    {243, 3, 162, 0},
    {256, 2, 128, 0},
    {289, 17, 272, 0},
    {343, 7, 294, 0},
    {361, 19, 342, 0},
    {512, 2, 256, 0},
    {529, 23, 506, 0},
    {625, 5, 500, 0},
    {729, 3, 486, 0},
    {841, 29, 812, 0},
    {961, 31, 930, 0},
    {1009, 1009, 1008, 255},
    {1013, 1013, 1012, 255},
    {1019, 1019, 1018, 255},
    {1021, 1021, 1020, 255},
    {1024, 2, 512, 0},
    {1331, 11, 1210, 0},
    {1369, 37, 1332, 0},
    {1681, 41, 1640, 0},
    {1849, 43, 1806, 0},
    {2048, 2, 1024, 0},
    {2187, 3, 1458, 0},
    {2197, 13, 2028, 0},
    {2209, 47, 2162, 0},
    {2310, 2, 480, 255},
    {2401, 7, 2058, 0},
    {2809, 53, 2756, 0},
    {3125, 5, 2500, 0},
    {3481, 59, 3422, 0},
    {3721, 61, 3660, 0},
    {4096, 2, 2048, 0},
    {4489, 67, 4422, 0},
    {4913, 17, 4624, 0},
    {5041, 71, 4970, 0},
    {5329, 73, 5256, 0},
    {6241, 79, 6162, 0},
    {6561, 3, 4374, 0},
    {6859, 19, 6498, 0},
    {6889, 83, 6806, 0},
    {7921, 89, 7832, 0},
    {8192, 2, 4096, 0},
    {9409, 97, 9312, 0},
    {10201, 101, 10100, 0},
    {10609, 103, 10506, 0},
    {11449, 107, 11342, 0},
    {11881, 109, 11772, 0},
    {12167, 23, 11638, 0},
    {12769, 113, 12656, 0},
    {14641, 11, 13310, 0},
    {15625, 5, 12500, 0},
    {16384, 2, 8192, 0},
    {16807, 7, 14406, 0},
    {19683, 3, 13122, 0},
    {24389, 29, 23548, 0},
    {28561, 13, 26364, 0},
    {29791, 31, 28830, 0},
    {30030, 2, 5760, 1},
    {32768, 2, 16384, 0},
    {50653, 37, 49284, 0},
    {59049, 3, 39366, 0},
    {65536, 2, 32768, 0},
    {68921, 41, 67240, 0},
    {78125, 5, 62500, 0},
    {79507, 43, 77658, 0},
    {83521, 17, 78608, 0},
    {103823, 47, 101614, 0},
    {117649, 7, 100842, 0},
    {130321, 19, 123462, 0},
    {131072, 2, 65536, 0},
    {148877, 53, 146068, 0},
    {161051, 11, 146410, 0},
    {177147, 3, 118098, 0},
    {205379, 59, 201898, 0},
    {226981, 61, 223260, 0},
    {262144, 2, 131072, 0},
    {279841, 23, 267674, 0},
    {300763, 67, 296274, 0},
    {357911, 71, 352870, 0},
    {371293, 13, 342732, 0},
    {389017, 73, 383688, 0},
    {390625, 5, 312500, 0},
    {493039, 79, 486798, 0},
    {510510, 2, 92160, 255},
    {524287, 524287, 524286, 255},
    {524288, 2, 262144, 0},
    {524289, 3, 349524, 1},
    {531441, 3, 354294, 0},
    {571787, 83, 564898, 0},
    {704969, 89, 697048, 0},
    {707281, 29, 682892, 0},
    {823543, 7, 705894, 0},
    {912673, 97, 903264, 0},
    {923521, 31, 893730, 0},
    {1022117, 1009, 1020096, 1},
    {1028171, 1009, 1026144, 1},
    {1030189, 1009, 1028160, 1},
    {1030301, 101, 1020100, 0},
    {1032247, 1013, 1030216, 1},
    {1034273, 1013, 1032240, 1},
    {1038361, 1019, 1037342, 0},
    {1040399, 1019, 1038360, 1},
    {1042441, 1021, 1041420, 0},
    {1048559, 1048559, 1048558, 255},
    {1048571, 1048571, 1048570, 255},
    {1048573, 1048573, 1048572, 255},
    {1048574, 2, 524286, 1},
    {1048575, 3, 480000, 0},
    {1048576, 2, 524288, 0},
};
// END GENERATED REFERENCE

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  void add(std::uint64_t v) {
    h = (h ^ v) * 0x100000001B3ull;
    ++cases;
  }
};

enum class Kind { Spf, Phi, Mu };

// The oracle: trial division per entry, one pass over the factorization.
struct Oracle {
  std::vector<std::uint32_t> spf, phi;
  std::vector<std::uint8_t> mu;
  Oracle() : spf(kMaxLimit + 1), phi(kMaxLimit + 1), mu(kMaxLimit + 1) {
    spf[0] = 0; spf[1] = 1; phi[0] = 0; phi[1] = 1; mu[0] = 0; mu[1] = 1;
    for (std::uint32_t n = 2; n <= kMaxLimit; ++n) {
      std::uint32_t rest = n, totient = n, smallest = 0;
      bool square = false;
      int count = 0;
      for (std::uint32_t d = 2; d * d <= rest; ++d) {
        if (rest % d != 0) continue;
        if (smallest == 0) smallest = d;
        std::uint32_t e = 0;
        while (rest % d == 0) { rest /= d; ++e; }
        if (e > 1) square = true;
        ++count;
        totient = totient / d * (d - 1);
      }
      if (rest > 1) {
        if (smallest == 0) smallest = rest;
        ++count;
        totient = totient / rest * (rest - 1);
      }
      spf[n] = smallest;
      phi[n] = totient;
      mu[n] = square ? 0 : (count % 2 == 1 ? 255 : 1);
    }
  }
};

const Oracle& oracle() {
  static const Oracle o;
  return o;
}

struct Tally {
  std::size_t bad = 0;
  std::string first;
  void fail(const std::string& s) {
    if (bad == 0) first = s;
    ++bad;
  }
};

// One successful call with guards: runs the function for `limit`, checks the guards, compares the table with the oracle prefix and folds it.
void run_limit(Kind kind, std::uint32_t limit, Fold& f, Tally& t) {
  const std::string tag = std::string(kind == Kind::Spf ? "spf" : kind == Kind::Phi ? "phi" : "mu") + " limit " + std::to_string(limit);
  if (kind == Kind::Mu) {
    std::vector<std::uint8_t> buf(kGuardBefore + limit + 1 + kGuardAfter, kSentinel8);
    if (mu_table(limit, buf.data() + kGuardBefore) != SieveResult::Ok) { t.fail(tag + ": not Ok"); return; }
    for (std::size_t i = 0; i < kGuardBefore; ++i) if (buf[i] != kSentinel8) { t.fail(tag + ": guard before written"); return; }
    for (std::size_t i = 0; i < kGuardAfter; ++i) if (buf[kGuardBefore + limit + 1 + i] != kSentinel8) { t.fail(tag + ": guard after written"); return; }
    const std::uint8_t* tab = buf.data() + kGuardBefore;
    if (std::memcmp(tab, oracle().mu.data(), limit + 1) != 0) t.fail(tag + ": table differs from the oracle");
    for (std::uint32_t i = 0; i <= limit; ++i) f.add(tab[i]);
    return;
  }
  std::vector<std::uint32_t> buf(kGuardBefore + limit + 1 + kGuardAfter, kSentinel32);
  const SieveResult r = kind == Kind::Spf ? spf_table(limit, buf.data() + kGuardBefore) : phi_table(limit, buf.data() + kGuardBefore);
  if (r != SieveResult::Ok) { t.fail(tag + ": not Ok"); return; }
  for (std::size_t i = 0; i < kGuardBefore; ++i) if (buf[i] != kSentinel32) { t.fail(tag + ": guard before written"); return; }
  for (std::size_t i = 0; i < kGuardAfter; ++i) if (buf[kGuardBefore + limit + 1 + i] != kSentinel32) { t.fail(tag + ": guard after written"); return; }
  const std::uint32_t* tab = buf.data() + kGuardBefore;
  const std::uint32_t* want = kind == Kind::Spf ? oracle().spf.data() : oracle().phi.data();
  if (std::memcmp(tab, want, (static_cast<std::size_t>(limit) + 1) * sizeof(std::uint32_t)) != 0) t.fail(tag + ": table differs from the oracle");
  for (std::uint32_t i = 0; i <= limit; ++i) f.add(tab[i]);
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.h == digest);
}

#define SMALL_TEST(NAME, KIND, TAG)                                                                                                 \
  TEST_CASE(NAME ": every limit 0..700, exhaustive only for the stated limits (not all limits through 2^20)", "[sieve]") {          \
    Fold f;                                                                                                                         \
    Tally t;                                                                                                                        \
    for (std::uint32_t limit = 0; limit <= 700; ++limit) run_limit(KIND, limit, f, t);                                              \
    expect_digest(f, t, kRefCases_##TAG##_small, kRefDigest_##TAG##_small);                                                         \
  }
#define LIST_TEST(NAME, KIND, TAG)                                                                                                  \
  TEST_CASE(NAME ": the listed limits (2^k +- 1, p^2 +- 1, 2^20), exhaustive only for the stated limits", "[sieve]") {              \
    Fold f;                                                                                                                         \
    Tally t;                                                                                                                        \
    for (std::size_t i = 0; i < kRefNumLimits; ++i) run_limit(KIND, kRefLimits[i], f, t);                                           \
    expect_digest(f, t, kRefCases_##TAG##_list, kRefDigest_##TAG##_list);                                                           \
  }

SMALL_TEST("spf", Kind::Spf, spf)
SMALL_TEST("phi", Kind::Phi, phi)
SMALL_TEST("mu", Kind::Mu, mu)
LIST_TEST("spf", Kind::Spf, spf)
LIST_TEST("phi", Kind::Phi, phi)
LIST_TEST("mu", Kind::Mu, mu)

}  // namespace

// The full table for limit 2^20, entry by entry against the independent oracle, and the oracle against the reference digests.
TEST_CASE("full table at limit 2^20: entry by entry against the oracle, all three functions", "[sieve]") {
  Fold fs, fp, fm;
  Tally t;
  run_limit(Kind::Spf, kMaxLimit, fs, t);
  run_limit(Kind::Phi, kMaxLimit, fp, t);
  run_limit(Kind::Mu, kMaxLimit, fm, t);
  expect_digest(fs, t, kRefCases_spf_full, kRefDigest_spf_full);
  expect_digest(fp, t, kRefCases_phi_full, kRefDigest_phi_full);
  expect_digest(fm, t, kRefCases_mu_full, kRefDigest_mu_full);
  Fold os, op, om;  // the oracle against the Python reference (the oracle is not trusted on its own)
  for (std::uint32_t i = 0; i <= kMaxLimit; ++i) {
    os.add(oracle().spf[i]);
    op.add(oracle().phi[i]);
    om.add(oracle().mu[i]);
  }
  CHECK(os.h == kRefDigest_spf_full);
  CHECK(op.h == kRefDigest_phi_full);
  CHECK(om.h == kRefDigest_mu_full);
}

// The product p * p must not wrap or be formed past the limit: the loops use p <= limit / p. The limits 2^20 - 1 and 2^20 are the ones next to the
// largest prime square 1021^2 and the top of the range.
TEST_CASE("p * p guard: limits 2^20 - 1 and 2^20 and the squares of 1019 and 1021", "[sieve]") {
  Fold f;
  Tally t;
  const std::uint32_t ls[] = {kMaxLimit - 1, kMaxLimit, 1019u * 1019u - 1u, 1019u * 1019u, 1021u * 1021u - 1u, 1021u * 1021u};
  for (std::uint32_t limit : ls) {
    run_limit(Kind::Spf, limit, f, t);
    run_limit(Kind::Phi, limit, f, t);
    run_limit(Kind::Mu, limit, f, t);
  }
  CHECK(t.bad == 0);
  INFO(t.first);
}

TEST_CASE("hand rows: prime powers, primorials, products of two large primes, header values", "[sieve]") {
  std::vector<std::uint32_t> s(kMaxLimit + 1), p(kMaxLimit + 1);
  std::vector<std::uint8_t> m(kMaxLimit + 1);
  REQUIRE(spf_table(kMaxLimit, s.data()) == SieveResult::Ok);
  REQUIRE(phi_table(kMaxLimit, p.data()) == SieveResult::Ok);
  REQUIRE(mu_table(kMaxLimit, m.data()) == SieveResult::Ok);
  std::size_t bad = 0;
  for (std::size_t i = 0; i < kRefNumRows; ++i) {
    const std::uint32_t n = kRefRows[i][0];
    if (s[n] != kRefRows[i][1] || p[n] != kRefRows[i][2] || m[n] != kRefRows[i][3]) ++bad;
  }
  CHECK(bad == 0);
  // The header values, written from the header text.
  CHECK(s[0] == 0); CHECK(s[1] == 1); CHECK(p[0] == 0); CHECK(p[1] == 1); CHECK(m[0] == 0); CHECK(m[1] == 1);
  CHECK(s[2] == 2); CHECK(s[97] == 97); CHECK(s[91] == 7);
  CHECK(p[97] == 96); CHECK(p[12] == 4); CHECK(p[1u << 19] == (1u << 18));
  CHECK(m[2] == 255); CHECK(m[4] == 0); CHECK(m[6] == 1); CHECK(m[30] == 255);
}

TEST_CASE("null output and oversized limits: order of checks, nothing written on failure", "[sieve]") {
  const std::uint32_t big[] = {kMaxLimit + 1, kMaxLimit + 2, 0x7FFFFFFFu, 0xFFFFFFFEu, 0xFFFFFFFFu};
  const std::uint32_t any[] = {0u, 1u, 2u, kMaxLimit - 1, kMaxLimit, kMaxLimit + 1, 0xFFFFFFFFu};
  for (std::uint32_t limit : any) {  // a null out comes first, also when the limit is too large
    CHECK(spf_table(limit, nullptr) == SieveResult::NullArgument);
    CHECK(phi_table(limit, nullptr) == SieveResult::NullArgument);
    CHECK(mu_table(limit, nullptr) == SieveResult::NullArgument);
  }
  for (std::uint32_t limit : big) {
    std::vector<std::uint32_t> a(64, kSentinel32), b(64, kSentinel32);
    std::vector<std::uint8_t> c(64, kSentinel8);
    CHECK(spf_table(limit, a.data()) == SieveResult::TooLarge);
    CHECK(phi_table(limit, b.data()) == SieveResult::TooLarge);
    CHECK(mu_table(limit, c.data()) == SieveResult::TooLarge);
    for (std::size_t i = 0; i < 64; ++i) {
      CHECK(a[i] == kSentinel32);
      CHECK(b[i] == kSentinel32);
      CHECK(c[i] == kSentinel8);
    }
  }
}
