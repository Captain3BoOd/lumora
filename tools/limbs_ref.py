#!/usr/bin/env python3
"""Independent reference for core/limbs (not run in CI).

Written from the header text with Python big integers: a number is sum(limb[i] << (32 * i)); every result is computed on the integer and
converted back to normalized little-endian 32-bit limbs. Nothing of the C++ (limb loops, carries, the length pre-passes) is shared. It prints a
C++ block with 64-bit digests that the tests recompute.

  small  every array of length 0 to 3 over 5 limb values (156 arrays) as a pair (24,336 pairs): cmp, add, sub, mul, each with the capacities
         {0, need - 1, need, need + 1, 7} (an Underflow with {0, 1, 7})
  div    the 156 arrays x 8 divisors x quotient capacities {0, need - 1, need, need + 1, 7}
  rand   400 seeds: two arrays of 0 to 64 limbs from a fixed 64-bit LCG (4 limb patterns), cmp, add, sub, mul with the exact capacity and one
         less, divmod by 4 divisors with the exact quotient capacity and one less
  spec   48 special arrays (2^(32k) - 1 for k = 1 to 24, 2^(32k) for k = 0 to 24) as a pair: cmp, add, sub, mul at the exact capacity

  python3 tools/limbs_ref.py            print the generated C++ block
  python3 tools/limbs_ref.py --check F  fail unless the block in F matches
The random scenario is NOT exhaustive.
"""
import itertools
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/limbs_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
M32 = (1 << 32) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
OK, UNDER, BTS, DIV0 = 0, 3, 4, 5

L5 = [0, 1, 0x7FFFFFFF, 0x80000000, M32]
SMALL_MAX = 3
DIVISORS = [0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, M32]
RAND_SEEDS, RAND_LEN = 400, 65
SPEC_K = 24
CAP_EXTRA = 7


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0
        self.ops = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64


def val(a):
    v = 0
    for i, x in enumerate(a):
        v |= x << (32 * i)
    return v


def limbs(v):
    out = []
    while v:
        out.append(v & M32)
        v >>= 32
    return out


def caps_for(need):
    s = {0, need, need + 1, CAP_EXTRA}
    if need >= 1:
        s.add(need - 1)
    return sorted(s)


def fold_cmp(f, a, b):
    va, vb = val(a), val(b)
    f.ops += 1
    f.add((va > vb) - (va < vb) + 1)


def fold_bin(f, op, a, b, cap):
    va, vb = val(a), val(b)
    f.ops += 1
    if op == "sub" and va < vb:
        f.add(UNDER << 32)
        return
    r = limbs({"add": va + vb, "sub": va - vb, "mul": va * vb}[op])
    if cap < len(r):
        f.add(BTS << 32)
        return
    f.add((OK << 32) | len(r))
    for x in r:
        f.add(x)


def fold_div(f, a, d, cap):
    f.ops += 1
    if d == 0:
        f.add(DIV0 << 32)
        return
    q, r = divmod(val(a), d)
    ql = limbs(q)
    if cap < len(ql):
        f.add(BTS << 32)
        return
    f.add((OK << 32) | len(ql))
    f.add(r)
    for x in ql:
        f.add(x)


def needed(op, a, b):
    va, vb = val(a), val(b)
    if op == "sub" and va < vb:
        return 0
    return len(limbs({"add": va + vb, "sub": va - vb, "mul": va * vb}[op]))


def small_arrays():
    for n in range(SMALL_MAX + 1):
        for t in itertools.product(range(5), repeat=n):
            yield [L5[i] for i in t]


def small_scenario():
    f = Fold()
    arrs = list(small_arrays())
    for a in arrs:
        for b in arrs:
            fold_cmp(f, a, b)
            for op in ("add", "sub", "mul"):
                for cap in caps_for(needed(op, a, b)):
                    fold_bin(f, op, a, b, cap)
            f.cases += 1
    return f


def div_scenario():
    f = Fold()
    for a in small_arrays():
        for d in DIVISORS:
            need = len(limbs(val(a) // d)) if d else 0
            for cap in caps_for(need):
                fold_div(f, a, d, cap)
            f.cases += 1
    return f


def lcg(seed):
    x = seed & M64
    while True:
        x = (x * 6364136223846793005 + 1442695040888963407) & M64
        yield x >> 32


def gen_array(g, n, mode):
    out = []
    for _ in range(n):
        r = next(g)
        if mode == 0:
            out.append(r)
        elif mode == 1:
            out.append((0, M32, r)[r % 3])
        elif mode == 2:
            out.append(r if r % 4 == 0 else 0)
        else:
            out.append(M32 if r % 8 else r)
    return out


def rand_scenario():
    f = Fold()
    for seed in range(1, RAND_SEEDS + 1):
        g = lcg(seed)
        n1 = next(g) % RAND_LEN
        n2 = next(g) % RAND_LEN
        a = gen_array(g, n1, seed % 4)
        b = gen_array(g, n2, (seed // 4) % 4)
        d0 = next(g)
        d1 = next(g) % 1000
        d3 = 1 << (next(g) % 32)
        fold_cmp(f, a, b)
        for op in ("add", "sub", "mul"):
            need = needed(op, a, b)
            fold_bin(f, op, a, b, need if op != "sub" or val(a) >= val(b) else 1)
            if need > 0:
                fold_bin(f, op, a, b, need - 1)
        for d in (d0, d1, M32, d3):
            need = len(limbs(val(a) // d)) if d else 0
            fold_div(f, a, d, need)
            if need > 0:
                fold_div(f, a, d, need - 1)
        f.cases += 1
    return f


def spec_arrays():
    out = [[M32] * k for k in range(1, SPEC_K + 1)]
    out += [[0] * k + [1] for k in range(0, SPEC_K + 1)]
    return out


def spec_scenario():
    f = Fold()
    arrs = spec_arrays()
    for a in arrs:
        for b in arrs:
            fold_cmp(f, a, b)
            for op in ("add", "sub", "mul"):
                need = needed(op, a, b)
                fold_bin(f, op, a, b, need if need > 0 or not (op == "sub" and val(a) < val(b)) else 1)
            f.cases += 1
    return f


def render():
    out = [BEGIN]
    out.append("const std::uint32_t kRefL5[] = {%s};" % ", ".join("0x%Xu" % v for v in L5))
    out.append("const std::uint32_t kRefDivisors[] = {%s};" % ", ".join("0x%Xu" % v for v in DIVISORS))
    out.append("const std::uint32_t kRefSmallMax = %d, kRefCapExtra = %d;" % (SMALL_MAX, CAP_EXTRA))
    out.append("const std::uint32_t kRefRandSeeds = %d, kRefRandLen = %d, kRefSpecK = %d;" % (RAND_SEEDS, RAND_LEN, SPEC_K))
    for name, fn in (("small", small_scenario), ("div", div_scenario), ("rand", rand_scenario), ("spec", spec_scenario)):
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
