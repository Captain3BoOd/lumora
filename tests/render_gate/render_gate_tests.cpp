// Tests for core/lifecycle/render_gate. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/render_gate_ref.py, an independent Python
// implementation. Regenerate it with: python3 tools/render_gate_ref.py
// The tests also carry a naive oracle (an array of three facts and an edge table) that shares no code with
// core/lifecycle/render_gate.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "render_gate.h"

using namespace lumora::lifecycle;

namespace {

// BEGIN GENERATED REFERENCE (tools/render_gate_ref.py)
const std::uint64_t kRefSeqSeeds[] = {1u, 2u, 3u, 4u};
const std::size_t kRefSeqN = 600;
const std::uint8_t kRefInvalid[] = {6u, 7u, 255u};
const std::uint8_t kRefUntouched = 9;
const std::uint64_t kRefDigest_grid = 0x287015C037705AEDull;
const std::size_t kRefCases_grid = 144;
const std::uint64_t kRefDigest_should = 0xF4B5CB5BCC647005ull;
const std::uint64_t kRefDigest_seq = 0x390970D81677F47Dull;
const std::size_t kRefCases_seq = 2400;
// END GENERATED REFERENCE

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
};

GateEvent ev(std::uint8_t v) { return static_cast<GateEvent>(v); }

RenderGate make(unsigned c) {
  RenderGate g;
  g.surface = (c & 1u) != 0;
  g.visible = (c & 2u) != 0;
  g.screen_on = (c & 4u) != 0;
  g.running = (c & 8u) != 0;
  return g;
}

bool same(const RenderGate& a, const RenderGate& b) {
  return a.surface == b.surface && a.visible == b.visible && a.screen_on == b.screen_on && a.running == b.running;
}

// Naive oracle: three integer facts, the event number picks the fact (n / 2) and the value (n even sets it).
struct Oracle {
  int f[3];
  int run;
};

Oracle to_oracle(const RenderGate& g) { return Oracle{{g.surface ? 1 : 0, g.visible ? 1 : 0, g.screen_on ? 1 : 0}, g.running ? 1 : 0}; }

bool matches(const RenderGate& g, const Oracle& o) {
  return (g.surface ? 1 : 0) == o.f[0] && (g.visible ? 1 : 0) == o.f[1] && (g.screen_on ? 1 : 0) == o.f[2] &&
         (g.running ? 1 : 0) == o.run;
}

// Returns the action number (0 None, 1 Start, 2 Stop).
int oracle_apply(Oracle* o, int n) {
  o->f[n / 2] = (n % 2 == 0) ? 1 : 0;
  const int was = o->run;
  const int now = (o->f[0] + o->f[1] + o->f[2] == 3) ? 1 : 0;
  o->run = now;
  static const int edge[2][2] = {{0, 1}, {2, 0}};
  return edge[was][now];
}

void feed(Fnv* f, const RenderGate& g, bool ok, std::uint8_t act) {
  f->byte(ok ? 1 : 0);
  f->byte(act);
  f->byte(g.surface ? 1 : 0);
  f->byte(g.visible ? 1 : 0);
  f->byte(g.screen_on ? 1 : 0);
  f->byte(g.running ? 1 : 0);
}

}  // namespace

TEST_CASE("defaults and enum values", "[render_gate]") {
  const RenderGate g;
  CHECK_FALSE(g.surface);
  CHECK_FALSE(g.visible);
  CHECK(g.screen_on);
  CHECK_FALSE(g.running);
  CHECK_FALSE(should_render(g));
  CHECK(static_cast<int>(GateEvent::SurfaceCreated) == 0);
  CHECK(static_cast<int>(GateEvent::SurfaceDestroyed) == 1);
  CHECK(static_cast<int>(GateEvent::VisibleTrue) == 2);
  CHECK(static_cast<int>(GateEvent::VisibleFalse) == 3);
  CHECK(static_cast<int>(GateEvent::ScreenOn) == 4);
  CHECK(static_cast<int>(GateEvent::ScreenOff) == 5);
  CHECK(static_cast<int>(GateAction::None) == 0);
  CHECK(static_cast<int>(GateAction::Start) == 1);
  CHECK(static_cast<int>(GateAction::Stop) == 2);
}

TEST_CASE("should_render is the and of the three facts and ignores running", "[render_gate][should_render]") {
  for (unsigned c = 0; c < 16; ++c) {
    const RenderGate g = make(c);
    const bool want = (c & 1u) != 0 && (c & 2u) != 0 && (c & 4u) != 0;
    INFO("combo " << c);
    CHECK(should_render(g) == want);
  }
  RenderGate all = make(7);
  CHECK(should_render(all));
  all.running = true;
  CHECK(should_render(all));
}

