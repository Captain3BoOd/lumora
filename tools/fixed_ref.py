#!/usr/bin/env python3
"""Independent reference for core/fixed (not run in CI).

Written from the header comments with Python integers and exact fractions. Rounding is done on a Fraction (nearest
integer, an exact half goes to the larger magnitude), saturation is a clamp, and the square root is math.isqrt. It prints a
C++ block with FNV-1a 64 digests that the tests recompute from the implementation.

  python3 tools/fixed_ref.py            print the generated C++ block
  python3 tools/fixed_ref.py --check F  fail unless the block in F matches
"""
import math
import sys
from fractions import Fraction

BEGIN = "// BEGIN GENERATED REFERENCE (tools/fixed_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = 2**64 - 1
M32 = 2**32 - 1
KMAX = 2**31 - 1
KMIN = -(2**31)
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
LCG_MUL = 6364136223846793005
LCG_ADD = 1442695040888963407
RAND_N = 20000


def rnd_away(fr):
    """Nearest integer; an exact half goes away from zero."""
    s = -1 if fr < 0 else 1
    return s * math.floor(abs(fr) + Fraction(1, 2))


def sat(v):
    return max(KMIN, min(KMAX, v))


def from_int(n):
    return sat(n * 65536)


def to_int_floor(a):
    return math.floor(Fraction(a, 65536))


def mul(a, b):
    return sat(rnd_away(Fraction(a * b, 65536)))


def div(a, b):
    if b == 0:
        return KMAX if a > 0 else (KMIN if a < 0 else 0)
    return sat(rnd_away(Fraction(a * 65536, b)))


def lerp(a, b, t):
    return sat(a + rnd_away(Fraction((b - a) * t, 65536)))


def isqrt_u64(v):
    return math.isqrt(v)


def sqrt(a):
    return 0 if a <= 0 else math.isqrt(a << 16)


class Fnv:
    def __init__(self):
        self.h = FNV_OFFSET
        self.n = 0

    def word(self, w):
        w &= M32
        for i in range(4):
            self.h = ((self.h ^ ((w >> (8 * i)) & 255)) * FNV_PRIME) & M64
        self.n += 1


def to_s32(u):
    u &= M32
    return u - 2**32 if u >= 2**31 else u


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def next(self):
        self.x = (self.x * LCG_MUL + LCG_ADD) & M64
        return self.x

    def q(self):
        """A 32-bit signed value with a random magnitude scale."""
        x = self.next()
        mag = (x >> 32) >> ((x >> 8) & 31)
        neg = (x >> 7) & 1
        return to_s32((-mag) if neg else mag)

    def u64(self):
        x = self.next()
        return x >> ((x >> 1) & 63)


E = [KMIN, KMIN + 1, KMIN + 65536, -65537, -65536, -65535, -32769, -32768, -3, -2, -1, 0, 1, 2, 3, 32767, 32768,
     32769, 65535, 65536, 65537, 98304, 131072, 3 * 65536 + 21845, 2**30, KMAX - 65536, KMAX - 1, KMAX,
     46340, 46341, 181 * 65536, 2147450880 - 1, 2147450880, -2147450880, 12345678, -12345678, 7, -7, 21845, 43691]
K = [1, 2, 3, 4, 5, 255, 256, 257, 65535, 65536, 65537, 2**20 - 1, 2**20, 2**24, 2**30 - 1, 2**30, 2**31 - 1, 2**31,
     2**31 + 1, 3037000499, 3037000500, 4294967295, 4294967294, 4294967293, 2**32 - 65536]


def isqrt_values():
    vals = list(range(0, 2001))
    for k in K:
        for v in (k * k - 1, k * k, k * k + 1):
            if 0 <= v <= M64:
                vals.append(v)
    vals += [M64, M64 - 1, 2**63, 2**63 - 1, 2**62, 2**62 - 1]
    g = Lcg(5)
    vals += [g.u64() for _ in range(RAND_N)]
    return vals


def digests():
    out = {}
    f = Fnv()
    for a in E:
        for b in E:
            f.word(mul(a, b))
    g = Lcg(1)
    for _ in range(RAND_N):
        a = g.q(); b = g.q()
        f.word(mul(a, b))
    out["mul"] = (f.h, f.n)
    f = Fnv()
    for a in E:
        for b in E:
            f.word(div(a, b))
    g = Lcg(2)
    for _ in range(RAND_N):
        a = g.q(); b = g.q()
        f.word(div(a, b))
    out["div"] = (f.h, f.n)
    f = Fnv()
    for a in E:
        for b in E:
            for t in E:
                f.word(lerp(a, b, t))
    g = Lcg(3)
    for _ in range(RAND_N):
        a = g.q(); b = g.q(); t = g.q()
        f.word(lerp(a, b, t))
    out["lerp"] = (f.h, f.n)
    f = Fnv()
    for a in E:
        f.word(to_int_floor(a))
    for a in range(-200000, 200001):
        f.word(to_int_floor(a))
    g = Lcg(6)
    for _ in range(RAND_N):
        f.word(to_int_floor(g.q()))
    out["floor"] = (f.h, f.n)
    f = Fnv()
    for n in range(-100000, 100001):
        f.word(from_int(n))
    for n in E:
        f.word(from_int(n))
    g = Lcg(7)
    for _ in range(RAND_N):
        f.word(from_int(g.q()))
    out["from_int"] = (f.h, f.n)
    f = Fnv()
    for a in E:
        f.word(sqrt(a))
    for a in range(0, 70001):
        f.word(sqrt(a))
    g = Lcg(4)
    for _ in range(RAND_N):
        f.word(sqrt(g.q()))
    out["sqrt"] = (f.h, f.n)
    f = Fnv()
    for v in isqrt_values():
        f.word(isqrt_u64(v))
    out["isqrt"] = (f.h, f.n)
    return out


def arr(name, typ, vals):
    def lit(v):
        if v == KMIN:
            return "INT32_MIN"
        return "%d" % v
    return "const %s %s[] = {%s};" % (typ, name, ", ".join(lit(v) for v in vals))


def render():
    d = digests()
    lines = [BEGIN,
             arr("kRefE", "std::int32_t", E),
             "const std::uint64_t kRefK[] = {%s};" % ", ".join("%dull" % k for k in K),
             "const std::size_t kRefRandN = %d;" % RAND_N]
    for name in ("mul", "div", "lerp", "floor", "from_int", "sqrt", "isqrt"):
        h, n = d[name]
        lines.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (name, h))
        lines.append("const std::size_t kRefCases_%s = %d;" % (name, n))
    lines.append(END)
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
