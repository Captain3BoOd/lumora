#!/usr/bin/env python3
"""Independent reference for core/iroot (not run in CI).

Written from the header text with Python's arbitrary precision integers. Results: Ok 0, NullArgument 1, BadExponent 2, Overflow 3.
  iroot(x, k)         = the largest r with r^k <= x (float guess, exact correction), k in 1..64.
  perfect_power(x)    = (b, e): the largest e >= 1 with x = b^e (from e = 63 down, exact power test), (x, 1) for x < 2 and for non-powers.
  ipow(b, e)          = b^e exactly; Overflow above 2^64 - 1 (decided on the exact value; b >= 2 and e > 63 is Overflow, as 2^64 > 2^64 - 1).
The selfcheck cross-checks perfect_power by prime factorization for x <= 20000 (e = gcd of the prime multiplicities), the header values, and the
largest-fitting-base table.

Extents (EXHAUSTIVE only over the stated subsets, NOT over all square and cube bases). A fold is the digest of the results in order.
  SQ   bases for k = 2: 1..70000 (includes the windows around 65535 and 65536), 2^31 - 2000 .. 2^31 + 2000, 2^32 - 2000 .. 2^32 - 1.
  CU   bases for k = 3: 1..70000, 2642245 +- 2000, 2^20 +- 2000, 2^21 - 2000 .. 2^21 + 2000 (2^21 - 1 and 2^21 are inside).
  ir_small  x in 0..5000, k in 1..64.
  ir_pow    k in 4..64, EVERY base b with b^k <= 2^64 - 1 (largest base B(k) in the table), x = b^k - 1, b^k, b^k + 1 (when at most 2^64 - 1).
  ir_sq     k = 2, b in SQ, x = b^2 - 1, b^2, b^2 + 1 (when they fit).
  ir_cu     k = 3, b in CU, the same three x.
  ir_rand   fixed seed: 20000 bases for k = 2 (1 .. 2^32 - 1) and 20000 for k = 3 (1 .. 2642245), x = b^k + d for d in -2..2 where it fits.
  ir_edge   k in 1..64, x = 2^64 - 1 - j (j 0..199), x = 2^63 + j (j -100..100), x = 2^p - 1, 2^p, 2^p + 1 (p 0..63, when they fit).
  pp_small  every x in 0..200000.
  pp_pow    e in 2..63: bases a subset of SQ (e = 2) or CU (e = 3) (b <= 1000, or every 16th list entry, or within 100 of 65535, 65536, 2^20, 2^21, 2642245, 2^31, 2^32), every base b >= 2 with b^e fitting (e >= 4); x = b^e - 1, b^e, b^e + 1 (when they fit).
  pp_mix    every p^a * q^b (p < q among the first 12 primes, a, b >= 1) and every p^a q^b r^c (p < q < r among the first 6 primes, a, b, c in 1..8)
            that fits.
  pp_big    3^40, 2^63, 2^64 - 1, (2^32 - 1)^2, the squares of the 10 primes just below 2^32, and each +-1.
  pw_small  b in 0..300, e in 0..70.
  pw_edge   for e in 2..63 the largest fitting base B(e), B(e) - 1 and B(e) + 1; for e = 1 the bases 2^64 - 1 and 2^64 - 2; e = 0 for several bases.
  pw_small_b  bases 0, 1, 2, 3 with every exponent 0..200 and 2^31 - 1, 2^31, 2^32 - 2, 2^32 - 1, 65535, 65536.
  pw_big    bases 2^31, 2^32 - 1, 2^32, 2^63, 2^64 - 2, 2^64 - 1 with exponents 0..4 and 63, 64, 2^32 - 1.
"""
import sys
from math import gcd

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
GOLD = 0x9E3779B97F4A7C15
BEGIN = "// BEGIN GENERATED REFERENCE (tools/iroot_ref.py)"
END = "// END GENERATED REFERENCE"
OK, OVER = 0, 3


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64

    def root(self, v):
        self.cases += 1
        self.add(v)

    def pp(self, b, e):
        self.cases += 1
        self.add(b)
        self.add(e)

    def pw(self, v):
        self.cases += 1
        if v is None:
            self.add(OVER)
        else:
            self.add(OK)
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


def iroot(x, k):
    """Exact floor root: a float guess, then exact integer correction in both directions."""
    if k == 1:
        return x
    r = int(x ** (1.0 / k))
    while (r + 1) ** k <= x:
        r += 1
    while r ** k > x:
        r -= 1
    return r