TEST_CASE("each event sets its fact", "[render_gate][apply_event]") {
  RenderGate g = make(0);
  GateAction a = GateAction::Stop;
  CHECK(apply_event(&g, GateEvent::SurfaceCreated, &a));
  CHECK(g.surface);
  CHECK(a == GateAction::None);
  CHECK(apply_event(&g, GateEvent::VisibleTrue, &a));
  CHECK(g.visible);
  CHECK(apply_event(&g, GateEvent::ScreenOff, &a));
  CHECK_FALSE(g.screen_on);
  CHECK(apply_event(&g, GateEvent::ScreenOn, &a));
  CHECK(g.screen_on);
  CHECK(a == GateAction::Start);
  CHECK(g.running);
  CHECK(apply_event(&g, GateEvent::VisibleFalse, &a));
  CHECK_FALSE(g.visible);
  CHECK(a == GateAction::Stop);
  CHECK_FALSE(g.running);
  CHECK(apply_event(&g, GateEvent::SurfaceDestroyed, &a));
  CHECK_FALSE(g.surface);
  CHECK(a == GateAction::None);
}

TEST_CASE("start and stop edges over a lifecycle", "[render_gate][apply_event]") {
  RenderGate g;
  GateAction a = GateAction::None;
  struct Step {
    GateEvent e;
    GateAction want;
    bool running;
  };
  const Step steps[] = {
      {GateEvent::VisibleTrue, GateAction::None, false},     {GateEvent::SurfaceCreated, GateAction::Start, true},
      {GateEvent::SurfaceCreated, GateAction::None, true},   {GateEvent::VisibleTrue, GateAction::None, true},
      {GateEvent::ScreenOff, GateAction::Stop, false},       {GateEvent::ScreenOff, GateAction::None, false},
      {GateEvent::SurfaceDestroyed, GateAction::None, false}, {GateEvent::ScreenOn, GateAction::None, false},
      {GateEvent::SurfaceCreated, GateAction::Start, true},  {GateEvent::VisibleFalse, GateAction::Stop, false},
      {GateEvent::VisibleFalse, GateAction::None, false},    {GateEvent::VisibleTrue, GateAction::Start, true},
  };
  int i = 0;
  for (const Step& s : steps) {
    INFO("step " << i++);
    a = GateAction::Stop;
    CHECK(apply_event(&g, s.e, &a));
    CHECK(a == s.want);
    CHECK(g.running == s.running);
  }
}

TEST_CASE("hand-built running that disagrees is healed from the given value", "[render_gate][apply_event]") {
  GateAction a = GateAction::Stop;
  // Header examples: all facts true.
  RenderGate t = make(15);
  CHECK(apply_event(&t, GateEvent::VisibleTrue, &a));
  CHECK(a == GateAction::None);
  CHECK(t.running);
  RenderGate f = make(7);
  CHECK(apply_event(&f, GateEvent::VisibleTrue, &a));
  CHECK(a == GateAction::Start);
  CHECK(f.running);
  // Facts not all true but running set: any valid event gives Stop, even one that does not change a fact.
  RenderGate s = make(8);
  CHECK(apply_event(&s, GateEvent::SurfaceDestroyed, &a));
  CHECK(a == GateAction::Stop);
  CHECK_FALSE(s.running);
  RenderGate s2 = make(9);
  CHECK(apply_event(&s2, GateEvent::SurfaceCreated, &a));
  CHECK(a == GateAction::Stop);
  CHECK_FALSE(s2.running);
}

