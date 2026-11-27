#!/usr/bin/env python3
"""Independent reference for core/slotmap (not run in CI).

Written from the header comments with Python lists and integers: the arrays gen[256], used[256] and a count, with every rule
spelled out one condition at a time (the C++ is a few lines of loops). It runs a scripted sequence of operations from a linear
congruential generator and a table of hand-built (forged) maps, and prints a C++ block with FNV-1a 64 digests that the tests
recompute. It also prints the handles around the 24-bit generation wrap, computed by plain arithmetic.

  python3 tools/slot_map_ref.py            print the generated C++ block
  python3 tools/slot_map_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/slot_map_ref.py)"
END = "// END GENERATED REFERENCE"

SLOTS = 256
MAXGEN = 0xFFFFFF
M32 = 2**32
M64 = 2**64
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
LCG_MUL = 6364136223846793005
LCG_ADD = 1442695040888963407
N_OPS = 12000
NO_HANDLE = 0xDEADBEEF
COUNTS = [0, 1, 5, 255, 256, 257, 0xFFFFFFFF]
N_USED = 6
N_GEN = 7
HANDLE_INDEXES = [0, 1, 2, 255]
HANDLE_GENS = [0, 1, 2, 0xFFFFFE, 0xFFFFFF]
FORGED_GENS = [0xFFFFFE, 0xFFFFFF, 0, 0x1000005]


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


class Map:
    def __init__(self):
        self.gen = [1] * SLOTS
        self.used = [0] * SLOTS
        self.count = 0

    def copy(self):
        m = Map()
        m.gen = list(self.gen)
        m.used = list(self.used)
        m.count = self.count
        return m

    def init(self):
        self.gen = [1] * SLOTS
        self.used = [0] * SLOTS
        self.count = 0

    def live(self, handle):
        if self.count > SLOTS:
            return False
        g = handle >> 8
        i = handle & 255
        if g == 0:
            return False
        if self.used[i] == 0:
            return False
        return self.gen[i] == g

    def allocate(self):
        """(ok, handle)"""
        if self.count > SLOTS:
            return False, NO_HANDLE
        for i in range(SLOTS):
            if self.used[i] == 0:
                self.used[i] = 1
                self.count += 1
                return True, ((self.gen[i] & MAXGEN) << 8) | i
        return False, NO_HANDLE

    def release(self, handle):
        if not self.live(handle):
            return False
        i = handle & 255
        self.used[i] = 0
        self.gen[i] = 1 if self.gen[i] == MAXGEN else self.gen[i] + 1
        if self.count > 0:
            self.count -= 1
        return True

    def size(self):
        return self.count if self.count <= SLOTS else 0

    def checksum(self):
        s = 0
        for i in range(SLOTS):
            s = (s * 31 + self.gen[i] + 1) % M32
            s = (s * 31 + self.used[i] + 1) % M32
        return (s * 31 + self.count + 1) % M32


def pick_handle(g, hist):
    r = g.u32()
    if hist and r % 4 != 0:
        return hist[g.u32() % len(hist)]
    return g.u32()


def run_ops(stats):
    g = Lcg(61)
    m = Map()
    hist = []
    f = Fnv()
    for _ in range(N_OPS):
        sel = g.u32() % 12
        res = 0
        h = 0
        if sel <= 2:
            ok, h = m.allocate()
            res = 1 if ok else 0
            stats["alloc_ok" if ok else "alloc_fail"] += 1
            if ok:
                hist.append(h)
        elif sel <= 5:
            h = pick_handle(g, hist)
            was = m.gen[h & 255]
            res = 1 if m.release(h) else 0
            stats["release_ok" if res else "release_fail"] += 1
            if res and was == MAXGEN:
                stats["wraps"] += 1
        elif sel == 6:
            h = pick_handle(g, hist)
            res = 1 if m.live(h) else 0
            stats["live_true" if res else "live_false"] += 1
        elif sel == 7:
            a = g.u32() % 5
            b = g.u32() & 255
            h = (a << 8) | b
            res = 1 if m.live(h) else 0
            stats["live_true" if res else "live_false"] += 1
        elif sel == 8:
            res = m.size()
        elif sel <= 10:
            s = g.u32() % SLOTS
            v = g.u32() % 8
            if v <= 3:
                if g.u32() % 2 == 0:
                    s %= 4
                m.gen[s] = FORGED_GENS[v]
            elif v == 4:
                m.used[s] = 2
            elif v == 5:
                m.used[s] = 0
            elif v == 6:
                c = g.u32() % 4
                if g.u32() % 4 == 0:
                    m.count = [0, 255, 256, 257][c]
            else:
                m.gen[s] = g.u32()
            stats["forged"] += 1
        else:
            if g.u32() % 4 == 0:
                m.init()
                stats["inits"] += 1
        if m.count > SLOTS:
            stats["corrupt_steps"] += 1
        f.byte(sel)
        f.u32(res)
        f.u32(h)
        f.u32(m.checksum())
    return f.h


def build_map(count_i, used_i, gen_i):
    m = Map()
    m.count = COUNTS[count_i]
    for i in range(SLOTS):
        if used_i == 0:
            m.used[i] = 0
        elif used_i == 1:
            m.used[i] = 1
        elif used_i == 2:
            m.used[i] = 1 if i < 3 else 0
        elif used_i == 3:
            m.used[i] = 1 if i % 3 == 0 else 0
        elif used_i == 4:
            m.used[i] = 1 if i != 255 else 0
        else:
            m.used[i] = {0: 2, 1: 255, 2: 1}.get(i, 0)
    if gen_i == 1:
        m.gen[0] = 0
    elif gen_i == 2:
        m.gen[0] = 0xFFFFFF
    elif gen_i == 3:
        m.gen[0] = 0xFFFFFE
    elif gen_i == 4:
        m.gen[0] = 0x1000001
    elif gen_i == 5:
        m.gen[0] = 0xFFFFFFFF
    elif gen_i == 6:
        m.gen[0] = 0xFFFFFF
        m.gen[1] = 0xFFFFFF
        m.gen[2] = 2
    return m


def table_handles():
    hs = [(g << 8) | i for i in HANDLE_INDEXES for g in HANDLE_GENS]
    hs.append(0xFFFFFFFF)
    return hs


def run_table():
    f = Fnv()
    cases = 0
    handles = table_handles()
    for c in range(len(COUNTS)):
        for u in range(N_USED):
            for gi in range(N_GEN):
                base = build_map(c, u, gi)
                f.u32(base.size())
                a = base.copy()
                ok, h = a.allocate()
                f.u32(1 if ok else 0)
                f.u32(h)
                f.u32(a.checksum())
                f.u32(1 if a.live(h) else 0)
                cases += 2
                for h in handles:
                    f.u32(1 if base.live(h) else 0)
                    r = base.copy()
                    f.u32(1 if r.release(h) else 0)
                    f.u32(r.checksum())
                    cases += 2
    return f.h, cases


def wrap_probe():
    """Handles of slot 0 after k release and allocate cycles from a fresh map, by plain arithmetic.

    The generation after k cycles is (k mod MAXGEN) + 1, so it is 1 again after exactly MAXGEN cycles."""
    ks = [0, 1, 2, MAXGEN - 2, MAXGEN - 1, MAXGEN, MAXGEN + 1, MAXGEN + 2]
    return ks, [(((k % MAXGEN) + 1) << 8) for k in ks]


def render():
    stats = {k: 0 for k in ["alloc_ok", "alloc_fail", "release_ok", "release_fail", "live_true", "live_false", "wraps", "forged", "inits", "corrupt_steps"]}
    d_ops = run_ops(stats)
    d_tab, n_tab = run_table()
    ks, hs = wrap_probe()
    out = [BEGIN]
    out.append("const std::uint32_t kRefCounts[] = {" + ", ".join("%du" % c for c in COUNTS) + "};")
    out.append("const std::uint32_t kRefWrapCycles[] = {" + ", ".join("%du" % k for k in ks) + "};")
    out.append("const std::uint32_t kRefWrapHandles[] = {" + ", ".join("0x%Xu" % h for h in hs) + "};")
    out.append("const std::size_t kRefOps = %d;" % N_OPS)
    out.append("const std::uint64_t kRefDigest_ops = 0x%016XULL;".replace("ULL", "ull") % d_ops)
    out.append("const std::uint64_t kRefDigest_table = 0x%016XULL;".replace("ULL", "ull") % d_tab)
    out.append("const std::size_t kRefCases_table = %d;" % n_tab)
    for k in ["alloc_ok", "alloc_fail", "release_ok", "release_fail", "live_true", "live_false", "wraps", "forged", "inits", "corrupt_steps"]:
        out.append("const std::size_t kRefStat_%s = %d;" % (k, stats[k]))
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
