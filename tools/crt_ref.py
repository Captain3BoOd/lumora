#!/usr/bin/env python3
"""Independent reference for core/crt (not run in CI).

Written from the header text with Python's arbitrary precision integers. System: x = r[i] (mod m[i]), r[i] any u64, m[i] >= 1, n <= 64. Result codes:
Ok 0, NullArgument 1, TooLarge 2, ZeroModulus 3, NoSolution 4, Overflow 5. A system has a solution iff r[i] = r[j] (mod gcd(m[i], m[j])) for every
pair; L is the exact lcm; the answer is (r_out, L) with 0 <= r_out < L; Overflow when L > 2^64 - 1 (and the system is consistent).

Two independent methods, cross-checked every time this script runs (and by --check):
  A  brute force: search x in 0 .. L-1 (used when L is small, here L <= 20000), and the pairwise test on the residues.
  B  exact merging with Python integers (math.lcm, pow(x, -1, m)), no u64 limit.
It prints a C++ block with the grids and 64-bit digests that the tests recompute from the real function.

Scenarios (EXHAUSTIVE over the stated set, except where marked); every case is one system, folded as (code,) or (0, r_out, L):
  n1       m in 1..64, r in 0..2m-1.
  n2       m1, m2 in 1..24, every r1 in [0, m1), r2 in [0, m2) (ordered).
  n3       m1, m2, m3 in 1..8, every residue in range (ordered, so every ordering of every system is included).
  n4       m1..m4 in {1, 2, 3, 4, 6}, every residue in range (ordered).
  resid    m1, m2 in 1..16, r1 in RS(m1), r2 in RS(m2); RS(m) = {0, 1, m-1, m, m+1, 2m+1, 2^63, 2^64-1, 2^64-m, 12345} cut to u64.
  bigpairs m1, m2 in BM (1, 2, 3, 2^32-1, 2^32, 2^32+1, 2^63-1, 2^63, 2^63+1, 2^64-3, 2^64-2, 2^64-1), r1 in RS(m1), r2 in RS(m2); then (r1, r1 mod g).
  bigtrip  m1, m2, m3 in BT (2, 3, 2^32-1, 2^32, 2^63, 2^64-1, 2^64-2, 2^62+1), X in XS: r = X mod m (consistent), and with the last residue shifted ((X mod m + 1) mod m).
  gcdheavy g in GS, a, b in AS, m1 = g*a, m2 = g*b (when both fit), c in {0, 7}: (c + g, c + 2g), and the inconsistent (c + g, c + 1 + 2g).
  fib      NOT exhaustive over all systems, but exhaustive over its pairs: F(a), F(b) for 3 <= a < b <= 93, residues X mod m (consistent) and shifted.
  big64    64-equation systems: first 64 primes (Overflow); 2^0..2^63 (Ok); 63 primes plus a repeated prime with a conflicting residue (NoSolution, 63
           cases); 64 equal moduli (4 values, consistent and shifted); the seven coprime factors of 2^64-1 cycled over 64 equations (rotations 0..6), and
           the same with the last modulus replaced by 2 (Overflow).
  rand     NOT exhaustive: 300 seeds, 10 systems each, up to 6 equations, moduli random or sharing a factor, residues consistent or random.

  python3 tools/crt_ref.py            print the generated C++ block
  python3 tools/crt_ref.py --check F  fail unless the block in F matches
"""
import sys
from math import gcd, lcm

BEGIN = "// BEGIN GENERATED REFERENCE (tools/crt_ref.py)"
END = "// END GENERATED REFERENCE"
M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
GOLD = 0x9E3779B97F4A7C15
OK, TOOBIG, ZEROM, NOSOL, OVER = 0, 2, 3, 4, 5
RAND_SEEDS, RAND_PER_SEED = 300, 10
BM = [1, 2, 3, (1 << 32) - 1, 1 << 32, (1 << 32) + 1, (1 << 63) - 1, 1 << 63, (1 << 63) + 1, M64 - 2, M64 - 1, M64]
BT = [2, 3, (1 << 32) - 1, 1 << 32, 1 << 63, M64, M64 - 1, (1 << 62) + 1]
XS = [0, 1, 12345678901234567, M64]
GS = [1 << 20, 1 << 31, (1 << 32) - 1, 1 << 32, (1 << 40) + 1, 1 << 48]
AS = [1, 3, 5, 7, 15, 17, 255, 65537, (1 << 31) - 1, (1 << 32) - 1]
F7 = [3, 5, 17, 257, 641, 65537, 6700417]


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def next(self):
        self.x = (self.x * 6364136223846793005 + 1442695040888963407) & M64
        return self.x >> 32

    def next64(self):
        hi = self.next()
        return (hi << 32) | self.next()


