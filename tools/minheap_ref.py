#!/usr/bin/env python3
"""Independent reference for core/minheap (not run in CI).

Written from the header text with Python integers. The model is a sorted Python list of (key, value) tuples (insertion by bisect, pop from
the front): it has no heap and no array layout, so it shares no idea with the C++ implementation. It runs the scenarios below and prints a
C++ block with 64-bit digests that the tests recompute.

  seq   every sequence of 0 to 6 operations over 6 operations (push of one of 4 pairs, pop, peek) with capacity 0 to 4
  perm  every permutation of the first n of 7 distinct pairs (n = 0 to 7): push all, then pop n + 1 times
  multi every sequence of 0 to 7 pushes over 3 pairs (duplicates): push all, then pop len + 1 times
  rand  200 seeds x 2000 operations, capacity 64, a fixed 64-bit LCG, mixed keys and values (small ranges give many ties)
  deep  20 seeds x 6000 operations, capacity 1000, push-heavy and pop-heavy phases of 500 operations (deep heaps)

  python3 tools/minheap_ref.py            print the generated C++ block
  python3 tools/minheap_ref.py --check F  fail unless the block in F matches
The random scenarios are NOT exhaustive.
"""
import bisect
import itertools
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/minheap_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
M32 = (1 << 32) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
OK, FULL, EMPTY = 0, 3, 4
PUSH, POP, PEEK = 0, 1, 2

P4 = [(0, 0), (0, M32), (1, 0), (M32, 0)]
Q7 = [(5, 1), (5, 0), (0, 9), (M32, M32), (0, 0), (0x80000000, 3), (5, 2)]
T3 = [(2, 2), (2, 1), (1, 5)]
SEQ_MAX, SEQ_CAPS = 6, 5
MULTI_MAX = 7
RAND_SEEDS, RAND_OPS, RAND_CAP = 200, 2000, 64
DEEP_SEEDS, DEEP_OPS, DEEP_CAP, DEEP_PHASE = 20, 6000, 1000, 500


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0
        self.ops = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64


class Model:
    def __init__(self, cap):
        self.cap = cap
        self.items = []

    def step(self, f, op, k, v):
        f.ops += 1
        if op == PUSH:
            if len(self.items) == self.cap:
                r = (FULL, 0, 0)
            else:
                bisect.insort(self.items, (k, v))
                r = (OK, 0, 0)
        elif not self.items:
            r = (EMPTY, 0, 0)
        elif op == POP:
            a = self.items.pop(0)
            r = (OK, a[0], a[1])
        else:
            a = self.items[0]
            r = (OK, a[0], a[1])
        f.add(r[0])
        f.add((r[1] << 32) | r[2])

    def end(self, f):
        f.add(len(self.items))
        f.cases += 1


def lcg(seed):
    x = seed & M64
    while True:
        x = (x * 6364136223846793005 + 1442695040888963407) & M64
        yield x >> 32


def seq_scenario():
    f = Fold()
    for cap in range(SEQ_CAPS):
        for n in range(SEQ_MAX + 1):
            for ops in itertools.product(range(6), repeat=n):
                m = Model(cap)
                for o in ops:
                    if o < 4:
                        m.step(f, PUSH, P4[o][0], P4[o][1])
                    else:
                        m.step(f, POP if o == 4 else PEEK, 0, 0)
                m.end(f)
    return f


def perm_scenario():
    f = Fold()
    for n in range(8):
        for p in itertools.permutations(range(n)):
            m = Model(n)
            for i in p:
                m.step(f, PUSH, Q7[i][0], Q7[i][1])
            for _ in range(n + 1):
                m.step(f, POP, 0, 0)
            m.end(f)
    return f


def multi_scenario():
    f = Fold()
    for n in range(MULTI_MAX + 1):
        for p in itertools.product(range(3), repeat=n):
            m = Model(n)
            for i in p:
                m.step(f, PUSH, T3[i][0], T3[i][1])
            for _ in range(n + 1):
                m.step(f, POP, 0, 0)
            m.end(f)
    return f


def rand_scenario():
    f = Fold()
    kmods = [4, 16, 1 << 32]
    vmods = [3, 5, 1 << 32]
    for seed in range(1, RAND_SEEDS + 1):
        g = lcg(seed)
        pb = 2 + seed % 4
        km, vm = kmods[seed % 3], vmods[(seed // 3) % 3]
        m = Model(RAND_CAP)
        for _ in range(RAND_OPS):
            sel, k, v = next(g) % 8, next(g) % km, next(g) % vm
            if sel < pb:
                m.step(f, PUSH, k, v)
            else:
                m.step(f, POP if (sel - pb) % 2 == 0 else PEEK, 0, 0)
        m.end(f)
    return f


def deep_scenario():
    f = Fold()
    for seed in range(1, DEEP_SEEDS + 1):
        g = lcg(seed)
        km = (50, 1 << 32)[seed % 2]
        vm = (7, 1 << 32)[(seed // 2) % 2]
        m = Model(DEEP_CAP)
        for j in range(DEEP_OPS):
            sel, k, v = next(g) % 8, next(g) % km, next(g) % vm
            push_phase = (j // DEEP_PHASE) % 2 == 0
            if (push_phase and sel < 7) or (not push_phase and sel < 1):
                m.step(f, PUSH, k, v)
            else:
                m.step(f, POP, 0, 0)
        m.end(f)
    return f


def pairs(name, a):
    return "const std::uint32_t %s[][2] = {%s};" % (name, ", ".join("{0x%Xu, 0x%Xu}" % p for p in a))


def render():
    out = [BEGIN]
    out.append(pairs("kRefP4", P4))
    out.append(pairs("kRefQ7", Q7))
    out.append(pairs("kRefT3", T3))
    out.append("const std::uint32_t kRefSeqMax = %d, kRefSeqCaps = %d, kRefMultiMax = %d;" % (SEQ_MAX, SEQ_CAPS, MULTI_MAX))
    out.append("const std::uint32_t kRefRandSeeds = %d, kRefRandOps = %d, kRefRandCap = %d;" % (RAND_SEEDS, RAND_OPS, RAND_CAP))
    out.append("const std::uint32_t kRefDeepSeeds = %d, kRefDeepOps = %d, kRefDeepCap = %d, kRefDeepPhase = %d;" % (DEEP_SEEDS, DEEP_OPS, DEEP_CAP, DEEP_PHASE))
    for name, fn in (("seq", seq_scenario), ("perm", perm_scenario), ("multi", multi_scenario), ("rand", rand_scenario), ("deep", deep_scenario)):
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
