// Tests for core/lifecycle/power_policy. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/power_policy_ref.py, an independent Python
// implementation on exact rationals and unbounded integers. Regenerate it with: python3 tools/power_policy_ref.py
// The tests also carry a naive oracle (repeated subtraction and a different algebraic split for the scaling) that
// shares no code with core/lifecycle/power_policy.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "power_policy.h"

using namespace lumora::lifecycle;

namespace {

constexpr std::uint32_t kU32 = UINT32_MAX;

// BEGIN GENERATED REFERENCE (tools/power_policy_ref.py)
const std::uint32_t kRefMaxes[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 255u, 4294967295u};
const std::uint32_t kRefNotches[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 100u, 255u, 65535u, 2147483648u, 4294967294u, 4294967295u};
const std::uint32_t kRefPercents[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u, 41u, 42u, 43u, 44u, 45u, 46u, 47u, 48u, 49u, 50u, 51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u, 59u, 60u, 61u, 62u, 63u, 64u, 65u, 66u, 67u, 68u, 69u, 70u, 71u, 72u, 73u, 74u, 75u, 76u, 77u, 78u, 79u, 80u, 81u, 82u, 83u, 84u, 85u, 86u, 87u, 88u, 89u, 90u, 91u, 92u, 93u, 94u, 95u, 96u, 97u, 98u, 99u, 100u, 101u, 102u, 255u, 4294967295u};
const std::uint32_t kRefSizes[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u, 41u, 42u, 43u, 44u, 45u, 46u, 47u, 48u, 49u, 50u, 51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u, 59u, 60u, 61u, 62u, 63u, 64u, 65u, 66u, 67u, 68u, 69u, 70u, 71u, 72u, 73u, 74u, 75u, 76u, 77u, 78u, 79u, 80u, 81u, 82u, 83u, 84u, 85u, 86u, 87u, 88u, 89u, 90u, 91u, 92u, 93u, 94u, 95u, 96u, 97u, 98u, 99u, 100u, 101u, 102u, 103u, 104u, 105u, 106u, 107u, 108u, 109u, 110u, 111u, 112u, 113u, 114u, 115u, 116u, 117u, 118u, 119u, 120u, 121u, 122u, 123u, 124u, 125u, 126u, 127u, 128u, 129u, 130u, 131u, 132u, 133u, 134u, 135u, 136u, 137u, 138u, 139u, 140u, 141u, 142u, 143u, 144u, 145u, 146u, 147u, 148u, 149u, 150u, 151u, 152u, 153u, 154u, 155u, 156u, 157u, 158u, 159u, 160u, 161u, 162u, 163u, 164u, 165u, 166u, 167u, 168u, 169u, 170u, 171u, 172u, 173u, 174u, 175u, 176u, 177u, 178u, 179u, 180u, 181u, 182u, 183u, 184u, 185u, 186u, 187u, 188u, 189u, 190u, 191u, 192u, 193u, 194u, 195u, 196u, 197u, 198u, 199u, 200u, 201u, 202u, 203u, 204u, 205u, 206u, 207u, 208u, 209u, 210u, 211u, 212u, 213u, 214u, 215u, 216u, 217u, 218u, 219u, 220u, 221u, 222u, 223u, 224u, 225u, 226u, 227u, 228u, 229u, 230u, 231u, 232u, 233u, 234u, 235u, 236u, 237u, 238u, 239u, 240u, 241u, 242u, 243u, 244u, 245u, 246u, 247u, 248u, 249u, 250u, 251u, 252u, 253u, 254u, 255u, 256u, 257u, 258u, 259u, 260u, 261u, 262u, 263u, 264u, 265u, 266u, 267u, 268u, 269u, 270u, 271u, 272u, 273u, 274u, 275u, 276u, 277u, 278u, 279u, 280u, 281u, 282u, 283u, 284u, 285u, 286u, 287u, 288u, 289u, 290u, 291u, 292u, 293u, 294u, 295u, 296u, 297u, 298u, 299u, 300u, 999u, 1000u, 1001u, 4095u, 4096u, 65535u, 65536u, 42949672u, 42949673u, 42949674u, 2147483647u, 2147483648u, 2147483649u, 4294967195u, 4294967294u, 4294967295u};
const std::uint32_t kRefSentinel = 0xDEADBEEFU;
const std::uint64_t kRefDigest_render = 0x4DDE4B428A5DCCC5ull;
const std::size_t kRefCases_render = 520;
const std::uint64_t kRefDigest_quality = 0x04EAF857B057933Eull;
const std::size_t kRefCases_quality = 1040;
const std::uint64_t kRefDigest_scaled = 0x9689D684F3A56F64ull;
const std::size_t kRefCases_scaled = 33285;
// END GENERATED REFERENCE

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
};