def consistent(r, m):
    return all((r[i] - r[j]) % gcd(m[i], m[j]) == 0 for i in range(len(m)) for j in range(i + 1, len(m)))


def method_b(r, m):
    x, big = 0, 1
    for ri, mi in zip(r, m):
        ri %= mi
        g = gcd(big, mi)
        assert (ri - x) % g == 0
        mp = mi // g
        t = (((ri - x) // g) * pow(big // g, -1, mp)) % mp if mp > 1 else 0
        x += big * t
        big *= mp
    return x, big


def method_a(r, m):
    """Brute force; only for small lcm."""
    big = 1
    for v in m:
        big = lcm(big, v)
    assert big <= 20000
    for x in range(big):
        if all(x % mi == ri % mi for ri, mi in zip(r, m)):
            return x, big
    return None


def expect(r, m):
    if any(v == 0 for v in m):
        return (ZEROM,)
    if not consistent(r, m):
        return (NOSOL,)
    x, big = method_b(r, m)
    assert 0 <= x < big and all(x % mi == ri % mi for ri, mi in zip(r, m))
    if big > M64:
        return (OVER,)
    return (OK, x, big)


def selfcheck():
    """A and B agree (including NoSolution) on random small systems and on a small exhaustive set."""
    for m1 in range(1, 13):
        for m2 in range(1, 13):
            for r1 in range(m1 + 2):
                for r2 in range(m2 + 2):
                    a = method_a([r1, r2], [m1, m2])
                    e = expect([r1, r2], [m1, m2])
                    if a is None:
                        assert e == (NOSOL,), (r1, r2, m1, m2)
                    else:
                        assert e == (OK,) + a, (r1, r2, m1, m2)
    q = Lcg(777)
    for _ in range(4000):
        n = q.next() % 5
        m = [q.next() % 12 + 1 for _ in range(n)]
        r = [q.next() % 40 for _ in range(n)]
        big = 1
        for v in m:
            big = lcm(big, v)
        a = method_a(r, m)
        e = expect(r, m)
        assert (e == (NOSOL,)) if a is None else (e == (OK,) + a)
    # method B on big values: result satisfies every congruence and is below the exact lcm
    for _ in range(300):
        n = q.next() % 6 + 1
        m = [q.next64() >> (q.next() % 60) or 1 for _ in range(n)]
        x = q.next64()
        r = [x % v for v in m]
        assert consistent(r, m)
        xx, big = method_b(r, m)
        assert xx == x % big


def primes(k):
    p, c = [], 2
    while len(p) < k:
        if all(c % q for q in p):
            p.append(c)
        c += 1
    return p


def rs(m):
    s = {0, 1, m - 1, m, m + 1, 2 * m + 1, 1 << 63, M64, M64 + 1 - m, 12345}
    return sorted(v for v in s if 0 <= v <= M64)


def fold_case(f, r, m):
    f.cases += 1
    for v in expect(r, m):
        f.add(v)


def fibs():
    f = [0, 1]
    for _ in range(2, 95):
        f.append(f[-1] + f[-2])
    return f


def build():
    out = {}
    f = Fold()
    for m in range(1, 65):
        for r in range(2 * m):
            fold_case(f, [r], [m])
    out["n1"] = f
    f = Fold()
    for m1 in range(1, 25):
        for m2 in range(1, 25):
            for r1 in range(m1):
                for r2 in range(m2):
                    fold_case(f, [r1, r2], [m1, m2])
    out["n2"] = f
    f = Fold()
    for m1 in range(1, 9):
        for m2 in range(1, 9):
            for m3 in range(1, 9):
                for r1 in range(m1):
                    for r2 in range(m2):
                        for r3 in range(m3):
                            fold_case(f, [r1, r2, r3], [m1, m2, m3])
    out["n3"] = f
    f = Fold()
    S4 = [1, 2, 3, 4, 6]
    for m1 in S4:
        for m2 in S4:
            for m3 in S4:
                for m4 in S4:
                    for r1 in range(m1):
                        for r2 in range(m2):
                            for r3 in range(m3):
                                for r4 in range(m4):
                                    fold_case(f, [r1, r2, r3, r4], [m1, m2, m3, m4])
    out["n4"] = f
    f = Fold()
    for m1 in range(1, 17):
        for m2 in range(1, 17):
            for r1 in rs(m1):
                for r2 in rs(m2):
                    fold_case(f, [r1, r2], [m1, m2])
    out["resid"] = f
    f = Fold()
    for m1 in BM:
        for m2 in BM:
            for r1 in rs(m1):
                for r2 in rs(m2):
                    fold_case(f, [r1, r2], [m1, m2])
            g = gcd(m1, m2)
            for r1 in rs(m1):
                fold_case(f, [r1, r1 % g], [m1, m2])
    out["bigpairs"] = f
    f = Fold()
    for m1 in BT:
        for m2 in BT:
            for m3 in BT:
                for x in XS:
                    fold_case(f, [x % m1, x % m2, x % m3], [m1, m2, m3])
                    fold_case(f, [x % m1, x % m2, (x % m3 + 1) % m3], [m1, m2, m3])
    out["bigtrip"] = f
    f = Fold()
    for g in GS:
        for a in AS:
            for b in AS:
                if a > M64 // g or b > M64 // g:
                    continue
                for c in (0, 7):
                    fold_case(f, [c + g, c + 2 * g], [g * a, g * b])
                    fold_case(f, [c + g, c + 1 + 2 * g], [g * a, g * b])
    out["gcdheavy"] = f
    f = Fold()
    F = fibs()
    X = 987654321987
    for a in range(3, 94):
        for b in range(a + 1, 94):
            fold_case(f, [X % F[a], X % F[b]], [F[a], F[b]])
            fold_case(f, [X % F[a], (X + 1) % F[b]], [F[a], F[b]])
    out["fib"] = f
    f = Fold()
    X = 0xDEADBEEFCAFEBABE
    p64 = primes(64)
    fold_case(f, [M64 % p for p in p64], p64)
    pw = [1 << k for k in range(64)]
    fold_case(f, [X % v for v in pw], pw)
    for j in range(63):
        m = p64[:63] + [p64[j]]
        r = [X % p for p in p64[:63]] + [(X % p64[j] + 1) % p64[j]]
        fold_case(f, r, m)
    for mv in (1, 7, M64, 1 << 63):
        fold_case(f, [X % mv] * 64, [mv] * 64)
        fold_case(f, [X % mv] * 63 + [(X + 1) % mv], [mv] * 64)
    for rot in range(7):
        m = [F7[(i + rot) % 7] for i in range(64)]
        fold_case(f, [X % v for v in m], m)
        m2 = m[:63] + [2]
        fold_case(f, [X % v for v in m2], m2)
    out["big64"] = f
    f = Fold()
    for s in range(RAND_SEEDS):
        q = Lcg(((s + 1) * GOLD) & M64)
        for _ in range(RAND_PER_SEED):
            n = q.next() % 7
            cons = q.next() & 1
            x = q.next64()
            gb = q.next() % 20 + 1
            G = (q.next64() >> (64 - gb)) or 1
            r, m = [], []
            for _ in range(n):
                fl = q.next() & 1
                bits = q.next() % 64 + 1
                m64, r64 = q.next64(), q.next64()
                if fl:
                    mi = ((m64 >> 52) + 1) * G
                else:
                    mi = (m64 >> (64 - bits)) or 1
                m.append(mi)
                r.append(x % mi if cons else r64)
            fold_case(f, r, m)
    out["rand"] = f
    return out


def lit(v):
    return "0x%XULL" % v if v > (1 << 40) else "%dull" % v


def arr(name, vals):
    return "const std::uint64_t %s[] = {%s};" % (name, ", ".join(lit(v) for v in vals))


NAMES = ("n1", "n2", "n3", "n4", "resid", "bigpairs", "bigtrip", "gcdheavy", "fib", "big64", "rand")


def block():
    selfcheck()
    out = build()
    L = [BEGIN]
    L.append("const std::uint32_t kRefRandSeeds = %d, kRefRandPerSeed = %d;" % (RAND_SEEDS, RAND_PER_SEED))
    L.append(arr("kRefBm", BM))
    L.append(arr("kRefBt", BT))
    L.append(arr("kRefXs", XS))
    L.append(arr("kRefGs", GS))
    L.append(arr("kRefAs", AS))
    L.append(arr("kRefF7", F7))
    L.append("const std::size_t kRefSizeBm = %d, kRefSizeBt = %d, kRefSizeXs = %d, kRefSizeGs = %d, kRefSizeAs = %d, kRefSizeF7 = %d;"
             % (len(BM), len(BT), len(XS), len(GS), len(AS), len(F7)))
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
