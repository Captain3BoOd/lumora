// Tests for core/prng. Evidence label: unit tests only. Every comparison is exact, and every bounded call is checked
// against the exact state afterwards (the number of consumed outputs is part of the contract).
// The block between BEGIN and END GENERATED REFERENCE comes from tools/prng_ref.py, an independent Python implementation
// (unbounded integers, a matrix power for the jump). Regenerate it with:
// python3 tools/prng_ref.py
// The tests also carry a naive oracle (a 64-bit product built from 32-bit limbs, a rotation one bit at a time) that shares
// no code with core/prng/prng.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "prng.h"

using namespace lumora::prng;

namespace {

// BEGIN GENERATED REFERENCE (tools/prng_ref.py)
struct Case {
  std::uint64_t state, inc;
  std::uint32_t bound, result;
  std::uint64_t state_after;
};
const std::uint64_t kRefSeeds[][2] = {{42ull, 54ull}, {0ull, 0ull}, {1ull, 1ull}, {18446744073709551615ull, 18446744073709551615ull}, {9600629759793949339ull, 15726070495360670683ull}, {123456789ull, 987654321ull}, {7ull, 9223372036854775808ull}, {7ull, 0ull}, {9223372036854775808ull, 4611686018427387909ull}};
const std::uint32_t kRefBounds[] = {0u, 1u, 2u, 3u, 5u, 6u, 7u, 10u, 100u, 65537u, 2147483647u, 2147483648u, 2147483649u, 4294967295u};
const std::uint64_t kRefDeltas[] = {0ull, 1ull, 2ull, 3ull, 5ull, 7ull, 63ull, 64ull, 65ull, 1000ull, 12345ull, 65536ull, 4294967295ull, 4294967296ull, 4294967297ull, 4611686018427387904ull, 9223372036854775808ull, 9223372036854775809ull, 18446744073709551614ull, 18446744073709551615ull};
const std::size_t kRefOut = 1000;
const std::size_t kRefBoundedCalls = 200;
const std::size_t kRefBoundedSeeds = 4;
const Case kRefCap[] = {
    {15968754582127934569ull, 16222713135584610036ull, 2147483649u, 582382075u, 15968754582127934569ull},
    {16505759121396833731ull, 14026559636245980540ull, 2147483649u, 1780509304u, 16505759121396833731ull},
    {15979324121840046334ull, 71141393598911064ull, 2147483653u, 1886548995u, 15979324121840046334ull},
    {1306187517919625606ull, 2657036354641740536ull, 2147483649u, 1414612033u, 1306187517919625606ull},
    {3266349224379569583ull, 12436401778640319724ull, 2147549183u, 302077269u, 3266349224379569583ull},
    {576498135834475650ull, 12530038835249748904ull, 2147483649u, 432u, 576498135834475650ull},
    {37383547548166ull, 2995912836888489208ull, 2147483649u, 768u, 37383547548166ull}};
const Case kRefRun[] = {
    {18123512945841774521ull, 15014287968896048084ull, 2147483649u, 1625716773u, 10139136823193581145ull},
    {10515280110826123675ull, 2381218020965965454ull, 2147483649u, 236068189u, 17156682004111070359ull},
    {9047085013921272389ull, 16024491155874869872ull, 2147483649u, 546583327u, 17418403920906927129ull},
    {8127082652125215077ull, 7777694914422190864ull, 2147483649u, 1312930219u, 10050224744471857781ull},
    {1865107271582249807ull, 2647879789770372914ull, 2147483649u, 905329710u, 2922388298151704253ull},
    {13708238960239122695ull, 7339953974384943242ull, 2147483649u, 920774544u, 4098418937494909011ull},
    {16041482479867887709ull, 12036889971290158248ull, 2147483649u, 678787000u, 10899249213219363721ull},
    {223082511309617997ull, 4845530046500970712ull, 2147483649u, 1441443435u, 2573861742740608045ull},
    {5055387621967887091ull, 1268714490865888006ull, 2147483649u, 1260988906u, 11684852097128757869ull},
    {12359742674767191511ull, 12796241834970540314ull, 2147483653u, 177992577u, 5703044555342565605ull},
    {12771296879065204389ull, 4492221335491053392ull, 2147483653u, 680569260u, 330872674361347469ull},
    {4546867539880328195ull, 18006974185835891414ull, 2147483653u, 1429037712u, 6223131873217150489ull},
    {17623609681648446989ull, 9348352870868995224ull, 2147483653u, 1232631819u, 15730658937900342269ull},
    {9809954122705939499ull, 5530609007285475806ull, 2147483653u, 27598778u, 793405927930300837ull},
    {2832430496815485065ull, 11321612241561772132ull, 2147483653u, 846083106u, 14311589650458645737ull},
    {16041469089158678779ull, 10684760526191188590ull, 2147483653u, 94566175u, 889809060968802001ull},
    {11345258796007952079ull, 11709642530698284210ull, 2147483653u, 1049301972u, 42605867657370079ull},
    {14367372203646965703ull, 437989170149206090ull, 2147483653u, 629655595u, 7059975395494047573ull}};
const Case kRefExact[] = {
    {0ull, 1ull, 3u, 1u, 13885033948157127959ull},
    {0ull, 109ull, 3u, 2u, 835686304943715019ull},
    {134217728ull, 1ull, 3u, 1u, 7666441553371463681ull},
    {134217728ull, 109ull, 3u, 1u, 7666441553371463789ull},
    {268435456ull, 1ull, 3u, 2u, 15332883106742927361ull},
    {268435456ull, 109ull, 3u, 2u, 15332883106742927469ull},
    {0ull, 1ull, 5u, 3u, 13885033948157127959ull},
    {0ull, 109ull, 5u, 1u, 835686304943715019ull},
    {134217728ull, 1ull, 5u, 1u, 7666441553371463681ull},
    {134217728ull, 109ull, 5u, 1u, 7666441553371463789ull},
    {268435456ull, 1ull, 5u, 2u, 15332883106742927361ull},
    {268435456ull, 109ull, 5u, 2u, 15332883106742927469ull},
    {402653184ull, 1ull, 6u, 3u, 9833447587537321774ull},
    {402653184ull, 109ull, 6u, 3u, 14630629035737556630ull},
    {536870912ull, 1ull, 6u, 4u, 12219022139776303105ull},
    {536870912ull, 109ull, 6u, 4u, 12219022139776303213ull},
    {671088640ull, 1ull, 6u, 5u, 1438719619438215169ull},
    {671088640ull, 109ull, 6u, 5u, 1438719619438215277ull},
    {402653184ull, 1ull, 7u, 2u, 9833447587537321774ull},
    {402653184ull, 109ull, 7u, 2u, 14630629035737556630ull},
    {536870912ull, 1ull, 7u, 4u, 12219022139776303105ull},
    {536870912ull, 109ull, 7u, 4u, 12219022139776303213ull},
    {671088640ull, 1ull, 7u, 5u, 1438719619438215169ull},
    {671088640ull, 109ull, 7u, 5u, 1438719619438215277ull},
    {671088640ull, 1ull, 10u, 0u, 12146321829997674286ull},
    {671088640ull, 109ull, 10u, 0u, 16943503278197909142ull},
    {805306368ull, 1ull, 10u, 6u, 9105161172809678849ull},
    {805306368ull, 109ull, 10u, 6u, 9105161172809678957ull},
    {939524096ull, 1ull, 10u, 7u, 16771602726181142529ull},
    {939524096ull, 109ull, 10u, 7u, 16771602726181142637ull},
    {12750684160ull, 1ull, 100u, 99u, 5545198298456227630ull},
    {12750684160ull, 109ull, 100u, 99u, 10342379746656462486ull},
    {12884901888ull, 1ull, 100u, 96u, 16555370248988000257ull},
    {12884901888ull, 109ull, 100u, 96u, 16555370248988000365ull},
    {13019119616ull, 1ull, 100u, 97u, 5775067728649912321ull},
    {13019119616ull, 109ull, 100u, 97u, 5775067728649912429ull},
    {0ull, 1ull, 65537u, 25288u, 13885033948157127959ull},
    {0ull, 109ull, 65537u, 17361u, 835686304943715019ull},
    {134217728ull, 1ull, 65537u, 1u, 7666441553371463681ull},
    {134217728ull, 109ull, 65537u, 1u, 7666441553371463789ull},
    {268435456ull, 1ull, 65537u, 2u, 15332883106742927361ull},
    {268435456ull, 109ull, 65537u, 2u, 15332883106742927469ull},
    {134217728ull, 1ull, 2147483647u, 1614963889u, 7520573345076969262ull},
    {134217728ull, 109ull, 2147483647u, 1614963889u, 12317754793277204118ull},
    {268435456ull, 1ull, 2147483647u, 2u, 15332883106742927361ull},
    {268435456ull, 109ull, 2147483647u, 2u, 15332883106742927469ull},
    {402653184ull, 1ull, 2147483647u, 3u, 4552580586404839425ull},
    {402653184ull, 109ull, 2147483647u, 3u, 4552580586404839533ull},
    {288229276774301696ull, 1ull, 2147483649u, 1275268692u, 8984831462693240622ull},
    {288229276774301696ull, 109ull, 2147483649u, 1275268692u, 13782012910893475478ull},
    {288229276640083968ull, 1ull, 2147483649u, 2147483647u, 2197988615110262785ull},
    {288229276640083968ull, 109ull, 2147483649u, 2147483647u, 2197988615110262893ull},
    {288231475663339520ull, 1ull, 2147483649u, 2147483648u, 5296001164834242561ull},
    {288231475663339520ull, 109ull, 2147483649u, 2147483648u, 5296001164834242669ull},
    {288229276774301696ull, 1ull, 2147483649u, 1275268692u, 8984831462693240622ull},
    {288229276774301696ull, 109ull, 2147483649u, 1275268692u, 13782012910893475478ull},
    {288229276640083968ull, 1ull, 2147483649u, 2147483647u, 2197988615110262785ull},
    {288229276640083968ull, 109ull, 2147483649u, 2147483647u, 2197988615110262893ull},
    {288231475663339520ull, 1ull, 2147483649u, 2147483648u, 5296001164834242561ull},
    {288231475663339520ull, 109ull, 2147483649u, 2147483648u, 5296001164834242669ull},
    {0ull, 1ull, 4294967295u, 3837872008u, 13885033948157127959ull},
    {0ull, 109ull, 4294967295u, 1203932051u, 835686304943715019ull},
    {134217728ull, 1ull, 4294967295u, 1u, 7666441553371463681ull},
    {134217728ull, 109ull, 4294967295u, 1u, 7666441553371463789ull},
    {268435456ull, 1ull, 4294967295u, 2u, 15332883106742927361ull},
    {268435456ull, 109ull, 4294967295u, 2u, 15332883106742927469ull}};
const Case kRefZero[] = {
    {0ull, 0ull, 2u, 0u, 0ull},
    {0ull, 0ull, 3u, 0u, 0ull},
    {0ull, 0ull, 4u, 0u, 0ull},
    {0ull, 0ull, 5u, 0u, 0ull},
    {0ull, 0ull, 6u, 0u, 0ull},
    {0ull, 0ull, 8u, 0u, 0ull},
    {0ull, 0ull, 100u, 0u, 0ull},
    {0ull, 0ull, 2147483648u, 0u, 0ull},
    {0ull, 0ull, 2147483649u, 0u, 0ull},
    {0ull, 0ull, 4294967295u, 0u, 0ull}};
const std::uint64_t kRefDigest_seed_next = 0x4F6ADE91E7E6F0DCull;
const std::size_t kRefCases_seed_next = 9000;
const std::uint64_t kRefDigest_bounded = 0xAA6EC1A96AB8ED45ull;
const std::size_t kRefCases_bounded = 11200;
const std::uint64_t kRefDigest_advance = 0xC269DEEC953CB61Full;
const std::size_t kRefCases_advance = 2360;
// END GENERATED REFERENCE

constexpr std::uint64_t kLcgMul = 6364136223846793005ull;
constexpr std::uint64_t kLcgAdd = 1442695040888963407ull;

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
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
};

