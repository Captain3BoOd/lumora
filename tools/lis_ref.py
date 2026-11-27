#!/usr/bin/env python3
"""Independent reference for core/lis (not run in CI).

Written from the header text with Python integers (exact, no wrap). Three implementations, all different from the C++ code:
  brute  every index subset is tested (n up to 12): strictly increasing ones are collected, the longest length, the number of them and the
         lexicographically smallest index list among them are read off. No dynamic programme.
  quad   the quadratic dynamic programme: L[i] = longest ending at i, C[i] = exact count, R[i] = longest starting at i, and the canonical list by the
         header's rule (first index = smallest index with R == L, next = the smallest later index with a larger value and R one smaller), by plain scans.
  fast   O(n log n): a Fenwick tree over the value ranks that keeps (longest length, number of ways) for L and C, patience tails for R, and the same
         scan for the canonical list (the scan pointer only moves right, so it is linear in all). Counts here are reduced modulo 2^64.
The selfcheck compares brute, quad and fast on every sequence over {0, 1, 2} up to length 8 (brute) and 9 (quad against fast), on 3000 random
sequences (brute up to length 12, quad up to 120) and on the block sequences with 1 to 70 pairs, where the true count 2^pairs is checked exactly.

Folds. A fold is a 64-bit FNV-style digest of, for every sequence in order: n, L, the count modulo 2^64, then the L indices of the canonical list.
"cases" is the number of sequences.
  ex3    EXHAUSTIVE over values {0, 1, 2}: every sequence of length 0 .. 8 (9841; by length, then the first value changes fastest).
  ex4    EXHAUSTIVE over values {0, 1, 0x80000000, 0xFFFFFFFF}: every sequence of length 0 .. 6 (5461).
  perm   EXHAUSTIVE: every permutation of 0 .. n-1 for n = 0 .. 7 (5914; lexicographic order of the permutations of each n).
  all256 EXHAUSTIVE: the empty sequence, all 256 one-element and all 65536 two-element sequences over 0 .. 255 (65793).
  rs4, rs16, rs32  SAMPLED, fixed seed: 600 random sequences of length 17 .. 400 (length 17 + draw % 384) over the values draw % 4, draw % 16 and
         the full u32 draw (seeds 3004, 3016, 3032).
  pairs63, pairs64, pairs65  the block sequences 2, 1, 4, 3, ... with 63, 64 and 65 pairs (true counts 2^63, 2^64 and 2^65).
  wrap3  63 pairs followed by the block 129, 128, 127 (three choices): the true count is 3 * 2^63, so the residue is 2^63 and not 0.
  big_<name>_<n>   the listed large patterns (see PATTERNS), digest over the same values for the one sequence.
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Usage: lis_ref.py (prints the generated block), lis_ref.py --check tests/lis/lis_tests.cpp
"""
import bisect
import itertools
import sys

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
BEGIN = "// BEGIN GENERATED REFERENCE (tools/lis_ref.py)"
END = "// END GENERATED REFERENCE"
V4 = [0, 1, 0x80000000, 0xFFFFFFFF]


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def next(self):
        self.x = (self.x * 6364136223846793005 + 1442695040888963407) & M64
        return self.x >> 32


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64


def brute(a):
    n = len(a)
    best_len, count, best = -1, 0, None
    for mask in range(1 << n):
        idx = [i for i in range(n) if mask >> i & 1]
        if all(a[idx[k]] < a[idx[k + 1]] for k in range(len(idx) - 1)):
            if len(idx) > best_len:
                best_len, count, best = len(idx), 1, idx
            elif len(idx) == best_len:
                count += 1
                if idx < best:
                    best = idx
    return best_len, count, best


