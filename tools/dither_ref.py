#!/usr/bin/env python3
"""Independent reference for core/dither (not run in CI).

Written from the header comments with Python integers. The Bayer threshold is built by the standard recursive doubling
(M(2n) = [[4M, 4M + 2], [4M + 3, 4M + 1]]) instead of the bit formula of the header, and the quantizer uses the exact
identity 65535 = 255 * 257 (q = v // 257, rem = 255 * (v % 257)). It prints a C++ block with FNV-1a 64 digests that the
tests recompute from the implementation.

  python3 tools/dither_ref.py            print the generated C++ block
  python3 tools/dither_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/dither_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = 2**64 - 1
M32 = 2**32 - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3


def bayer8():
    m = [[0]]
    while len(m) < 8:
        n = len(m)
        big = [[0] * (2 * n) for _ in range(2 * n)]
        for y in range(n):
            for x in range(n):
                v = 4 * m[y][x]
                big[y][x] = v
                big[y][x + n] = v + 2
                big[y + n][x] = v + 3
                big[y + n][x + n] = v + 1
        m = big
    return m


B = bayer8()


def threshold(x, y):
    return B[y & 7][x & 7]


def quantize(v, x, y):
    q = v // 257
    rem = 255 * (v % 257)
    return q + 1 if rem * 128 >= (2 * threshold(x, y) + 1) * 65535 else q


class Fnv:
    def __init__(self):
        self.h = FNV_OFFSET

    def byte(self, b):
        self.h = ((self.h ^ (b & 255)) * FNV_PRIME) & M64


XS = list(range(0, 20)) + [100, 255, 256, 0x7FFFFFFF, 0x80000000, M32 - 9, M32 - 8, M32 - 7, M32 - 1, M32]
YS = list(range(0, 20)) + [100, 255, 256, 0x7FFFFFFF, 0x80000000, M32 - 9, M32 - 8, M32 - 7, M32 - 1, M32]
VS = (list(range(0, 600)) + list(range(65000, 65536)) + [1000, 4096, 12345, 21845, 32767, 32768, 32769, 43690, 50000,
                                                          257, 258, 514, 65534, 65535])
ROW_COUNTS = [1, 2, 7, 8, 9, 63, 64, 65, 255, 1000]
ROW_X0 = [0, 1, 7, 8, 100, M32 - 1000, M32 - 8, M32 - 7, M32 - 3, M32]
ROW_Y = [0, 1, 5, 8, M32]
ROW_PATTERNS = 4


def pattern(kind, n):
    out, x = [], 12345 + kind
    for i in range(n):
        x = (x * 6364136223846793005 + 1442695040888963407) & M64
        r = x >> 33
        if kind == 0:
            out.append((i * 257) & 0xFFFF)
        elif kind == 1:
            out.append(r & 0xFFFF)
        elif kind == 2:
            out.append(65535)
        else:
            out.append(i * 65 & 0xFFFF)
    return out


def digests():
    th, nth = Fnv(), 0
    for y in YS:
        for x in XS:
            th.byte(threshold(x, y))
            nth += 1
    gold, ng = Fnv(), 0
    for v in range(65536):
        for y in range(8):
            for x in range(8):
                gold.byte(quantize(v, x, y))
                ng += 1
    ex, nex = Fnv(), 0
    for y in YS:
        for x in XS:
            for v in VS:
                ex.byte(quantize(v, x, y))
                nex += 1
    row, nrow = Fnv(), 0
    for kind in range(ROW_PATTERNS):
        for n in ROW_COUNTS:
            src = pattern(kind, n)
            for x0 in ROW_X0:
                for y in ROW_Y:
                    for i in range(n):
                        row.byte(quantize(src[i], (x0 + i) & M32, y))
                    nrow += 1
    return th.h, nth, gold.h, ng, ex.h, nex, row.h, nrow


def arr(name, typ, vals):
    return "const %s %s[] = {%s};" % (typ, name, ", ".join("%du" % v for v in vals))


def render():
    th, nth, gd, ng, ex, nex, rw, nrw = digests()
    lines = [BEGIN,
             arr("kRefXs", "std::uint32_t", XS),
             arr("kRefYs", "std::uint32_t", YS),
             arr("kRefVs", "std::uint32_t", VS),
             arr("kRefRowCounts", "std::uint32_t", ROW_COUNTS),
             arr("kRefRowX0", "std::uint32_t", ROW_X0),
             arr("kRefRowY", "std::uint32_t", ROW_Y),
             "const int kRefRowPatterns = %d;" % ROW_PATTERNS,
             "const std::uint64_t kRefDigest_threshold = 0x%016Xull;" % th,
             "const std::size_t kRefCases_threshold = %d;" % nth,
             "const std::uint64_t kRefDigest_golden = 0x%016Xull;" % gd,
             "const std::size_t kRefCases_golden = %d;" % ng,
             "const std::uint64_t kRefDigest_extreme = 0x%016Xull;" % ex,
             "const std::size_t kRefCases_extreme = %d;" % nex,
             "const std::uint64_t kRefDigest_row = 0x%016Xull;" % rw,
             "const std::size_t kRefCases_row = %d;" % nrw,
             END]
    return "\n".join(lines) + "\n"


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