// floor(a / b) by repeated subtraction; only called with a <= 750 and b in 1..15.
std::uint32_t slow_div(std::uint32_t a, std::uint32_t b) {
  std::uint32_t q = 0;
  while (a >= b) {
    a -= b;
    ++q;
  }
  return q;
}

std::uint32_t oracle_percent(std::uint32_t notch, std::uint32_t mx) {
  std::uint32_t n = notch;
  if (n > mx) n = mx;
  std::uint32_t times50 = 0;
  for (std::uint32_t i = 0; i < n; ++i) times50 += 50;
  return 100 - slow_div(times50, mx);
}

int oracle_quality(bool saver, std::uint32_t notch, std::uint32_t mx) {
  std::uint32_t eff = notch;
  if (eff > mx) eff = mx;
  if (saver) eff = mx;
  if (eff == 0) return 0;
  // Low when eff / mx >= 1/2, that is eff >= ceil(mx / 2).
  const std::uint32_t half_up = slow_div(mx + 1, 2);
  return eff >= half_up ? 2 : 1;
}

// floor(size * p / 100) as (size / 100) * p + ((size % 100) * p) / 100, in 64-bit.
std::uint32_t oracle_scaled(std::uint32_t size, std::uint32_t p) {
  const std::uint64_t whole = static_cast<std::uint64_t>(size / 100) * p;
  const std::uint64_t rest = (static_cast<std::uint64_t>(size % 100) * p) / 100;
  const std::uint64_t v = whole + rest;
  return v == 0 ? 1u : static_cast<std::uint32_t>(v);
}

void feed(Fnv* f, bool ok, std::uint32_t v) {
  f->byte(ok ? 1 : 0);
  f->u32(v);
}

}  // namespace

TEST_CASE("render_scale_percent header examples", "[power_policy][render]") {
  std::uint32_t p = 0;
  CHECK(render_scale_percent(0, 3, &p));
  CHECK(p == 100);
  CHECK(render_scale_percent(1, 3, &p));
  CHECK(p == 84);
  CHECK(render_scale_percent(2, 3, &p));
  CHECK(p == 67);
  CHECK(render_scale_percent(3, 3, &p));
  CHECK(p == 50);
  CHECK(render_scale_percent(0, 1, &p));
  CHECK(p == 100);
  CHECK(render_scale_percent(1, 1, &p));
  CHECK(p == 50);
  CHECK(render_scale_percent(15, 15, &p));
  CHECK(p == 50);
  CHECK(render_scale_percent(7, 15, &p));
  CHECK(p == 77);
}

TEST_CASE("render_scale_percent clamps and rejects", "[power_policy][render]") {
  std::uint32_t p = 12345;
  CHECK(render_scale_percent(4, 3, &p));
  CHECK(p == 50);
  CHECK(render_scale_percent(kU32, 3, &p));
  CHECK(p == 50);
  CHECK(render_scale_percent(kU32, 15, &p));
  CHECK(p == 50);
  p = 12345;
  CHECK_FALSE(render_scale_percent(0, 0, &p));
  CHECK(p == 12345);
  CHECK_FALSE(render_scale_percent(0, 16, &p));
  CHECK_FALSE(render_scale_percent(0, kU32, &p));
  CHECK_FALSE(render_scale_percent(1, 255, &p));
  CHECK(p == 12345);
  CHECK_FALSE(render_scale_percent(0, 3, nullptr));
  CHECK_FALSE(render_scale_percent(0, 0, nullptr));
}