def quad(a):
    n = len(a)
    if n == 0:
        return 0, 1, []
    L = [1] * n
    C = [1] * n
    for i in range(n):
        for j in range(i):
            if a[j] < a[i]:
                if L[j] + 1 > L[i]:
                    L[i], C[i] = L[j] + 1, C[j]
                elif L[j] + 1 == L[i]:
                    C[i] += C[j]
    best = max(L)
    total = sum(C[i] for i in range(n) if L[i] == best)
    R = [1] * n
    for i in range(n - 1, -1, -1):
        for j in range(i + 1, n):
            if a[j] > a[i] and R[j] + 1 > R[i]:
                R[i] = R[j] + 1
    out = []
    need = best
    prev = -1
    while need > 0:
        for j in range(prev + 1, n):
            if R[j] == need and (prev < 0 or a[j] > a[prev]):
                out.append(j)
                prev = j
                break
        need -= 1
    return best, total, out


def fast(a):
    n = len(a)
    if n == 0:
        return 0, 1, []
    ranks = {v: r + 1 for r, v in enumerate(sorted(set(a)))}
    m = len(ranks)
    bl = [0] * (m + 1)
    bc = [0] * (m + 1)
    top, total = 0, 0
    for v in a:
        r = ranks[v] - 1  # query the ranks strictly below v
        ql, qc = 0, 1
        while r > 0:
            if bl[r] > ql:
                ql, qc = bl[r], bc[r]
            elif bl[r] == ql:
                qc = (qc + bc[r]) & M64 if ql > 0 else qc
            r -= r & -r
        if ql == 0:
            qc = 1
        li, ci = ql + 1, qc
        r = ranks[v]
        while r <= m:
            if bl[r] < li:
                bl[r], bc[r] = li, ci
            elif bl[r] == li:
                bc[r] = (bc[r] + ci) & M64
            r += r & -r
        if li > top:
            top, total = li, ci
        elif li == top:
            total = (total + ci) & M64
    tails = []
    R = [0] * n
    for i in range(n - 1, -1, -1):
        # tails holds the negated heads so that it is increasing; the number of heads above a[i] is the position
        p = bisect.bisect_left(tails, -a[i])
        R[i] = p + 1
        if p == len(tails):
            tails.append(-a[i])
        else:
            tails[p] = -a[i]
    out = []
    need = top
    j = 0
    prev = -1
    while need > 0:
        while not (R[j] == need and (prev < 0 or a[j] > a[prev])):
            j += 1
        out.append(j)
        prev = j
        j += 1
        need -= 1
    return top, total, out


def values(a, res):
    ln, cnt, idx = res
    return [len(a), ln, cnt & M64] + idx


def fold_seq(f, a, how="quad"):
    res = quad(a) if how == "quad" else fast(a)
    for v in values(a, res):
        f.add(v)
    f.cases += 1


def seqs(alpha, maxlen):
    k = len(alpha)
    for L in range(maxlen + 1):
        for c in range(k ** L):
            s = []
            x = c
            for _ in range(L):
                s.append(alpha[x % k])
                x //= k
            yield s


def pairs(k):
    out = []
    for p in range(k):
        out += [2 * p + 2, 2 * p + 1]
    return out


def pat_inc(n):
    return list(range(n))


def pat_dec(n):
    return [n - 1 - i for i in range(n)]


def pat_const(n, v):
    return [v] * n


def pat_alt(n, a, b):
    return [a if i % 2 == 0 else b for i in range(n)]


def pat_saw(n, p):
    return [i % p for i in range(n)]