def perfect_power(x):
    """x = b^e: for each e the float root is within 1e-6 of an exact root, so rounding then the exact power test finds every exact power."""
    if x < 2:
        return (x, 1)
    for e in range(min(63, x.bit_length() - 1), 1, -1):
        r = round(x ** (1.0 / e))
        if r >= 2 and r ** e == x:
            return (r, e)
    return (x, 1)


def ipow(b, e):
    v = b ** e if (b <= 1 or e <= 70) else None
    if v is None:
        return None  # base >= 2 and exponent above 70: far above 2^64
    return v if v <= M64 else None


BMAX = {k: iroot(M64, k) for k in range(2, 65)}
PP_CENTRES = (65535, 65536, 1 << 20, 1 << 21, 2642245, 1 << 31, 1 << 32)


def pp_sub(lst):
    # pp_pow subset of a square or cube list: b <= 1000, or every 16th entry, or within 100 of a centre.
    return [b for i, b in enumerate(lst) if b <= 1000 or i % 16 == 0 or any(abs(b - c) <= 100 for c in PP_CENTRES)]


SQ = sorted(set(range(1, 70001)) | set(range((1 << 31) - 2000, (1 << 31) + 2001)) | set(range((1 << 32) - 2000, 1 << 32)))
CU = sorted(set(range(1, 70001)) | set(range(2642245 - 2000, 2642245 + 2001)) | set(range((1 << 20) - 2000, (1 << 20) + 2001))
            | set(range((1 << 21) - 2000, (1 << 21) + 2001)))
PRIMES12 = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37]


def is_prime(n):
    if n < 2:
        return False
    i = 2
    while i * i <= n:
        if n % i == 0:
            return False
        i += 1
    return True


P_NEAR32 = []
_c = (1 << 32) - 1
while len(P_NEAR32) < 10:
    if is_prime(_c):
        P_NEAR32.append(_c)
    _c -= 1


def trip(b, k, lo=-1, hi=1):
    out = []
    for d in range(lo, hi + 1):
        v = b ** k + d
        if 0 <= v <= M64:
            out.append(v)
    return out


