#!/usr/bin/env python3
"""Independent reference for core/lifecycle/vsync (not run in CI).

Written from the header comments with Python integers: the window is the `count` slots ending at next - 1, the
median is sorted(window)[(count - 1) // 2], and every operation works on a dict-like object. It prints a C++ block
with the grids and FNV-1a 64 digests (full object state after every step, including stale slots) that the tests
recompute from the implementation.

  python3 tools/vsync_ref.py            print the generated C++ block
  python3 tools/vsync_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/vsync_ref.py)"
END = "// END GENERATED REFERENCE"

U64 = 2**64 - 1
U32 = 2**32 - 1
W = 16
MIN_SAMPLES = 4
GAP = 1000000000
SENTINEL = 0xDEADBEEFCAFEF00D


class Est:
    def __init__(self, deltas=None, count=0, nxt=0, last=0, has=False):
        self.deltas = list(deltas) if deltas else [0] * W
        self.count = count
        self.next = nxt
        self.last = last
        self.has = has

    def valid(self):
        return self.count <= W and self.next < W


def reset(e):
    e.deltas = [0] * W
    e.count = 0
    e.next = 0
    e.last = 0
    e.has = False


def on_vsync(e, ts):
    if not e.valid():
        e.has = False
        e.count = 0
        e.next = 0
    if not e.has:
        e.last = ts
        e.has = True
        return True
    if ts <= e.last:
        return False
    d = ts - e.last
    if d > GAP:
        e.count = 0
        e.next = 0
        e.last = ts
        return True
    e.deltas[e.next] = d
    e.next = (e.next + 1) % W
    e.count = min(W, e.count + 1)
    e.last = ts
    return True


def estimate(e):
    """(ok, value); value is the sentinel when not ok."""
    if not e.valid() or e.count < MIN_SAMPLES:
        return False, SENTINEL
    win = [e.deltas[(e.next - e.count + i) % W] for i in range(e.count)]
    m = sorted(win)[(e.count - 1) // 2]
    if m == 0:
        return False, SENTINEL
    return True, m


def lcg_values(seed, n, mod, add):
    x = seed
    out = []
    for _ in range(n):
        x = (x * 6364136223846793005 + 1442695040888963407) % 2**64
        out.append((x >> 33) % mod + add)
    return out


EXT = [U64, 1, U64 - 1, 2, 2**63, GAP, GAP + 1, 16666667, 3, 0, 1, 7, 100, U64, 5, 9]


def pattern(kind):
    """Delta slots for hand-built objects."""
    if kind == 0:
        return lcg_values(4242, W, 40000000, 1)
    if kind == 1:
        v = lcg_values(4242, W, 40000000, 1)
        return [0 if i % 3 == 0 else v[i] for i in range(W)]
    if kind == 2:
        return [0] * W
    if kind == 3:
        return [i + 1 for i in range(W)]
    if kind == 4:
        return [W - i for i in range(W)]
    return list(EXT)


def seq_timestamps(kind, n):
    if kind == 0:  # steady 60 Hz with jitter
        t, out = 5, []
        for d in lcg_values(99, n, 4000, 16000000):
            t += d
            out.append(t)
        return out
    if kind == 1:  # gaps, duplicates, backwards
        t, out = 1000, []
        steps = lcg_values(7, n, 12, 0)
        for s in steps:
            if s == 0:
                pass
            elif s == 1:
                t = t - 3 if t > 3 else t
            elif s == 2:
                t += GAP
            elif s == 3:
                t += GAP + 1
            elif s == 4:
                t += GAP - 1
            else:
                t += 16666667 + s
            out.append(t)
        return out
    if kind == 2:  # near the top of the range
        out = [U64 - 400 + 3 * i for i in range(n // 3)]
        out += [U64 - 5, U64, U64 - 1, U64]
        return out
    # random deltas up to 2 s, starting at 0
    t, out = 0, [0]
    for d in lcg_values(5, n - 1, 2000000000, 0):
        t += d
        out.append(t)
    return out


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


def feed_est(f, e):
    f.u64(e.last)
    f.byte(1 if e.has else 0)
    f.u32(e.count)
    f.u32(e.next)
    for v in e.deltas:
        f.u64(v)


SEQ_N = 300
SEQ_KINDS = [0, 1, 2, 3]
ES_COUNTS = [0, 1, 2, 3, 4, 5, 8, 15, 16, 17, 255, U32]
ES_NEXTS = [0, 1, 3, 4, 8, 15, 16, 17, U32]
ES_KINDS = [0, 1, 2, 3, 4, 5]
HB_KINDS = [0, 1, 5]
HB_LAST = 1000
HB_TS = [0, 999, 1000, 1001, 1000 + GAP, 1001 + GAP, U64]


def digests():
    out = {}
    f = Fnv(); n = 0
    for kind in SEQ_KINDS:
        e = Est()
        for ts in seq_timestamps(kind, SEQ_N):
            ok = on_vsync(e, ts)
            f.byte(1 if ok else 0)
            feed_est(f, e)
            eok, ev = estimate(e)
            f.byte(1 if eok else 0)
            f.u64(ev)
            n += 1
    out["seq"] = (f.h, n)

    f = Fnv(); n = 0
    for kind in ES_KINDS:
        for c in ES_COUNTS:
            for nx in ES_NEXTS:
                for has in (False, True):
                    e = Est(pattern(kind), c, nx, 12345, has)
                    ok, v = estimate(e)
                    f.byte(1 if ok else 0)
                    f.u64(v)
                    n += 1
    out["estimate"] = (f.h, n)

    f = Fnv(); n = 0
    for kind in HB_KINDS:
        for c in ES_COUNTS:
            for nx in ES_NEXTS:
                for has in (False, True):
                    for ts in HB_TS:
                        e = Est(pattern(kind), c, nx, HB_LAST, has)
                        ok = on_vsync(e, ts)
                        f.byte(1 if ok else 0)
                        feed_est(f, e)
                        n += 1
    out["hand"] = (f.h, n)
    return out


def arr(name, typ, vals, suffix=""):
    return "const %s %s[] = {%s};" % (typ, name, ", ".join("%d%s" % (v, suffix) for v in vals))


def render():
    d = digests()
    L = [BEGIN]
    L.append("const int kRefSeqN = %d;" % SEQ_N)
    L.append(arr("kRefSeqKinds", "int", SEQ_KINDS))
    L.append(arr("kRefEsCounts", "std::uint32_t", ES_COUNTS, "u"))
    L.append(arr("kRefEsNexts", "std::uint32_t", ES_NEXTS, "u"))
    L.append(arr("kRefEsKinds", "int", ES_KINDS))
    L.append(arr("kRefHbKinds", "int", HB_KINDS))
    L.append("const std::uint64_t kRefHbLast = %dull;" % HB_LAST)
    L.append(arr("kRefHbTs", "std::uint64_t", HB_TS, "ull"))
    L.append("const std::uint64_t kRefSentinel = 0x%016Xull;" % SENTINEL)
    for k in ("seq", "estimate", "hand"):
        L.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (k, d[k][0]))
        L.append("const std::size_t kRefCases_%s = %d;" % (k, d[k][1]))
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
