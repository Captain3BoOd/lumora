#!/usr/bin/env python3
"""Independent reference for core/input/offset_filter (not run in CI).

Written from the header comments with Python integers and exact fractions: the dead zone is a floor of an exact
fraction of the magnitude, the smoothing is a truncation toward zero of an exact fraction. It prints a C++ block with the
grids and FNV-1a 64 digests (return value, outputs and the whole filter after every call) that the tests recompute.

  python3 tools/offset_filter_ref.py            print the generated C++ block
  python3 tools/offset_filter_ref.py --check F  fail unless the block in F matches
"""
import math
import sys
from fractions import Fraction

BEGIN = "// BEGIN GENERATED REFERENCE (tools/offset_filter_ref.py)"
END = "// END GENERATED REFERENCE"

ONE = 65536
MAXDZ = 32768
I32MIN = -(2**31)
I32MAX = 2**31 - 1
MASK = 2**64 - 1
SENTINEL = 0x5EEDBEEF


def clamp(v):
    return max(-ONE, min(ONE, v))


def trunc(fr):
    return int(fr)  # Fraction.__int__ truncates toward zero


def dead(v, dz):
    v = clamp(v)
    m = abs(v)
    if m <= dz:
        return 0
    mag = math.floor(Fraction((m - dz) * ONE, ONE - dz))
    return mag if v >= 0 else -mag


def smooth(state, value, sp):
    return state + trunc(Fraction((value - state) * (100 - sp), 100))


def valid(sp, dz):
    return 0 <= sp <= 99 and 0 <= dz <= MAXDZ


class F:
    def __init__(self, x=0, y=0, primed=False):
        self.x, self.y, self.primed = x, y, primed


def step(f, ix, iy, sp, dz):
    """(ok, out_x, out_y); outputs are the sentinel when not ok."""
    if not valid(sp, dz):
        return False, SENTINEL, SENTINEL
    vx, vy = dead(ix, dz), dead(iy, dz)
    f.x, f.y = clamp(f.x), clamp(f.y)
    if not f.primed:
        f.x, f.y = vx, vy
    else:
        f.x, f.y = smooth(f.x, vx, sp), smooth(f.y, vy, sp)
    f.primed = True
    return True, f.x, f.y


DEADZONES = [0, 1, 2, 100, 1966, 1967, 16384, 32767, 32768]
SMOOTHS = list(range(0, 100))
INPUTS = [I32MIN, I32MIN + 1, -2000000, -ONE - 1, -ONE, -ONE + 1, -32769, -32768, -1967, -1966, -1, 0, 1, 2, 1965, 1966,
          1967, 32767, 32768, 32769, ONE - 1, ONE, ONE + 1, 2000000, I32MAX - 1, I32MAX]
STATES = [I32MIN, -ONE - 1, -ONE, -ONE + 1, -13107, -100, -1, 0, 1, 99, 100, 13107, ONE - 1, ONE, ONE + 1, I32MAX]
BAD_SMOOTH = [100, 101, 255, 2**32 - 1]
BAD_DZ = [-1, 32769, 65536, I32MIN, I32MAX]
SEQ_SEEDS = [11, 22, 33, 44]
SEQ_N = 500


class Fnv:
    def __init__(self):
        self.h = 0xCBF29CE484222325

    def byte(self, b):
        self.h = ((self.h ^ (b & 255)) * 0x100000001B3) & MASK

    def u32(self, v):
        v &= 0xFFFFFFFF
        for i in range(4):
            self.byte(v >> (8 * i))


def feed(h, ok, ox, oy, f):
    h.byte(1 if ok else 0)
    h.u32(ox)
    h.u32(oy)
    h.u32(f.x)
    h.u32(f.y)
    h.byte(1 if f.primed else 0)


def neg(v):
    return I32MAX if v == I32MIN else -v


