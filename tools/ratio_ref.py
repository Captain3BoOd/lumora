#!/usr/bin/env python3
"""Independent reference for core/ratio (not run in CI).

Written from the header text. It uses Python's fractions.Fraction for the exact value and plain Python integers for every check: a value is
valid when den > 0 and math.gcd(|num|, den) == 1; a result fits when its numerator is in [-2^63, 2^63 - 1] and its denominator is at most
2^63 - 1; floor is n // d and ceil is -(-n // d). No code or method is shared with core/ratio. It prints a C++ block with the grids and the
64-bit digests that the tests recompute from the real implementation. Result codes: Ok 0, NullArgument 1, BadRatio 2, ZeroDivision 3, Overflow 4.

Grids (all exact, the sizes are printed in the block):
  Ns  41 numerators: -8..8, +-(2^31-1, 2^31, 2^31+1, 2^32-1, 2^32, 2^32+1, 2^62-1, 2^62, 2^62+1), INT64_MIN, INT64_MIN+1, INT64_MIN+2,
      INT64_MAX-2, INT64_MAX-1, INT64_MAX.
  Ds  20 denominators: 1..8, 2^31-1, 2^31, 2^31+1, 2^32-1, 2^32, 2^32+1, 2^62-1, 2^62, 2^62+1, INT64_MAX-2, INT64_MAX-1, INT64_MAX.
  Dm  42 make denominators (-(INT64_MAX) and -(INT64_MAX-1) are INT64_MIN+1 and +2, counted once): Ds, -Ds, 0, INT64_MIN, INT64_MIN+1, INT64_MIN+2.
  G   every (n, d) in Ns x Ds with gcd(|n|, d) = 1 (zero only as 0/1): every valid ratio of the grid.
  B   every (n, d) in Ns x Dm that is NOT a valid ratio (den <= 0 or not in lowest terms).
  P   12 valid partners used against B.
Scenarios (all EXHAUSTIVE over the stated set, except randp and randm):
  make   ratio_make over Ns x Dm.
  grid   add, sub, mul, div, compare over every ordered pair of G.
  fc     floor and ceil of every element of G.
  bad    for every x in B and p in P: add, sub, mul, div, compare as (x, p) and as (p, x); floor and ceil of every x in B.
  d12    add, sub, mul, div, compare over every ordered pair of valid (n, d), n in -12..12, d in 1..12.
  randp  NOT exhaustive: 400 seeds, 10 pairs of random valid ratios (bit lengths up to 63) each, all five two-argument operations.
  randm  NOT exhaustive: 400 seeds, 10 random (num, den) with bit lengths up to 64 each, ratio_make.

  python3 tools/ratio_ref.py            print the generated C++ block
  python3 tools/ratio_ref.py --check F  fail unless the block in F matches
"""
import sys
from fractions import Fraction
from math import gcd

BEGIN = "// BEGIN GENERATED REFERENCE (tools/ratio_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
GOLD = 0x9E3779B97F4A7C15
MIN = -(1 << 63)
MAX = (1 << 63) - 1
OK, BAD, ZERO, OVER = 0, 2, 3, 4
RAND_SEEDS = 400
RAND_PER_SEED = 10


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0
        self.ops = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64

    def rec(self, res):
        self.ops += 1
        for v in res:
            self.add(v)


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def next(self):
        self.x = (self.x * 6364136223846793005 + 1442695040888963407) & M64
        return self.x >> 32

    def next64(self):
        hi = self.next()
        return (hi << 32) | self.next()


def valid(v):
    return v[1] > 0 and gcd(abs(v[0]), v[1]) == 1


def fits(fr):
    return MIN <= fr.numerator <= MAX and fr.denominator <= MAX


def op_bin(op, a, b):
    if not valid(a) or not valid(b):
        return (BAD,)
    if op == "div" and b[0] == 0:
        return (ZERO,)
    x, y = Fraction(a[0], a[1]), Fraction(b[0], b[1])
    r = {"add": lambda: x + y, "sub": lambda: x - y, "mul": lambda: x * y, "div": lambda: x / y}[op]()
    if not fits(r):
        return (OVER,)
    return (OK, r.numerator, r.denominator)


def op_cmp(a, b):
    if not valid(a) or not valid(b):
        return (BAD,)
    x, y = Fraction(a[0], a[1]), Fraction(b[0], b[1])
    return (OK, -1 if x < y else (1 if x > y else 0))