// ---- naive oracle -------------------------------------------------------------------------------------------------
// a * b modulo 2^64 from 32-bit limbs: only the low halves of the cross terms matter.
std::uint64_t o_mul(std::uint64_t a, std::uint64_t b) {
  const std::uint64_t alo = a & 0xFFFFFFFFull, ahi = a >> 32;
  const std::uint64_t blo = b & 0xFFFFFFFFull, bhi = b >> 32;
  const std::uint64_t cross = (alo * bhi + ahi * blo) & 0xFFFFFFFFull;
  return alo * blo + (cross << 32);
}

struct Oracle {
  std::uint64_t state, inc;
  std::uint32_t next() {
    const std::uint64_t old = state;
    state = o_mul(old, kLcgMul) + inc;
    const std::uint64_t mixed = (old >> 18) ^ old;
    std::uint32_t x = static_cast<std::uint32_t>((mixed >> 27) & 0xFFFFFFFFull);
    const unsigned rot = static_cast<unsigned>(old >> 59);
    for (unsigned i = 0; i < rot; ++i) x = (x >> 1) | ((x & 1u) << 31);  // rotate right one bit at a time
    return x;
  }
  void seed(std::uint64_t initstate, std::uint64_t initseq) {
    state = 0;
    inc = (initseq * 2u) + 1u;
    next();
    state += initstate;
    next();
  }
  std::uint32_t bounded(std::uint32_t bound, std::size_t* draws) {
    *draws = 0;
    if (bound == 0) return 0;
    const std::uint32_t threshold = static_cast<std::uint32_t>((4294967296ull - bound) % bound);
    std::uint32_t r = 0;
    while (*draws < 64) {
      r = next();
      ++*draws;
      if (r >= threshold) break;
    }
    return r % bound;
  }
};

