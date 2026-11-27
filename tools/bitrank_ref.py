#!/usr/bin/env python3
"""Independent reference for core/bitrank (not run in CI).

Written from the header text with Python integers. A bit vector is one Python integer v of nbits bits (position p is bit p of v). rank(i) is
the count of set bits of v & ((1 << i) - 1) (int.bit_count), valid for i = 0 .. nbits; select(k) is the k-th element of the list of set
positions, checked each time against bisect on the prefix counts; k at or above the total, and i above nbits, are OutOfRange (code 4). The
padding bits above nbits in the last word are filled with zeros or with ones by the caller of this reference; they never change a result, so
each result is folded once per padding choice. It prints a C++ block with 64-bit digests that the tests recompute.

  exh    every bit vector of nbits 0 to 14 (2^nbits of them), every i in 0..nbits+1 for rank and every k in 0..total+1 for select, each with
         zero and all-ones padding (the vector is folded twice)
  shape  nbits in 1, 63, 64, 65, 127, 128, 129, 191, 192, 193, 256, 257; vectors: empty, full, even positions, odd positions, every single
         bit, every pair of bits (nbits up to 65 only), every run [0, p) and every run [p, nbits); all i and all k, zero and all-ones padding
  rand   400 seeds, a fixed 64-bit LCG: 0 to 4096 bits in 4 density modes, 70 sampled rank positions and 70 sampled select ranks plus the
         endpoints, zero or all-ones padding

  python3 tools/bitrank_ref.py            print the generated C++ block
  python3 tools/bitrank_ref.py --check F  fail unless the block in F matches
The rand scenario is NOT exhaustive. The maximum size (2^24 bits) is covered by sampled queries in the tests, not here.
"""
import bisect
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/bitrank_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
OK, OUT_OF_RANGE = 0, 4
EXH_MAX_BITS = 14
SHAPE_SIZES = [1, 63, 64, 65, 127, 128, 129, 191, 192, 193, 256, 257]
RAND_SEEDS = 400


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


def rank(v, nbits, i):
    if i > nbits:
        return OUT_OF_RANGE, 0
    return OK, (v & ((1 << i) - 1)).bit_count()


def make_select(v, nbits):
    pos = [p for p in range(nbits) if (v >> p) & 1]
    prefix = [(v & ((1 << p) - 1)).bit_count() for p in range(nbits + 1)]

    def select(k):
        if k >= len(pos):
            return OUT_OF_RANGE, 0
        # the position p of the k-th set bit is the one with prefix[p] == k and bit p set: the last p with prefix[p] <= k
        p = bisect.bisect_right(prefix, k) - 1
        assert p == pos[k], (v, nbits, k)
        return OK, pos[k]

    return select, len(pos)


def fold_query(f, res):
    code, out = res
    f.ops += 1
    f.add(code)
    if code == OK:
        f.add(out)


def fold_vector_all(f, v, nbits):
    """All i in 0..nbits+1 and all k in 0..total+1, once per padding choice (zero, all ones)."""
    select, total = make_select(v, nbits)
    results = [rank(v, nbits, i) for i in range(nbits + 2)] + [select(k) for k in range(total + 2)]
    for _ in range(2):
        f.cases += 1
        for r in results:
            fold_query(f, r)


def exh_scenario():
    f = Fold()
    for nbits in range(EXH_MAX_BITS + 1):
        for v in range(1 << nbits):
            fold_vector_all(f, v, nbits)
    return f


def shape_vectors(nbits):
    full = (1 << nbits) - 1
    even = sum(1 << p for p in range(0, nbits, 2))
    yield 0
    yield full
    yield even
    yield full ^ even
    for p in range(nbits):
        yield 1 << p
    if nbits <= 65:
        for p in range(nbits):
            for q in range(p + 1, nbits):
                yield (1 << p) | (1 << q)
    for p in range(1, nbits):
        yield (1 << p) - 1
    for p in range(1, nbits):
        yield full ^ ((1 << p) - 1)


def shape_scenario():
    f = Fold()
    for nbits in SHAPE_SIZES:
        for v in shape_vectors(nbits):
            fold_vector_all(f, v, nbits)
    return f


def rand_vector(g, nbits):
    mode = g.next() % 4
    v = 0
    for w in range((nbits + 63) // 64):
        raw = (g.next() << 32) | g.next()
        if mode == 0:
            raw &= ((g.next() << 32) | g.next()) & ((g.next() << 32) | g.next())
        elif mode == 2:
            raw |= ((g.next() << 32) | g.next()) | ((g.next() << 32) | g.next())
        elif mode == 3:
            raw = M64 if g.next() % 2 else 0
        v |= raw << (64 * w)
    return v & ((1 << nbits) - 1)


def rand_scenario():
    f = Fold()
    for seed in range(1, RAND_SEEDS + 1):
        g = Lcg(seed * 7919 + 13)
        nbits = g.next() % 4097
        v = rand_vector(g, nbits)
        g.next()  # the padding choice (zero or all ones): it never changes a result
        select, total = make_select(v, nbits)
        f.cases += 1
        for i in [0, nbits, nbits + 1] + [g.next() % (nbits + 2) for _ in range(70)]:
            fold_query(f, rank(v, nbits, i))
        for k in [0, total, total + 1] + [g.next() % (total + 2) for _ in range(70)]:
            fold_query(f, select(k))
    return f


def render():
    out = [BEGIN]
    out.append("const std::uint32_t kRefExhMaxBits = %d, kRefRandSeeds = %d;" % (EXH_MAX_BITS, RAND_SEEDS))
    out.append("const std::uint32_t kRefShapeSizes[] = {%s};" % ", ".join(str(s) for s in SHAPE_SIZES))
    for name, fn in (("exh", exh_scenario), ("shape", shape_scenario), ("rand", rand_scenario)):
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