def op_fc(a):
    if not valid(a):
        return (BAD, BAD)
    return (OK, a[0] // a[1], OK, -(-a[0] // a[1]))


def op_make(n, d):
    if d == 0:
        return (ZERO,)
    r = Fraction(n, d)
    if not fits(r):
        return (OVER,)
    return (OK, r.numerator, r.denominator)


def five(f, a, b):
    for op in ("add", "sub", "mul", "div"):
        f.rec(op_bin(op, a, b))
    f.rec(op_cmp(a, b))


def grids():
    big = [(1 << 31) - 1, 1 << 31, (1 << 31) + 1, (1 << 32) - 1, 1 << 32, (1 << 32) + 1, (1 << 62) - 1, 1 << 62, (1 << 62) + 1]
    ns = sorted(set(list(range(-8, 9)) + big + [-v for v in big] + [MIN, MIN + 1, MIN + 2, MAX - 2, MAX - 1, MAX]))
    ds = sorted(set(list(range(1, 9)) + big + [MAX - 2, MAX - 1, MAX]))
    dm = sorted(set(ds + [-v for v in ds] + [0, MIN, MIN + 1, MIN + 2]))
    g = [(n, d) for n in ns for d in ds if gcd(abs(n), d) == 1]
    b = [(n, d) for n in ns for d in dm if not valid((n, d))]
    p = [(0, 1), (1, 1), (-1, 1), (2, 3), (-3, 2), (MIN, 1), (MAX, 1), (1, MAX), (-1, MAX), (MIN, 3), (MAX, 2), (1, 2)]
    d12 = [(n, d) for n in range(-12, 13) for d in range(1, 13) if gcd(abs(n), d) == 1]
    return ns, ds, dm, g, b, p, d12


def signed(v):
    return v - (1 << 64) if v >= (1 << 63) else v


def rand_ratio(r):
    bn = r.next() % 64
    bd = r.next() % 63 + 1
    n64 = r.next64()
    nm = n64 >> (64 - bn) if bn else 0
    dm = r.next64() >> (64 - bd)
    sg = r.next() & 1
    if dm == 0:
        dm = 1
    g = gcd(nm, dm)
    nm //= g
    dm //= g
    return (-nm if sg else nm, dm)


def rand_wide(r):
    bn = r.next() % 65
    bd = r.next() % 65
    n64 = r.next64()
    d64 = r.next64()
    n = signed(n64 >> (64 - bn)) if bn else 0
    d = signed(d64 >> (64 - bd)) if bd else 0
    return n, d


def build():
    ns, ds, dm, g, b, p, d12 = grids()
    out = {}
    f = Fold()
    for n in ns:
        for d in dm:
            f.cases += 1
            f.rec(op_make(n, d))
    out["make"] = f
    f = Fold()
    for a in g:
        for c in g:
            f.cases += 1
            five(f, a, c)
    out["grid"] = f
    f = Fold()
    for a in g:
        f.cases += 1
        f.rec(op_fc(a))
    out["fc"] = f
    f = Fold()
    for x in b:
        for q in p:
            f.cases += 1
            five(f, x, q)
            five(f, q, x)
        f.cases += 1
        f.rec(op_fc(x))
    out["bad"] = f
    f = Fold()
    for a in d12:
        for c in d12:
            f.cases += 1
            five(f, a, c)
    out["d12"] = f
    f = Fold()
    for s in range(RAND_SEEDS):
        r = Lcg(((s + 1) * GOLD) & M64)
        for _ in range(RAND_PER_SEED):
            a = rand_ratio(r)
            c = rand_ratio(r)
            f.cases += 1
            five(f, a, c)
    out["randp"] = f
    f = Fold()
    for s in range(RAND_SEEDS):
        r = Lcg(((s + 1) * GOLD) & M64)
        for _ in range(RAND_PER_SEED):
            n, d = rand_wide(r)
            f.cases += 1
            f.rec(op_make(n, d))
    out["randm"] = f
    return ns, ds, dm, g, b, p, d12, out


def lit(v):
    if v == MIN:
        return "kMin"
    return "%dll" % v if v < 0 else "%dll" % v


def arr(name, vals):
    return "const std::int64_t %s[] = {%s};" % (name, ", ".join(lit(v) for v in vals))


def pairs(name, vals):
    return "const Ratio %s[] = {%s};" % (name, ", ".join("{%s, %s}" % (lit(a), lit(c)) for a, c in vals))


def block():
    ns, ds, dm, g, b, p, d12, out = build()
    L = [BEGIN]
    L.append("const std::uint32_t kRefRandSeeds = %d, kRefRandPerSeed = %d;" % (RAND_SEEDS, RAND_PER_SEED))
    L.append(arr("kRefNs", ns))
    L.append(arr("kRefDs", ds))
    L.append(arr("kRefDm", dm))
    L.append(pairs("kRefPartners", p))
    L.append("const std::size_t kRefSizeNs = %d, kRefSizeDs = %d, kRefSizeDm = %d, kRefSizeG = %d, kRefSizeB = %d, kRefSizeD12 = %d;"
             % (len(ns), len(ds), len(dm), len(g), len(b), len(d12)))
    for k in ("make", "grid", "fc", "bad", "d12", "randp", "randm"):
        f = out[k]
        L.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (k, f.h))
        L.append("const std::size_t kRefCases_%s = %d, kRefOps_%s = %d;" % (k, f.cases, k, f.ops))
    L.append(END)
    return "\n".join(L) + "\n"


def main():
    blk = block()
    if len(sys.argv) >= 3 and sys.argv[1] == "--check":
        txt = open(sys.argv[2]).read()
        i, j = txt.find(BEGIN), txt.find(END)
        if i < 0 or j < 0:
            print("no generated block in", sys.argv[2])
            return 1
        have = [l for l in txt[i:j + len(END)].split("\n") if l.strip()]
        want = [l for l in blk.split("\n") if l.strip()]
        if have != want:
            print("generated block differs")
            return 1
        print("ok")
        return 0
    sys.stdout.write(blk)
    return 0


if __name__ == "__main__":
    sys.exit(main())