TEST_CASE("render_scale_percent exhaustive over max_notch 1..15 and notches 0..20", "[power_policy][render][exhaustive]") {
  std::size_t bad = 0;
  for (std::uint32_t mx = 1; mx <= 15; ++mx) {
    std::uint32_t prev = 101;
    for (std::uint32_t n = 0; n <= 20; ++n) {
      std::uint32_t p = 7777;
      if (!render_scale_percent(n, mx, &p) || p != oracle_percent(n, mx)) ++bad;
      if (p > prev || p < 50 || p > 100) ++bad;  // non-increasing, within 50..100
      prev = p;
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("quality_preset header examples", "[power_policy][quality]") {
  Quality q = Quality::Medium;
  CHECK(quality_preset(false, 0, 1, &q));
  CHECK(q == Quality::High);
  CHECK(quality_preset(false, 1, 1, &q));
  CHECK(q == Quality::Low);
  const Quality want3[] = {Quality::High, Quality::Medium, Quality::Low, Quality::Low};
  for (std::uint32_t n = 0; n < 4; ++n) {
    CHECK(quality_preset(false, n, 3, &q));
    CHECK(q == want3[n]);
  }
  for (std::uint32_t n = 0; n <= 15; ++n) {
    CHECK(quality_preset(false, n, 15, &q));
    CHECK(q == (n == 0 ? Quality::High : n <= 7 ? Quality::Medium : Quality::Low));
  }
  CHECK(quality_preset(false, 1, 2, &q));
  CHECK(q == Quality::Low);
  CHECK(quality_preset(false, 1, 4, &q));
  CHECK(q == Quality::Medium);
  CHECK(quality_preset(false, 2, 4, &q));
  CHECK(q == Quality::Low);
}

TEST_CASE("quality_preset battery saver, clamp and rejection", "[power_policy][quality]") {
  Quality q = Quality::Medium;
  for (std::uint32_t mx = 1; mx <= 15; ++mx) {
    for (std::uint32_t n : {0u, 1u, mx, mx + 1, kU32}) {
      q = Quality::High;
      CHECK(quality_preset(true, n, mx, &q));
      CHECK(q == Quality::Low);
    }
  }
  CHECK(quality_preset(false, kU32, 15, &q));
  CHECK(q == Quality::Low);
  CHECK(quality_preset(false, 16, 3, &q));
  CHECK(q == Quality::Low);
  q = Quality::Medium;
  for (bool s : {false, true}) {
    CHECK_FALSE(quality_preset(s, 0, 0, &q));
    CHECK_FALSE(quality_preset(s, 1, 16, &q));
    CHECK_FALSE(quality_preset(s, 1, kU32, &q));
    CHECK(q == Quality::Medium);
    CHECK_FALSE(quality_preset(s, 1, 3, nullptr));
    CHECK_FALSE(quality_preset(s, 1, 0, nullptr));
  }
}

TEST_CASE("quality_preset exhaustive over max_notch 1..15 and notches 0..20", "[power_policy][quality][exhaustive]") {
  std::size_t bad = 0;
  for (int saver = 0; saver < 2; ++saver) {
    for (std::uint32_t mx = 1; mx <= 15; ++mx) {
      for (std::uint32_t n = 0; n <= 20; ++n) {
        Quality q = Quality::Medium;
        if (!quality_preset(saver != 0, n, mx, &q) || static_cast<int>(q) != oracle_quality(saver != 0, n, mx)) ++bad;
      }
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("scaled_size examples and boundaries", "[power_policy][scaled]") {
  std::uint32_t o = 5;
  CHECK(scaled_size(0, 100, &o));
  CHECK(o == 1);
  CHECK(scaled_size(0, 1, &o));
  CHECK(o == 1);
  CHECK(scaled_size(1, 1, &o));
  CHECK(o == 1);
  CHECK(scaled_size(99, 1, &o));
  CHECK(o == 1);
  CHECK(scaled_size(100, 1, &o));
  CHECK(o == 1);
  CHECK(scaled_size(199, 1, &o));
  CHECK(o == 1);
  CHECK(scaled_size(200, 1, &o));
  CHECK(o == 2);
  CHECK(scaled_size(1920, 50, &o));
  CHECK(o == 960);
  CHECK(scaled_size(1081, 84, &o));
  CHECK(o == 908);  // floor(908.04)
  CHECK(scaled_size(1, 99, &o));
  CHECK(o == 1);
  CHECK(scaled_size(2, 50, &o));
  CHECK(o == 1);
  CHECK(scaled_size(3, 50, &o));
  CHECK(o == 1);
  CHECK(scaled_size(kU32, 100, &o));
  CHECK(o == kU32);
  CHECK(scaled_size(kU32, 99, &o));
  CHECK(o == 4252017622u);  // floor(4294967295 * 99 / 100)
  CHECK(scaled_size(kU32, 50, &o));
  CHECK(o == 2147483647u);
  CHECK(scaled_size(kU32, 1, &o));
  CHECK(o == 42949672u);
  CHECK(scaled_size(kU32 - 1, 100, &o));
  CHECK(o == kU32 - 1);
  CHECK(scaled_size(2147483648u, 100, &o));
  CHECK(o == 2147483648u);
}

TEST_CASE("scaled_size rejects bad percent and null", "[power_policy][scaled]") {
  std::uint32_t o = 4242;
  for (std::uint32_t p : {0u, 101u, 102u, 255u, kU32}) {
    CHECK_FALSE(scaled_size(1000, p, &o));
    CHECK(o == 4242);
  }
  CHECK_FALSE(scaled_size(1000, 50, nullptr));
  CHECK_FALSE(scaled_size(1000, 0, nullptr));
}

TEST_CASE("scaled_size sweep against the oracle", "[power_policy][scaled][exhaustive]") {
  std::size_t bad = 0;
  for (std::uint32_t p = 1; p <= 100; ++p) {
    for (std::uint32_t s = 0; s <= 1000; ++s) {
      std::uint32_t o = 0;
      if (!scaled_size(s, p, &o) || o != oracle_scaled(s, p)) ++bad;
    }
    // Around every multiple of 100/p boundary near the top of the 32-bit range, and at the edges.
    for (std::uint32_t d = 0; d <= 200; ++d) {
      const std::uint32_t hi = kU32 - d;
      std::uint32_t o = 0;
      if (!scaled_size(hi, p, &o) || o != oracle_scaled(hi, p)) ++bad;
      const std::uint32_t mid = 2147483648u + d - 100;
      if (!scaled_size(mid, p, &o) || o != oracle_scaled(mid, p)) ++bad;
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("to_string", "[power_policy][to_string]") {
  CHECK(std::string_view(to_string(Quality::High)) == "High");
  CHECK(std::string_view(to_string(Quality::Medium)) == "Medium");
  CHECK(std::string_view(to_string(Quality::Low)) == "Low");
  CHECK(to_string(Quality::Low) == to_string(Quality::Low));
  for (std::uint8_t v : {std::uint8_t{3}, std::uint8_t{4}, std::uint8_t{255}}) {
    const char* s = to_string(static_cast<Quality>(v));
    REQUIRE(s != nullptr);
    CHECK(std::string_view(s) == "unknown");
  }
}

TEST_CASE("render digest", "[power_policy][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint32_t mx : kRefMaxes) {
    for (std::uint32_t nt : kRefNotches) {
      std::uint32_t p = kRefSentinel;
      const bool ok = render_scale_percent(nt, mx, &p);
      if (ok != (mx >= 1 && mx <= 15) || (ok && p != oracle_percent(nt, mx)) || (!ok && p != kRefSentinel)) ++bad;
      feed(&f, ok, p);
      ++n;
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_render);
  CHECK(f.h == kRefDigest_render);
}

TEST_CASE("quality digest", "[power_policy][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (int saver = 0; saver < 2; ++saver) {
    for (std::uint32_t mx : kRefMaxes) {
      for (std::uint32_t nt : kRefNotches) {
        Quality q = static_cast<Quality>(static_cast<std::uint8_t>(kRefSentinel));
        const bool ok = quality_preset(saver != 0, nt, mx, &q);
        const std::uint32_t v = static_cast<std::uint32_t>(static_cast<std::uint8_t>(q));
        const std::uint32_t untouched = static_cast<std::uint32_t>(static_cast<std::uint8_t>(kRefSentinel));
        if (ok != (mx >= 1 && mx <= 15) || (ok && static_cast<int>(q) != oracle_quality(saver != 0, nt, mx)) ||
            (!ok && v != untouched))
          ++bad;
        // A failed call leaves the pre-loaded value (the enum is 8 bits wide); the digest feeds the full sentinel.
        feed(&f, ok, ok ? v : kRefSentinel);
        ++n;
      }
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_quality);
  CHECK(f.h == kRefDigest_quality);
}

TEST_CASE("scaled digest", "[power_policy][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint32_t s : kRefSizes) {
    for (std::uint32_t p : kRefPercents) {
      std::uint32_t o = kRefSentinel;
      const bool ok = scaled_size(s, p, &o);
      if (ok != (p >= 1 && p <= 100) || (ok && o != oracle_scaled(s, p)) || (!ok && o != kRefSentinel)) ++bad;
      feed(&f, ok, o);
      ++n;
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_scaled);
  CHECK(f.h == kRefDigest_scaled);
}
