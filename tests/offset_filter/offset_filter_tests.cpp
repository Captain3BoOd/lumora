// Tests for core/input/offset_filter. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/offset_filter_ref.py, an independent Python
// implementation on exact fractions. Regenerate it with: python3 tools/offset_filter_ref.py
// The tests also carry a naive oracle (binary search for the quotients instead of division) that shares no code with
// core/input/offset_filter.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "offset_filter.h"

using namespace lumora::input;

namespace {

// BEGIN GENERATED REFERENCE (tools/offset_filter_ref.py)
const std::int32_t kRefDeadzones[] = {0, 1, 2, 100, 1966, 1967, 16384, 32767, 32768};
const std::int32_t kRefInputs[] = {INT32_MIN, -2147483647, -2000000, -65537, -65536, -65535, -32769, -32768, -1967, -1966, -1, 0, 1, 2, 1965, 1966, 1967, 32767, 32768, 32769, 65535, 65536, 65537, 2000000, 2147483646, INT32_MAX};
const std::int32_t kRefStates[] = {INT32_MIN, -65537, -65536, -65535, -13107, -100, -1, 0, 1, 99, 100, 13107, 65535, 65536, 65537, INT32_MAX};
const std::uint32_t kRefBadSmooth[] = {100u, 101u, 255u, 4294967295u};
const std::int32_t kRefBadDeadzone[] = {-1, 32769, 65536, INT32_MIN, INT32_MAX};
const std::uint64_t kRefSeqSeeds[] = {11, 22, 33, 44};
const std::size_t kRefSeqN = 500;
const std::uint32_t kRefSentinel = 0x5EEDBEEFU;
const std::uint64_t kRefDigest_deadzone = 0x5B8E31C3F0802E89ull;
const std::size_t kRefCases_deadzone = 234;
const std::uint64_t kRefDigest_smooth = 0x1CD5FAF26340BA41ull;
const std::size_t kRefCases_smooth = 86400;
const std::uint64_t kRefDigest_invalid = 0xCECE1D8DD11A5EC1ull;
const std::size_t kRefCases_invalid = 11;
const std::uint64_t kRefDigest_seq = 0x7C0FC54516FFA655ull;
const std::size_t kRefCases_seq = 2000;
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

void feed(Fnv* f, bool ok, std::int32_t ox, std::int32_t oy, const OffsetFilter& s) {
  f->byte(ok ? 1 : 0);
  f->u32(static_cast<std::uint32_t>(ox));
  f->u32(static_cast<std::uint32_t>(oy));
  f->u32(static_cast<std::uint32_t>(s.x));
  f->u32(static_cast<std::uint32_t>(s.y));
  f->byte(s.primed ? 1 : 0);
}

std::int32_t neg(std::int32_t v) { return v == INT32_MIN ? INT32_MAX : -v; }

OffsetFilterConfig cfg(std::uint32_t sp, std::int32_t dz) {
  OffsetFilterConfig c;
  c.smooth_percent = sp;
  c.deadzone = dz;
  return c;
}

// Largest q in [0, hi] with q * d <= n (n >= 0, d >= 1), by binary search: no division.
std::int64_t search_quot(std::int64_t n, std::int64_t d, std::int64_t hi) {
  std::int64_t lo = 0;
  while (lo < hi) {
    const std::int64_t mid = lo + (hi - lo + 1) / 2;
    if (mid * d <= n) lo = mid; else hi = mid - 1;
  }
  return lo;
}

std::int32_t oracle_dead(std::int64_t v, std::int64_t dz) {
  if (v > 65536) v = 65536;
  if (v < -65536) v = -65536;
  const std::int64_t m = v < 0 ? -v : v;
  if (m <= dz) return 0;
  const std::int64_t q = search_quot((m - dz) * 65536, 65536 - dz, 65536);
  return static_cast<std::int32_t>(v < 0 ? -q : q);
}

std::int32_t oracle_smooth(std::int64_t state, std::int64_t value, std::int64_t sp) {
  const std::int64_t delta = value - state;
  const std::int64_t mag = delta < 0 ? -delta : delta;
  const std::int64_t q = search_quot(mag * (100 - sp), 100, 200000);
  return static_cast<std::int32_t>(state + (delta < 0 ? -q : q));
}

struct Oracle {
  std::int64_t x, y;
  bool primed;
};

void oracle_step(Oracle* o, std::int64_t ix, std::int64_t iy, std::int64_t sp, std::int64_t dz) {
  const std::int64_t vx = oracle_dead(ix, dz), vy = oracle_dead(iy, dz);
  if (o->x > 65536) o->x = 65536;
  if (o->x < -65536) o->x = -65536;
  if (o->y > 65536) o->y = 65536;
  if (o->y < -65536) o->y = -65536;
  if (!o->primed) {
    o->x = vx;
    o->y = vy;
  } else {
    o->x = oracle_smooth(o->x, vx, sp);
    o->y = oracle_smooth(o->y, vy, sp);
  }
  o->primed = true;
}

}  // namespace