def digests():
    # dead zone: fresh filter, smooth 0 (unprimed anyway); y is the negated x
    dz, ndz = Fnv(), 0
    for d in DEADZONES:
        for v in INPUTS:
            f = F(7, 9, False)
            feed(dz, *step(f, v, neg(v), 50, d), f)
            ndz += 1
    # smoothing: every smooth percent, hand-built primed or unprimed states, three dead zones
    sm, nsm = Fnv(), 0
    for sp in SMOOTHS:
        for d in (0, 1966, 32768):
            for primed in (False, True):
                for i, s in enumerate(STATES):
                    t = STATES[(i * 5 + 3) % len(STATES)]
                    for v in INPUTS[::3]:
                        f = F(s, t, primed)
                        feed(sm, *step(f, v, neg(v), sp, d), f)
                        nsm += 1
    # invalid configs: nothing is written
    iv, niv = Fnv(), 0
    cfgs = [(sp, 1966) for sp in BAD_SMOOTH] + [(80, d) for d in BAD_DZ] + [(100, -1), (2**32 - 1, I32MIN)]
    for sp, d in cfgs:
        f = F(5, -5, True)
        ok = valid(sp, d)
        feed(iv, ok, SENTINEL, SENTINEL, f)
        niv += 1
    # scripted sequences
    sq, nsq = Fnv(), 0
    x = 0
    for seed in SEQ_SEEDS:
        sp, d = seed * 2 % 100, (seed * 733) % (MAXDZ + 1)
        f = F()
        x = seed
        for k in range(SEQ_N):
            x = (x * 6364136223846793005 + 1442695040888963407) & MASK
            r = x >> 33
            pick = r % 11
            if pick == 0:
                v = INPUTS[(r >> 8) % len(INPUTS)]
                w = INPUTS[(r >> 16) % len(INPUTS)]
            else:
                v = (r % 150001) - 75000
                w = ((r >> 17) % 150001) - 75000
            if k % 97 == 96:
                f = F()
            feed(sq, *step(f, v, w, sp, d), f)
            nsq += 1
    return dz.h, ndz, sm.h, nsm, iv.h, niv, sq.h, nsq


def arr(name, typ, vals):
    return "const %s %s[] = {%s};" % (typ, name, ", ".join(cpp(v, typ) for v in vals))


def cpp(v, typ):
    if typ == "std::uint32_t":
        return "%du" % v
    if v == I32MIN:
        return "INT32_MIN"
    if v == I32MAX:
        return "INT32_MAX"
    return str(v)


def render():
    dz, ndz, sm, nsm, iv, niv, sq, nsq = digests()
    lines = [BEGIN,
             arr("kRefDeadzones", "std::int32_t", DEADZONES),
             arr("kRefInputs", "std::int32_t", INPUTS),
             arr("kRefStates", "std::int32_t", STATES),
             arr("kRefBadSmooth", "std::uint32_t", BAD_SMOOTH),
             arr("kRefBadDeadzone", "std::int32_t", BAD_DZ),
             arr("kRefSeqSeeds", "std::uint64_t", SEQ_SEEDS).replace("std::uint64_t", "std::uint64_t"),
             "const std::size_t kRefSeqN = %d;" % SEQ_N,
             "const std::uint32_t kRefSentinel = 0x%XU;" % SENTINEL,
             "const std::uint64_t kRefDigest_deadzone = 0x%016Xull;" % dz,
             "const std::size_t kRefCases_deadzone = %d;" % ndz,
             "const std::uint64_t kRefDigest_smooth = 0x%016Xull;" % sm,
             "const std::size_t kRefCases_smooth = %d;" % nsm,
             "const std::uint64_t kRefDigest_invalid = 0x%016Xull;" % iv,
             "const std::size_t kRefCases_invalid = %d;" % niv,
             "const std::uint64_t kRefDigest_seq = 0x%016Xull;" % sq,
             "const std::size_t kRefCases_seq = %d;" % nsq,
             END]
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
