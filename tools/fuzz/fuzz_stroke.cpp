// Fuzz target: stroke. Decodes bytes into a polyline and a style (invalid style values included) and calls stroke_polyline. A success
// must respect the bounds and give contours of 3 or 4 points that are counter-clockwise or degenerate (never clockwise by area).
// An error must leave all outputs untouched. Anything else aborts, so the fuzzer reports it as a crash.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "stroke.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::stroke;
  if (size < 5) return 0;
  const std::uint32_t count = 1 + data[0] % 12;
  Style st{};
  st.half_width = data[1] % 6 == 5 ? 0 : 1 + data[1] % 40;
  st.miter_limit = data[2] % 70;
  st.join = static_cast<Join>(data[3] % 3);
  st.cap = static_cast<Cap>((data[3] >> 2) % 3);
  st.closed = (data[4] & 1) != 0;
  std::size_t pos = 5;
  std::vector<Point> in(count);
  for (auto& p : in) {
    const std::int32_t x = pos < size ? static_cast<std::int8_t>(data[pos++]) : 0;
    const std::int32_t y = pos < size ? static_cast<std::int8_t>(data[pos++]) : 0;
    p = Point{x * 3, y * 3};
  }
  const std::uint32_t cb = contour_bound(count, st.closed), pb = point_bound(count, st.closed);
  std::vector<Point> pts(pb, Point{9, 9});
  std::vector<std::uint32_t> ends(cb, 0xABCDu);
  std::uint32_t nc = 0x1234, np = 0x5678;
  const Status s = stroke_polyline(in.data(), count, st, pts.data(), pb, ends.data(), cb, &nc, &np);
  if (s != Status::Ok) {
    if (nc != 0x1234 || np != 0x5678) std::abort();
    for (const Point& p : pts)
      if (p.x != 9 || p.y != 9) std::abort();
    for (std::uint32_t e : ends)
      if (e != 0xABCDu) std::abort();
    return 0;
  }
  if (nc > cb || np > pb) std::abort();
  std::uint32_t start = 0;
  for (std::uint32_t i = 0; i < nc; ++i) {
    const std::uint32_t end = ends[i];
    if (end < start + 3 || end > start + 4 || end > np) std::abort();
    std::int64_t area2 = 0;
    for (std::uint32_t k = start; k < end; ++k) {
      const Point& a = pts[k];
      const Point& b = pts[k + 1 < end ? k + 1 : start];
      area2 += static_cast<std::int64_t>(a.x) * b.y - static_cast<std::int64_t>(b.x) * a.y;
    }
    if (area2 < 0) std::abort();
    start = end;
  }
  if (digest_points(pts.data(), np) != digest_points(pts.data(), np)) std::abort();
  return 0;
}
