// Tests for core/spans. Evidence label: unit tests only. The long streams are checked against digests printed by tools/spans_ref.py, an
// independent model on a set of integer points.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <utility>

#include "spans.h"

using namespace lumora::spans;

namespace {

constexpr std::uint64_t kFnvOff = 0xCBF29CE484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001B3ULL;

void fnv_add(std::uint64_t& h, std::uint64_t v) {
  for (int k = 0; k < 8; ++k) {
    h ^= (v >> (8 * k)) & 0xFF;
    h *= kFnvPrime;
  }
}

std::uint32_t xorshift(std::uint32_t s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

struct Case {
  std::uint32_t seed;
  std::uint64_t digest;
};

const Case kCases[] = {
    {0x00000001, 0x6288F09AA3FD214EULL},
    {0x00000002, 0x595C39579EC5145AULL},
    {0x00000003, 0x7493D3EAFB8EFAF3ULL},
    {0x00000007, 0x43F5ACF0F9CAD961ULL},
    {0x0000000B, 0x48762A5271DBC23FULL},
    {0x00003039, 0x6D69A95DC80C878BULL},
    {0xDEADBEEF, 0x2CCD365AD5E6A4A4ULL},
    {0x9E3779B9, 0x435CB086A4C75811ULL},
    {0x0000002A, 0xCDA4DBCB2ABEEBD8ULL},
    {0x000007E8, 0x4065020DB721C18BULL},
    {0x00018697, 0x625D15DC6FD05090ULL},
    {0x00007A69, 0xC31C3948F3D7D9F1ULL},
};

std::uint64_t run_stream(std::uint32_t seed, int steps, int& failures, std::uint32_t& max_count) {
  SpanSet set;
  std::uint64_t h = kFnvOff;
  std::uint32_t st = seed;
  for (int i = 0; i < steps; ++i) {
    st = xorshift(st);
    const std::uint32_t kind = st % 16;
    st = xorshift(st);
    const std::int32_t a = -40 + static_cast<std::int32_t>(st % 120);
    st = xorshift(st);
    const std::int32_t w = static_cast<std::int32_t>(kind < 12 ? st % 3 : st % 2);
    bool ok = true;
    if (kind < 12) {
      ok = add(&set, a, a + w);
    } else if (kind < 15) {
      ok = remove(&set, a, a + w);
    } else {
      clear(&set);
    }
    if (!ok) ++failures;
    if (set.count > max_count) max_count = set.count;
    fnv_add(h, ok ? 1 : 0);
    fnv_add(h, size(&set));
    for (std::uint32_t k = 0; k < size(&set); ++k) {
      Span sp;
      REQUIRE(get(&set, k, &sp));
      fnv_add(h, static_cast<std::uint64_t>(static_cast<std::int64_t>(sp.begin)));
      fnv_add(h, static_cast<std::uint64_t>(static_cast<std::int64_t>(sp.end)));
    }
    std::uint64_t points = 0;
    for (std::int32_t x = -40; x < 90; ++x) points += contains(&set, x) ? 1 : 0;
    fnv_add(h, points);
    REQUIRE(total_length(&set) == points);
  }
  return h;
}

void build(SpanSet& s, std::initializer_list<std::pair<int, int>> list) {
  clear(&s);
  for (auto p : list) REQUIRE(add(&s, p.first, p.second));
}

bool same(const SpanSet& s, std::initializer_list<std::pair<int, int>> list) {
  if (size(&s) != list.size()) return false;
  std::uint32_t i = 0;
  for (auto p : list) {
    Span sp;
    if (!get(&s, i++, &sp)) return false;
    if (sp.begin != p.first || sp.end != p.second) return false;
  }
  return true;
}

// 16 separate spans [0,1) [2,3) ... [30,31)
void fill16(SpanSet& s) {
  clear(&s);
  for (int i = 0; i < 16; ++i) REQUIRE(add(&s, 2 * i, 2 * i + 1));
  REQUIRE(size(&s) == 16);
}

bool unchanged16(const SpanSet& s) {
  if (s.count != 16) return false;
  for (int i = 0; i < 16; ++i) {
    if (s.s[i].begin != 2 * i || s.s[i].end != 2 * i + 1) return false;
  }
  return true;
}

}  // namespace

TEST_CASE("add orders, merges overlapping and touching spans", "[spans]") {
  SpanSet s;
  build(s, {{10, 12}, {0, 2}, {20, 25}});
  CHECK(same(s, {{0, 2}, {10, 12}, {20, 25}}));
  REQUIRE(add(&s, 2, 4));  // touches [0,2)
  CHECK(same(s, {{0, 4}, {10, 12}, {20, 25}}));
  REQUIRE(add(&s, 5, 6));  // a gap of one integer stays separate
  CHECK(same(s, {{0, 4}, {5, 6}, {10, 12}, {20, 25}}));
  REQUIRE(add(&s, 3, 21));  // bridges everything from 0 to 25
  CHECK(same(s, {{0, 25}}));
  REQUIRE(add(&s, 5, 7));  // already covered
  CHECK(same(s, {{0, 25}}));
  REQUIRE(add(&s, -5, 0));  // touches at the left
  CHECK(same(s, {{-5, 25}}));
  REQUIRE(add(&s, 25, 30));  // touches at the right
  CHECK(same(s, {{-5, 30}}));
}

TEST_CASE("empty intervals succeed and change nothing", "[spans]") {
  SpanSet s;
  build(s, {{1, 3}});
  CHECK(add(&s, 5, 5));
  CHECK(add(&s, 9, 4));
  CHECK(remove(&s, 2, 2));
  CHECK(remove(&s, 8, -8));
  CHECK(same(s, {{1, 3}}));
}

TEST_CASE("remove cuts, splits and deletes", "[spans]") {
  SpanSet s;
  build(s, {{0, 10}, {20, 30}});
  REQUIRE(remove(&s, 3, 5));  // strictly inside: split
  CHECK(same(s, {{0, 3}, {5, 10}, {20, 30}}));
  REQUIRE(remove(&s, -5, 2));  // cuts the left part
  CHECK(same(s, {{2, 3}, {5, 10}, {20, 30}}));
  REQUIRE(remove(&s, 8, 25));  // cuts the right of one and the left of another
  CHECK(same(s, {{2, 3}, {5, 8}, {25, 30}}));
  REQUIRE(remove(&s, 5, 8));  // exact match
  CHECK(same(s, {{2, 3}, {25, 30}}));
  REQUIRE(remove(&s, 3, 25));  // touches only: nothing changes
  CHECK(same(s, {{2, 3}, {25, 30}}));
  REQUIRE(remove(&s, 0, 100));
  CHECK(size(&s) == 0);
}

TEST_CASE("contains uses a half-open interval", "[spans]") {
  SpanSet s;
  build(s, {{4, 7}, {10, 11}});
  CHECK_FALSE(contains(&s, 3));
  CHECK(contains(&s, 4));
  CHECK(contains(&s, 6));
  CHECK_FALSE(contains(&s, 7));
  CHECK_FALSE(contains(&s, 9));
  CHECK(contains(&s, 10));
  CHECK_FALSE(contains(&s, 11));
  CHECK(total_length(&s) == 4);
}

TEST_CASE("a full set refuses a new separate span and stays unchanged", "[spans]") {
  SpanSet s;
  fill16(s);
  CHECK_FALSE(add(&s, 100, 101));
  CHECK(unchanged16(s));
  CHECK_FALSE(add(&s, -50, -49));
  CHECK(unchanged16(s));
  CHECK(add(&s, 31, 33));  // touches the last span: merges, so it succeeds
  CHECK(s.count == 16);
  CHECK(s.s[15].begin == 30);
  CHECK(s.s[15].end == 33);
}

TEST_CASE("a full set merges without needing a slot", "[spans]") {
  SpanSet s;
  fill16(s);
  REQUIRE(add(&s, 1, 2));  // joins [0,1) and [2,3)
  CHECK(s.count == 15);
  CHECK(s.s[0].begin == 0);
  CHECK(s.s[0].end == 3);
  REQUIRE(add(&s, 100, 101));  // now there is room
  CHECK(s.count == 16);
}

TEST_CASE("remove that would exceed the slots fails and leaves the set unchanged", "[spans]") {
  SpanSet s;
  clear(&s);
  for (int i = 0; i < 15; ++i) REQUIRE(add(&s, 10 * i, 10 * i + 2));
  REQUIRE(add(&s, 200, 210));
  REQUIRE(size(&s) == 16);
  const SpanSet before = s;
  CHECK_FALSE(remove(&s, 203, 205));  // split of the last span needs a 17th slot
  CHECK(s.count == before.count);
  for (std::uint32_t i = 0; i < 16; ++i) {
    CHECK(s.s[i].begin == before.s[i].begin);
    CHECK(s.s[i].end == before.s[i].end);
  }
  CHECK(remove(&s, 200, 205));  // a cut from the left needs no slot
  CHECK(s.s[15].begin == 205);
  CHECK(remove(&s, 0, 2));  // deleting a span frees a slot
  CHECK(s.count == 15);
  CHECK(remove(&s, 207, 208));  // now a split fits
  CHECK(s.count == 16);
}

TEST_CASE("extreme coordinates", "[spans]") {
  SpanSet s;
  constexpr std::int32_t kMin = INT32_MIN;
  constexpr std::int32_t kMax = INT32_MAX;
  REQUIRE(add(&s, kMin, kMax));
  CHECK(total_length(&s) == 4294967295ULL);
  CHECK(contains(&s, kMin));
  CHECK(contains(&s, kMax - 1));
  CHECK_FALSE(contains(&s, kMax));
  REQUIRE(remove(&s, -1, 1));
  CHECK(same(s, {{kMin, -1}, {1, kMax}}));
  CHECK(total_length(&s) == 4294967295ULL - 2);
  REQUIRE(add(&s, -1, 1));
  CHECK(same(s, {{kMin, kMax}}));
}

TEST_CASE("null and corrupt sets", "[spans]") {
  SpanSet s;
  build(s, {{1, 2}});
  Span out{7, 8};
  CHECK_FALSE(add(nullptr, 0, 1));
  CHECK_FALSE(remove(nullptr, 0, 1));
  CHECK_FALSE(contains(nullptr, 1));
  CHECK(total_length(nullptr) == 0);
  CHECK(size(nullptr) == 0);
  CHECK_FALSE(get(nullptr, 0, &out));
  CHECK_FALSE(get(&s, 0, nullptr));
  CHECK_FALSE(get(&s, 1, &out));
  CHECK(out.begin == 7);
  CHECK(out.end == 8);
  clear(nullptr);
  for (std::uint32_t bad : {17u, 100u, 0xFFFFFFFFu}) {
    s.count = bad;
    CHECK_FALSE(add(&s, 0, 1));
    CHECK_FALSE(remove(&s, 0, 1));
    CHECK_FALSE(add(&s, 5, 5));
    CHECK_FALSE(remove(&s, 5, 5));
    CHECK_FALSE(contains(&s, 1));
    CHECK(total_length(&s) == 0);
    CHECK(size(&s) == 0);
    CHECK_FALSE(get(&s, 0, &out));
    CHECK(s.count == bad);
  }
  clear(&s);  // repairs
  CHECK(s.count == 0);
  CHECK(add(&s, 0, 1));
}

TEST_CASE("get returns every used span", "[spans]") {
  SpanSet s;
  build(s, {{0, 1}, {3, 5}, {8, 9}});
  Span sp;
  for (std::uint32_t i = 0; i < 3; ++i) CHECK(get(&s, i, &sp));
  CHECK_FALSE(get(&s, 3, &sp));
  CHECK(get(&s, 2, &sp));
  CHECK(sp.begin == 8);
  CHECK(sp.end == 9);
}

TEST_CASE("long streams match the independent point-set model", "[spans]") {
  int failures_total = 0;
  std::uint32_t max_count = 0;
  for (const Case& c : kCases) {
    int failures = 0;
    CHECK(run_stream(c.seed, 500, failures, max_count) == c.digest);
    failures_total += failures;
  }
  CHECK(max_count == 16);
  CHECK(failures_total > 0);
}
