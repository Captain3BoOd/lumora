// Integer rectangles. See rect.h for the contract and docs/rect.md for the check order and the bounds. Edges are computed in 64 bits
// (std::int64_t), so no sum can overflow. No floating point, no libm, no allocation.
#include "rect.h"

#include <cstdint>

namespace lumora::rect {

namespace {

constexpr std::int64_t kMax = INT32_MAX;

std::int64_t right_of(const Rect& r) { return static_cast<std::int64_t>(r.x) + r.w; }
std::int64_t bottom_of(const Rect& r) { return static_cast<std::int64_t>(r.y) + r.h; }

}  // namespace

bool is_valid(const Rect& r) { return r.w >= 0 && r.h >= 0 && right_of(r) <= kMax && bottom_of(r) <= kMax; }

bool is_empty(const Rect& r) { return !is_valid(r) || r.w == 0 || r.h == 0; }

Rect intersect(const Rect& a, const Rect& b) {
  if (is_empty(a) || is_empty(b)) return Rect{};
  const std::int64_t left = a.x > b.x ? a.x : b.x;
  const std::int64_t top = a.y > b.y ? a.y : b.y;
  const std::int64_t ra = right_of(a), rb = right_of(b);
  const std::int64_t ba = bottom_of(a), bb = bottom_of(b);
  const std::int64_t right = ra < rb ? ra : rb;
  const std::int64_t bottom = ba < bb ? ba : bb;
  if (right <= left || bottom <= top) return Rect{};
  Rect out;
  out.x = static_cast<std::int32_t>(left);
  out.y = static_cast<std::int32_t>(top);
  out.w = static_cast<std::int32_t>(right - left);
  out.h = static_cast<std::int32_t>(bottom - top);
  return out;
}

bool overlaps(const Rect& a, const Rect& b) { return !is_empty(intersect(a, b)); }

bool bounds_union(const Rect& a, const Rect& b, Rect* out) {
  if (out == nullptr) return false;
  const bool empty_a = is_empty(a);
  const bool empty_b = is_empty(b);
  Rect result;
  if (empty_a && empty_b) {
    result = Rect{};
  } else if (empty_a) {
    result = b;
  } else if (empty_b) {
    result = a;
  } else {
    const std::int64_t left = a.x < b.x ? a.x : b.x;
    const std::int64_t top = a.y < b.y ? a.y : b.y;
    const std::int64_t ra = right_of(a), rb = right_of(b);
    const std::int64_t ba = bottom_of(a), bb = bottom_of(b);
    const std::int64_t right = ra > rb ? ra : rb;
    const std::int64_t bottom = ba > bb ? ba : bb;
    if (right - left > kMax || bottom - top > kMax) return false;
    result.x = static_cast<std::int32_t>(left);
    result.y = static_cast<std::int32_t>(top);
    result.w = static_cast<std::int32_t>(right - left);
    result.h = static_cast<std::int32_t>(bottom - top);
  }
  *out = result;  // single assignment at the end: out may alias a or b
  return true;
}

bool contains_point(const Rect& r, std::int32_t px, std::int32_t py) {
  if (is_empty(r)) return false;
  return px >= r.x && static_cast<std::int64_t>(px) < right_of(r) && py >= r.y && static_cast<std::int64_t>(py) < bottom_of(r);
}

std::uint64_t area(const Rect& r) {
  if (is_empty(r)) return 0;
  return static_cast<std::uint64_t>(r.w) * static_cast<std::uint64_t>(r.h);
}

}  // namespace lumora::rect