Pcg32 mk(std::uint64_t a, std::uint64_t b) {
  Pcg32 r;
  pcg32_seed(&r, a, b);
  return r;
}

}  // namespace

TEST_CASE("defaults and constants", "[prng]") {
  const Pcg32 r;
  CHECK(r.state == 0);
  CHECK(r.inc == 1);
  CHECK(kMaxDraws == 64);
}

TEST_CASE("reference vector seed(42, 54)", "[prng][seed]") {
  Pcg32 r;
  REQUIRE(pcg32_seed(&r, 42, 54));
  CHECK(r.inc == 109);
  const std::uint32_t want[6] = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
  for (std::uint32_t w : want) CHECK(pcg32_next(&r) == w);
}

TEST_CASE("seed details", "[prng][seed]") {
  // state after seeding is the state after the two steps, hand-built
  Pcg32 r{12345, 99};  // seeding overwrites both fields
  CHECK(pcg32_seed(&r, 0, 0));
  CHECK(r.inc == 1);
  Pcg32 a = mk(5, 7);
  Pcg32 b{0, 15};  // inc = (7 << 1) | 1 = 15
  const std::uint64_t s1 = b.state * kLcgMul + b.inc;  // first step from state 0
  const std::uint64_t s2 = (s1 + 5) * kLcgMul + b.inc;
  CHECK(a.inc == 15);
  CHECK(a.state == s2);
  // the top bit of initseq is lost
  const Pcg32 c = mk(9, 3);
  const Pcg32 d = mk(9, 3 ^ (1ull << 63));
  CHECK(c.state == d.state);
  CHECK(c.inc == d.inc);
  CHECK(c.inc % 2 == 1);
  const std::uint64_t kSeedQs[] = {0, 1, 2, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, UINT64_MAX};
  // inc is always odd after seeding
  for (std::uint64_t q : kSeedQs) CHECK((mk(1, q).inc & 1u) == 1u);
  // different initstate or initseq give different generators
  CHECK(mk(1, 1).state != mk(2, 1).state);
  CHECK(mk(1, 1).inc != mk(1, 2).inc);
  CHECK_FALSE(pcg32_seed(nullptr, 1, 2));
  // seeding a used generator gives the same state as seeding a fresh one (the old state is not an input)
  Pcg32 dirty{0xDEADBEEFCAFEF00Dull, 0x1234567ull};
  REQUIRE(pcg32_seed(&dirty, 5, 7));
  CHECK(dirty.state == a.state);
  CHECK(dirty.inc == a.inc);
}

