#!/usr/bin/env python3
"""Independent reference for core/ratapprox (not run in CI).

Written from the header text with Python's fractions.Fraction and plain integers. The input is the exact value num / den (den != 0). A candidate is
p / q in lowest terms with 1 <= q <= N. bounds: lo is the largest candidate <= x, hi the smallest candidate >= x. nearest: the candidate with the
smallest |x - p / q|, ties by the smaller denominator and then the smaller value. Result codes: Ok 0, NullArgument 1, ZeroDivision 2, BadBound 3,
Overflow 4. The one family of inputs that gives Overflow here (the maintainer confirmed this predicate, see docs/ratapprox.md) is num = INT64_MIN
with den = -d for an odd d <= N: the reduced numerator is +2^63, so the candidate x itself needs it.

Two independent methods, cross-checked every time this script runs (and by --check):
  A  brute force over every denominator q = 1..N (N at most 64): the two integer numerators next to x * q, exact distances, the tie rule as a key.
  B  continued fraction of the fractional part with convergents and one semiconvergent (textbook), for any N; its result is also checked for the
     Farey conditions hi.num * lo.den - lo.num * hi.den = 1 and lo.den + hi.den > N.
It prints a C++ block with the grids and 64-bit digests that the tests recompute from the real functions.

Grids (exact; the unique counts are printed in the block):
  NI numerators: -20..20, +-(2^31-1, 2^31, 2^31+1, 2^32-1, 2^32, 2^32+1, 2^62-1, 2^62, 2^62+1), INT64_MIN, INT64_MIN+1, INT64_MIN+2, INT64_MAX-2,
     INT64_MAX-1, INT64_MAX.
  DI denominators: +-(1..20), +-(the same nine big values), INT64_MIN, INT64_MIN+1, INT64_MIN+2, INT64_MAX-2, INT64_MAX-1, INT64_MAX.
  NB bounds: 1..12, 15, 16, 17, 100, 255, 256, 257, 65535, 65536, 65537, 2^31-1, 2^31, 2^32-1, 2^32, 2^32+1, 2^62, 2^62+1, INT64_MAX-1, INT64_MAX.
Scenarios (EXHAUSTIVE over the stated set, except where marked):
  gridA  every (num in NI, den in DI, N in NB plus d-1, d, d+1 around the reduced denominator d when in 1..INT64_MAX), both functions. Inputs whose
         answer is Overflow are NOT in this digest (they are in ovf).
  gridB  every x = +-p / q with 1 <= p <= 40 (and 0), 1 <= q <= 40 in lowest terms, in two forms (n, d) and (-3n, -3d), every N in 1..64, both functions.
  ties   for N = 1..16 every pair of consecutive Farey terms in [0, 1] of order N: the midpoint (an exact tie) plus j for j in 0..2, and the negatives,
         in two forms, at bound N, both functions.
  ovf    (INT64_MIN, -d) for odd d in a fixed list, at the bounds d-1, d, d+1, 1, 2 and INT64_MAX.
  fib    NOT exhaustive: x = +-F(k) / F(k+1) and +-F(k+1) / F(k) for k = 60..91 at the bounds F(j) - 1, F(j), F(j) + 1 (j = 20..92, step 6), 1, 2, MAX.
  rand   NOT exhaustive: 400 seeds, 10 random inputs each (bit lengths up to 64, den != 0) with a random bound of 1..63 bits.

  python3 tools/ratapprox_ref.py            print the generated C++ block
  python3 tools/ratapprox_ref.py --check F  fail unless the block in F matches
"""
import sys
from fractions import Fraction
from math import gcd, floor

BEGIN = "// BEGIN GENERATED REFERENCE (tools/ratapprox_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
GOLD = 0x9E3779B97F4A7C15
MIN = -(1 << 63)
MAX = (1 << 63) - 1
OK, ZERO, BADB, OVER = 0, 2, 3, 4
RAND_SEEDS = 400
RAND_PER_SEED = 10
BIGS = [(1 << 31) - 1, 1 << 31, (1 << 31) + 1, (1 << 32) - 1, 1 << 32, (1 << 32) + 1, (1 << 62) - 1, 1 << 62, (1 << 62) + 1]


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


def signed(v):
    return v - (1 << 64) if v >= (1 << 63) else v


def is_cand(x, n):
    return x.denominator <= n