def selfcheck():
    assert iroot(M64, 2) == 4294967295 and iroot(M64, 3) == 2642245 and iroot(1 << 63, 63) == 2 and iroot((1 << 63) - 1, 63) == 1
    assert 3 ** 40 <= M64 < 3 ** 41 and ipow(1 << 63, 1) == 1 << 63 and ipow(2, 64) is None
    for k, b in BMAX.items():
        assert b ** k <= M64 < (b + 1) ** k
    for x in range(2, 20001):
        n, f, g = x, {}, 0
        p = 2
        while p * p <= n:
            while n % p == 0:
                f[p] = f.get(p, 0) + 1
                n //= p
            p += 1
        if n > 1:
            f[n] = f.get(n, 0) + 1
        for v in f.values():
            g = gcd(g, v)
        # the maximal e is gcd of the multiplicities; base = prod p^(m/g)
        base = 1
        for p, m in f.items():
            base *= p ** (m // g)
        assert perfect_power(x) == (base, g), x
    assert perfect_power(64) == (2, 6) and perfect_power(36) == (6, 2) and perfect_power(12) == (12, 1)
    assert perfect_power(1 << 63) == (2, 63) and perfect_power(3 ** 40) == (3, 40) and perfect_power(0) == (0, 1) and perfect_power(1) == (1, 1)
    assert len(P_NEAR32) == 10


def build():
    out = {}
    f = Fold()
    for x in range(5001):
        for k in range(1, 65):
            f.root(iroot(x, k))
    out["ir_small"] = f
    f = Fold()
    for k in range(4, 65):
        for b in range(1, BMAX[k] + 1):
            for x in trip(b, k):
                f.root(iroot(x, k))
    out["ir_pow"] = f
    f = Fold()
    for b in SQ:
        for x in trip(b, 2):
            f.root(iroot(x, 2))
    out["ir_sq"] = f
    f = Fold()
    for b in CU:
        for x in trip(b, 3):
            f.root(iroot(x, 3))
    out["ir_cu"] = f
    f = Fold()
    q = Lcg(GOLD)
    for _ in range(20000):
        b = q.next64() % 4294967295 + 1
        for x in trip(b, 2, -2, 2):
            f.root(iroot(x, 2))
    for _ in range(20000):
        b = q.next64() % 2642245 + 1
        for x in trip(b, 3, -2, 2):
            f.root(iroot(x, 3))
    out["ir_rand"] = f
    f = Fold()
    xs = [M64 - j for j in range(200)] + [(1 << 63) + j for j in range(-100, 101)]
    for p in range(64):
        xs += [v for v in ((1 << p) - 1, 1 << p, (1 << p) + 1) if v <= M64]
    for k in range(1, 65):
        for x in xs:
            f.root(iroot(x, k))
    out["ir_edge"] = f
    f = Fold()
    for x in range(200001):
        f.pp(*perfect_power(x))
    out["pp_small"] = f
    f = Fold()
    for e in range(2, 64):
        bases = pp_sub(SQ) if e == 2 else pp_sub(CU) if e == 3 else range(2, BMAX[e] + 1)
        for b in bases:
            if b < 2:
                continue
            for x in trip(b, e):
                f.pp(*perfect_power(x))
    out["pp_pow"] = f
    f = Fold()
    for i, p in enumerate(PRIMES12):
        for q2 in PRIMES12[i + 1:]:
            a = 1
            while p ** a <= M64:
                b = 1
                while p ** a * q2 ** b <= M64:
                    f.pp(*perfect_power(p ** a * q2 ** b))
                    b += 1
                a += 1
    P6 = PRIMES12[:6]
    for i in range(6):
        for j in range(i + 1, 6):
            for l in range(j + 1, 6):
                for a in range(1, 9):
                    for b in range(1, 9):
                        for c in range(1, 9):
                            v = P6[i] ** a * P6[j] ** b * P6[l] ** c
                            if v <= M64:
                                f.pp(*perfect_power(v))
    out["pp_mix"] = f
    f = Fold()
    bigs = [3 ** 40, 1 << 63, M64, ((1 << 32) - 1) ** 2] + [p * p for p in P_NEAR32]
    for v in bigs:
        for d in (-1, 0, 1):
            if 0 <= v + d <= M64:
                f.pp(*perfect_power(v + d))
    out["pp_big"] = f
    f = Fold()
    for b in range(301):
        for e in range(71):
            f.pw(ipow(b, e))
    out["pw_small"] = f
    f = Fold()
    for e in range(2, 64):
        B = BMAX[e]
        for b in (B - 1, B, B + 1):
            f.pw(ipow(b, e))
    for b in (M64, M64 - 1):
        f.pw(ipow(b, 1))
    for b in (0, 1, 2, 7, M64):
        f.pw(ipow(b, 0))
    out["pw_edge"] = f
    f = Fold()
    ex = list(range(201)) + [(1 << 31) - 1, 1 << 31, (1 << 32) - 2, (1 << 32) - 1, 65535, 65536]
    for b in (0, 1, 2, 3):
        for e in ex:
            f.pw(ipow(b, e))
    out["pw_small_b"] = f
    f = Fold()
    for b in ((1 << 31), (1 << 32) - 1, 1 << 32, 1 << 63, M64 - 1, M64):
        for e in list(range(5)) + [63, 64, (1 << 32) - 1]:
            f.pw(ipow(b, e))
    out["pw_big"] = f
    return out


def lit(v):
    return "0x%XULL" % v if v > (1 << 40) else "%dull" % v


NAMES = ("ir_small", "ir_pow", "ir_sq", "ir_cu", "ir_rand", "ir_edge", "pp_small", "pp_pow", "pp_mix", "pp_big", "pw_small", "pw_edge",
         "pw_small_b", "pw_big")


def block():
    selfcheck()
    out = build()
    L = [BEGIN]
    L.append("const std::uint64_t kRefBmax[65] = {0ull, 0ull, %s};" % ", ".join(lit(BMAX[k]) for k in range(2, 65)))
    L.append("const std::uint64_t kRefPrimes[10] = {%s};" % ", ".join(lit(p) for p in P_NEAR32))
    L.append("const std::size_t kRefSizeSq = %d, kRefSizeCu = %d;" % (len(SQ), len(CU)))
    for k in NAMES:
        f = out[k]
        L.append("const std::uint64_t kRefDigest_%s = 0x%016XULL;" % (k, f.h))
        L.append("const std::size_t kRefCases_%s = %d;" % (k, f.cases))
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