TEST_CASE("null generator", "[prng]") {
  CHECK(pcg32_next(nullptr) == 0);
  CHECK(pcg32_bounded(nullptr, 10) == 0);
  CHECK(pcg32_bounded(nullptr, 0) == 0);
  CHECK_FALSE(pcg32_advance(nullptr, 5));
  CHECK_FALSE(pcg32_advance(nullptr, 0));
}

TEST_CASE("next from a forged state, even inc used as stored", "[prng][next]") {
  Pcg32 z{0, 0};
  for (int i = 0; i < 5; ++i) CHECK(pcg32_next(&z) == 0);
  CHECK(z.state == 0);
  CHECK(z.inc == 0);
  Pcg32 e{0, 2};  // even inc, no repair
  CHECK(pcg32_next(&e) == 0);
  CHECK(e.state == 2);
  CHECK(e.inc == 2);
  CHECK(pcg32_next(&e) == 0);
  CHECK(e.state == 2 * kLcgMul + 2);
  Pcg32 f{1ull << 63, 0};  // output uses the OLD state: top bit gives rot 16
  const std::uint32_t out = pcg32_next(&f);
  Oracle o{1ull << 63, 0};
  CHECK(out == o.next());
  CHECK(f.state == o.state);
  Pcg32 g{UINT64_MAX, UINT64_MAX};
  Oracle og{UINT64_MAX, UINT64_MAX};
  CHECK(pcg32_next(&g) == og.next());
  CHECK(g.state == og.state);
}

