#!/usr/bin/env python3
"""Independent reference for core/lifecycle/render_gate (not run in CI).

Written from the header comments with a dict of three facts and a running flag. It prints a C++ block with FNV-1a 64
digests (the whole object, the return value and the action after every call) that the tests recompute from the
implementation.

  python3 tools/render_gate_ref.py            print the generated C++ block
  python3 tools/render_gate_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/render_gate_ref.py)"
END = "// END GENERATED REFERENCE"

# event number -> (fact, value)
EVENTS = {0: ("surface", True), 1: ("surface", False), 2: ("visible", True), 3: ("visible", False),
          4: ("screen_on", True), 5: ("screen_on", False)}
EVENT_NAMES = ["SurfaceCreated", "SurfaceDestroyed", "VisibleTrue", "VisibleFalse", "ScreenOn", "ScreenOff"]
ACTION_NAMES = ["None", "Start", "Stop"]
INVALID = [6, 7, 255]
UNTOUCHED = 9  # what the test pre-loads into *action
MASK = 2**64 - 1


def should_render(g):
    return g["surface"] and g["visible"] and g["screen_on"]


def apply(g, ev):
    """(ok, action or UNTOUCHED); mutates g."""
    if ev not in EVENTS:
        return False, UNTOUCHED
    fact, val = EVENTS[ev]
    g[fact] = val
    was = g["running"]
    g["running"] = should_render(g)
    if not was and g["running"]:
        return True, 1
    if was and not g["running"]:
        return True, 2
    return True, 0


def combo(c):
    return {"surface": bool(c & 1), "visible": bool(c & 2), "screen_on": bool(c & 4), "running": bool(c & 8)}


class Fnv:
    def __init__(self):
        self.h = 0xCBF29CE484222325

    def byte(self, b):
        self.h = ((self.h ^ (b & 255)) * 0x100000001B3) & MASK


def feed(f, g, ok, act):
    f.byte(1 if ok else 0)
    f.byte(act)
    for k in ("surface", "visible", "screen_on", "running"):
        f.byte(1 if g[k] else 0)


def lcg(seed, n):
    out, x = [], seed
    for _ in range(n):
        x = (x * 6364136223846793005 + 1442695040888963407) & MASK
        out.append(x >> 33)
    return out


SEQ_SEEDS = [1, 2, 3, 4]
SEQ_N = 600


def digests():
    grid = Fnv()
    n_grid = 0
    for c in range(16):
        for ev in list(range(6)) + INVALID:
            g = combo(c)
            ok, act = apply(g, ev)
            feed(grid, g, ok, act)
            n_grid += 1
    sr = Fnv()
    for c in range(16):
        g = combo(c)
        sr.byte(1 if should_render(g) else 0)
    seq = Fnv()
    n_seq = 0
    for seed in SEQ_SEEDS:
        g = {"surface": False, "visible": False, "screen_on": True, "running": False}
        for r in lcg(seed, SEQ_N):
            ev = r % 6 if r % 17 else INVALID[(r >> 8) % 3]
            ok, act = apply(g, ev)
            feed(seq, g, ok, act)
            n_seq += 1
    return grid.h, n_grid, sr.h, seq.h, n_seq


def render():
    grid, n_grid, sr, seq, n_seq = digests()
    lines = [BEGIN,
             "const std::uint64_t kRefSeqSeeds[] = {%s};" % ", ".join("%du" % s for s in SEQ_SEEDS),
             "const std::size_t kRefSeqN = %d;" % SEQ_N,
             "const std::uint8_t kRefInvalid[] = {%s};" % ", ".join("%du" % i for i in INVALID),
             "const std::uint8_t kRefUntouched = %d;" % UNTOUCHED,
             "const std::uint64_t kRefDigest_grid = 0x%016Xull;" % grid,
             "const std::size_t kRefCases_grid = %d;" % n_grid,
             "const std::uint64_t kRefDigest_should = 0x%016Xull;" % sr,
             "const std::uint64_t kRefDigest_seq = 0x%016Xull;" % seq,
             "const std::size_t kRefCases_seq = %d;" % n_seq,
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