def method_a(x, n):
    """Brute force over every denominator. Returns (lo, hi, nearest) as Fractions."""
    lo = None
    hi = None
    best = None
    for q in range(1, n + 1):
        f = (x * q).__floor__()
        for p in (f, f + 1):
            c = Fraction(p, q)
            if c <= x and (lo is None or c > lo):
                lo = c
            if c >= x and (hi is None or c < hi):
                hi = c
    for c in (lo, hi):
        key = (abs(x - c), c.denominator, c)
        if best is None or key < best[0]:
            best = (key, c)
    return lo, hi, best[1]


def method_b(x, n):
    """Continued fraction with convergents and one semiconvergent. Returns (lo, hi, nearest)."""
    if is_cand(x, n):
        return x, x, x
    fl = x.__floor__()
    r = x - fl  # 0 < r < 1, denominator > n >= 1
    # continued fraction of fl + r with a0 = fl: do it on y = x itself
    terms = []
    num, den = x.numerator, x.denominator
    while den:
        a = num // den
        terms.append(a)
        num, den = den, num - a * den
    h = [1, terms[0]]  # h_{-1}, h_0
    k = [0, 1]
    idx = 0  # last convergent with k <= n
    i = 0
    while True:
        i += 1
        if i >= len(terms):
            break
        hn = terms[i] * h[-1] + h[-2]
        kn = terms[i] * k[-1] + k[-2]
        if kn > n:
            break
        h.append(hn)
        k.append(kn)
        idx = i
    # convergent idx = (h[-1], k[-1]); previous = (h[-2], k[-2])
    t = (n - k[-2]) // k[-1]
    sh = h[-2] + t * h[-1]
    sk = k[-2] + t * k[-1]
    c = Fraction(h[-1], k[-1])
    s = Fraction(sh, sk)
    if idx % 2 == 0:
        lo, hi = c, s
    else:
        lo, hi = s, c
    assert lo < x < hi, (x, n, lo, hi)
    assert hi.numerator * lo.denominator - lo.numerator * hi.denominator == 1
    assert lo.denominator + hi.denominator > n and lo.denominator <= n and hi.denominator <= n
    dl, dh = x - lo, hi - x
    if dl != dh:
        near = lo if dl < dh else hi
    elif lo.denominator != hi.denominator:
        near = lo if lo.denominator < hi.denominator else hi
    else:
        near = lo
    return lo, hi, near


def overflows(num, den, n):
    x = Fraction(num, den)
    return x.numerator == (1 << 63) and x.denominator <= n


def expect(num, den, n):
    """Result tuple: (code,) or (code, lo.num, lo.den, hi.num, hi.den, code, near.num, near.den)."""
    if den == 0:
        return (ZERO,)
    if n < 1 or n > MAX:
        return (BADB,)
    if overflows(num, den, n):
        return (OVER,)
    x = Fraction(num, den)
    lo, hi, near = method_b(x, n)
    return (OK, lo.numerator, lo.denominator, hi.numerator, hi.denominator, OK, near.numerator, near.denominator)


def selfcheck():
    """Methods A and B agree on every x = p / q (|p| <= 40, q <= 40, both signs) for every N in 1..64, and on 3000 random wide inputs."""
    for q in range(1, 41):
        for p in range(-40, 41):
            x = Fraction(p, q)
            for n in range(1, 65):
                a = method_a(x, n)
                b = method_b(x, n)
                assert a == b, (p, q, n, a, b)
    r = Lcg(12345)
    for _ in range(3000):
        bits = r.next() % 40 + 1
        num = signed(r.next64() >> (64 - bits))
        den = (r.next64() >> (64 - (r.next() % 40 + 1))) or 1
        x = Fraction(num, den)
        n = r.next() % 64 + 1
        assert method_a(x, n) == method_b(x, n), (num, den, n)


def grids():
    ni = sorted(set(list(range(-20, 21)) + BIGS + [-v for v in BIGS] + [MIN, MIN + 1, MIN + 2, MAX - 2, MAX - 1, MAX]))
    di = sorted(set(list(range(1, 21)) + [-v for v in range(1, 21)] + BIGS + [-v for v in BIGS] + [MIN, MIN + 1, MIN + 2, MAX - 2, MAX - 1, MAX]))
    nb = sorted(set(list(range(1, 13)) + [15, 16, 17, 100, 255, 256, 257, 65535, 65536, 65537, (1 << 31) - 1, 1 << 31, (1 << 32) - 1, 1 << 32,
                                            (1 << 32) + 1, 1 << 62, (1 << 62) + 1, MAX - 1, MAX]))
    return ni, di, nb


def fold_case(f, num, den, n):
    f.cases += 1
    f.rec(expect(num, den, n))


def farey_pairs(n):
    fr = sorted({Fraction(a, b) for b in range(1, n + 1) for a in range(0, b + 1)})
    return list(zip(fr, fr[1:]))