TEST_CASE("seed and next match the oracle and the reference digest", "[prng][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (const auto& s : kRefSeeds) {
    Pcg32 r = mk(s[0], s[1]);
    Oracle o;
    o.seed(s[0], s[1]);
    if (r.state != o.state || r.inc != o.inc) ++bad;
    f.u64(r.state);
    f.u64(r.inc);
    for (std::size_t i = 0; i < kRefOut; ++i) {
      const std::uint32_t v = pcg32_next(&r);
      if (v != o.next() || r.state != o.state) ++bad;
      f.u32(v);
      ++n;
    }
    f.u64(r.state);
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_seed_next);
  CHECK(f.h == kRefDigest_seed_next);
}

TEST_CASE("bounded special cases", "[prng][bounded]") {
  Pcg32 r = mk(42, 54);
  const Pcg32 before = r;
  CHECK(pcg32_bounded(&r, 0) == 0);  // does not advance
  CHECK(r.state == before.state);
  CHECK(r.inc == before.inc);
  CHECK(pcg32_bounded(&r, 1) == 0);  // consumes exactly one output
  Pcg32 one = before;
  pcg32_next(&one);
  CHECK(r.state == one.state);
  // power-of-two bounds have threshold 0 and take the first output
  for (std::uint32_t shift = 1; shift < 32; ++shift) {
    Pcg32 a = before;
    Pcg32 b = before;
    const std::uint32_t bound = 1u << shift;
    const std::uint32_t v = pcg32_bounded(&a, bound);
    CHECK(v == pcg32_next(&b) % bound);
    CHECK(a.state == b.state);
  }
  // the first outputs of seed(42, 54) are 0xa15c02b7 0x7b47f409 ...
  Pcg32 c = before;
  CHECK(pcg32_bounded(&c, 10) == 0xa15c02b7u % 10);
}

TEST_CASE("bounded exact state after every call and the oracle", "[prng][bounded]") {
  std::size_t bad = 0, rejected = 0;
  for (std::size_t si = 0; si < kRefBoundedSeeds; ++si) {
    for (std::uint32_t bound : kRefBounds) {
      Pcg32 r = mk(kRefSeeds[si][0], kRefSeeds[si][1]);
      Oracle o;
      o.seed(kRefSeeds[si][0], kRefSeeds[si][1]);
      for (std::size_t i = 0; i < kRefBoundedCalls; ++i) {
        std::size_t draws = 0;
        const std::uint32_t want = o.bounded(bound, &draws);
        const std::uint32_t got = pcg32_bounded(&r, bound);
        if (got != want || (bound != 0 && got >= bound) || r.state != o.state) ++bad;
        if (draws > 1) ++rejected;
      }
    }
  }
  CHECK(bad == 0);
  CHECK(rejected > 100);  // the rejection path is exercised on seeded streams
}

