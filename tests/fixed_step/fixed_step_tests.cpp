// Tests for core/timing/fixed_step. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/fixed_step_ref.py, an independent Python
// implementation with unbounded integers. Regenerate it with: python3 tools/fixed_step_ref.py
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

#include "fixed_step.h"

using namespace lumora::timing;

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();

// BEGIN GENERATED REFERENCE (tools/fixed_step_ref.py)
const std::uint64_t kRefExtremes[] = {0x0000000000000000ull, 0x0000000000000001ull, 0x0000000000000002ull, 0x0000000000000003ull, 0x0000000000000007ull, 0x000000000000FFFFull, 0x0000000000010000ull, 0x0000000000010001ull, 0x00000000FFFFFFFFull, 0x0000000100000000ull, 0x0000000100000001ull, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0x8000000000000001ull, 0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFFFFFFFFFFull};
const std::uint32_t kRefExtMaxSteps[] = {1ull, 5ull, 4294967295ull};
const std::uint64_t kRefExtDropped[] = {0x0000000000000000ull, 0x0000000000000005ull, 0xFFFFFFFFFFFFFFFDull};
const std::uint64_t kRefSmallDropped[] = {0x0000000000000000ull, 0xFFFFFFFFFFFFFFFDull};
constexpr std::uint64_t kRefSmallStep = 8, kRefSmallMaxSteps = 4, kRefSmallMaxDelta = 24, kRefSmallAcc = 24, kRefSmallDelta = 30;
constexpr std::uint64_t kRefAlphaSeed = 0x9E3779B97F4A7C15ull;
constexpr std::size_t kRefAlphaCases = 20000;
const std::uint64_t kRefDigest_small = 0x19133FEABC2659F9ull;
const std::uint64_t kRefDigest_extreme = 0x851C5066A603ADDDull;
const std::uint64_t kRefDigest_alpha = 0xAFAD1EAE21B0DEDCull;
const std::size_t kRefSmallCases = 1240000;
const std::size_t kRefExtremeCases = 552960;
// END GENERATED REFERENCE

// FNV-1a 64, the same byte streams as tools/fixed_step_ref.py.
struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void num(std::uint64_t v, int n) {
    for (int i = 0; i < n; ++i) byte(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
  }
};

// xorshift64*, the same as the reference.
struct Rng {
  std::uint64_t s;
  std::uint64_t next() {
    std::uint64_t x = s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    s = x;
    return x * 0x2545F4914F6CDD1Dull;
  }
};

struct Out {
  bool ok = false;
  StepState state;
  StepResult result;
};

Out run(std::uint64_t acc, std::uint64_t dropped, std::uint64_t delta, std::uint64_t step, std::uint32_t max_steps,
        std::uint64_t max_delta) {
  Out o;
  o.state.accumulator_ns = acc;
  o.state.dropped_ns = dropped;
  StepConfig c;
  c.step_ns = step;
  c.max_steps = max_steps;
  c.max_delta_ns = max_delta;
  o.result.steps = 0xDEADu;
  o.result.alpha_q16 = 0xBEEFu;
  o.ok = advance(&o.state, delta, c, &o.result);
  return o;
}

void feed(Fnv& f, const Out& o) {
  f.num(o.state.accumulator_ns, 8);
  f.num(o.state.dropped_ns, 8);
  f.num(o.result.steps, 4);
  f.num(o.result.alpha_q16, 4);
}

// ---- Naive oracles, sharing no code with the implementation ----------------------------------------------------
std::uint64_t sat(std::uint64_t a, std::uint64_t b) {
  if (kMax - a < b) return kMax;
  return a + b;
}

// Step-by-step simulation, for small values only: take steps one at a time, then throw away whole steps one at a time.
struct Naive {
  std::uint64_t acc, dropped;
  std::uint32_t steps, alpha;
};

Naive naive_small(std::uint64_t acc, std::uint64_t dropped, std::uint64_t delta, std::uint64_t step,
                  std::uint32_t max_steps, std::uint64_t max_delta) {
  std::uint64_t used = delta;
  while (used > max_delta) {
    used -= 1;
    dropped = sat(dropped, 1);
  }
  acc = sat(acc, used);
  Naive n{};
  while (acc >= step && n.steps < max_steps) {
    acc -= step;
    n.steps += 1;
  }
  while (acc >= step) {
    acc -= step;
    dropped = sat(dropped, step);
  }
  // Small values only, so the native product and quotient are exact here.
  const std::uint32_t q = static_cast<std::uint32_t>(acc * 65536 / step);
  n.acc = acc;
  n.dropped = dropped;
  n.alpha = q;
  return n;
}

