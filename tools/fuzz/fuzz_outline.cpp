// Fuzz target: outline. Decodes bytes into path commands (with invalid ops, extreme and garbage unused coordinates) and a tolerance, and
// calls build_outline. A success must respect the bounds, give contours of at least 2 points with no consecutive duplicates and no
// closed contour whose last point equals its first, and a stable digest. An error must leave all outputs untouched. Anything else
// aborts, so the fuzzer reports it as a crash.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "outline.h"

namespace {
std::int32_t coord(const std::uint8_t* d, std::size_t size, std::size_t& pos) {
  const std::uint32_t b = pos < size ? d[pos++] : 0;
  const std::int32_t v = static_cast<std::int8_t>(b) * 16;
  if (b == 0x80) return 1 << 24;           // exactly the limit
  if (b == 0x81) return (1 << 24) + 1;     // just beyond
  if (b == 0x82) return -(1 << 24);
  return v;
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::outline;
  if (size < 2) return 0;
  const std::uint32_t count = 1 + data[0] % 24;
  const std::int32_t tol = data[1] % 40;  // 0 is invalid on purpose
  std::size_t pos = 2;
  std::vector<Command> cmds(count);
  for (auto& c : cmds) {
    c.op = static_cast<Op>(pos < size ? data[pos++] % 7 : 0);
    c.a = Point{coord(data, size, pos), coord(data, size, pos)};
    c.b = Point{coord(data, size, pos), coord(data, size, pos)};
    c.c = Point{coord(data, size, pos), coord(data, size, pos)};
  }
  const std::uint32_t pb = point_bound(count), cb = contour_bound(count);
  std::vector<Point> pts(pb, Point{7, 7});
  std::vector<std::uint32_t> ends(cb, 0xABCDu);
  std::vector<std::uint8_t> closed(cb, 0x55);
  std::uint32_t nc = 0x1234, np = 0x5678;
  const Status st = build_outline(cmds.data(), count, tol, pts.data(), pb, ends.data(), cb, closed.data(), cb, &nc, &np);
  if (st != Status::Ok) {
    if (nc != 0x1234 || np != 0x5678) std::abort();
    for (const Point& p : pts)
      if (p.x != 7 || p.y != 7) std::abort();
    for (std::uint32_t e : ends)
      if (e != 0xABCDu) std::abort();
    for (std::uint8_t f : closed)
      if (f != 0x55) std::abort();
    return 0;
  }
  if (nc > cb || np > pb) std::abort();
  std::uint32_t start = 0;
  for (std::uint32_t i = 0; i < nc; ++i) {
    const std::uint32_t end = ends[i];
    if (end < start + 2 || end > np || closed[i] > 1) std::abort();
    for (std::uint32_t k = start + 1; k < end; ++k)
      if (pts[k].x == pts[k - 1].x && pts[k].y == pts[k - 1].y) std::abort();
    if (closed[i] == 1 && pts[end - 1].x == pts[start].x && pts[end - 1].y == pts[start].y) std::abort();
    for (std::uint32_t k = start; k < end; ++k)
      if (pts[k].x > (1 << 24) || pts[k].x < -(1 << 24) || pts[k].y > (1 << 24) || pts[k].y < -(1 << 24)) std::abort();
    start = end;
  }
  if (nc != 0 && start != np && start > np) std::abort();
  if (digest_outline(pts.data(), ends.data(), closed.data(), nc) != digest_outline(pts.data(), ends.data(), closed.data(), nc)) std::abort();
  return 0;
}
