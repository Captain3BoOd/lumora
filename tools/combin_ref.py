#!/usr/bin/env python3
"""Independent reference for core/combin (not run in CI).

Written from the header text with Python's arbitrary precision integers and math.comb. Result codes: Ok 0, NullArgument 1, Overflow 2. A case is folded
as (2,) for Overflow and (0, value) for Ok.
  binom(n, k)       = 0 when k > n, else C(n, min(k, n - k)); Overflow when above 2^64 - 1.
  multichoose(n, k) = 1 for (0, 0), 0 for (0, k > 0), else C(n + k - 1, k) (the exact integer n + k - 1; it can need 65 bits).
  factorial(n)      = n!; Overflow above 2^64 - 1 (n > 20).
  catalan(n)        = C(2n, n) / (n + 1); Overflow above 2^64 - 1 (n > 36).
For a pair of huge arguments the value is C(N, kk) with kk = the smaller symmetric argument. C(N, kk) with N >= 2 kk is at least C(2 kk, kk), which is
above 2^64 - 1 for kk >= 34, so for kk above 70 the answer is Overflow without forming the number; for kk <= 70 it is computed exactly with math.comb.
The selfcheck cross-checks the shortcut against math.comb for kk in 34..70, Pascal's triangle for n <= 80, and the largest-fitting tables.

Scenarios (EXHAUSTIVE over the stated set unless marked), digests recomputed by the tests:
  bsmall  n in 0..80, k in 0..n+2.
  bgrid   n, k in S x S, where S = 0..40, 63..67, 2^32 -+ 1, 2^32, 2^63 -+ 1, 2^63, 2^64 - 1 .. 2^64 - 4 and the tables: for k = 2..33 the largest n with
          C(n, k) fitting, and n + 1; for m = 1..33 the largest k with C(m + k, m) fitting, and +1, +2 (capped at 2^64 - 1).
  bnear   n in S, k = n - j for j = 0..40 (n >= j): the symmetric side.
  msmall  n, k in 0..70.
  mgrid   n, k in S x S.
  mnear   n in S (n >= 1), j in 0..40: multichoose(n, j) and multichoose(j, n) (both orders, one argument small).
  fact    n in 0..22, 2^31, 2^32 - 1.
  cat     n in 0..40, 1000, 2^31, 2^32 - 1.
"""
import sys
from math import comb, factorial

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
BEGIN = "// BEGIN GENERATED REFERENCE (tools/combin_ref.py)"
END = "// END GENERATED REFERENCE"
OK, OVER = 0, 2


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64

    def case(self, res):
        self.cases += 1
        if res is None:
            self.add(OVER)
        else:
            self.add(OK)
            self.add(res)


def fit(v):
    return v if v <= M64 else None


def c_sym(N, kk):
    """C(N, kk) for N >= 2 kk, kk the smaller symmetric argument."""
    if kk > 70:
        return None
    return fit(comb(N, kk))


def binom(n, k):
    if k > n:
        return 0
    return c_sym(n, min(k, n - k))


def multichoose(n, k):
    if n == 0:
        return 1 if k == 0 else 0
    m = n - 1
    return c_sym(m + k, min(k, m))


def fact(n):
    return fit(factorial(n)) if n <= 22 else None


def cat(n):
    if n > 40:
        return None
    return fit(comb(2 * n, n) // (n + 1))


def largest(pred_ok, lo, hi):
    """largest x in [lo, hi] with pred_ok(x) (monotone: true then false); lo must satisfy it."""
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if pred_ok(mid):
            lo = mid
        else:
            hi = mid - 1
    return lo


def tables():
    nb = [largest(lambda n, k=k: comb(n, k) <= M64, k, M64) for k in range(2, 34)]
    nm = [largest(lambda kk, m=m: comb(m + kk, m) <= M64, 0, M64) for m in range(1, 34)]
    return nb, nm


NB, NM = tables()


def build_s():
    s = set(range(0, 41)) | set(range(63, 68)) | {(1 << 32) - 1, 1 << 32, (1 << 32) + 1, (1 << 63) - 1, 1 << 63, (1 << 63) + 1}
    s |= {M64 - j for j in range(4)}
    for v in NB:
        s |= {v, v + 1}
    for v in NM:
        s |= {v, v + 1, v + 2}
    return sorted(x for x in s if x <= M64)


S = build_s()
FACT_X = [2 ** 31, (1 << 32) - 1]
CAT_X = [1000, 2 ** 31, (1 << 32) - 1]


def selfcheck():
    tri = [[1]]
    for n in range(1, 81):
        p = tri[-1]
        tri.append([1] + [p[i] + p[i + 1] for i in range(n - 1)] + [1])
    for n in range(81):
        for k in range(n + 1):
            assert tri[n][k] == comb(n, k)
    for kk in range(34, 71):
        assert comb(2 * kk, kk) > M64
    assert comb(67, 33) == 14226520737620288370 and comb(68, 34) > M64
    assert comb(M64, 1) <= M64 and comb(M64, 2) > M64
    assert cat(36) == 11959798385860453492 and cat(37) is None
    assert fact(20) == 2432902008176640000 and fact(21) is None
    for k, n in zip(range(2, 34), NB):
        assert comb(n, k) <= M64 < comb(n + 1, k)
    for m, k in zip(range(1, 34), NM):
        assert comb(m + k, m) <= M64 and (k == M64 or comb(m + k + 1, m) > M64)
    assert multichoose(0, 0) == 1 and multichoose(0, 5) == 0 and multichoose(M64, 1) == M64
    assert multichoose(2, M64 - 1) == M64 and multichoose(2, M64) is None and multichoose(1, M64) == 1


def build():
    out = {}
    f = Fold()
    for n in range(81):
        for k in range(n + 3):
            f.case(binom(n, k))
    out["bsmall"] = f
    f = Fold()
    for n in S:
        for k in S:
            f.case(binom(n, k))
    out["bgrid"] = f
    f = Fold()
    for n in S:
        for j in range(41):
            if n >= j:
                f.case(binom(n, n - j))
    out["bnear"] = f
    f = Fold()
    for n in range(71):
        for k in range(71):
            f.case(multichoose(n, k))
    out["msmall"] = f
    f = Fold()
    for n in S:
        for k in S:
            f.case(multichoose(n, k))
    out["mgrid"] = f
    f = Fold()
    for n in S:
        if n >= 1:
            for j in range(41):
                f.case(multichoose(n, j))
                f.case(multichoose(j, n))
    out["mnear"] = f
    f = Fold()
    for n in list(range(23)) + FACT_X:
        f.case(fact(n))
    out["fact"] = f
    f = Fold()
    for n in list(range(41)) + CAT_X:
        f.case(cat(n))
    out["cat"] = f
    return out


def lit(v):
    return "0x%XULL" % v if v > (1 << 40) else "%dull" % v


NAMES = ("bsmall", "bgrid", "bnear", "msmall", "mgrid", "mnear", "fact", "cat")


def block():
    selfcheck()
    out = build()
    L = [BEGIN]
    L.append("const std::uint64_t kRefS[] = {%s};" % ", ".join(lit(v) for v in S))
    L.append("const std::size_t kRefSizeS = %d;" % len(S))
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