// 64x64 -> 128 bit product in two 64-bit halves, by 32-bit limbs.
void mul64(std::uint64_t a, std::uint64_t b, std::uint64_t& hi, std::uint64_t& lo) {
  const std::uint64_t a0 = a & 0xFFFFFFFFull, a1 = a >> 32, b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
  const std::uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  const std::uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
  lo = (p00 & 0xFFFFFFFFull) | (mid << 32);
  hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

// Largest q in [0, 65535] with q * step <= accumulator * 65536, by binary search on the exact 128-bit comparison.
std::uint32_t alpha_oracle(std::uint64_t acc, std::uint64_t step) {
  const std::uint64_t t_hi = acc >> 48, t_lo = acc << 16;
  std::uint32_t lo = 0, hi = 65535;
  while (lo < hi) {
    const std::uint32_t mid = lo + (hi - lo + 1) / 2;
    std::uint64_t p_hi, p_lo;
    mul64(mid, step, p_hi, p_lo);
    const bool fits = p_hi < t_hi || (p_hi == t_hi && p_lo <= t_lo);
    if (fits) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  return lo;
}

}  // namespace

TEST_CASE("defaults and valid()", "[timing]") {
  StepConfig c;
  CHECK(c.step_ns == 16666667ull);
  CHECK(c.max_steps == 5u);
  CHECK(c.max_delta_ns == 250000000ull);
  CHECK(valid(c));
  c.step_ns = 0;
  CHECK_FALSE(valid(c));
  c.step_ns = 1;
  c.max_steps = 0;
  CHECK_FALSE(valid(c));
  c.max_steps = 1;
  c.max_delta_ns = 0;
  CHECK(valid(c));
  c.max_delta_ns = kMax;
  c.step_ns = kMax;
  c.max_steps = 0xFFFFFFFFu;
  CHECK(valid(c));
}

TEST_CASE("failure leaves state and out untouched", "[timing]") {
  StepConfig good;
  StepState st;
  st.accumulator_ns = 77;
  st.dropped_ns = 9;
  StepResult res;
  res.steps = 123;
  res.alpha_q16 = 456;
  CHECK_FALSE(advance(nullptr, 5, good, &res));
  CHECK(res.steps == 123u);
  CHECK(res.alpha_q16 == 456u);
  CHECK_FALSE(advance(&st, 5, good, nullptr));
  CHECK(st.accumulator_ns == 77u);
  CHECK(st.dropped_ns == 9u);
  CHECK_FALSE(advance(nullptr, 5, good, nullptr));
  StepConfig bad = good;
  bad.step_ns = 0;
  CHECK_FALSE(advance(&st, 5, bad, &res));
  CHECK(st.accumulator_ns == 77u);
  CHECK(st.dropped_ns == 9u);
  CHECK(res.steps == 123u);
  CHECK(res.alpha_q16 == 456u);
  bad = good;
  bad.max_steps = 0;
  CHECK_FALSE(advance(&st, 5, bad, &res));
  CHECK(st.accumulator_ns == 77u);
  CHECK(st.dropped_ns == 9u);
  CHECK(res.steps == 123u);
  CHECK(res.alpha_q16 == 456u);
}

TEST_CASE("hand-computed frames", "[timing]") {
  SECTION("one step plus remainder, half a step of alpha") {
    const Out o = run(0, 0, 150, 100, 5, 1000);
    REQUIRE(o.ok);
    CHECK(o.result.steps == 1u);
    CHECK(o.state.accumulator_ns == 50u);
    CHECK(o.result.alpha_q16 == 32768u);
    CHECK(o.state.dropped_ns == 0u);
  }
  SECTION("under one step gives zero steps") {
    const Out o = run(10, 0, 89, 100, 5, 1000);
    REQUIRE(o.ok);
    CHECK(o.result.steps == 0u);
    CHECK(o.state.accumulator_ns == 99u);
    CHECK(o.result.alpha_q16 == 64880u);  // floor(99 * 65536 / 100)
  }
  SECTION("exactly max_steps steps drops nothing") {
    const Out o = run(0, 0, 500, 100, 5, 1000);
    REQUIRE(o.ok);
    CHECK(o.result.steps == 5u);
    CHECK(o.state.accumulator_ns == 0u);
    CHECK(o.state.dropped_ns == 0u);
    CHECK(o.result.alpha_q16 == 0u);
  }
  SECTION("one step past max_steps drops it, remainder kept") {
    const Out o = run(0, 0, 650, 100, 5, 1000);
    REQUIRE(o.ok);
    CHECK(o.result.steps == 5u);
    CHECK(o.state.accumulator_ns == 50u);
    CHECK(o.state.dropped_ns == 100u);
  }
  SECTION("delta clamp adds the excess to dropped") {
    const Out o = run(0, 7, 1000, 100, 5, 300);
    REQUIRE(o.ok);
    CHECK(o.state.dropped_ns == 7u + 700u);
    CHECK(o.result.steps == 3u);
    CHECK(o.state.accumulator_ns == 0u);
  }
  SECTION("both caps, excess first then discard") {
    const Out o = run(0, 0, 10000, 100, 2, 1050);
    REQUIRE(o.ok);
    CHECK(o.result.steps == 2u);
    CHECK(o.state.accumulator_ns == 50u);
    CHECK(o.state.dropped_ns == (10000u - 1050u) + 800u);
  }
  SECTION("max_delta 0 clamps everything") {
    const Out o = run(40, 0, 123, 100, 5, 0);
    REQUIRE(o.ok);
    CHECK(o.state.dropped_ns == 123u);
    CHECK(o.state.accumulator_ns == 40u);
    CHECK(o.result.steps == 0u);
  }
  SECTION("a delta equal to max_delta is not clamped") {
    const Out o = run(0, 0, 300, 100, 5, 300);
    REQUIRE(o.ok);
    CHECK(o.state.dropped_ns == 0u);
    CHECK(o.result.steps == 3u);
  }
  SECTION("default config at 60 Hz frames") {
    StepConfig c;
    StepState st;
    StepResult r;
    REQUIRE(advance(&st, 16666667ull, c, &r));
    CHECK(r.steps == 1u);
    CHECK(st.accumulator_ns == 0u);
    REQUIRE(advance(&st, 250000001ull, c, &r));
    CHECK(r.steps == 5u);
    // clamped 250000000 (1 dropped); 14 whole steps fit, 5 run, 9 whole steps (150000003) are discarded.
    CHECK(st.dropped_ns == 150000004ull);
    CHECK(st.accumulator_ns == 16666662ull);
    CHECK(r.alpha_q16 == 65535u);
  }
}

TEST_CASE("extreme values", "[timing]") {
  SECTION("step_ns 1: every nanosecond is a step") {
    const Out o = run(0, 0, 3, 1, 5, 1000);
    CHECK(o.result.steps == 3u);
    CHECK(o.state.accumulator_ns == 0u);
    CHECK(o.result.alpha_q16 == 0u);
    const Out p = run(0, 0, 9, 1, 5, 1000);
    CHECK(p.result.steps == 5u);
    CHECK(p.state.accumulator_ns == 0u);
    CHECK(p.state.dropped_ns == 4u);
  }
  SECTION("step_ns UINT64_MAX") {
    const Out o = run(0, 0, kMax, kMax, 5, kMax);
    CHECK(o.result.steps == 1u);
    CHECK(o.state.accumulator_ns == 0u);
    CHECK(o.result.alpha_q16 == 0u);
    const Out p = run(0, 0, kMax - 1, kMax, 5, kMax);
    CHECK(p.result.steps == 0u);
    CHECK(p.state.accumulator_ns == kMax - 1);
    CHECK(p.result.alpha_q16 == 65535u);
    const Out q = run(kMax / 2, 0, 0, kMax, 5, kMax);
    CHECK(q.result.alpha_q16 == 32767u);
    const Out q2 = run(kMax / 2 + 1, 0, 0, kMax, 5, kMax);
    CHECK(q2.result.alpha_q16 == 32768u);
  }
  SECTION("accumulator saturates instead of wrapping") {
    const Out o = run(kMax, 0, kMax, 3, 5, kMax);
    REQUIRE(o.ok);
    CHECK(o.result.steps == 5u);
    CHECK(o.state.accumulator_ns == 0u);  // UINT64_MAX is a multiple of 3
    // The time lost to the saturation itself is not counted in dropped_ns.
    CHECK(o.state.dropped_ns == kMax - 15u);
  }
  SECTION("a forged oversized accumulator is cut below one step") {
    const Out o = run(kMax, 0, 0, 1000, 5, 100);
    REQUIRE(o.ok);
    CHECK(o.result.steps == 5u);
    CHECK(o.state.accumulator_ns == kMax % 1000);
    CHECK(o.state.dropped_ns == kMax - kMax % 1000 - 5000u);
    CHECK(o.result.alpha_q16 == (kMax % 1000) * 65536u / 1000u);
  }
  SECTION("forged accumulator with step_ns 1 and max_steps UINT32_MAX") {
    const Out o = run(kMax, 0, 0, 1, 0xFFFFFFFFu, 0);
    CHECK(o.result.steps == 0xFFFFFFFFu);
    CHECK(o.state.accumulator_ns == 0u);
    CHECK(o.state.dropped_ns == kMax - 0xFFFFFFFFull);
  }
  SECTION("dropped_ns saturates at UINT64_MAX and stays there") {
    const Out o = run(0, kMax - 2, 1000, 100, 5, 0);
    CHECK(o.state.dropped_ns == kMax);
    const Out p = run(0, kMax, 1000, 100, 5, 0);
    CHECK(p.state.dropped_ns == kMax);
    const Out q = run(0, kMax - 50, 0, 100, 1, 0);
    CHECK(q.state.dropped_ns == kMax - 50);
    const Out r = run(1000, kMax - 50, 0, 100, 1, 0);
    CHECK(r.state.dropped_ns == kMax);
    CHECK(r.state.accumulator_ns == 0u);
    const Out s = run(0, kMax - 1000, kMax, 100, 5, 0);
    CHECK(s.state.dropped_ns == kMax);
  }
  SECTION("max_steps UINT32_MAX with a huge step count") {
    const Out o = run(0, 0, kMax, 1, 0xFFFFFFFFu, kMax);
    CHECK(o.result.steps == 0xFFFFFFFFu);
    CHECK(o.state.dropped_ns == kMax - 0xFFFFFFFFull);
  }
  SECTION("wanted above 2^32 does not truncate steps") {
    const Out o = run(0, 0, 0x100000000ull + 5, 1, 7, kMax);
    CHECK(o.result.steps == 7u);
    CHECK(o.state.dropped_ns == 0x100000000ull + 5 - 7);
  }
  SECTION("alpha at the edge of 16 bit") {
    const Out o = run(1, 0, 0, 65536, 1, 0);
    CHECK(o.result.alpha_q16 == 1u);
    const Out p = run(65535, 0, 0, 65536, 1, 0);
    CHECK(p.result.alpha_q16 == 65535u);
    const Out q = run(1, 0, 0, 65537, 1, 0);
    CHECK(q.result.alpha_q16 == 0u);
    const Out r = run(2, 0, 0, 65537, 1, 0);
    CHECK(r.result.alpha_q16 == 1u);
  }
}

TEST_CASE("reset_accumulator", "[timing]") {
  StepState st;
  st.accumulator_ns = 123;
  st.dropped_ns = 456;
  reset_accumulator(&st);
  CHECK(st.accumulator_ns == 0u);
  CHECK(st.dropped_ns == 456u);
  reset_accumulator(nullptr);
}

TEST_CASE("small parameters match a step-by-step oracle and the reference digest", "[timing][sweep]") {
  Fnv f;
  std::size_t n = 0;
  for (std::uint64_t step = 1; step <= kRefSmallStep; ++step)
    for (std::uint64_t ms = 1; ms <= kRefSmallMaxSteps; ++ms)
      for (std::uint64_t md = 0; md <= kRefSmallMaxDelta; ++md)
        for (std::uint64_t acc = 0; acc <= kRefSmallAcc; ++acc)
          for (std::uint64_t dr : kRefSmallDropped)
            for (std::uint64_t delta = 0; delta <= kRefSmallDelta; ++delta) {
              const Out o = run(acc, dr, delta, step, static_cast<std::uint32_t>(ms), md);
              const Naive e = naive_small(acc, dr, delta, step, static_cast<std::uint32_t>(ms), md);
              if (!o.ok || o.state.accumulator_ns != e.acc || o.state.dropped_ns != e.dropped || o.result.steps != e.steps ||
                  o.result.alpha_q16 != e.alpha) {
                FAIL("small sweep mismatch: step " << step << " max_steps " << ms << " max_delta " << md << " acc " << acc
                                                   << " dropped " << dr << " delta " << delta);
              }
              feed(f, o);
              ++n;
            }
  CHECK(n == kRefSmallCases);
  CHECK(f.h == kRefDigest_small);
}

TEST_CASE("extreme grid matches the big-integer reference digest and the alpha oracle", "[timing][sweep]") {
  Fnv f;
  std::size_t n = 0;
  for (std::size_t si = 1; si < sizeof(kRefExtremes) / sizeof(kRefExtremes[0]); ++si)
    for (std::uint32_t ms : kRefExtMaxSteps)
      for (std::uint64_t md : kRefExtremes)
        for (std::uint64_t acc : kRefExtremes)
          for (std::uint64_t dr : kRefExtDropped)
            for (std::uint64_t delta : kRefExtremes) {
              const std::uint64_t step = kRefExtremes[si];
              const Out o = run(acc, dr, delta, step, ms, md);
              if (!o.ok || o.state.accumulator_ns >= step || o.result.steps > ms || o.result.alpha_q16 >= 65536u ||
                  o.state.dropped_ns < dr || o.result.alpha_q16 != alpha_oracle(o.state.accumulator_ns, step)) {
                FAIL("extreme grid mismatch: step " << step << " max_steps " << ms << " max_delta " << md << " acc " << acc
                                                    << " dropped " << dr << " delta " << delta);
              }
              feed(f, o);
              ++n;
            }
  CHECK(n == kRefExtremeCases);
  CHECK(f.h == kRefDigest_extreme);
}

TEST_CASE("alpha is exact on random steps of every bit length", "[timing][alpha]") {
  Rng r{kRefAlphaSeed};
  Fnv f;
  for (std::size_t i = 0; i < kRefAlphaCases; ++i) {
    const std::uint64_t bits = 1 + r.next() % 64;
    std::uint64_t step = r.next() >> (64 - bits);
    if (step == 0) step = 1;
    const std::uint64_t acc = r.next() % step;
    const Out o = run(acc, 0, 0, step, 1, 0);
    if (!o.ok || o.result.steps != 0u || o.state.accumulator_ns != acc || o.result.alpha_q16 != alpha_oracle(acc, step)) {
      FAIL("alpha mismatch: step " << step << " acc " << acc);
    }
    f.num(o.result.alpha_q16, 4);
  }
  CHECK(f.h == kRefDigest_alpha);
}

TEST_CASE("alpha stays exact at the boundaries of each step", "[timing][alpha]") {
  const std::uint64_t steps[] = {1, 2, 3, 65535, 65536, 65537, 0xFFFFFFFFull, 0x100000000ull, kMax / 2, kMax / 2 + 1, kMax - 1, kMax};
  for (std::uint64_t step : steps) {
    const std::uint64_t accs[] = {0, 1, step / 3, step / 2, step - step / 3, step - 1};
    for (std::uint64_t acc : accs) {
      if (acc >= step) continue;
      const Out o = run(acc, 0, 0, step, 1, 0);
      REQUIRE(o.result.alpha_q16 == alpha_oracle(acc, step));
    }
    // The first accumulator that reaches each alpha value k: ceil(k * step / 65536), found with the oracle.
    for (std::uint32_t k : {1u, 2u, 255u, 256u, 32768u, 65535u}) {
      std::uint64_t lo = 0, hi = step - 1;
      if (alpha_oracle(hi, step) < k) continue;
      while (lo < hi) {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        if (alpha_oracle(mid, step) >= k) {
          hi = mid;
        } else {
          lo = mid + 1;
        }
      }
      // lo is the first accumulator whose alpha reaches k; the one before it stays below k.
      REQUIRE(run(lo, 0, 0, step, 1, 0).result.alpha_q16 >= k);
      if (lo > 0) REQUIRE(run(lo - 1, 0, 0, step, 1, 0).result.alpha_q16 < k);
    }
  }
}
