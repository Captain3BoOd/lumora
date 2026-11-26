#!/usr/bin/env python3
"""Independent reference for core/lifecycle/power_policy (not run in CI).

Written from the header comments with Python integers. It prints a C++ block with FNV-1a 64 digests (return value and
output after every call, the sentinel when nothing was written) that the tests recompute from the implementation.

  python3 tools/power_policy_ref.py            print the generated C++ block
  python3 tools/power_policy_ref.py --check F  fail unless the block in F matches
"""
import sys
from fractions import Fraction

BEGIN = "// BEGIN GENERATED REFERENCE (tools/power_policy_ref.py)"
END = "// END GENERATED REFERENCE"

U32 = 2**32 - 1
MASK = 2**64 - 1
SENTINEL = 0xDEADBEEF
HIGH, MEDIUM, LOW = 0, 1, 2

MAXES = list(range(0, 18)) + [255, U32]
NOTCHES = list(range(0, 20)) + [100, 255, 65535, 2**31, U32 - 1, U32]
PERCENTS = list(range(0, 103)) + [255, U32]
SIZES = (list(range(0, 301)) + [999, 1000, 1001, 4095, 4096, 65535, 65536, 42949672, 42949673, 42949674, 2**31 - 1,
                                2**31, 2**31 + 1, U32 - 100, U32 - 1, U32])


def render(notch, mx):
    if not 1 <= mx <= 15:
        return False, SENTINEL
    n = min(notch, mx)
    # exact rational, then floor: 100 - floor(50 n / mx)
    return True, 100 - (Fraction(50 * n, mx).__floor__())


def quality(saver, notch, mx):
    if not 1 <= mx <= 15:
        return False, SENTINEL
    eff = mx if saver else min(notch, mx)
    if eff == 0:
        return True, HIGH
    if Fraction(eff, mx) >= Fraction(1, 2):
        return True, LOW
    return True, MEDIUM


def scaled(size, pct):
    if not 1 <= pct <= 100:
        return False, SENTINEL
    return True, max(1, (size * pct) // 100)


class Fnv:
    def __init__(self):
        self.h = 0xCBF29CE484222325

    def byte(self, b):
        self.h = ((self.h ^ (b & 255)) * 0x100000001B3) & MASK

    def u32(self, v):
        for i in range(4):
            self.byte(v >> (8 * i))


def feed(f, ok, v):
    f.byte(1 if ok else 0)
    f.u32(v)


def digests():
    fr, nr = Fnv(), 0
    for mx in MAXES:
        for n in NOTCHES:
            feed(fr, *render(n, mx))
            nr += 1
    fq, nq = Fnv(), 0
    for saver in (0, 1):
        for mx in MAXES:
            for n in NOTCHES:
                feed(fq, *quality(saver, n, mx))
                nq += 1
    fs, ns = Fnv(), 0
    for s in SIZES:
        for p in PERCENTS:
            feed(fs, *scaled(s, p))
            ns += 1
    return fr.h, nr, fq.h, nq, fs.h, ns


def arr(name, typ, vals):
    return "const %s %s[] = {%s};" % (typ, name, ", ".join("%du" % v if v <= U32 else str(v) for v in vals))


def render_block():
    fr, nr, fq, nq, fs, ns = digests()
    lines = [BEGIN,
             arr("kRefMaxes", "std::uint32_t", MAXES),
             arr("kRefNotches", "std::uint32_t", NOTCHES),
             arr("kRefPercents", "std::uint32_t", PERCENTS),
             arr("kRefSizes", "std::uint32_t", SIZES),
             "const std::uint32_t kRefSentinel = 0x%XU;" % SENTINEL,
             "const std::uint64_t kRefDigest_render = 0x%016Xull;" % fr,
             "const std::size_t kRefCases_render = %d;" % nr,
             "const std::uint64_t kRefDigest_quality = 0x%016Xull;" % fq,
             "const std::size_t kRefCases_quality = %d;" % nq,
             "const std::uint64_t kRefDigest_scaled = 0x%016Xull;" % fs,
             "const std::size_t kRefCases_scaled = %d;" % ns,
             END]
    return "\n".join(lines) + "\n"


def main(argv):
    block = render_block()
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
