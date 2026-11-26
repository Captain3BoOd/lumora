#!/usr/bin/env python3
"""Independent reference for core/lifecycle/pacing (not run in CI).

Written from the header comments with Python integers, so nothing overflows and nothing is shared with the C++:
the frame interval uses a "smallest k with k * vsync + tolerance >= desired" formulation rather than quotient and
remainder, and thresholds are min(UINT64_MAX, budget * percent // 100) on unbounded integers. It prints a C++ block
with the grids, the fixed tables and FNV-1a 64 digests that the tests recompute from the implementation.

  python3 tools/pacing_ref.py            print the generated C++ block
  python3 tools/pacing_ref.py --check F  fail unless the block in F matches
"""
import itertools
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/pacing_ref.py)"
END = "// END GENERATED REFERENCE"

U64 = 2**64 - 1
U32 = 2**32 - 1
CAPS = ["Auto", "Fps30", "Fps60", "Fps90", "Fps120"]
CAP_PERIOD = [0, 33333333, 16666667, 11111111, 8333333]
BAD_CAPS = [5, 6, 127, 128, 255]
# Period of one rate rounded half up: floor(1e9 / fps + 1/2), in integers.
for _fps, _p in ((30, 33333333), (60, 16666667), (90, 11111111), (120, 8333333)):
    assert (2 * 10**9 + _fps) // (2 * _fps) == _p


