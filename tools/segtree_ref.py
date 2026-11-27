#!/usr/bin/env python3
"""Independent reference for core/segtree (not run in CI).

Written from the header text and it is NOT a segment tree: the array is a plain Python list and every add and every sum is a loop over the
range with every value kept modulo 2^64. A range [l, r) is valid when l <= r <= n (OutOfRange, code 4, otherwise). It prints a C++ block with
64-bit digests that the tests recompute from the real implementation.

  exh    n in 0..12 with a fixed value array. Part A, on a fresh array: every (l, r) in 0..n+1 for sum; every (l, r) in 0..n+1 and delta in
         {1, 2^64 - 1} for add, followed (when the add is valid) by every sum (l, r) in 0..n+1. Part B, n in 0..6: every ordered pair of valid
         adds (l1, r1, 2^63 + 1) then (l2, r2, 3), followed by every valid sum.
  bound  n in 1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1000: the points {0, 1, 2, n/2, n-1, n};
         every sum over a pair of points, then on ONE running array every add over a pair of points with deltas 1, 2^64 - 1 and 2^63, each
         followed by all those sums.
  rand   400 seeds, a fixed 64-bit LCG: n in 0..300, 40 random operations (add or sum, random l <= r, random delta).

  python3 tools/segtree_ref.py            print the generated C++ block
  python3 tools/segtree_ref.py --check F  fail unless the block in F matches
The bound and rand scenarios are NOT exhaustive. Max-size cases (65535 and 65536 elements) are checked in the tests against a plain vector
only.
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/segtree_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
OK, OUT_OF_RANGE = 0, 4
EXH_MAX_N = 12
PAIR_MAX_N = 6
BOUND_SIZES = [1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1000]
BOUND_DELTAS = [1, M64, 1 << 63]
RAND_SEEDS = 400
RAND_OPS = 40
RAND_MAX_N = 300
GOLD = 0x9E3779B97F4A7C15


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0
        self.ops = 0

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


def fixed_values(n):
    return [M64 if i % 5 == 3 else (GOLD * (i + 1)) & M64 for i in range(n)]


class Ref:
    def __init__(self, vals):
        self.a = list(vals)
        self.n = len(vals)

    def add(self, l, r, d):
        if l > r or r > self.n:
            return OUT_OF_RANGE
        for i in range(l, r):
            self.a[i] = (self.a[i] + d) & M64
        return OK

    def sum(self, l, r):
        if l > r or r > self.n:
            return OUT_OF_RANGE, 0
        s = 0
        for i in range(l, r):
            s = (s + self.a[i]) & M64
        return OK, s


def fold_sum(f, ref, l, r):
    code, out = ref.sum(l, r)
    f.ops += 1
    f.add(code)
    if code == OK:
        f.add(out)


def fold_add(f, ref, l, r, d):
    code = ref.add(l, r, d)
    f.ops += 1
    f.add(code)
    return code


def exh_scenario():
    f = Fold()
    for n in range(EXH_MAX_N + 1):
        vals = fixed_values(n)
        f.cases += 1
        ref = Ref(vals)
        for l in range(n + 2):
            for r in range(n + 2):
                fold_sum(f, ref, l, r)
        for d in (1, M64):
            for l in range(n + 2):
                for r in range(n + 2):
                    f.cases += 1
                    ref = Ref(vals)
                    if fold_add(f, ref, l, r, d) == OK:
                        for l2 in range(n + 2):
                            for r2 in range(n + 2):
                                fold_sum(f, ref, l2, r2)
    for n in range(PAIR_MAX_N + 1):
        vals = fixed_values(n)
        valid = [(l, r) for l in range(n + 1) for r in range(l, n + 1)]
        for (l1, r1) in valid:
            for (l2, r2) in valid:
                f.cases += 1
                ref = Ref(vals)
                fold_add(f, ref, l1, r1, (1 << 63) + 1)
                fold_add(f, ref, l2, r2, 3)
                for (l3, r3) in valid:
                    fold_sum(f, ref, l3, r3)
    return f


def points(n):
    return sorted(set([0, 1, 2, n // 2, n - 1, n]) & set(range(n + 1)))


def bound_scenario():
    f = Fold()
    for n in BOUND_SIZES:
        pts = points(n)
        rs = [(a, b) for a in pts for b in pts if a <= b]
        ref = Ref(fixed_values(n))
        f.cases += 1
        for (a, b) in rs:
            fold_sum(f, ref, a, b)
        for (a, b) in rs:
            for d in BOUND_DELTAS:
                fold_add(f, ref, a, b, d)
                for (c, e) in rs:
                    fold_sum(f, ref, c, e)
    return f


def rand_scenario():
    f = Fold()
    for seed in range(1, RAND_SEEDS + 1):
        g = Lcg(seed * 7919 + 13)
        n = g.next() % (RAND_MAX_N + 1)
        vals = [g.next64() for _ in range(n)]
        ref = Ref(vals)
        f.cases += 1
        for _ in range(RAND_OPS):
            kind = g.next() % 2
            a = g.next() % (n + 1)
            b = g.next() % (n + 1)
            l, r = min(a, b), max(a, b)
            if kind == 0:
                mode = g.next() % 4
                d = g.next64() if mode == 0 else (1 if mode == 1 else (M64 if mode == 2 else g.next64() >> 40))
                fold_add(f, ref, l, r, d)
            else:
                fold_sum(f, ref, l, r)
    return f


def render():
    out = [BEGIN]
    out.append("const std::uint32_t kRefExhMaxN = %d, kRefPairMaxN = %d, kRefRandSeeds = %d, kRefRandOps = %d, kRefRandMaxN = %d;" % (EXH_MAX_N, PAIR_MAX_N, RAND_SEEDS, RAND_OPS, RAND_MAX_N))
    out.append("const std::uint32_t kRefBoundSizes[] = {%s};" % ", ".join(str(s) for s in BOUND_SIZES))
    for name, fn in (("exh", exh_scenario), ("bound", bound_scenario), ("rand", rand_scenario)):
        f = fn()
        out.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (name, f.h))
        out.append("const std::size_t kRefCases_%s = %d, kRefOps_%s = %d;" % (name, f.cases, name, f.ops))
    out.append(END)
    return "\n".join(out) + "\n"


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