TEST_CASE("constants, defaults and valid", "[offset_filter]") {
  CHECK(kOne == 65536);
  CHECK(kMaxDeadzone == 32768);
  const OffsetFilterConfig c;
  CHECK(c.smooth_percent == 80u);
  CHECK(c.deadzone == 1966);
  CHECK(valid(c));
  const OffsetFilter f;
  CHECK(f.x == 0);
  CHECK(f.y == 0);
  CHECK_FALSE(f.primed);
  CHECK(valid(cfg(0, 0)));
  CHECK(valid(cfg(99, 32768)));
  CHECK(valid(cfg(0, 32768)));
  CHECK(valid(cfg(99, 0)));
  CHECK_FALSE(valid(cfg(100, 0)));
  CHECK_FALSE(valid(cfg(0, -1)));
  CHECK_FALSE(valid(cfg(0, 32769)));
  CHECK_FALSE(valid(cfg(UINT32_MAX, 0)));
  CHECK_FALSE(valid(cfg(0, INT32_MIN)));
  CHECK_FALSE(valid(cfg(0, INT32_MAX)));
  for (std::uint32_t sp = 0; sp <= 99; ++sp) CHECK(valid(cfg(sp, 1966)));
  for (std::uint32_t sp = 100; sp <= 130; ++sp) CHECK_FALSE(valid(cfg(sp, 1966)));
}

TEST_CASE("reset", "[offset_filter][reset]") {
  OffsetFilter f;
  f.x = 5;
  f.y = -7;
  f.primed = true;
  CHECK(reset(&f));
  CHECK(f.x == 0);
  CHECK(f.y == 0);
  CHECK_FALSE(f.primed);
  f.x = INT32_MAX;
  f.y = INT32_MIN;
  CHECK(reset(&f));
  CHECK(f.x == 0);
  CHECK(f.y == 0);
  CHECK_FALSE(reset(nullptr));
}

TEST_CASE("step check order and failures write nothing", "[offset_filter][step]") {
  const OffsetFilterConfig good;
  const OffsetFilterConfig bad = cfg(100, 1966);
  std::int32_t ox = 111, oy = 222;
  OffsetFilter f;
  f.x = 5;
  f.y = 6;
  f.primed = true;
  OffsetFilter g = f;
  CHECK_FALSE(step(nullptr, 1, 1, good, &ox, &oy));
  CHECK_FALSE(step(nullptr, 1, 1, bad, nullptr, nullptr));
  CHECK_FALSE(step(&f, 1, 1, good, nullptr, &oy));
  CHECK_FALSE(step(&f, 1, 1, good, &ox, nullptr));
  CHECK_FALSE(step(&f, 1, 1, good, nullptr, nullptr));
  CHECK_FALSE(step(&f, 1, 1, bad, nullptr, &oy));
  CHECK_FALSE(step(&f, 1, 1, bad, &ox, &oy));
  CHECK_FALSE(step(&f, 1, 1, cfg(0, -1), &ox, &oy));
  CHECK_FALSE(step(&f, 1, 1, cfg(0, 32769), &ox, &oy));
  CHECK(ox == 111);
  CHECK(oy == 222);
  CHECK(f.x == g.x);
  CHECK(f.y == g.y);
  CHECK(f.primed == g.primed);
  // An out-of-range hand-built state is not repaired by a failed call.
  OffsetFilter h;
  h.x = 70000;
  h.y = -70000;
  h.primed = true;
  CHECK_FALSE(step(&h, 1, 1, bad, &ox, &oy));
  CHECK(h.x == 70000);
  CHECK(h.y == -70000);
}

