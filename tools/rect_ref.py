#!/usr/bin/env python3
"""Independent reference for core/rect (not run in CI).

Written from the header comments with Python integers. The rules are checked against a pixel-set model first (a rectangle is the
set of integer points it covers; intersection is the common set, union is the bounding box of the covered set), then the same
rules are applied to boundary and random rectangles with explicit edge arithmetic. It prints a C++ block with the boundary tables
and FNV-1a 64 digests that the tests recompute.

  python3 tools/rect_ref.py            print the generated C++ block
  python3 tools/rect_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/rect_ref.py)"
END = "// END GENERATED REFERENCE"

I32_MIN = -2**31
I32_MAX = 2**31 - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 2**64
LCG_MUL = 6364136223846793005
LCG_ADD = 1442695040888963407

# one-dimensional (start, length) pairs; a rectangle is a pair from D1 for x/w and a pair for y/h
D1 = [(0, 0), (-1, 2), (5, 3), (I32_MIN, I32_MAX), (-1, I32_MAX), (I32_MAX - 1, 1), (1, I32_MAX), (3, -1), (I32_MIN, 1)]
POINTS = [I32_MIN, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, I32_MAX - 2, I32_MAX - 1, I32_MAX]
N_SMALL = 20000
N_BIG = 20000
N_EDGE = 20000


def u32(v):
    return v % 2**32


def s32(v):
    v %= 2**32
    return v - 2**32 if v >= 2**31 else v


class Fnv:
    def __init__(self):
        self.h = FNV_OFFSET

    def byte(self, b):
        self.h = ((self.h ^ (b & 0xFF)) * FNV_PRIME) % M64

    def u32(self, v):
        v = u32(v)
        for i in range(4):
            self.byte(v >> (8 * i))

    def u64(self, v):
        v %= M64
        for i in range(8):
            self.byte(v >> (8 * i))


class Lcg:
    def __init__(self, seed):
        self.x = seed % M64

    def next(self):
        self.x = (self.x * LCG_MUL + LCG_ADD) % M64
        return self.x

    def u32(self):
        return self.next() >> 32


# ---- rules, from the header ------------------------------------------------------------------------------------------------
def valid(r):
    x, y, w, h = r
    return w >= 0 and h >= 0 and x + w <= I32_MAX and y + h <= I32_MAX


def empty(r):
    return (not valid(r)) or r[2] == 0 or r[3] == 0


def intersect(a, b):
    if empty(a) or empty(b):
        return (0, 0, 0, 0)
    x = max(a[0], b[0]); y = max(a[1], b[1])
    right = min(a[0] + a[2], b[0] + b[2]); bottom = min(a[1] + a[3], b[1] + b[3])
    if right <= x or bottom <= y:
        return (0, 0, 0, 0)
    return (x, y, right - x, bottom - y)


def overlaps(a, b):
    return not empty(intersect(a, b))


def union(a, b):
    """(ok, rect). On failure the rect is None."""
    if empty(a) and empty(b):
        return True, (0, 0, 0, 0)
    if empty(a):
        return True, b
    if empty(b):
        return True, a
    x = min(a[0], b[0]); y = min(a[1], b[1])
    right = max(a[0] + a[2], b[0] + b[2]); bottom = max(a[1] + a[3], b[1] + b[3])
    if right - x > I32_MAX or bottom - y > I32_MAX:
        return False, None
    return True, (x, y, right - x, bottom - y)


def contains(r, px, py):
    if empty(r):
        return False
    return r[0] <= px < r[0] + r[2] and r[1] <= py < r[1] + r[3]


def area(r):
    return 0 if empty(r) else r[2] * r[3]


# ---- pixel-set model (small coordinates only) -------------------------------------------------------------------------------
def pixels(r):
    if empty(r):
        return set()
    return {(i, j) for i in range(r[0], r[0] + r[2]) for j in range(r[1], r[1] + r[3])}


def bbox(s):
    xs = [p[0] for p in s]; ys = [p[1] for p in s]
    return (min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1)


def self_check():
    vals = [(x, y, w, h) for x in range(-3, 3) for y in range(-3, 3) for w in range(-1, 4) for h in range(-1, 4)]
    sample = vals[::7]
    for a in sample:
        pa = pixels(a)
        assert area(a) == len(pa)
        for px in range(-5, 6):
            for py in range(-5, 6):
                assert contains(a, px, py) == ((px, py) in pa)
        for b in sample[::3]:
            pb = pixels(b)
            common = pa & pb
            assert intersect(a, b) == (bbox(common) if common else (0, 0, 0, 0))
            assert overlaps(a, b) == bool(common)
            both = pa | pb
            ok, u = union(a, b)
            assert ok
            assert u == (bbox(both) if both else (0, 0, 0, 0))
    # validity edges
    assert valid((0, 0, I32_MAX, 1)) and not valid((1, 0, I32_MAX, 1))
    assert valid((I32_MAX, 0, 0, 0)) and not valid((I32_MAX, 0, 1, 1))
    assert not valid((0, 0, -1, 1)) and valid((I32_MIN, I32_MIN, 0, 0))


def pairs_list():
    rects = [(x, y, w, h) for (x, w) in D1 for (y, h) in D1]
    return rects


def rand_small(g):
    x = g.u32() % 21 - 10; y = g.u32() % 21 - 10; w = g.u32() % 9 - 2; h = g.u32() % 9 - 2
    return (x, y, w, h)


def rand_big(g):
    x = s32(g.u32()); y = s32(g.u32())
    d = g.u32(); e = g.u32(); w = s32(d >> (e % 32))
    d = g.u32(); e = g.u32(); h = s32(d >> (e % 32))
    return (x, y, w, h)


def rand_edge(g):
    # right and bottom edges near INT32_MAX (some valid, some invalid) and widths up to 39
    w = g.u32() % 40; h = g.u32() % 40
    x = s32(I32_MAX - w + (g.u32() % 5) - 2)  # an x above INT32_MAX wraps to a very negative int32, as in the C++ cast
    y = s32(I32_MAX - h + (g.u32() % 5) - 2)
    return (x, y, w, h)


def feed_pair(fi, fu, a, b):
    r = intersect(a, b)
    for v in r:
        fi.u32(v)
    fi.byte(1 if overlaps(a, b) else 0)
    ok, u = union(a, b)
    fu.byte(1 if ok else 0)
    for v in (u if ok else (0, 0, 0, 0)):
        fu.u32(v)


def digests():
    fi, fu, fp, fa = Fnv(), Fnv(), Fnv(), Fnv()
    n = {"pairs": 0, "points": 0, "rects": 0}
    L = pairs_list()
    for a in L:
        for b in L:
            feed_pair(fi, fu, a, b); n["pairs"] += 1
    g = Lcg(41)
    for gen, count in ((rand_small, N_SMALL), (rand_big, N_BIG), (rand_edge, N_EDGE)):
        for _ in range(count):
            a = gen(g); b = gen(g)
            feed_pair(fi, fu, a, b); n["pairs"] += 1
    for a in L:
        for px in POINTS:
            for py in POINTS:
                fp.byte(1 if contains(a, px, py) else 0); n["points"] += 1
    g = Lcg(42)
    allr = list(L)
    for gen, count in ((rand_small, 2000), (rand_big, 2000), (rand_edge, 2000)):
        for _ in range(count):
            r = gen(g)
            allr.append(r)
            for px, py in ((r[0], r[1]), (r[0] + r[2], r[1]), (r[0] + r[2] - 1, r[1] + r[3] - 1), (r[0] - 1, r[1])):
                px = s32(px); py = s32(py)
                fp.byte(1 if contains(r, px, py) else 0); n["points"] += 1
    for r in allr:
        fa.byte(1 if valid(r) else 0); fa.byte(1 if empty(r) else 0); fa.u64(area(r)); n["rects"] += 1
    return {"intersect": (fi.h, n["pairs"]), "union": (fu.h, n["pairs"]), "point": (fp.h, n["points"]),
            "area": (fa.h, n["rects"])}


def render():
    self_check()
    d = digests()
    lines = [BEGIN,
             "const std::int32_t kRefD1[][2] = {%s};" % ", ".join("{%s, %s}" % (cint(x), cint(w)) for x, w in D1),
             "const std::int32_t kRefPoints[] = {%s};" % ", ".join(cint(p) for p in POINTS),
             "const std::size_t kRefSmall = %d;" % N_SMALL,
             "const std::size_t kRefBig = %d;" % N_BIG,
             "const std::size_t kRefEdge = %d;" % N_EDGE]
    for name in ("intersect", "union", "point", "area"):
        h, n = d[name]
        lines.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (name, h))
        lines.append("const std::size_t kRefCases_%s = %d;" % (name, n))
    lines.append(END)
    return "\n".join(lines) + "\n"


def cint(v):
    if v == I32_MIN:
        return "INT32_MIN"
    if v == I32_MAX:
        return "INT32_MAX"
    return "%d" % v


def main(argv):
    block = render()
    if len(argv) == 3 and argv[1] == "--check":
        text = open(argv[2]).read()
        a, b = text.index(BEGIN), text.index(END) + len(END) + 1
        if text[a:b] != block:
            print("reference block differs; regenerate it", file=sys.stderr)
            return 1
        return 0
    sys.stdout.write(block)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
