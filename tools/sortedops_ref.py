#!/usr/bin/env python3
"""Independent reference for core/sortedops (not run in CI).

Written from the header text with Python integers: bounds by a linear scan for the first index that satisfies the definition, merge as
sorted(a + b), unique as itertools.groupby. No binary search, no merge loop and no in-place write is shared with the C++. It prints a C++
block with 64-bit digests that the tests recompute.

  bound  every non-decreasing array of length 0 to 6 over a 4-value alphabet x 7 search keys (lower and upper bound)
  merge  every pair of non-decreasing arrays of length 0 to 5 over the alphabet x every dst_cap from 0 to na + nb + 1
  uniq   every non-decreasing array of length 0 to 7 over the alphabet
  rand   300 seeds: two sorted arrays of up to 300 elements from a fixed 64-bit LCG (gaps of three sizes), 40 search keys each, merge with
         the exact capacity and one less, unique of both arrays

  python3 tools/sortedops_ref.py            print the generated C++ block
  python3 tools/sortedops_ref.py --check F  fail unless the block in F matches
The random scenario and the probed search keys are NOT exhaustive over uint32 values.
"""
import itertools
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/sortedops_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
M32 = (1 << 32) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
OK, BUF = 0, 3

V4 = [0, 1, 2, M32]
KEYS7 = [0, 1, 2, 3, 0x80000000, M32 - 1, M32]
BOUND_MAX, MERGE_MAX, UNIQ_MAX = 6, 5, 7
RAND_SEEDS, RAND_LEN, RAND_PROBES = 300, 301, 40
GAP_MODS = [1, 3, 1 << 20]


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0
        self.ops = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64


def lower(a, key):
    for i, x in enumerate(a):
        if x >= key:
            return i
    return len(a)


def upper(a, key):
    for i, x in enumerate(a):
        if x > key:
            return i
    return len(a)


def do_bound(f, a, key):
    f.ops += 2
    f.add((lower(a, key) << 32) | upper(a, key))


def do_merge(f, a, b, cap):
    f.ops += 1
    total = len(a) + len(b)
    if cap < total:
        f.add((BUF << 32) | 0)
        return
    f.add((OK << 32) | total)
    for x in sorted(a + b):
        f.add(x)


def do_uniq(f, a):
    f.ops += 1
    u = [k for k, _ in itertools.groupby(a)]
    f.add((OK << 32) | len(u))
    for x in u:
        f.add(x)


def arrays(maxlen):
    for n in range(maxlen + 1):
        for c in itertools.combinations_with_replacement(range(4), n):
            yield [V4[i] for i in c]


def bound_scenario():
    f = Fold()
    for a in arrays(BOUND_MAX):
        for key in KEYS7:
            do_bound(f, a, key)
            f.cases += 1
    return f


def merge_scenario():
    f = Fold()
    for a in arrays(MERGE_MAX):
        for b in arrays(MERGE_MAX):
            for cap in range(len(a) + len(b) + 2):
                do_merge(f, a, b, cap)
                f.cases += 1
    return f


def uniq_scenario():
    f = Fold()
    for a in arrays(UNIQ_MAX):
        do_uniq(f, a)
        f.cases += 1
    return f


def lcg(seed):
    x = seed & M64
    while True:
        x = (x * 6364136223846793005 + 1442695040888963407) & M64
        yield x >> 32


def rand_scenario():
    f = Fold()
    for seed in range(1, RAND_SEEDS + 1):
        g = lcg(seed)
        n1 = next(g) % RAND_LEN
        n2 = next(g) % RAND_LEN
        s1 = next(g) % (1 << 31)
        s2 = next(g) % (1 << 31)
        gm1, gm2 = GAP_MODS[seed % 3], GAP_MODS[(seed // 3) % 3]
        a, b = [], []
        x = s1
        for _ in range(n1):
            x += next(g) % gm1
            a.append(x)
        x = s2
        for _ in range(n2):
            x += next(g) % gm2
            b.append(x)
        for p in range(RAND_PROBES):
            r = next(g)
            base = a[r % n1] if n1 > 0 else 0
            sel = p % 4
            key = (base, (base + 1) & M32, (base - 1) & M32, r)[sel]
            do_bound(f, a, key)
        do_merge(f, a, b, n1 + n2)
        if n1 + n2 > 0:
            do_merge(f, a, b, n1 + n2 - 1)
        do_uniq(f, a)
        do_uniq(f, b)
        f.cases += 1
    return f


def render():
    out = [BEGIN]
    out.append("const std::uint32_t kRefV4[] = {%s};" % ", ".join("0x%Xu" % v for v in V4))
    out.append("const std::uint32_t kRefKeys7[] = {%s};" % ", ".join("0x%Xu" % v for v in KEYS7))
    out.append("const std::uint32_t kRefBoundMax = %d, kRefMergeMax = %d, kRefUniqMax = %d;" % (BOUND_MAX, MERGE_MAX, UNIQ_MAX))
    out.append("const std::uint32_t kRefRandSeeds = %d, kRefRandLen = %d, kRefRandProbes = %d;" % (RAND_SEEDS, RAND_LEN, RAND_PROBES))
    out.append("const std::uint32_t kRefGapMods[] = {%s};" % ", ".join("%du" % v for v in GAP_MODS))
    for name, fn in (("bound", bound_scenario), ("merge", merge_scenario), ("uniq", uniq_scenario), ("rand", rand_scenario)):
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