TEST_CASE("dead zone examples", "[offset_filter][deadzone]") {
  struct Case {
    std::int32_t dz, in, want;
  };
  const Case cases[] = {
      {1966, 1966, 0},       {1966, 1967, 1},     {1966, 1968, 2},        {1966, 2000, 35},      {1966, 65535, 65534},
      {1966, 65536, 65536},  {1966, -1966, 0},    {1966, -1967, -1},      {1966, -65536, -65536}, {1966, 32768, 31754},
      {1966, 40000, 39210},  {1966, 0, 0},        {0, 1, 1},              {0, -1, -1},           {0, 65536, 65536},
      {0, -65536, -65536},   {0, 5, 5},           {32768, 32768, 0},      {32768, 32769, 2},     {32768, 32770, 4},
      {32768, 49152, 32768}, {32768, 65535, 65534}, {32768, 65536, 65536}, {32768, -65536, -65536},
  };
  for (const Case& c : cases) {
    OffsetFilter f;
    std::int32_t ox = 0, oy = 0;
    INFO("dz " << c.dz << " in " << c.in);
    CHECK(step(&f, c.in, neg(c.in), cfg(0, c.dz), &ox, &oy));
    CHECK(ox == c.want);
    CHECK(oy == -c.want);
  }
}

TEST_CASE("input clamp edges include INT32_MIN and INT32_MAX", "[offset_filter][clamp]") {
  for (std::int32_t dz : {0, 1966, 32768}) {
    for (std::int32_t v : {INT32_MAX, INT32_MAX - 1, 2000000, 65537, 65536}) {
      OffsetFilter f;
      std::int32_t ox = 0, oy = 0;
      CHECK(step(&f, v, neg(v), cfg(0, dz), &ox, &oy));
      CHECK(ox == 65536);
      CHECK(oy == -65536);
    }
    for (std::int32_t v : {INT32_MIN, INT32_MIN + 1, -2000000, -65537, -65536}) {
      OffsetFilter f;
      std::int32_t ox = 0, oy = 0;
      CHECK(step(&f, v, INT32_MIN, cfg(0, dz), &ox, &oy));
      CHECK(ox == -65536);
      CHECK(oy == -65536);
    }
  }
  OffsetFilter f;
  std::int32_t ox = 0, oy = 0;
  CHECK(step(&f, INT32_MIN, INT32_MAX, cfg(0, 0), &ox, &oy));
  CHECK(ox == -65536);
  CHECK(oy == 65536);
}

TEST_CASE("axes are independent", "[offset_filter][step]") {
  OffsetFilter f;
  std::int32_t ox = 0, oy = 0;
  CHECK(step(&f, 65536, 0, cfg(0, 1966), &ox, &oy));
  CHECK(ox == 65536);
  CHECK(oy == 0);
  CHECK(step(&f, 0, 65536, cfg(50, 1966), &ox, &oy));
  CHECK(ox == 32768);
  CHECK(oy == 32768);
  CHECK(step(&f, 1000, -1000, cfg(50, 1966), &ox, &oy));  // both inside the dead zone: value 0
  CHECK(ox == 16384);
  CHECK(oy == 16384);
}

