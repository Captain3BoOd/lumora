#!/usr/bin/env python3
"""Independent reference for core/xxh32 (not run in CI).

Written from the algorithm text in core/xxh32/xxh32.h only, with Python integers (every step reduced modulo 2**32 by hand), and
not from any xxHash source or package. It hashes whole byte lists in one pass; a stream is modelled as "the hash of all bytes added
so far", so it shares no stripe or buffer logic with the C++. It also evaluates the finishing rule on hand-built (forged) state fields.
It prints a C++ block with the known values, FNV-1a 64 digests that the tests recompute, and a few hash values.

  python3 tools/xxh32_ref.py            print the generated C++ block
  python3 tools/xxh32_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/xxh32_ref.py)"
END = "// END GENERATED REFERENCE"

M32 = 2**32
M64 = 2**64
P1 = 2654435761
P2 = 2246822519
P3 = 3266489917
P4 = 668265263
P5 = 374761393
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
LCG_MUL = 6364136223846793005
LCG_ADD = 1442695040888963407

SEEDS = [0, 1, P1, 0xFFFFFFFF, 0x12345678]
LENGTHS = list(range(0, 81)) + [95, 96, 97, 255, 256, 257, 1000, 4096]
FORGED_TOTALS = [0, 5, 15, 16, 17, 100, 0xFFFFFFFF]
FORGED_SEEDS = [0, P1]
N_RANDOM = 2000
BIG_LEN = 1 << 20
BIG_SEED = 0x1234567


class Fnv:
    def __init__(self):
        self.h = FNV_OFFSET

    def byte(self, b):
        self.h = ((self.h ^ (b & 0xFF)) * FNV_PRIME) % M64

    def u32(self, v):
        v %= M32
        for i in range(4):
            self.byte(v >> (8 * i))


class Lcg:
    def __init__(self, seed):
        self.x = seed % M64

    def u32(self):
        self.x = (self.x * LCG_MUL + LCG_ADD) % M64
        return self.x >> 32


def rotl(x, r):
    x %= M32
    return ((x << r) | (x >> (32 - r))) % M32


def word(b, i):
    """little-endian word from four bytes starting at b[i]"""
    return b[i] + 256 * b[i + 1] + 65536 * b[i + 2] + 16777216 * b[i + 3]


def finish(v, total, seed, tail):
    """The finishing part of the algorithm: v are the four lanes (used when total >= 16), tail the remaining bytes."""
    if total >= 16:
        h = (rotl(v[0], 1) + rotl(v[1], 7) + rotl(v[2], 12) + rotl(v[3], 18)) % M32
    else:
        h = (seed + P5) % M32
    h = (h + total) % M32
    i = 0
    while len(tail) - i >= 4:
        h = (rotl(h + word(tail, i) * P3, 17) * P4) % M32
        i += 4
    while i < len(tail):
        h = (rotl(h + tail[i] * P5, 11) * P1) % M32
        i += 1
    h ^= h >> 15
    h = (h * P2) % M32
    h ^= h >> 13
    h = (h * P3) % M32
    h ^= h >> 16
    return h


def hash_bytes(data, seed):
    n = len(data)
    v = [(seed + P1 + P2) % M32, (seed + P2) % M32, seed % M32, (seed - P1) % M32]
    pos = 0
    if n >= 16:
        while n - pos >= 16:
            for k in range(4):
                v[k] = (rotl(v[k] + word(data, pos + 4 * k) * P2, 13) * P1) % M32
            pos += 16
    return finish(v, n, seed, data[pos:])


def gen_data(n, seed):
    return [(i * 131 + 17 + seed) & 255 for i in range(n)]


def run_table():
    f = Fnv()
    cases = 0
    for seed in SEEDS:
        for n in LENGTHS:
            f.u32(hash_bytes(gen_data(n, seed & 255), seed))
            cases += 1
    return f.h, cases


def forged_state(total, seed, buf_len):
    v = [(0x01000193 * (k + 1) + total) % M32 for k in range(4)]
    buf = [(i * 37 + 5) & 255 for i in range(buf_len)]
    return v, buf


def run_forged():
    f = Fnv()
    cases = 0
    for total in FORGED_TOTALS:
        for seed in FORGED_SEEDS:
            for buf_len in range(16):
                v, buf = forged_state(total, seed, buf_len)
                f.u32(finish(v, total, seed, buf))
                cases += 1
    return f.h, cases


def run_random():
    g = Lcg(71)
    f = Fnv()
    for _ in range(N_RANDOM):
        kind = g.u32() % 4
        n = g.u32() % 40 if kind == 0 else (g.u32() % 300 if kind < 3 else 16 * (g.u32() % 20) + g.u32() % 3)
        seed = g.u32() if g.u32() % 2 else 0
        base = g.u32()
        data = [(base + i * 7) & 255 for i in range(n)]
        f.u32(hash_bytes(data, seed))
    return f.h


def render():
    d_tab, n_tab = run_table()
    d_forged, n_forged = run_forged()
    d_rand = run_random()
    big = hash_bytes(gen_data(BIG_LEN, 0x33), BIG_SEED)
    out = [BEGIN]
    out.append("const std::uint32_t kRefSeeds[] = {" + ", ".join("%du" % s for s in SEEDS) + "};")
    out.append("const std::uint32_t kRefLengths[] = {" + ", ".join("%du" % n for n in LENGTHS) + "};")
    out.append("const std::uint32_t kRefForgedTotals[] = {" + ", ".join("%du" % t for t in FORGED_TOTALS) + "};")
    out.append("const std::uint32_t kRefForgedSeeds[] = {" + ", ".join("%du" % s for s in FORGED_SEEDS) + "};")
    out.append("const std::size_t kRefRandom = %d;" % N_RANDOM)
    out.append("const std::uint32_t kRefEmpty0 = 0x%08Xu;" % hash_bytes([], 0))
    out.append("const std::uint32_t kRefEmptyP1 = 0x%08Xu;" % hash_bytes([], P1))
    out.append("const std::uint32_t kRefBigLen = %du;" % BIG_LEN)
    out.append("const std::uint32_t kRefBigSeed = %du;" % BIG_SEED)
    out.append("const std::uint32_t kRefBig = 0x%08Xu;" % big)
    out.append("const std::uint64_t kRefDigest_table = 0x%016Xull;" % d_tab)
    out.append("const std::size_t kRefCases_table = %d;" % n_tab)
    out.append("const std::uint64_t kRefDigest_forged = 0x%016Xull;" % d_forged)
    out.append("const std::size_t kRefCases_forged = %d;" % n_forged)
    out.append("const std::uint64_t kRefDigest_random = 0x%016Xull;" % d_rand)
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