TEST_CASE("bounded digest", "[prng][sweep]") {
  Fnv f;
  std::size_t n = 0;
  for (std::size_t si = 0; si < kRefBoundedSeeds; ++si)
    for (std::uint32_t bound : kRefBounds) {
      Pcg32 r = mk(kRefSeeds[si][0], kRefSeeds[si][1]);
      for (std::size_t i = 0; i < kRefBoundedCalls; ++i) {
        f.u32(pcg32_bounded(&r, bound));
        f.u64(r.state);
        ++n;
      }
    }
  CHECK(n == kRefCases_bounded);
  CHECK(f.h == kRefDigest_bounded);
}

namespace {
void check_cases(const Case* cases, std::size_t count, std::size_t* bad) {
  for (std::size_t i = 0; i < count; ++i) {
    Pcg32 r{cases[i].state, cases[i].inc};
    const std::uint32_t got = pcg32_bounded(&r, cases[i].bound);
    if (got != cases[i].result || r.state != cases[i].state_after || r.inc != cases[i].inc) ++*bad;
    Oracle o{cases[i].state, cases[i].inc};
    std::size_t draws = 0;
    if (o.bounded(cases[i].bound, &draws) != got || o.state != r.state) ++*bad;
  }
}
}  // namespace

TEST_CASE("bounded cap path on forged cycles reaches exactly 64 outputs", "[prng][bounded][cap]") {
  // Each forged generator walks a cycle of length 2, 4, 8, 16 or 32 whose outputs are all below the threshold. After 64
  // draws the state is back at the start, and any other cap (63, 65, 66, 68, 72, ...) ends at a different state.
  std::size_t bad = 0;
  check_cases(kRefCap, sizeof(kRefCap) / sizeof(kRefCap[0]), &bad);
  CHECK(bad == 0);
  for (const Case& c : kRefCap) {
    CHECK(c.state_after == c.state);  // 64 is a multiple of every cycle length used
    Pcg32 r{c.state, c.inc};
    std::uint64_t seen = 0;
    for (int i = 0; i < 64; ++i) {
      const std::uint32_t out = pcg32_next(&r);
      if (i == 63) seen = out;  // the 64th output is the result before the modulo
    }
    CHECK(c.result == seen % c.bound);
    CHECK(r.state == c.state_after);
  }
}

TEST_CASE("bounded with forged state 0 and inc 0", "[prng][bounded][cap]") {
  // Output is 0 forever and the state never moves. A positive threshold rejects every draw and the cap returns
  // 0 % bound; a threshold of 0 (power of two) accepts the first output. Both give 0 and leave the state at 0.
  std::size_t bad = 0;
  check_cases(kRefZero, sizeof(kRefZero) / sizeof(kRefZero[0]), &bad);
  CHECK(bad == 0);
  for (std::uint32_t bound : {3u, 5u, 6u, 0x80000001u, 0xFFFFFFFFu, 2u, 4u, 0x80000000u}) {
    Pcg32 z{0, 0};
    CHECK(pcg32_bounded(&z, bound) == 0);
    CHECK(z.state == 0);
    CHECK(z.inc == 0);
  }
}

TEST_CASE("bounded rejection runs end at the exact draw", "[prng][bounded][run]") {
  std::size_t bad = 0;
  check_cases(kRefRun, sizeof(kRefRun) / sizeof(kRefRun[0]), &bad);
  CHECK(bad == 0);
}

TEST_CASE("bounded when an output equals the threshold or is next to it", "[prng][bounded][edge]") {
  // the comparison is output >= threshold: threshold - 1 is rejected, threshold and threshold + 1 are accepted
  std::size_t bad = 0;
  check_cases(kRefExact, sizeof(kRefExact) / sizeof(kRefExact[0]), &bad);
  CHECK(bad == 0);
  // direct statements for bound 3 (threshold 1): output 0 is rejected, output 1 is accepted on the first draw
  Pcg32 a{0, 1};  // output of state 0 is 0
  const std::uint64_t after_one = 0 * kLcgMul + 1;
  (void)after_one;
  const std::uint32_t v = pcg32_bounded(&a, 3);
  Oracle o{0, 1};
  std::size_t draws = 0;
  CHECK(v == o.bounded(3, &draws));
  CHECK(draws >= 2);
  CHECK(a.state == o.state);
}

