#!/usr/bin/env python3
"""Independent reference for core/timing/fixed_step (not run in CI).

A second implementation in plain Python with unbounded integers, written from the rules in
core/timing/fixed_step.h and not from the C++ code. Saturation is an explicit min() against 2**64 - 1, and the
interpolation factor is the exact big-integer floor(accumulator * 65536 / step_ns). It prints a C++ block with the
sweep parameters and FNV-1a 64 digests over the full result of every case, which the tests recompute from the C++
functions.

  python3 tools/fixed_step_ref.py            print the generated C++ block
  python3 tools/fixed_step_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/fixed_step_ref.py)"
END = "// END GENERATED REFERENCE"

MAX = 2**64 - 1
# Small exhaustive sweep (inclusive upper bounds); loop order step, max_steps, max_delta, acc, dropped, delta.
S_STEP, S_MAXSTEPS, S_MAXDELTA, S_ACC, S_DELTA = 8, 4, 24, 24, 30
S_DROPPED = [0, MAX - 2]
# Extreme grid.
E = [0, 1, 2, 3, 7, 65535, 65536, 65537, 2**32 - 1, 2**32, 2**32 + 1, 2**63 - 1, 2**63, 2**63 + 1, MAX - 1, MAX]
X_MAXSTEPS = [1, 5, 2**32 - 1]
X_DROPPED = [0, 5, MAX - 2]
ALPHA_CASES = 20000
SEED = 0x9E3779B97F4A7C15
M64 = MAX


def advance(acc, dropped, delta, step, max_steps, max_delta):
    """Returns (acc, dropped, steps, alpha) for a valid config."""
    clamped = min(delta, max_delta)
    dropped = min(MAX, dropped + (delta - clamped))
    acc = min(MAX, acc + clamped)
    wanted = acc // step
    steps = min(wanted, max_steps)
    acc -= steps * step
    if wanted > max_steps:
        rem = acc % step
        dropped = min(MAX, dropped + (acc - rem))
        acc = rem
    alpha = acc * 65536 // step
    assert 0 <= alpha < 65536 and acc < step
    return acc, dropped, steps, alpha


class Fnv:
    def __init__(self):
        self.h = 0xcbf29ce484222325

    def byte(self, b):
        self.h ^= b & 0xFF
        self.h = (self.h * 0x100000001b3) % 2**64

    def num(self, v, n):
        for i in range(n):
            self.byte((v >> (8 * i)) & 0xFF)

    def case(self, r):
        a, d, s, al = r
        self.num(a, 8)
        self.num(d, 8)
        self.num(s, 4)
        self.num(al, 4)


class Rng:
    """xorshift64*, the same in the tests."""

    def __init__(self, seed):
        self.s = seed

    def next(self):
        x = self.s
        x ^= x >> 12
        x ^= (x << 25) & M64
        x ^= x >> 27
        self.s = x
        return (x * 0x2545F4914F6CDD1D) & M64


def digests():
    f = Fnv()
    n_small = 0
    for step in range(1, S_STEP + 1):
        for ms in range(1, S_MAXSTEPS + 1):
            for md in range(0, S_MAXDELTA + 1):
                for acc in range(0, S_ACC + 1):
                    for dr in S_DROPPED:
                        for delta in range(0, S_DELTA + 1):
                            f.case(advance(acc, dr, delta, step, ms, md))
                            n_small += 1
    small = f.h
    f = Fnv()
    n_ext = 0
    for step in E[1:]:
        for ms in X_MAXSTEPS:
            for md in E:
                for acc in E:
                    for dr in X_DROPPED:
                        for delta in E:
                            f.case(advance(acc, dr, delta, step, ms, md))
                            n_ext += 1
    ext = f.h
    f = Fnv()
    r = Rng(SEED)
    for _ in range(ALPHA_CASES):
        bits = 1 + r.next() % 64
        step = r.next() >> (64 - bits)
        if step == 0:
            step = 1
        acc = r.next() % step
        a = advance(acc, 0, 0, step, 1, 0)
        assert a[0] == acc and a[2] == 0
        f.num(a[3], 4)
    alpha = f.h
    return {"small": small, "small_cases": n_small, "ext": ext, "ext_cases": n_ext, "alpha": alpha}


def render():
    d = digests()
    L = [BEGIN]
    L.append("const std::uint64_t kRefExtremes[] = {%s};" % ", ".join(
        "0x%016Xull" % v for v in E))
    L.append("const std::uint32_t kRefExtMaxSteps[] = {%s};" % ", ".join("%dull" % v for v in X_MAXSTEPS))
    L.append("const std::uint64_t kRefExtDropped[] = {%s};" % ", ".join("0x%016Xull" % v for v in X_DROPPED))
    L.append("const std::uint64_t kRefSmallDropped[] = {%s};" % ", ".join("0x%016Xull" % v for v in S_DROPPED))
    L.append("constexpr std::uint64_t kRefSmallStep = %d, kRefSmallMaxSteps = %d, kRefSmallMaxDelta = %d, "
             "kRefSmallAcc = %d, kRefSmallDelta = %d;" % (S_STEP, S_MAXSTEPS, S_MAXDELTA, S_ACC, S_DELTA))
    L.append("constexpr std::uint64_t kRefAlphaSeed = 0x%016Xull;" % SEED)
    L.append("constexpr std::size_t kRefAlphaCases = %d;" % ALPHA_CASES)
    L.append("const std::uint64_t kRefDigest_small = 0x%016Xull;" % d["small"])
    L.append("const std::uint64_t kRefDigest_extreme = 0x%016Xull;" % d["ext"])
    L.append("const std::uint64_t kRefDigest_alpha = 0x%016Xull;" % d["alpha"])
    L.append("const std::size_t kRefSmallCases = %d;" % d["small_cases"])
    L.append("const std::size_t kRefExtremeCases = %d;" % d["ext_cases"])
    L.append(END)
    return "\n".join(L) + "\n"


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
