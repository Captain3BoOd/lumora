#!/usr/bin/env python3
"""Independent reference for core/sieve (not run in CI).

Written from the header text; NO sieve is used. Every entry comes from trial division: the smallest prime factor is the first divisor found, the
totient and the Moebius value come from the prime factorization (distinct primes and their exponents) found by trial division. The selfcheck
cross-checks the totient by counting gcd(m, n) = 1 directly and the Moebius value by its definition (a square factor, else the parity of the number
of prime factors), for every n <= 3000, and checks the header values. Moebius -1 is stored as 255.

Extents (exhaustive ONLY for the stated limits, not all limits through 2^20). A table for a limit L has the entries 0..L. A fold is a 64-bit FNV-style
digest of the entry values in order (one value per step); "cases" is the number of entries folded.
  small  every limit 0..700, the whole table of each limit.
  list   the limits 1023, 1024, 1025, 4095, 4096, 65535, 65536, 65537, 2^20 - 1, 2^20; p^2 - 1, p^2, p^2 + 1 for p = 1019 and 1021 (1031^2 is above
         2^20); 2^k - 1, 2^k, 2^k + 1 for k = 2..19; each limit once, in ascending order, the whole table of each.
  full   the table for limit 2^20 (entries 0..1048576).
  rows   hand rows (n, spf, phi, mu): the prime powers of the first 30 primes, 1019^2, 1021^2, the primorials up to 510510, products of two large
         primes near 2^10 and near 2^19, the largest prime below 2^20 and its neighbours, and values near 2^20.
Usage: sieve_ref.py (prints the generated block), sieve_ref.py --check tests/sieve/sieve_tests.cpp
"""
import sys
from math import gcd

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
BEGIN = "// BEGIN GENERATED REFERENCE (tools/sieve_ref.py)"
END = "// END GENERATED REFERENCE"
MAXL = 1 << 20


def factor(n):
    """Prime factorization by trial division: list of (p, e)."""
    f = []
    d = 2
    while d * d <= n:
        if n % d == 0:
            e = 0
            while n % d == 0:
                n //= d
                e += 1
            f.append((d, e))
        d += 1
    if n > 1:
        f.append((n, 1))
    return f


def tables(limit):
    S, P, U = [0, 1], [0, 1], [0, 1]
    for n in range(2, limit + 1):
        f = factor(n)
        S.append(f[0][0])
        r = 1
        for p, e in f:
            r *= (p - 1) * p ** (e - 1)
        P.append(r)
        U.append(0 if any(e > 1 for _, e in f) else (255 if len(f) % 2 else 1))
    return S[:limit + 1], P[:limit + 1], U[:limit + 1]


def selfcheck(S, P, U):
    for n in range(2, 3001):
        assert P[n] == sum(1 for m in range(1, n + 1) if gcd(m, n) == 1), n
        sq = any(n % (d * d) == 0 for d in range(2, n + 1) if d * d <= n)
        cnt, m, d = 0, n, 2
        while d <= m:
            if m % d == 0:
                cnt += 1
                while m % d == 0:
                    m //= d
            d += 1
        assert U[n] == (0 if sq else (255 if cnt % 2 else 1)), n
        assert S[n] == min(d for d in range(2, n + 1) if n % d == 0), n
    assert (S[0], S[1], P[0], P[1], U[0], U[1]) == (0, 1, 0, 1, 0, 1)
    assert S[2] == 2 and S[9] == 3 and S[97] == 97 and P[12] == 4 and P[1 << 19] == 1 << 18 and P[97] == 96
    assert U[2] == 255 and U[4] == 0 and U[6] == 1 and U[30] == 255


def lit(v):
    return "%du" % v


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64
        self.cases += 1


def limits_list():
    s = {1023, 1024, 1025, 4095, 4096, 65535, 65536, 65537, MAXL - 1, MAXL}
    for p in (1019, 1021):
        s |= {p * p - 1, p * p, p * p + 1}
    for k in range(2, 20):
        s |= {(1 << k) - 1, 1 << k, (1 << k) + 1}
    assert max(s) <= MAXL
    return sorted(s)


PRIMES30 = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113]


def rows_list(S, P, U):
    ns = set()
    for p in PRIMES30:
        q = p
        while q <= MAXL:
            ns.add(q)
            q *= p
    ns |= {1019 * 1019, 1021 * 1021, 1019, 1021, 1009, 1013}
    pr = 1
    for p in PRIMES30[:7]:
        pr *= p
        ns.add(pr)
    for a in (1009, 1013, 1019, 1021):
        for b in (1009, 1013, 1019, 1021):
            if a < b:
                ns.add(a * b)
    big = [n for n in range(MAXL, 0, -1) if all(n % d for d in range(2, int(n ** 0.5) + 1))][:3]  # the three largest primes below 2^20 + 1
    ns |= set(big)
    ns |= {524287, 2 * 524287, MAXL, MAXL - 1, 1 << 19, (1 << 19) + 1}
    ns = {n for n in ns if 2 <= n <= MAXL}
    return sorted(ns)


NAMES = []


def build():
    S, P, U = tables(MAXL)
    selfcheck(S, P, U)
    T = {"spf": S, "phi": P, "mu": U}
    out = {}
    for name, arr in T.items():
        f = Fold()
        for L in range(0, 701):
            for v in arr[:L + 1]:
                f.add(v)
        out[name + "_small"] = f
        f = Fold()
        for L in limits_list():
            for v in arr[:L + 1]:
                f.add(v)
        out[name + "_list"] = f
        f = Fold()
        for v in arr:
            f.add(v)
        out[name + "_full"] = f
    return out, (S, P, U)


def block():
    out, (S, P, U) = build()
    L = [BEGIN]
    for k in sorted(out):
        L.append("const std::uint64_t kRefDigest_%s = 0x%016XULL;" % (k, out[k].h))
        L.append("const std::size_t kRefCases_%s = %d;" % (k, out[k].cases))
    lim = limits_list()
    L.append("const std::size_t kRefNumLimits = %d;" % len(lim))
    L.append("const std::uint32_t kRefLimits[%d] = {%s};" % (len(lim), ", ".join(lit(v) for v in lim)))
    rows = rows_list(S, P, U)
    L.append("const std::size_t kRefNumRows = %d;" % len(rows))
    L.append("const std::uint32_t kRefRows[%d][4] = {" % len(rows))
    for n in rows:
        L.append("    {%d, %d, %d, %d}," % (n, S[n], P[n], U[n]))
    L.append("};")
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