def fibs():
    f = [0, 1]
    for _ in range(2, 95):
        f.append(f[-1] + f[-2])
    return f


OVF_D = [1, 3, 5, 7, 9, 11, 13, 15, 17, 19, (1 << 31) - 1, (1 << 31) + 1, (1 << 32) - 1, (1 << 32) + 1, (1 << 62) - 1, (1 << 62) + 1, MAX - 2, MAX]


def build():
    ni, di, nb = grids()
    out = {}
    f = Fold()
    skipped = 0
    for num in ni:
        for den in di:
            x = Fraction(num, den)
            d = x.denominator
            bounds = set(nb)
            for v in (d - 1, d, d + 1):
                if 1 <= v <= MAX:
                    bounds.add(v)
            for n in sorted(bounds):
                if overflows(num, den, n):
                    skipped += 1
                    continue
                fold_case(f, num, den, n)
    out["gridA"] = f
    out["gridA_skipped"] = skipped
    f = Fold()
    xs = [Fraction(0)]
    for q in range(1, 41):
        for p in range(1, 41):
            if gcd(p, q) == 1:
                xs.append(Fraction(p, q))
                xs.append(Fraction(-p, q))
    for x in xs:
        forms = [(x.numerator, x.denominator), (-3 * x.numerator, -3 * x.denominator)]
        for n in range(1, 65):
            for num, den in forms:
                fold_case(f, num, den, n)
    out["gridB"] = f
    out["gridB_x"] = len(xs)
    f = Fold()
    for n in range(1, 17):
        for a, c in farey_pairs(n):
            m = (a + c) / 2
            for j in range(3):
                for sgn in (1, -1):
                    y = sgn * (m + j)
                    for num, den in ((y.numerator, y.denominator), (-2 * y.numerator, -2 * y.denominator)):
                        fold_case(f, num, den, n)
    out["ties"] = f
    f = Fold()
    for d in OVF_D:
        for n in sorted({d - 1, d, d + 1, 1, 2, MAX}):
            if 1 <= n <= MAX:
                fold_case(f, MIN, -d, n)
    out["ovf"] = f
    f = Fold()
    F = fibs()
    for k in range(60, 92):
        for num, den in ((F[k], F[k + 1]), (F[k + 1], F[k])):
            for sgn in (1, -1):
                for j in list(range(20, 93, 6)) + [None]:
                    for n in ([F[j] - 1, F[j], F[j] + 1] if j else [1, 2, MAX]):
                        if 1 <= n <= MAX and num <= MAX and den <= MAX:
                            fold_case(f, sgn * num, den, n)
    out["fib"] = f
    f = Fold()
    for s in range(RAND_SEEDS):
        r = Lcg(((s + 1) * GOLD) & M64)
        for _ in range(RAND_PER_SEED):
            bn = r.next() % 65
            bd = r.next() % 65
            bb = r.next() % 63 + 1
            n64, d64, b64 = r.next64(), r.next64(), r.next64()
            num = signed(n64 >> (64 - bn)) if bn else 0
            den = signed(d64 >> (64 - bd)) if bd else 0
            if den == 0:
                den = 1
            nb_ = b64 >> (64 - bb)
            if nb_ == 0:
                nb_ = 1
            fold_case(f, num, den, nb_)
    out["rand"] = f
    return ni, di, nb, out


def lit(v):
    if v == MIN:
        return "kMin"
    return "%dll" % v


def arr(name, vals, typ="std::int64_t"):
    return "const %s %s[] = {%s};" % (typ, name, ", ".join(("%dull" % v if typ == "std::uint64_t" else lit(v)) for v in vals))


def block():
    selfcheck()
    ni, di, nb, out = build()
    L = [BEGIN]
    L.append("const std::uint32_t kRefRandSeeds = %d, kRefRandPerSeed = %d;" % (RAND_SEEDS, RAND_PER_SEED))
    L.append(arr("kRefNi", ni))
    L.append(arr("kRefDi", di))
    L.append(arr("kRefNb", nb, "std::uint64_t"))
    L.append(arr("kRefOvfD", OVF_D))
    L.append("const std::size_t kRefSizeNi = %d, kRefSizeDi = %d, kRefSizeNb = %d, kRefSizeOvfD = %d, kRefGridASkipped = %d, kRefGridBX = %d;"
             % (len(ni), len(di), len(nb), len(OVF_D), out["gridA_skipped"], out["gridB_x"]))
    for k in ("gridA", "gridB", "ties", "ovf", "fib", "rand"):
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