def pat_dup(n, k):
    return [i // k for i in range(n)]


def pat_rand(n, seed, mod):
    g = Lcg(seed)
    return [g.next() % mod if mod else g.next() for _ in range(n)]


def pat_perm(n, seed):
    g = Lcg(seed)
    a = list(range(n))
    for i in range(n - 1, 0, -1):
        j = g.next() % (i + 1)
        a[i], a[j] = a[j], a[i]
    return a


def pat_pairs_tail(n):
    return pairs(n // 2)


PATTERNS = [
    ("inc_262144", lambda: pat_inc(262144)),
    ("inc_262143", lambda: pat_inc(262143)),
    ("dec_262144", lambda: pat_dec(262144)),
    ("const0_262144", lambda: pat_const(262144, 0)),
    ("constmax_262143", lambda: pat_const(262143, 0xFFFFFFFF)),
    ("alt01_262144", lambda: pat_alt(262144, 0, 1)),
    ("altext_262144", lambda: pat_alt(262144, 0xFFFFFFFF, 0)),
    ("saw1000_262144", lambda: pat_saw(262144, 1000)),
    ("saw512_262142", lambda: pat_saw(262142, 512)),
    ("dup2_262144", lambda: pat_dup(262144, 2)),
    ("dup3_262143", lambda: pat_dup(262143, 3)),
    ("pairs_262144", lambda: pat_pairs_tail(262144)),
    ("rand2_262144", lambda: pat_rand(262144, 5011, 2)),
    ("rand16_262144", lambda: pat_rand(262144, 5012, 16)),
    ("rand32_262144", lambda: pat_rand(262144, 5013, 0)),
    ("rand32_262143", lambda: pat_rand(262143, 5014, 0)),
    ("perm_262144", lambda: pat_perm(262144, 5015)),
    ("rand32_1000", lambda: pat_rand(1000, 5016, 0)),
]


def selfcheck():
    for s in seqs([0, 1, 2], 9):
        q, f = quad(s), fast(s)
        assert (q[0], q[1] & M64, q[2]) == f, s
        if len(s) <= 8:
            assert brute(s) == q, s
    g = Lcg(31337)
    for _ in range(3000):
        n = g.next() % 121
        k = (2, 3, 5, 1 << 32)[g.next() % 4]
        s = [g.next() % k for _ in range(n)]
        q, f = quad(s), fast(s)
        assert (q[0], q[1] & M64, q[2]) == f, s
        if n <= 12:
            assert brute(s) == q, s
    for k in range(1, 71):
        s = pairs(k)
        q, f = quad(s), fast(s)
        assert q[0] == k and q[1] == 2 ** k and q[2] == [2 * p for p in range(k)], k
        assert (q[0], q[1] & M64, q[2]) == f, k
    assert quad(pairs(64))[1] & M64 == 0 and quad(pairs(63))[1] == 2 ** 63
    w = pairs(63) + [129, 128, 127]
    assert quad(w)[1] == 3 * 2 ** 63 and quad(w)[1] & M64 == 2 ** 63 and quad(w)[2][-1] == 126 and fast(w)[1] == 2 ** 63


def build():
    out = {}
    for name, alpha, mx in (("ex3", [0, 1, 2], 8), ("ex4", V4, 6)):
        f = Fold()
        for s in seqs(alpha, mx):
            fold_seq(f, s)
        out[name] = f
    f = Fold()
    for n in range(8):
        for p in itertools.permutations(range(n)):
            fold_seq(f, list(p))
    out["perm"] = f
    f = Fold()
    fold_seq(f, [])
    for a in range(256):
        fold_seq(f, [a])
    for c in range(65536):
        fold_seq(f, [c % 256, c // 256])
    out["all256"] = f
    for name, mod, seed in (("rs4", 4, 3004), ("rs16", 16, 3016), ("rs32", 0, 3032)):
        g = Lcg(seed)
        f = Fold()
        for _ in range(600):
            n = 17 + g.next() % 384
            s = [(g.next() % mod if mod else g.next()) for _ in range(n)]
            fold_seq(f, s)
        out[name] = f
    for k in (63, 64, 65):
        f = Fold()
        fold_seq(f, pairs(k))
        out["pairs%d" % k] = f
    f = Fold()
    fold_seq(f, pairs(63) + [129, 128, 127])
    out["wrap3"] = f
    for name, make in PATTERNS:
        s = make()
        f = Fold()
        fold_seq(f, s, "fast")
        out["big_" + name] = f
    return out


def block():
    selfcheck()
    out = build()
    L = [BEGIN]
    for k in sorted(out):
        L.append("const std::uint64_t kRefDigest_%s = 0x%016XULL;" % (k, out[k].h))
        L.append("const std::size_t kRefCases_%s = %d;" % (k, out[k].cases))
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