TEST_CASE("first sample passes through, whatever the hand-built state", "[offset_filter][step]") {
  OffsetFilter f;
  f.x = 9;
  f.y = 9;
  std::int32_t ox = 0, oy = 0;
  CHECK(step(&f, 65536, -65536, cfg(80, 1966), &ox, &oy));
  CHECK(ox == 65536);
  CHECK(oy == -65536);
  CHECK(f.primed);
  OffsetFilter g;
  g.x = 70000;
  g.y = INT32_MIN;
  CHECK(step(&g, 3, -3, cfg(99, 0), &ox, &oy));
  CHECK(ox == 3);
  CHECK(oy == -3);
  CHECK(g.x == 3);
  CHECK(g.y == -3);
  CHECK(g.primed);
}

TEST_CASE("smoothing truncates toward zero", "[offset_filter][smooth]") {
  struct Case {
    std::uint32_t sp;
    std::int32_t state, value, want;
  };
  const Case cases[] = {
      {80, 0, 65536, 13107},  {80, 0, -65536, -13107}, {80, 13107, 65536, 23592}, {99, 0, 99, 0},     {99, 0, 100, 1},
      {99, 0, -99, 0},        {99, 0, -100, -1},       {99, 65536, 0, 64881},      {0, 1234, -4321, -4321}, {50, 0, 3, 1},
      {50, 0, -3, -1},        {50, 0, 1, 0},           {50, 1, 0, 1},              {50, -1, 0, -1},
  };
  for (const Case& c : cases) {
    OffsetFilter f;
    f.x = c.state;
    f.y = c.state;
    f.primed = true;
    std::int32_t ox = 0, oy = 0;
    INFO("sp " << c.sp << " state " << c.state << " value " << c.value);
    CHECK(step(&f, c.value, c.value, cfg(c.sp, 0), &ox, &oy));
    CHECK(ox == c.want);
    CHECK(oy == c.want);
    CHECK(f.x == c.want);
    CHECK(f.y == c.want);
  }
}

TEST_CASE("smoothing converges and stalls within 99 units at smooth 99", "[offset_filter][smooth]") {
  OffsetFilter f;
  std::int32_t ox = 0, oy = 0;
  CHECK(step(&f, 0, 0, cfg(99, 0), &ox, &oy));
  for (int i = 0; i < 5000; ++i) CHECK(step(&f, 65536, 65536, cfg(99, 0), &ox, &oy));
  CHECK(ox == 65536 - 99);
  CHECK(oy == 65536 - 99);
  OffsetFilter g;
  CHECK(step(&g, 0, 0, cfg(0, 0), &ox, &oy));
  CHECK(step(&g, 65536, -65536, cfg(0, 0), &ox, &oy));
  CHECK(ox == 65536);
  CHECK(oy == -65536);
}

TEST_CASE("hand-built out-of-range state is clamped before smoothing", "[offset_filter][repair]") {
  OffsetFilter f;
  f.x = 70000;
  f.y = -70000;
  f.primed = true;
  std::int32_t ox = 0, oy = 0;
  CHECK(step(&f, 0, 0, cfg(99, 0), &ox, &oy));
  CHECK(ox == 64881);
  CHECK(oy == -64881);
  CHECK(f.primed);
  OffsetFilter g;
  g.x = INT32_MAX;
  g.y = INT32_MIN;
  g.primed = true;
  CHECK(step(&g, 65536, -65536, cfg(99, 0), &ox, &oy));
  CHECK(ox == 65536);
  CHECK(oy == -65536);
  OffsetFilter h;
  h.x = 65537;
  h.y = -65537;
  h.primed = true;
  CHECK(step(&h, 0, 0, cfg(0, 0), &ox, &oy));
  CHECK(ox == 0);
  CHECK(oy == 0);
  OffsetFilter u;  // an unprimed filter keeps primed false until the step succeeds
  u.x = 70000;
  CHECK_FALSE(u.primed);
  CHECK(step(&u, 3, -3, cfg(99, 0), &ox, &oy));
  CHECK(u.primed);
}