TEST_CASE("exhaustive 16 combos x 6 events against the oracle", "[render_gate][exhaustive]") {
  std::size_t bad = 0;
  for (unsigned c = 0; c < 16; ++c) {
    for (int n = 0; n < 6; ++n) {
      RenderGate g = make(c);
      Oracle o = to_oracle(g);
      GateAction a = static_cast<GateAction>(kRefUntouched);
      const bool ok = apply_event(&g, ev(static_cast<std::uint8_t>(n)), &a);
      const int want = oracle_apply(&o, n);
      if (!ok || static_cast<int>(a) != want || !matches(g, o)) ++bad;
      // The null-action call leaves the same gate.
      RenderGate h = make(c);
      if (!apply_event(&h, ev(static_cast<std::uint8_t>(n)), nullptr) || !same(g, h)) ++bad;
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("invalid events and null gate fail without writing", "[render_gate][apply_event]") {
  std::size_t bad = 0;
  for (std::uint8_t v : kRefInvalid) {
    for (unsigned c = 0; c < 16; ++c) {
      RenderGate g = make(c);
      const RenderGate before = g;
      GateAction a = static_cast<GateAction>(kRefUntouched);
      if (apply_event(&g, ev(v), &a)) ++bad;
      if (!same(g, before) || static_cast<std::uint8_t>(a) != kRefUntouched) ++bad;
      if (apply_event(&g, ev(v), nullptr) || !same(g, before)) ++bad;
    }
    GateAction a = static_cast<GateAction>(kRefUntouched);
    if (apply_event(nullptr, ev(v), &a) || static_cast<std::uint8_t>(a) != kRefUntouched) ++bad;
    if (apply_event(nullptr, ev(v), nullptr)) ++bad;
  }
  for (std::uint8_t n = 0; n < 6; ++n) {
    GateAction a = static_cast<GateAction>(kRefUntouched);
    CHECK_FALSE(apply_event(nullptr, ev(n), &a));
    CHECK(static_cast<std::uint8_t>(a) == kRefUntouched);
    CHECK_FALSE(apply_event(nullptr, ev(n), nullptr));
  }
  CHECK(bad == 0);
}

TEST_CASE("to_string", "[render_gate][to_string]") {
  const char* const en[] = {"SurfaceCreated", "SurfaceDestroyed", "VisibleTrue", "VisibleFalse", "ScreenOn", "ScreenOff"};
  for (std::uint8_t n = 0; n < 6; ++n) {
    const char* s = to_string(ev(n));
    REQUIRE(s != nullptr);
    CHECK(std::string_view(s) == en[n]);
    CHECK(to_string(ev(n)) == s);
  }
  const char* const an[] = {"None", "Start", "Stop"};
  for (std::uint8_t n = 0; n < 3; ++n) {
    const char* s = to_string(static_cast<GateAction>(n));
    REQUIRE(s != nullptr);
    CHECK(std::string_view(s) == an[n]);
  }
  for (std::uint8_t v : {std::uint8_t{6}, std::uint8_t{7}, std::uint8_t{255}}) {
    const char* e = to_string(ev(v));
    const char* a = to_string(static_cast<GateAction>(v));
    REQUIRE(e != nullptr);
    REQUIRE(a != nullptr);
    CHECK(std::string_view(e) == "unknown");
    CHECK(std::string_view(a) == "unknown");
  }
  CHECK(std::string_view(to_string(static_cast<GateAction>(3))) == "unknown");
}

TEST_CASE("grid digest", "[render_gate][sweep]") {
  Fnv f;
  std::size_t n = 0;
  for (unsigned c = 0; c < 16; ++c) {
    std::uint8_t evs[9] = {0, 1, 2, 3, 4, 5, kRefInvalid[0], kRefInvalid[1], kRefInvalid[2]};
    for (std::uint8_t v : evs) {
      RenderGate g = make(c);
      GateAction a = static_cast<GateAction>(kRefUntouched);
      const bool ok = apply_event(&g, ev(v), &a);
      feed(&f, g, ok, static_cast<std::uint8_t>(a));
      ++n;
    }
  }
  CHECK(n == kRefCases_grid);
  CHECK(f.h == kRefDigest_grid);
}

TEST_CASE("should_render digest", "[render_gate][sweep]") {
  Fnv f;
  for (unsigned c = 0; c < 16; ++c) f.byte(should_render(make(c)) ? 1 : 0);
  CHECK(f.h == kRefDigest_should);
}

TEST_CASE("scripted event sequences against digest and oracle", "[render_gate][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint64_t seed : kRefSeqSeeds) {
    RenderGate g;
    Oracle o = to_oracle(g);
    std::uint64_t x = seed;
    for (std::size_t i = 0; i < kRefSeqN; ++i) {
      x = x * 6364136223846793005ull + 1442695040888963407ull;
      const std::uint64_t r = x >> 33;
      const std::uint8_t v = (r % 17 != 0) ? static_cast<std::uint8_t>(r % 6) : kRefInvalid[(r >> 8) % 3];
      GateAction a = static_cast<GateAction>(kRefUntouched);
      const bool ok = apply_event(&g, ev(v), &a);
      if (v < 6) {
        const int want = oracle_apply(&o, v);
        if (!ok || static_cast<int>(a) != want) ++bad;
      } else if (ok || static_cast<std::uint8_t>(a) != kRefUntouched) {
        ++bad;
      }
      if (!matches(g, o)) ++bad;
      feed(&f, g, ok, static_cast<std::uint8_t>(a));
      ++n;
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_seq);
  CHECK(f.h == kRefDigest_seq);
}
