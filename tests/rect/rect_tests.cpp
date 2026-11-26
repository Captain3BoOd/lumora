// Tests for core/rect. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/rect_ref.py, an independent Python implementation (pixel-set
// model checked first, then explicit edge arithmetic). Regenerate it with: python3 tools/rect_ref.py
// The tests also carry naive oracles that share no code with core/rect/rect.cpp: a pixel grid for small coordinates and an
// edge-by-edge int64 oracle for large ones.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "rect.h"

using namespace lumora::rect;

namespace {

// BEGIN GENERATED REFERENCE (tools/rect_ref.py)
const std::int32_t kRefD1[][2] = {{0, 0}, {-1, 2}, {5, 3}, {INT32_MIN, INT32_MAX}, {-1, INT32_MAX}, {2147483646, 1}, {1, INT32_MAX}, {3, -1}, {INT32_MIN, 1}};
const std::int32_t kRefPoints[] = {INT32_MIN, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 2147483645, 2147483646, INT32_MAX};
const std::size_t kRefSmall = 20000;
const std::size_t kRefBig = 20000;
const std::size_t kRefEdge = 20000;
const std::uint64_t kRefDigest_intersect = 0x03F3E6ABD6F4197Full;
const std::size_t kRefCases_intersect = 66561;
const std::uint64_t kRefDigest_union = 0xD6755622F2EDFA00ull;
const std::size_t kRefCases_union = 66561;
const std::uint64_t kRefDigest_point = 0x274108609F33E6DFull;
const std::size_t kRefCases_point = 42225;
const std::uint64_t kRefDigest_area = 0xA6014A19BAD3B9E5ull;
const std::size_t kRefCases_area = 6081;
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
  void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
  void rect(const Rect& r) {
    i32(r.x);
    i32(r.y);
    i32(r.w);
    i32(r.h);
  }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t u32() {
    x = x * kLcgMul + kLcgAdd;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

std::int32_t wrap32(std::uint32_t v) { return static_cast<std::int32_t>(v); }

Rect mk(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h) {
  Rect r;
  r.x = x;
  r.y = y;
  r.w = w;
  r.h = h;
  return r;
}

bool same(const Rect& a, const Rect& b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

std::vector<Rect> boundary_rects() {
  std::vector<Rect> v;
  for (const auto& xw : kRefD1)
    for (const auto& yh : kRefD1) v.push_back(mk(xw[0], yh[0], xw[1], yh[1]));
  return v;
}

Rect rand_small(Lcg& g) {
  const std::int32_t x = static_cast<std::int32_t>(g.u32() % 21u) - 10;
  const std::int32_t y = static_cast<std::int32_t>(g.u32() % 21u) - 10;
  const std::int32_t w = static_cast<std::int32_t>(g.u32() % 9u) - 2;
  const std::int32_t h = static_cast<std::int32_t>(g.u32() % 9u) - 2;
  return mk(x, y, w, h);
}

Rect rand_big(Lcg& g) {
  const std::int32_t x = wrap32(g.u32());
  const std::int32_t y = wrap32(g.u32());
  std::uint32_t d = g.u32();
  std::uint32_t e = g.u32();
  const std::int32_t w = wrap32(d >> (e % 32u));
  d = g.u32();
  e = g.u32();
  const std::int32_t h = wrap32(d >> (e % 32u));
  return mk(x, y, w, h);
}

Rect rand_edge(Lcg& g) {
  const std::int64_t w = g.u32() % 40u;
  const std::int64_t h = g.u32() % 40u;
  const std::int64_t x = INT32_MAX - w + static_cast<std::int64_t>(g.u32() % 5u) - 2;
  const std::int64_t y = INT32_MAX - h + static_cast<std::int64_t>(g.u32() % 5u) - 2;
  return mk(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), static_cast<std::int32_t>(w), static_cast<std::int32_t>(h));
}

// ---- oracle for large coordinates: edges by explicit comparisons ----------------------------------------------------------------
struct Edges {
  bool ok;  // non-empty
  long long l, t, r, b;
};

Edges edges(const Rect& q) {
  Edges e;
  e.l = q.x;
  e.t = q.y;
  e.r = static_cast<long long>(q.x) + q.w;
  e.b = static_cast<long long>(q.y) + q.h;
  const long long limit = 2147483647ll;
  const bool valid = !(q.w < 0 || q.h < 0 || e.r > limit || e.b > limit);
  e.ok = valid && q.w != 0 && q.h != 0;
  return e;
}

bool o_intersect(const Rect& a, const Rect& b, Rect* out) {  // false: zeros
  const Edges p = edges(a), q = edges(b);
  *out = Rect{};
  if (!p.ok || !q.ok) return false;
  long long l = p.l, t = p.t, r = p.r, bt = p.b;
  if (q.l > l) l = q.l;
  if (q.t > t) t = q.t;
  if (q.r < r) r = q.r;
  if (q.b < bt) bt = q.b;
  if (!(l < r && t < bt)) return false;
  *out = mk(static_cast<std::int32_t>(l), static_cast<std::int32_t>(t), static_cast<std::int32_t>(r - l), static_cast<std::int32_t>(bt - t));
  return true;
}

// returns false when the union must fail
bool o_union(const Rect& a, const Rect& b, Rect* out) {
  const Edges p = edges(a), q = edges(b);
  if (!p.ok && !q.ok) {
    *out = Rect{};
    return true;
  }
  if (!p.ok) {
    *out = b;
    return true;
  }
  if (!q.ok) {
    *out = a;
    return true;
  }
  long long l = p.l, t = p.t, r = p.r, bt = p.b;
  if (q.l < l) l = q.l;
  if (q.t < t) t = q.t;
  if (q.r > r) r = q.r;
  if (q.b > bt) bt = q.b;
  if (r - l > 2147483647ll || bt - t > 2147483647ll) return false;
  *out = mk(static_cast<std::int32_t>(l), static_cast<std::int32_t>(t), static_cast<std::int32_t>(r - l), static_cast<std::int32_t>(bt - t));
  return true;
}

// ---- oracle for small coordinates: a pixel grid ---------------------------------------------------------------------------------
constexpr int kLo = -16, kN = 40;  // grid covers x, y in [-16, 24)

struct Grid {
  bool p[kN][kN];
};

Grid pixels(const Rect& q) {
  Grid g;
  const bool bad = q.w <= 0 || q.h <= 0;
  for (int i = 0; i < kN; ++i)
    for (int j = 0; j < kN; ++j) g.p[i][j] = !bad && (kLo + i) >= q.x && (kLo + i) < q.x + q.w && (kLo + j) >= q.y && (kLo + j) < q.y + q.h;
  return g;
}

bool box(const Grid& g, Rect* out) {  // bounding box of the set pixels
  int x0 = kN, x1 = -1, y0 = kN, y1 = -1;
  for (int i = 0; i < kN; ++i)
    for (int j = 0; j < kN; ++j)
      if (g.p[i][j]) {
        if (i < x0) x0 = i;
        if (i > x1) x1 = i;
        if (j < y0) y0 = j;
        if (j > y1) y1 = j;
      }
  if (x1 < 0) {
    *out = Rect{};
    return false;
  }
  *out = mk(kLo + x0, kLo + y0, x1 - x0 + 1, y1 - y0 + 1);
  return true;
}

constexpr Rect kSentinel = {-777, 888, -999, 555};

}  // namespace

TEST_CASE("defaults", "[rect]") {
  const Rect r;
  CHECK(r.x == 0);
  CHECK(r.y == 0);
  CHECK(r.w == 0);
  CHECK(r.h == 0);
  CHECK(is_valid(r));
  CHECK(is_empty(r));
}

TEST_CASE("validity edges", "[rect][valid]") {
  CHECK(is_valid(mk(0, 0, 1, 1)));
  CHECK(is_valid(mk(-5, -7, 3, 2)));
  CHECK(is_valid(mk(0, 0, 0, 0)));
  CHECK(is_valid(mk(INT32_MAX, 0, 0, 5)));  // zero width at the edge: right == INT32_MAX
  CHECK(is_valid(mk(0, 0, INT32_MAX, 1)));  // right == INT32_MAX
  CHECK(is_valid(mk(INT32_MAX - 1, 0, 1, 1)));
  CHECK(is_valid(mk(0, 0, 1, INT32_MAX)));  // bottom == INT32_MAX
  CHECK(is_valid(mk(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX)));
  CHECK(is_valid(mk(INT32_MIN, 0, 0, 0)));
  CHECK_FALSE(is_valid(mk(1, 0, INT32_MAX, 1)));  // right == INT32_MAX + 1
  CHECK_FALSE(is_valid(mk(0, 1, 1, INT32_MAX)));  // bottom == INT32_MAX + 1
  CHECK_FALSE(is_valid(mk(INT32_MAX, 0, 1, 1)));
  CHECK_FALSE(is_valid(mk(0, INT32_MAX, 1, 1)));
  CHECK_FALSE(is_valid(mk(0, 0, -1, 1)));
  CHECK_FALSE(is_valid(mk(0, 0, 1, -1)));
  CHECK_FALSE(is_valid(mk(0, 0, INT32_MIN, 1)));
  CHECK_FALSE(is_valid(mk(0, 0, 1, INT32_MIN)));
  CHECK_FALSE(is_valid(mk(5, 5, -1, -1)));
  // a negative width is invalid even when x + w would be tiny
  CHECK_FALSE(is_valid(mk(INT32_MIN, 0, -1, 1)));
}

TEST_CASE("emptiness", "[rect][valid]") {
  CHECK(is_empty(mk(3, 3, 0, 5)));
  CHECK(is_empty(mk(3, 3, 5, 0)));
  CHECK(is_empty(mk(3, 3, 0, 0)));
  CHECK_FALSE(is_empty(mk(3, 3, 1, 1)));
  CHECK_FALSE(is_empty(mk(0, 0, INT32_MAX, INT32_MAX)));
  CHECK(is_empty(mk(0, 0, -1, 5)));  // invalid is empty
  CHECK(is_empty(mk(1, 1, INT32_MAX, 1)));
  CHECK(is_empty(mk(INT32_MAX, 0, 1, 1)));
  CHECK_FALSE(is_empty(mk(INT32_MIN, INT32_MIN, 1, 1)));
}

TEST_CASE("intersect known cases", "[rect][intersect]") {
  CHECK(same(intersect(mk(0, 0, 10, 10), mk(5, 5, 10, 10)), mk(5, 5, 5, 5)));
  CHECK(same(intersect(mk(5, 5, 10, 10), mk(0, 0, 10, 10)), mk(5, 5, 5, 5)));
  CHECK(same(intersect(mk(0, 0, 10, 10), mk(2, 3, 2, 2)), mk(2, 3, 2, 2)));  // contained
  CHECK(same(intersect(mk(0, 0, 10, 10), mk(0, 0, 10, 10)), mk(0, 0, 10, 10)));
  CHECK(same(intersect(mk(0, 0, 10, 10), mk(9, 9, 5, 5)), mk(9, 9, 1, 1)));  // one shared pixel
  CHECK(same(intersect(mk(-5, -5, 10, 10), mk(-2, -8, 4, 5)), mk(-2, -5, 4, 2)));
  // touching edges do not overlap
  CHECK(same(intersect(mk(0, 0, 10, 10), mk(10, 0, 5, 5)), Rect{}));
  CHECK(same(intersect(mk(0, 0, 10, 10), mk(0, 10, 5, 5)), Rect{}));
  CHECK(same(intersect(mk(10, 0, 5, 5), mk(0, 0, 10, 10)), Rect{}));
  // overlapping in x only
  CHECK(same(intersect(mk(0, 0, 10, 5), mk(2, 5, 4, 4)), Rect{}));
  // an empty or invalid input gives all zeros, even when the other is valid
  CHECK(same(intersect(mk(0, 0, 0, 10), mk(0, 0, 10, 10)), Rect{}));
  CHECK(same(intersect(mk(3, 3, 5, 5), mk(4, 4, 0, 0)), Rect{}));
  CHECK(same(intersect(mk(0, 0, -1, 10), mk(0, 0, 10, 10)), Rect{}));
  CHECK(same(intersect(mk(0, 0, 10, 10), mk(1, 1, INT32_MAX, 1)), Rect{}));
  CHECK(same(intersect(mk(7, 7, 0, 0), mk(7, 7, 0, 0)), Rect{}));
  // large coordinates
  CHECK(same(intersect(mk(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX), mk(-5, -5, 9, 9)), mk(-5, -5, 4, 4)));  // right edge is -1
  CHECK(same(intersect(mk(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX), mk(-1, -1, 2, 2)), Rect{}));
  CHECK(same(intersect(mk(INT32_MAX - 3, 0, 3, 1), mk(INT32_MAX - 2, 0, 2, 1)), mk(INT32_MAX - 2, 0, 2, 1)));
  CHECK(same(intersect(mk(0, 0, INT32_MAX, INT32_MAX), mk(INT32_MAX - 1, INT32_MAX - 1, 1, 1)), mk(INT32_MAX - 1, INT32_MAX - 1, 1, 1)));
  CHECK(same(intersect(mk(INT32_MIN, 0, 1, 1), mk(INT32_MIN, 0, 1, 1)), mk(INT32_MIN, 0, 1, 1)));
}

TEST_CASE("overlaps", "[rect][intersect]") {
  CHECK(overlaps(mk(0, 0, 10, 10), mk(9, 9, 1, 1)));
  CHECK_FALSE(overlaps(mk(0, 0, 10, 10), mk(10, 10, 1, 1)));
  CHECK_FALSE(overlaps(mk(0, 0, 10, 10), mk(10, 0, 1, 1)));
  CHECK_FALSE(overlaps(mk(0, 0, 10, 10), mk(0, 10, 1, 1)));
  CHECK_FALSE(overlaps(mk(0, 0, 0, 0), mk(0, 0, 10, 10)));
  CHECK_FALSE(overlaps(mk(0, 0, 10, 10), mk(0, 0, -3, 10)));
  CHECK_FALSE(overlaps(mk(0, 0, 10, 10), mk(1, 0, INT32_MAX, 1)));
  CHECK(overlaps(mk(0, 0, 10, 10), mk(0, 0, 10, 10)));
  CHECK(overlaps(mk(INT32_MIN, INT32_MIN, 1, 1), mk(INT32_MIN, INT32_MIN, 1, 1)));
  CHECK_FALSE(overlaps(mk(INT32_MIN, INT32_MIN, 1, 1), mk(INT32_MIN + 1, INT32_MIN, 1, 1)));
}

TEST_CASE("union known cases", "[rect][union]") {
  Rect out = kSentinel;
  REQUIRE(bounds_union(mk(0, 0, 2, 2), mk(5, 5, 2, 2), &out));
  CHECK(same(out, mk(0, 0, 7, 7)));
  REQUIRE(bounds_union(mk(5, 5, 2, 2), mk(0, 0, 2, 2), &out));
  CHECK(same(out, mk(0, 0, 7, 7)));
  REQUIRE(bounds_union(mk(-4, 1, 2, 2), mk(3, -6, 1, 1), &out));
  CHECK(same(out, mk(-4, -6, 8, 9)));
  REQUIRE(bounds_union(mk(0, 0, 10, 10), mk(2, 2, 2, 2), &out));  // contained
  CHECK(same(out, mk(0, 0, 10, 10)));
  // empty inputs are ignored
  REQUIRE(bounds_union(mk(0, 0, 0, 0), mk(5, 5, 2, 2), &out));
  CHECK(same(out, mk(5, 5, 2, 2)));
  REQUIRE(bounds_union(mk(5, 5, 2, 2), mk(100, 100, 0, 7), &out));
  CHECK(same(out, mk(5, 5, 2, 2)));
  REQUIRE(bounds_union(mk(-3, 0, -1, 4), mk(5, 5, 2, 2), &out));  // invalid is ignored
  CHECK(same(out, mk(5, 5, 2, 2)));
  REQUIRE(bounds_union(mk(5, 5, 2, 2), mk(INT32_MAX, 0, 1, 1), &out));
  CHECK(same(out, mk(5, 5, 2, 2)));
  // both empty: all zeros and true, even with nonzero coordinates
  out = kSentinel;
  REQUIRE(bounds_union(mk(9, 9, 0, 3), mk(4, 4, 5, 0), &out));
  CHECK(same(out, Rect{}));
  out = kSentinel;
  REQUIRE(bounds_union(mk(9, 9, -1, 3), mk(INT32_MAX, 4, 5, 5), &out));
  CHECK(same(out, Rect{}));
  // a valid single rectangle is returned as is, whatever its size
  REQUIRE(bounds_union(mk(0, 0, 0, 0), mk(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX), &out));
  CHECK(same(out, mk(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX)));
}

TEST_CASE("union width and height limits", "[rect][union]") {
  Rect out = kSentinel;
  // width exactly INT32_MAX is accepted: [-1, INT32_MAX - 1)
  REQUIRE(bounds_union(mk(-1, 0, 1, 1), mk(INT32_MAX - 2, 0, 1, 1), &out));  // [-1, INT32_MAX - 1): width INT32_MAX
  CHECK(same(out, mk(-1, 0, INT32_MAX, 1)));
  out = kSentinel;
  CHECK_FALSE(bounds_union(mk(-1, 0, 1, 1), mk(INT32_MAX - 1, 0, 1, 1), &out));  // [-1, INT32_MAX): width INT32_MAX + 1
  CHECK(same(out, kSentinel));
  // same for height
  REQUIRE(bounds_union(mk(0, -1, 1, 1), mk(0, INT32_MAX - 2, 1, 1), &out));
  CHECK(same(out, mk(0, -1, 1, INT32_MAX)));
  out = kSentinel;
  CHECK_FALSE(bounds_union(mk(0, -1, 1, 1), mk(0, INT32_MAX - 1, 1, 1), &out));
  CHECK(same(out, kSentinel));
  // far apart, each edge fits in int32
  CHECK_FALSE(bounds_union(mk(INT32_MIN, 0, 1, 1), mk(INT32_MAX - 1, 0, 1, 1), &out));
  CHECK_FALSE(bounds_union(mk(0, INT32_MIN, 1, 1), mk(0, INT32_MAX - 1, 1, 1), &out));
  CHECK(same(out, kSentinel));
  // width fine, height too large: still false
  CHECK_FALSE(bounds_union(mk(0, -1, 1, 1), mk(5, INT32_MAX - 1, 1, 1), &out));
  CHECK(same(out, kSentinel));
  // a large union that does fit
  REQUIRE(bounds_union(mk(INT32_MIN, INT32_MIN, 1, 1), mk(-2, -2, 1, 1), &out));
  CHECK(same(out, mk(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX)));
}

TEST_CASE("union null out and unchanged on every false return", "[rect][union]") {
  CHECK_FALSE(bounds_union(mk(0, 0, 1, 1), mk(2, 2, 1, 1), nullptr));
  CHECK_FALSE(bounds_union(mk(0, 0, 0, 0), mk(0, 0, 0, 0), nullptr));
  CHECK_FALSE(bounds_union(mk(0, 0, 1, 1), mk(0, 0, 0, 0), nullptr));
  CHECK_FALSE(bounds_union(mk(0, 0, -1, 1), mk(0, 0, -2, 0), nullptr));
  Rect out = kSentinel;
  CHECK_FALSE(bounds_union(mk(INT32_MIN, 0, 1, 1), mk(INT32_MAX - 1, 0, 1, 1), &out));
  CHECK(same(out, kSentinel));
}

TEST_CASE("union with out aliasing a or b", "[rect][union][alias]") {
  Rect a = mk(0, 0, 2, 2);
  const Rect b = mk(5, 5, 2, 2);
  REQUIRE(bounds_union(a, b, &a));
  CHECK(same(a, mk(0, 0, 7, 7)));
  Rect a2 = mk(0, 0, 2, 2);
  Rect b2 = mk(5, 5, 2, 2);
  REQUIRE(bounds_union(a2, b2, &b2));
  CHECK(same(b2, mk(0, 0, 7, 7)));
  CHECK(same(a2, mk(0, 0, 2, 2)));
  // aliasing with an empty input, with both inputs the same object, and on a failure
  Rect e = mk(3, 3, 0, 0);
  const Rect c = mk(1, 1, 4, 4);
  REQUIRE(bounds_union(e, c, &e));
  CHECK(same(e, c));
  Rect f = mk(1, 1, 4, 4);
  REQUIRE(bounds_union(f, mk(3, 3, 0, 0), &f));
  CHECK(same(f, mk(1, 1, 4, 4)));
  Rect g = mk(2, 2, 3, 3);
  REQUIRE(bounds_union(g, g, &g));
  CHECK(same(g, mk(2, 2, 3, 3)));
  Rect h = mk(3, 3, 0, 0);
  REQUIRE(bounds_union(h, h, &h));
  CHECK(same(h, Rect{}));
  Rect far1 = mk(INT32_MIN, 0, 1, 1);
  const Rect far2 = mk(INT32_MAX - 1, 0, 1, 1);
  CHECK_FALSE(bounds_union(far1, far2, &far1));
  CHECK(same(far1, mk(INT32_MIN, 0, 1, 1)));
  Rect far3 = mk(INT32_MAX - 1, 0, 1, 1);
  CHECK_FALSE(bounds_union(mk(INT32_MIN, 0, 1, 1), far3, &far3));
  CHECK(same(far3, mk(INT32_MAX - 1, 0, 1, 1)));
  // an invalid input that is also the output is replaced by the other rectangle
  Rect bad = mk(0, 0, -4, 2);
  REQUIRE(bounds_union(bad, c, &bad));
  CHECK(same(bad, c));
}

TEST_CASE("contains_point", "[rect][point]") {
  const Rect r = mk(2, 3, 4, 5);  // x 2..5, y 3..7
  CHECK(contains_point(r, 2, 3));
  CHECK(contains_point(r, 5, 7));
  CHECK(contains_point(r, 3, 5));
  CHECK_FALSE(contains_point(r, 1, 3));
  CHECK_FALSE(contains_point(r, 2, 2));
  CHECK_FALSE(contains_point(r, 6, 5));  // x + w is outside
  CHECK_FALSE(contains_point(r, 3, 8));  // y + h is outside
  CHECK_FALSE(contains_point(r, 6, 8));
  CHECK_FALSE(contains_point(r, 1, 2));
  // empty and invalid: never
  CHECK_FALSE(contains_point(mk(2, 3, 0, 5), 2, 3));
  CHECK_FALSE(contains_point(mk(2, 3, 5, 0), 2, 3));
  CHECK_FALSE(contains_point(mk(2, 3, -1, 5), 2, 3));
  CHECK_FALSE(contains_point(mk(2, 3, 5, -1), 2, 2));
  CHECK_FALSE(contains_point(mk(0, 0, INT32_MAX, 1), INT32_MAX, 0));
  CHECK_FALSE(contains_point(mk(1, 0, INT32_MAX, 1), 1, 0));  // invalid: right edge INT32_MAX + 1
  CHECK_FALSE(contains_point(mk(1, 0, INT32_MAX, 1), INT32_MAX, 0));
  // 64-bit comparison near the limits
  CHECK(contains_point(mk(0, 0, INT32_MAX, INT32_MAX), INT32_MAX - 1, INT32_MAX - 1));
  CHECK_FALSE(contains_point(mk(0, 0, INT32_MAX, INT32_MAX), INT32_MAX, 0));
  CHECK_FALSE(contains_point(mk(0, 0, INT32_MAX, INT32_MAX), 0, INT32_MAX));
  CHECK(contains_point(mk(INT32_MIN, INT32_MIN, 2, 2), INT32_MIN, INT32_MIN));
  CHECK(contains_point(mk(INT32_MIN, INT32_MIN, 2, 2), INT32_MIN + 1, INT32_MIN + 1));
  CHECK_FALSE(contains_point(mk(INT32_MIN, INT32_MIN, 2, 2), INT32_MIN + 2, INT32_MIN));
  CHECK_FALSE(contains_point(mk(INT32_MIN, INT32_MIN, 2, 2), INT32_MAX, INT32_MAX));
  CHECK(contains_point(mk(INT32_MAX - 1, INT32_MAX - 1, 1, 1), INT32_MAX - 1, INT32_MAX - 1));
  CHECK_FALSE(contains_point(mk(INT32_MAX - 1, INT32_MAX - 1, 1, 1), INT32_MAX, INT32_MAX));
}

TEST_CASE("area", "[rect][area]") {
  CHECK(area(mk(0, 0, 3, 4)) == 12u);
  CHECK(area(mk(-9, -9, 3, 4)) == 12u);
  CHECK(area(mk(0, 0, 1, 1)) == 1u);
  CHECK(area(mk(0, 0, 0, 5)) == 0u);
  CHECK(area(mk(0, 0, 5, 0)) == 0u);
  CHECK(area(mk(0, 0, -3, 4)) == 0u);
  CHECK(area(mk(0, 0, 3, -4)) == 0u);
  CHECK(area(mk(1, 0, INT32_MAX, 3)) == 0u);  // invalid
  CHECK(area(mk(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX)) == 4611686014132420609ull);  // (2^31 - 1)^2
  CHECK(area(mk(0, 0, INT32_MAX, 2)) == 4294967294ull);
  CHECK(area(mk(0, 0, 65536, 65536)) == 4294967296ull);  // above 32 bits
  CHECK(area(mk(0, 0, 65537, 65537)) == 4295098369ull);
}

TEST_CASE("pixel grid oracle for small rectangles", "[rect][oracle]") {
  Lcg g(7);
  std::size_t bad = 0, nonempty_cut = 0;
  for (int i = 0; i < 6000; ++i) {
    const Rect a = rand_small(g), b = rand_small(g);
    const Grid pa = pixels(a), pb = pixels(b);
    Grid both = pa, either = pa;
    for (int x = 0; x < kN; ++x)
      for (int y = 0; y < kN; ++y) {
        both.p[x][y] = pa.p[x][y] && pb.p[x][y];
        either.p[x][y] = pa.p[x][y] || pb.p[x][y];
      }
    Rect want;
    const bool any = box(both, &want);
    if (any) ++nonempty_cut;
    if (!same(intersect(a, b), want) || overlaps(a, b) != any) ++bad;
    box(either, &want);
    Rect got = kSentinel;
    if (!bounds_union(a, b, &got) || !same(got, want)) ++bad;
    // point and area against the grid
    for (int x = 0; x < kN; x += 3)
      for (int y = 0; y < kN; y += 5)
        if (contains_point(a, kLo + x, kLo + y) != pa.p[x][y]) ++bad;
    std::uint64_t count = 0;
    for (int x = 0; x < kN; ++x)
      for (int y = 0; y < kN; ++y) count += pa.p[x][y] ? 1u : 0u;
    if (area(a) != count) ++bad;
  }
  CHECK(bad == 0);
  CHECK(nonempty_cut > 50);  // the overlap path is exercised
}

TEST_CASE("edge oracle for large rectangles", "[rect][oracle]") {
  Lcg g(9);
  std::size_t bad = 0, union_false = 0, union_true = 0, overlapped = 0;
  for (int i = 0; i < 30000; ++i) {
    Rect a, b;
    if (i % 3 == 0) {
      a = rand_big(g);
      b = rand_big(g);
    } else if (i % 3 == 1) {
      a = rand_edge(g);
      b = rand_edge(g);
    } else {
      a = rand_big(g);
      b = rand_edge(g);
    }
    Rect want;
    const bool ov = o_intersect(a, b, &want);
    if (ov) ++overlapped;
    if (!same(intersect(a, b), want) || overlaps(a, b) != ov) ++bad;
    const bool ok = o_union(a, b, &want);
    Rect got = kSentinel;
    const bool gok = bounds_union(a, b, &got);
    if (gok != ok) ++bad;
    if (ok ? !same(got, want) : !same(got, kSentinel)) ++bad;
    ok ? ++union_true : ++union_false;
    const Edges ea = edges(a);
    if (is_empty(a) != !ea.ok) ++bad;
    if (is_valid(a) != (a.w >= 0 && a.h >= 0 && ea.r <= 2147483647ll && ea.b <= 2147483647ll)) ++bad;
    if (!ea.ok) {
      if (area(a) != 0u) ++bad;
    } else if (area(a) != static_cast<std::uint64_t>(ea.r - ea.l) * static_cast<std::uint64_t>(ea.b - ea.t)) {
      ++bad;
    }
  }
  CHECK(bad == 0);
  CHECK(union_false > 100);
  CHECK(union_true > 100);
  CHECK(overlapped > 100);
}

TEST_CASE("invalid acts as empty in every function", "[rect][valid]") {
  const Rect bads[] = {mk(0, 0, -1, 5), mk(0, 0, 5, -1), mk(1, 0, INT32_MAX, 1), mk(0, 1, 1, INT32_MAX), mk(INT32_MAX, INT32_MAX, 1, 1), mk(3, 3, INT32_MIN, 3)};
  const Rect good = mk(0, 0, 10, 10);
  for (const Rect& b : bads) {
    CHECK(is_empty(b));
    CHECK_FALSE(is_valid(b));
    CHECK(same(intersect(b, good), Rect{}));
    CHECK(same(intersect(good, b), Rect{}));
    CHECK_FALSE(overlaps(b, good));
    CHECK_FALSE(overlaps(good, b));
    CHECK_FALSE(contains_point(b, b.x, b.y));
    CHECK(area(b) == 0u);
    Rect out = kSentinel;
    REQUIRE(bounds_union(b, good, &out));
    CHECK(same(out, good));
    REQUIRE(bounds_union(good, b, &out));
    CHECK(same(out, good));
    REQUIRE(bounds_union(b, b, &out));
    CHECK(same(out, Rect{}));
  }
}

TEST_CASE("intersect and union digests", "[rect][sweep]") {
  Fnv fi, fu;
  std::size_t n = 0;
  const std::vector<Rect> list = boundary_rects();
  const auto feed = [&](const Rect& a, const Rect& b) {
    fi.rect(intersect(a, b));
    fi.byte(overlaps(a, b) ? 1 : 0);
    Rect out = kSentinel;
    const bool ok = bounds_union(a, b, &out);
    fu.byte(ok ? 1 : 0);
    fu.rect(ok ? out : Rect{});
    ++n;
  };
  for (const Rect& a : list)
    for (const Rect& b : list) feed(a, b);
  Lcg g(41);
  for (std::size_t i = 0; i < kRefSmall; ++i) {
    const Rect a = rand_small(g);
    feed(a, rand_small(g));
  }
  for (std::size_t i = 0; i < kRefBig; ++i) {
    const Rect a = rand_big(g);
    feed(a, rand_big(g));
  }
  for (std::size_t i = 0; i < kRefEdge; ++i) {
    const Rect a = rand_edge(g);
    feed(a, rand_edge(g));
  }
  CHECK(n == kRefCases_intersect);
  CHECK(n == kRefCases_union);
  CHECK(fi.h == kRefDigest_intersect);
  CHECK(fu.h == kRefDigest_union);
}

TEST_CASE("point and area digests", "[rect][sweep]") {
  Fnv fp, fa;
  std::size_t np = 0, nr = 0;
  const std::vector<Rect> list = boundary_rects();
  for (const Rect& a : list)
    for (const std::int32_t px : kRefPoints)
      for (const std::int32_t py : kRefPoints) {
        fp.byte(contains_point(a, px, py) ? 1 : 0);
        ++np;
      }
  Lcg g(42);
  std::vector<Rect> all = list;
  for (int cls = 0; cls < 3; ++cls)
    for (int i = 0; i < 2000; ++i) {
      const Rect r = cls == 0 ? rand_small(g) : (cls == 1 ? rand_big(g) : rand_edge(g));
      all.push_back(r);
      const std::uint32_t ux = static_cast<std::uint32_t>(r.x), uy = static_cast<std::uint32_t>(r.y);
      const std::uint32_t uw = static_cast<std::uint32_t>(r.w), uh = static_cast<std::uint32_t>(r.h);
      const std::int32_t pts[4][2] = {{wrap32(ux), wrap32(uy)},
                                      {wrap32(ux + uw), wrap32(uy)},
                                      {wrap32(ux + uw - 1u), wrap32(uy + uh - 1u)},
                                      {wrap32(ux - 1u), wrap32(uy)}};
      for (const auto& p : pts) {
        fp.byte(contains_point(r, p[0], p[1]) ? 1 : 0);
        ++np;
      }
    }
  for (const Rect& r : all) {
    fa.byte(is_valid(r) ? 1 : 0);
    fa.byte(is_empty(r) ? 1 : 0);
    fa.u64(area(r));
    ++nr;
  }
  CHECK(np == kRefCases_point);
  CHECK(nr == kRefCases_area);
  CHECK(fp.h == kRefDigest_point);
  CHECK(fa.h == kRefDigest_area);
}