TEST_CASE("dead zone sweep against the oracle", "[offset_filter][deadzone][exhaustive]") {
  std::size_t bad = 0;
  std::int32_t dzs[400];
  std::size_t nd = 0;
  for (std::int32_t d = 0; d <= 150; ++d) dzs[nd++] = d;
  for (std::int32_t d = 32768 - 100; d <= 32768; ++d) dzs[nd++] = d;
  for (std::int32_t d = 1966 - 20; d <= 1966 + 20; ++d) dzs[nd++] = d;
  dzs[nd++] = 16384;
  dzs[nd++] = 8192;
  dzs[nd++] = 24576;
  for (std::size_t i = 0; i < nd; ++i) {
    const std::int32_t d = dzs[i];
    for (std::int32_t v : {d - 1, d, d + 1, d + 2, 65534, 65535, 65536, 65537, 2 * d, 2 * d + 1, 1, 2, 3, 32767, 32768}) {
      for (int sgn = 0; sgn < 2; ++sgn) {
        const std::int32_t in = sgn ? -v : v;
        OffsetFilter f;
        std::int32_t ox = 0, oy = 0;
        if (!step(&f, in, neg(in), cfg(0, d), &ox, &oy) || ox != oracle_dead(in, d) || oy != oracle_dead(neg(in), d)) ++bad;
      }
    }
    // Full sweep of one dead zone in a few spots.
    if (d % 37 == 0) {
      for (std::int32_t v = -65600; v <= 65600; v += 7) {
        OffsetFilter f;
        std::int32_t ox = 0, oy = 0;
        if (!step(&f, v, 0, cfg(0, d), &ox, &oy) || ox != oracle_dead(v, d) || oy != 0) ++bad;
      }
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("smoothing sweep against the oracle", "[offset_filter][smooth][exhaustive]") {
  std::size_t bad = 0;
  for (std::uint32_t sp = 0; sp <= 99; ++sp) {
    for (std::int32_t st : {-65536, -40000, -1234, -100, -1, 0, 1, 99, 100, 777, 30000, 65536}) {
      for (std::int32_t v : {-65536, -32768, -1967, -100, -1, 0, 1, 100, 1967, 30000, 65536}) {
        OffsetFilter f;
        f.x = st;
        f.y = neg(st);
        f.primed = true;
        std::int32_t ox = 0, oy = 0;
        if (!step(&f, v, neg(v), cfg(sp, 0), &ox, &oy) || ox != oracle_smooth(st, v, sp) ||
            oy != oracle_smooth(neg(st), neg(v), sp) || f.x != ox || f.y != oy)
          ++bad;
      }
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("dead zone grid digest", "[offset_filter][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::int32_t d : kRefDeadzones) {
    for (std::int32_t v : kRefInputs) {
      OffsetFilter s;
      s.x = 7;
      s.y = 9;
      std::int32_t ox = static_cast<std::int32_t>(kRefSentinel), oy = static_cast<std::int32_t>(kRefSentinel);
      const bool ok = step(&s, v, neg(v), cfg(50, d), &ox, &oy);
      if (!ok || ox != oracle_dead(v, d) || oy != oracle_dead(neg(v), d)) ++bad;
      feed(&f, ok, ox, oy, s);
      ++n;
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_deadzone);
  CHECK(f.h == kRefDigest_deadzone);
}

TEST_CASE("smoothing grid digest", "[offset_filter][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  constexpr std::size_t kNS = sizeof(kRefStates) / sizeof(kRefStates[0]);
  constexpr std::size_t kNI = sizeof(kRefInputs) / sizeof(kRefInputs[0]);
  for (std::uint32_t sp = 0; sp <= 99; ++sp) {
    for (std::int32_t d : {0, 1966, 32768}) {
      for (int primed = 0; primed < 2; ++primed) {
        for (std::size_t i = 0; i < kNS; ++i) {
          const std::int32_t t = kRefStates[(i * 5 + 3) % kNS];
          for (std::size_t k = 0; k < kNI; k += 3) {
            const std::int32_t v = kRefInputs[k];
            OffsetFilter s;
            s.x = kRefStates[i];
            s.y = t;
            s.primed = primed != 0;
            Oracle o{kRefStates[i], t, primed != 0};
            std::int32_t ox = 0, oy = 0;
            const bool ok = step(&s, v, neg(v), cfg(sp, d), &ox, &oy);
            oracle_step(&o, v, neg(v), sp, d);
            if (!ok || ox != o.x || oy != o.y || s.x != o.x || s.y != o.y || !s.primed) ++bad;
            feed(&f, ok, ox, oy, s);
            ++n;
          }
        }
      }
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_smooth);
  CHECK(f.h == kRefDigest_smooth);
}

TEST_CASE("invalid config digest", "[offset_filter][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  struct C {
    std::uint32_t sp;
    std::int32_t dz;
  };
  C cs[16];
  std::size_t nc = 0;
  for (std::uint32_t sp : kRefBadSmooth) cs[nc++] = C{sp, 1966};
  for (std::int32_t d : kRefBadDeadzone) cs[nc++] = C{80, d};
  cs[nc++] = C{100, -1};
  cs[nc++] = C{UINT32_MAX, INT32_MIN};
  for (std::size_t i = 0; i < nc; ++i) {
    OffsetFilter s;
    s.x = 5;
    s.y = -5;
    s.primed = true;
    std::int32_t ox = static_cast<std::int32_t>(kRefSentinel), oy = ox;
    const bool ok = step(&s, 1000, -1000, cfg(cs[i].sp, cs[i].dz), &ox, &oy);
    if (ok || valid(cfg(cs[i].sp, cs[i].dz)) || s.x != 5 || s.y != -5 || !s.primed ||
        ox != static_cast<std::int32_t>(kRefSentinel) || oy != ox)
      ++bad;
    feed(&f, ok, ox, oy, s);
    ++n;
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_invalid);
  CHECK(f.h == kRefDigest_invalid);
}

TEST_CASE("scripted sequences against digest and oracle", "[offset_filter][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  constexpr std::size_t kNI = sizeof(kRefInputs) / sizeof(kRefInputs[0]);
  for (std::uint64_t seed : kRefSeqSeeds) {
    const std::uint32_t sp = static_cast<std::uint32_t>(seed * 2 % 100);
    const std::int32_t dz = static_cast<std::int32_t>((seed * 733) % (32768 + 1));
    OffsetFilter s;
    Oracle o{0, 0, false};
    std::uint64_t x = seed;
    for (std::size_t k = 0; k < kRefSeqN; ++k) {
      x = x * 6364136223846793005ull + 1442695040888963407ull;
      const std::uint64_t r = x >> 33;
      std::int32_t v, w;
      if (r % 11 == 0) {
        v = kRefInputs[(r >> 8) % kNI];
        w = kRefInputs[(r >> 16) % kNI];
      } else {
        v = static_cast<std::int32_t>(static_cast<std::int64_t>(r % 150001) - 75000);
        w = static_cast<std::int32_t>(static_cast<std::int64_t>((r >> 17) % 150001) - 75000);
      }
      if (k % 97 == 96) {
        s = OffsetFilter();
        o = Oracle{0, 0, false};
      }
      std::int32_t ox = 0, oy = 0;
      const bool ok = step(&s, v, w, cfg(sp, dz), &ox, &oy);
      oracle_step(&o, v, w, sp, dz);
      if (!ok || ox != o.x || oy != o.y) ++bad;
      feed(&f, ok, ox, oy, s);
      ++n;
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_seq);
  CHECK(f.h == kRefDigest_seq);
}