def interval(cap, vsync, saver):
    """None when invalid. cap is an integer."""
    if cap > 4 or vsync == 0:
        return None
    desired = vsync if cap == 0 else CAP_PERIOD[cap]
    if saver:
        desired = max(desired, 33333333)
    desired = max(desired, vsync)
    tol = vsync // 20
    # smallest k >= 1 with k * vsync + tol >= desired, found by a closed form on unbounded integers
    k = max(1, -((tol - desired) // vsync))
    assert k * vsync + tol >= desired and (k == 1 or (k - 1) * vsync + tol < desired)
    return k * vsync


def valid(mx, of, rf, op, fp):
    return 1 <= mx <= 15 and of >= 1 and rf >= 1 and 100 <= op <= 1000 and 10 <= fp <= 100


def thr(budget, pct):
    return min(U64, budget * pct // 100)


def step(st, frame, budget, cfg):
    """st = (notch, over_run, fast_run); returns (ok, st, changed). cfg = (mx, of, rf, op, fp)."""
    mx, of, rf, op, fp = cfg
    if not valid(*cfg) or budget == 0:
        return False, st, None
    notch, o, f = st
    orig = notch
    notch = min(notch, mx)
    if frame > thr(budget, op):
        o = min(U32, o + 1)
        f = 0
        if o >= of and notch < mx:
            notch += 1
            o = 0
    elif frame <= thr(budget, fp):
        f = min(U32, f + 1)
        o = 0
        if f >= rf and notch > 0:
            notch -= 1
            f = 0
    else:
        o = f = 0
    return True, (notch, o, f), notch != orig


def thermal(st, cfg):
    mx = cfg[0]
    if not valid(*cfg):
        return False, st, None
    notch = min(st[0], mx)
    notch = min(notch + 1, mx)
    return True, (notch, 0, 0), notch != st[0]


class Fnv:
    def __init__(self):
        self.h = 0xcbf29ce484222325

    def byte(self, b):
        self.h ^= b & 0xFF
        self.h = (self.h * 0x100000001b3) % 2**64

    def u64(self, v):
        for i in range(8):
            self.byte(v >> (8 * i))

    def u32(self, v):
        for i in range(4):
            self.byte(v >> (8 * i))


# ---- grids ---------------------------------------------------------------------------------------------------
VSYNC_SPECIAL = [4166667, 6944444, 8333333, 8333334, 11111111, 13888889, 16666666, 16666667, 16666668, 20833333,
                 22222222, 33333332, 33333333, 33333334, 33333335, 41666667, 66666666, 66666667, 100000000,
                 2**32, 2**32 + 1, 2**62, 2**63 - 1, 2**63, 2**63 + 1, U64 - 1, U64]
VSYNCS = list(range(0, 401)) + VSYNC_SPECIAL
ALL_CAPS = list(range(5)) + BAD_CAPS

VALID_GRID = [
    [0, 1, 2, 14, 15, 16, U32],  # max_notch
    [0, 1, 2, U32],  # overrun_frames
    [0, 1, 2, U32],  # recover_frames
    [0, 99, 100, 101, 1000, 1001, U32],  # overrun_percent
    [0, 9, 10, 11, 99, 100, 101, U32],  # fast_percent
]

FRAME_CONFIGS = [
    (3, 30, 300, 110, 90),
    (1, 1, 1, 100, 100),
    (15, 2, 3, 1000, 10),
    (2, U32, U32, 100, 10),
    (2, 2, 2, 150, 50),
    (1, 3, 2, 100, 100),
]
BUDGETS = [1, 2, 3, 9, 10, 11, 99, 100, 101, 999, 1000, 1001, 16666667, 10**18, U64 // 1000, U64 // 1000 + 1,
           U64 // 100, U64 // 100 + 1, U64 // 10, U64 // 10 + 1, 2**63, U64 - 1, U64]
START_NOTCHES = [0, 1, 2, 3, 15, 16, U32]
START_RUNS = [0, 1, 2, U32 - 1, U32]
# Frames are taken relative to the two thresholds: the threshold, one below and one above, plus fixed values.
FRAME_OFFSETS = [-1, 0, 1]
FRAME_FIXED = [0, 1, U64 - 1, U64]


def frames_for(budget, cfg):
    out = list(FRAME_FIXED)
    for pct in (cfg[3], cfg[4]):
        t = thr(budget, pct)
        for d in FRAME_OFFSETS:
            v = t + d
            if 0 <= v <= U64:
                out.append(v)
    return sorted(set(out))


# Exhaustive sequences: symbols 0 = overrun frame, 1 = in-between frame, 2 = fast frame, 3 = thermal throttle.
SEQ_BUDGET = 100
SEQ_FRAMES = [111, 100, 90]
SEQ_CONFIGS = [(2, 2, 2, 110, 90), (1, 1, 1, 100, 100), (3, 3, 2, 120, 80)]
SEQ_LEN = 7
SEQ_START_NOTCHES = [0, 1, 2, 3, 5]


def digests():
    out = {}
    f = Fnv()
    n = 0
    for c in ALL_CAPS:
        f.u64(CAP_PERIOD[c] if c < 5 else 0)
        n += 1
    out["cap"] = (f.h, n)

    f = Fnv()
    n = 0
    for v in VSYNCS:
        for c in ALL_CAPS:
            for s in (0, 1):
                r = interval(c, v, s)
                f.byte(0 if r is None else 1)
                if r is not None:
                    f.u64(r)
                n += 1
    out["interval"] = (f.h, n)

    f = Fnv()
    n = 0
    for cfg in itertools.product(*VALID_GRID):
        f.byte(1 if valid(*cfg) else 0)
        n += 1
    out["valid"] = (f.h, n)

    f = Fnv()
    n = 0
    for cfg in FRAME_CONFIGS:
        for b in BUDGETS:
            for fr in frames_for(b, cfg):
                for nt in START_NOTCHES:
                    for orun in START_RUNS:
                        for frun in START_RUNS:
                            ok, st, ch = step((nt, orun, frun), fr, b, cfg)
                            assert ok
                            f.byte(1 if ch else 0)
                            f.u32(st[0])
                            f.u32(st[1])
                            f.u32(st[2])
                            n += 1
    out["frames"] = (f.h, n)

    f = Fnv()
    n = 0
    for cfg in FRAME_CONFIGS:
        for nt in START_NOTCHES:
            for orun in START_RUNS:
                for frun in START_RUNS:
                    ok, st, ch = thermal((nt, orun, frun), cfg)
                    f.byte(1 if ch else 0)
                    f.u32(st[0])
                    f.u32(st[1])
                    f.u32(st[2])
                    n += 1
    out["thermal"] = (f.h, n)

    f = Fnv()
    n = 0
    for cfg in SEQ_CONFIGS:
        for nt in SEQ_START_NOTCHES:
            for seq in itertools.product(range(4), repeat=SEQ_LEN):
                st = (nt, 0, 0)
                for sym in seq:
                    if sym == 3:
                        ok, st, ch = thermal(st, cfg)
                    else:
                        ok, st, ch = step(st, SEQ_FRAMES[sym], SEQ_BUDGET, cfg)
                    f.byte(1 if ch else 0)
                    f.u32(st[0])
                    f.u32(st[1])
                    f.u32(st[2])
                n += 1
    out["seq"] = (f.h, n)
    return out


def arr(name, typ, vals, suffix=""):
    return "const %s %s[] = {%s};" % (typ, name, ", ".join("%d%s" % (v, suffix) for v in vals))


def render():
    d = digests()
    L = [BEGIN]
    L.append(arr("kRefCapPeriods", "std::uint64_t", CAP_PERIOD, "ull"))
    L.append(arr("kRefAllCaps", "std::uint8_t", ALL_CAPS))
    L.append(arr("kRefVsyncs", "std::uint64_t", VSYNCS, "ull"))
    for i, g in enumerate(VALID_GRID):
        L.append(arr("kRefValidGrid%d" % i, "std::uint32_t", g, "u"))
    L.append(arr("kRefBudgets", "std::uint64_t", BUDGETS, "ull"))
    L.append(arr("kRefStartNotches", "std::uint32_t", START_NOTCHES, "u"))
    L.append(arr("kRefStartRuns", "std::uint32_t", START_RUNS, "u"))
    L.append(arr("kRefFrameFixed", "std::uint64_t", FRAME_FIXED, "ull"))
    L.append(arr("kRefFrameOffsets", "int", FRAME_OFFSETS))
    L.append("const RefCfg kRefFrameConfigs[] = {%s};" % ", ".join("{%du, %du, %du, %du, %du}" % c for c in FRAME_CONFIGS))
    L.append("const RefCfg kRefSeqConfigs[] = {%s};" % ", ".join("{%du, %du, %du, %du, %du}" % c for c in SEQ_CONFIGS))
    L.append("const std::uint64_t kRefSeqBudget = %dull;" % SEQ_BUDGET)
    L.append(arr("kRefSeqFrames", "std::uint64_t", SEQ_FRAMES, "ull"))
    L.append("const int kRefSeqLen = %d;" % SEQ_LEN)
    L.append(arr("kRefSeqStartNotches", "std::uint32_t", SEQ_START_NOTCHES, "u"))
    for k, name in (("cap", "cap"), ("interval", "interval"), ("valid", "valid"), ("frames", "frames"),
                    ("thermal", "thermal"), ("seq", "sequences")):
        L.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (name, d[k][0]))
        L.append("const std::size_t kRefCases_%s = %d;" % (name, d[k][1]))
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
