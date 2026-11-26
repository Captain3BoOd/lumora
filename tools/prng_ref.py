#!/usr/bin/env python3
"""Independent reference for core/prng (not run in CI).

Written from the header comments with Python integers. A step works on unbounded integers and is reduced modulo 2^64
afterwards, the jump-ahead raises the 2x2 matrix [[mult, inc], [0, 1]] to the power delta (the header describes a
square-and-add loop instead), and the bounded draw counts rejections in a plain loop with the cap of 64. It also searches
(deterministically) for forged generators that reach the draw cap, and builds states whose output is exactly the
rejection threshold. It prints a C++ block with the cases and FNV-1a 64 digests that the tests recompute.

  python3 tools/prng_ref.py            print the generated C++ block
  python3 tools/prng_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/prng_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = 2**64
M32 = 2**32
MULT = 6364136223846793005
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
LCG_MUL = 6364136223846793005
LCG_ADD = 1442695040888963407
MAX_DRAWS = 64


class Pcg:
    def __init__(self, state=0, inc=1):
        self.state = state % M64
        self.inc = inc % M64

    def seed(self, initstate, initseq):
        self.state = 0
        self.inc = ((initseq << 1) | 1) % M64
        self.next()
        self.state = (self.state + initstate) % M64
        self.next()

    def next(self):
        old = self.state
        self.state = (old * MULT + self.inc) % M64
        xorshifted = (((old >> 18) ^ old) >> 27) % M32
        rot = old >> 59
        return ((xorshifted >> rot) | (xorshifted << ((-rot) % 32))) % M32

    def bounded(self, bound):
        if bound == 0:
            return 0
        threshold = ((M32 - bound) % M32) % bound
        r = 0
        for _ in range(MAX_DRAWS):
            r = self.next()
            if r >= threshold:
                break
        return r % bound

    def advance(self, delta):
        delta %= M64
        # [[m, c], [0, 1]]^delta applied to (state, 1)
        a, b, c, d = 1, 0, 0, 1
        m, p = MULT, self.inc
        e = delta
        while e:
            if e & 1:
                a, b = (a * m) % M64, (a * p + b) % M64
            m, p = (m * m) % M64, (m * p + p) % M64
            e >>= 1
        self.state = (a * self.state + b) % M64


def threshold_of(bound):
    return ((M32 - bound) % M32) % bound


def output_of(state):
    return Pcg(state, 0).next()


class Fnv:
    def __init__(self):
        self.h = FNV_OFFSET
        self.n = 0

    def byte(self, b):
        self.h = ((self.h ^ (b & 255)) * FNV_PRIME) % M64

    def u32(self, v):
        for i in range(4):
            self.byte((v % M32) >> (8 * i))

    def u64(self, v):
        for i in range(8):
            self.byte((v % M64) >> (8 * i))


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def next(self):
        self.x = (self.x * LCG_MUL + LCG_ADD) % M64
        return self.x


SEEDS = [(42, 54), (0, 0), (1, 1), (M64 - 1, M64 - 1), (0x853C49E6748FEA9B, 0xDA3E39CB94B95BDB),
         (123456789, 987654321), (7, 2**63), (7, 0), (2**63, 2**62 + 5)]
BOUNDS = [0, 1, 2, 3, 5, 6, 7, 10, 100, 65537, 2**31 - 1, 2**31, 2**31 + 1, 2**32 - 1]
DELTAS = [0, 1, 2, 3, 5, 7, 63, 64, 65, 1000, 12345, 2**16, 2**32 - 1, 2**32, 2**32 + 1, 2**62, 2**63, 2**63 + 1,
          M64 - 2, M64 - 1]
N_OUT = 1000
N_BOUNDED = 200
BOUNDED_SEEDS = 4
CAP_BOUNDS = [(1, 0x80000001), (2, 0x80000001), (3, 0x80000005), (4, 0x80000001), (4, 0x8000FFFF)]


def self_check():
    g = Pcg()
    g.seed(42, 54)
    assert [g.next() for _ in range(6)] == [0xA15C02B7, 0x7B47F409, 0xBA1D3330, 0x83D2F293, 0xBFA4784B, 0xCBED606E]
    for st, inc in ((5, 3), (M64 - 1, 7), (123, 124)):
        for n in (0, 1, 2, 5, 100, 1000):
            a = Pcg(st, inc)
            for _ in range(n):
                a.next()
            b = Pcg(st, inc)
            b.advance(n)
            assert a.state == b.state


def cap_cases():
    """Forged generators on a cycle of length 2^j (a fixed point c plus an offset 2^(62-j), increment (1 - mult) * c) whose
    outputs are all below the rejection threshold, so a bounded draw consumes exactly the cap."""
    out = []
    lcg = Lcg(31)
    for j, bound in CAP_BOUNDS:
        t = threshold_of(bound)
        period = 2**j
        while True:
            c = lcg.next()
            inc = ((1 - MULT) * c) % M64
            s0 = (c + 2**(62 - j)) % M64
            g = Pcg(s0, inc)
            outs = []
            for _ in range(period):
                outs.append(g.next())
            if g.state == s0 and all(o < t for o in outs):
                break
        g = Pcg(s0, inc)
        res = g.bounded(bound)
        out.append((s0, inc, bound, res, g.state))
    return out


def run_cases():
    """Forged generators where the first k outputs are rejected and output k + 1 is accepted."""
    out = []
    lcg = Lcg(32)
    for bound in (0x80000001, 0x80000005):
        t = threshold_of(bound)
        for k in range(0, 9):
            while True:
                s = lcg.next()
                inc = lcg.next() & ~1  # even: forged
                g = Pcg(s, inc)
                outs = [g.next() for _ in range(k + 1)]
                if all(o < t for o in outs[:k]) and outs[k] >= t:
                    break
            g = Pcg(s, inc)
            res = g.bounded(bound)
            out.append((s, inc, bound, res, g.state))
    return out


def exact_cases():
    """States whose output equals the threshold (accepted), threshold - 1 and threshold + 1 (rejected / accepted)."""
    out = []
    for bound in (3, 5, 6, 7, 10, 100, 65537, 2**31 - 1, 2**31 + 1, 0x80000001, 2**32 - 1):
        t = threshold_of(bound)
        for target in (t - 1, t, t + 1):
            if target < 0 or target >= M32:
                continue
            x = target ^ (target >> 18)
            state = (x << 27) % M64
            assert output_of(state) == target
            for inc in (1, 54 * 2 + 1):
                g = Pcg(state, inc)
                res = g.bounded(bound)
                out.append((state, inc, bound, res, g.state))
    return out


def forged_zero_cases():
    out = []
    for bound in (2, 3, 4, 5, 6, 8, 100, 2**31, 2**31 + 1, 2**32 - 1):
        g = Pcg(0, 0)
        res = g.bounded(bound)
        out.append((0, 0, bound, res, g.state))
    return out


def digests():
    out = {}
    f = Fnv(); n = 0
    for (a, b) in SEEDS:
        g = Pcg()
        g.seed(a, b)
        f.u64(g.state); f.u64(g.inc)
        for _ in range(N_OUT):
            f.u32(g.next())
            n += 1
        f.u64(g.state)
    out["seed_next"] = (f.h, n)
    f = Fnv(); n = 0
    for (a, b) in SEEDS[:BOUNDED_SEEDS]:
        for bound in BOUNDS:
            g = Pcg()
            g.seed(a, b)
            for _ in range(N_BOUNDED):
                f.u32(g.bounded(bound))
                f.u64(g.state)
                n += 1
    out["bounded"] = (f.h, n)
    f = Fnv(); n = 0
    for (a, b) in SEEDS:
        for forged in (False, True):
            for d in DELTAS:
                g = Pcg()
                g.seed(a, b)
                if forged:
                    g.inc = (g.inc - 1) % M64  # even
                g.advance(d)
                f.u64(g.state); f.u64(g.inc)
                f.u32(g.next())
                n += 1
    lcg = Lcg(33)
    for _ in range(2000):
        s = lcg.next(); inc = lcg.next(); d = lcg.next() >> (lcg.next() & 63)
        g = Pcg(s, inc)
        g.advance(d)
        f.u64(g.state); f.u64(g.inc)
        n += 1
    out["advance"] = (f.h, n)
    return out


def fmt_cases(name, cases):
    rows = ["{%dull, %dull, %du, %du, %dull}" % c for c in cases]
    return "const Case %s[] = {\n    %s};" % (name, ",\n    ".join(rows))


def render():
    self_check()
    d = digests()
    lines = [BEGIN,
             "struct Case {",
             "  std::uint64_t state, inc;",
             "  std::uint32_t bound, result;",
             "  std::uint64_t state_after;",
             "};",
             "const std::uint64_t kRefSeeds[][2] = {%s};" % ", ".join("{%dull, %dull}" % s for s in SEEDS),
             "const std::uint32_t kRefBounds[] = {%s};" % ", ".join("%du" % b for b in BOUNDS),
             "const std::uint64_t kRefDeltas[] = {%s};" % ", ".join("%dull" % x for x in DELTAS),
             "const std::size_t kRefOut = %d;" % N_OUT,
             "const std::size_t kRefBoundedCalls = %d;" % N_BOUNDED,
             "const std::size_t kRefBoundedSeeds = %d;" % BOUNDED_SEEDS,
             fmt_cases("kRefCap", cap_cases()),
             fmt_cases("kRefRun", run_cases()),
             fmt_cases("kRefExact", exact_cases()),
             fmt_cases("kRefZero", forged_zero_cases())]
    for name in ("seed_next", "bounded", "advance"):
        h, n = d[name]
        lines.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (name, h))
        lines.append("const std::size_t kRefCases_%s = %d;" % (name, n))
    lines.append(END)
    return "\n".join(lines) + "\n"


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