TEST_CASE("advance equals repeated next", "[prng][advance]") {
  std::size_t bad = 0;
  const std::uint64_t incs[] = {1, 3, 109, 0xDA3E39CB94B95BDBull, 2, 0, UINT64_MAX - 1};
  const std::uint64_t states[] = {0, 1, 42, 0x853C49E6748FEA9Bull, UINT64_MAX};
  for (std::uint64_t inc : incs)
    for (std::uint64_t st : states) {
      Pcg32 step{st, inc};
      for (std::uint64_t d = 0; d <= 300; ++d) {
        Pcg32 jump{st, inc};
        if (!pcg32_advance(&jump, d)) ++bad;
        if (jump.state != step.state || jump.inc != inc) ++bad;
        pcg32_next(&step);
      }
      Pcg32 big{st, inc};
      Oracle o{st, inc};
      for (std::uint64_t i = 0; i < 3000; ++i) o.next();
      pcg32_advance(&big, 3000);
      if (big.state != o.state) ++bad;
    }
  CHECK(bad == 0);
}

TEST_CASE("advance composes, reverses and keeps inc", "[prng][advance]") {
  std::size_t bad = 0;
  Lcg g(77);
  for (int i = 0; i < 2000; ++i) {
    const std::uint64_t st = g.next();
    const std::uint64_t inc = g.next();
    const std::uint64_t a = g.next() >> (g.next() & 63);
    const std::uint64_t b = g.next() >> (g.next() & 63);
    Pcg32 x{st, inc}, y{st, inc};
    pcg32_advance(&x, a);
    pcg32_advance(&x, b);
    pcg32_advance(&y, a + b);  // wraps modulo 2^64
    if (x.state != y.state || x.inc != inc || y.inc != inc) ++bad;
    // a jump of delta and then of 2^64 - delta is the identity
    Pcg32 z{st, inc};
    pcg32_advance(&z, a);
    pcg32_advance(&z, 0 - a);
    if (z.state != st) ++bad;
  }
  CHECK(bad == 0);
  // 2^64 - 1 steps a seeded stream back by one
  Pcg32 r = mk(42, 54);
  const Pcg32 start = r;
  pcg32_next(&r);
  CHECK(pcg32_advance(&r, UINT64_MAX));
  CHECK(r.state == start.state);
  CHECK(pcg32_next(&r) == 0xa15c02b7u);
  // delta 0 changes nothing and returns true
  Pcg32 s = mk(1, 2);
  const Pcg32 before = s;
  CHECK(pcg32_advance(&s, 0));
  CHECK(s.state == before.state);
  CHECK(s.inc == before.inc);
  // the reference vector after a jump of 3
  Pcg32 t = mk(42, 54);
  pcg32_advance(&t, 3);
  CHECK(pcg32_next(&t) == 0x83d2f293u);
}

TEST_CASE("advance digest", "[prng][advance][sweep]") {
  Fnv f;
  std::size_t n = 0;
  for (const auto& s : kRefSeeds)
    for (int forged = 0; forged < 2; ++forged)
      for (std::uint64_t d : kRefDeltas) {
        Pcg32 r = mk(s[0], s[1]);
        if (forged != 0) r.inc -= 1;  // even
        pcg32_advance(&r, d);
        f.u64(r.state);
        f.u64(r.inc);
        f.u32(pcg32_next(&r));
        ++n;
      }
  Lcg g(33);
  for (int i = 0; i < 2000; ++i) {
    Pcg32 r{g.next(), g.next()};
    const std::uint64_t first = g.next();
    const std::uint64_t d = first >> (g.next() & 63);
    pcg32_advance(&r, d);
    f.u64(r.state);
    f.u64(r.inc);
    ++n;
  }
  CHECK(n == kRefCases_advance);
  CHECK(f.h == kRefDigest_advance);
}
