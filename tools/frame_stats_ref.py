#!/usr/bin/env python3
"""Independent reference for core/lifecycle/frame_stats (not run in CI).

Written from the header comments with Python integers: the window is taken as the `count` slots ending at next - 1,
sorted with sorted(), the rank is the smallest r with r * 100 >= 99 * count (ceil(0.99 * count), not the
count - count / 100 form the C++ uses), the average is exact_sum clamped to UINT64_MAX divided by count, and the text
comes from Python format strings. It prints a C++ block with the grids and FNV-1a 64 digests that the tests recompute
from the implementation.

  python3 tools/frame_stats_ref.py            print the generated C++ block
  python3 tools/frame_stats_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/frame_stats_ref.py)"
END = "// END GENERATED REFERENCE"

U64 = 2**64 - 1
U32 = 2**32 - 1
W = 120

EXT = [1, U64, 2, U64 - 1, 2**63, 10**9, 16666667, 3]


def lcg_values(seed, n):
    x = seed
    out = []
    for _ in range(n):
        x = (x * 6364136223846793005 + 1442695040888963407) % 2**64
        out.append((x >> 33) % 40000000 + 1)
    return out


def pattern(kind, n=W):
    """Sample values for struct fills: 0 lcg, 1 extremes, 2 lcg with zeros, 3 ascending, 4 descending."""
    if kind == 0:
        return lcg_values(12345, n)
    if kind == 1:
        return [EXT[i % 8] for i in range(n)]
    if kind == 2:
        v = lcg_values(12345, n)
        return [0 if i % 5 == 0 else v[i] for i in range(n)]
    if kind == 3:
        return [i + 1 for i in range(n)]
    return [W - i for i in range(n)]


def seq_frames(kind, n):
    if kind == 0:
        return lcg_values(7, n)
    if kind == 1:
        return [EXT[i % 8] for i in range(n)]
    if kind == 2:
        return [1] * n
    return [U64] * n


class Stats:
    def __init__(self, samples=None, count=0, nxt=0):
        self.samples = list(samples) if samples else [0] * W
        self.count = count
        self.next = nxt

    def valid(self):
        return self.count <= W and self.next < W


def record(st, frame):
    if frame == 0:
        return False
    if not st.valid():
        st.count = 0
        st.next = 0
    st.samples[st.next] = frame
    st.next = (st.next + 1) % W
    st.count = min(W, st.count + 1)
    return True


def summarize(st):
    """(count, avg, worst, p99, fps_tenths)."""
    if not st.valid() or st.count == 0:
        return (0, 0, 0, 0, 0)
    c = st.count
    win = [st.samples[(st.next - c + i) % W] for i in range(c)]
    total = min(U64, sum(win))
    avg = total // c
    s = sorted(win)
    r = 1
    while r * 100 < 99 * c:
        r += 1
    p99 = s[r - 1]
    fps = 0 if avg == 0 else min(U32, 10**10 // avg)
    return (c, avg, max(win), p99, fps)


def ms(ns):
    h = ns // 10000
    return "%d.%02d" % (h // 100, h % 100)


def text(count, fps, avg, p99, worst):
    if count == 0:
        return "no frames"
    return "%d.%d fps  avg %s ms  p99 %s ms  worst %s ms" % (fps // 10, fps % 10, ms(avg), ms(p99), ms(worst))


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


def feed_summary(f, ok, s):
    f.byte(1 if ok else 0)
    f.u32(s[0])
    f.u64(s[1])
    f.u64(s[2])
    f.u64(s[3])
    f.u32(s[4])


def feed_struct(f, st):
    f.u32(st.count)
    f.u32(st.next)
    for v in st.samples:
        f.u64(v)


SEQ_N = 300
SEQ_KINDS = [0, 1, 2, 3]
ST_COUNTS = [0, 1, 2, 3, 50, 98, 99, 100, 101, 119, 120, 121, 255, U32]
ST_NEXTS = [0, 1, 2, 59, 60, 99, 100, 119, 120, 121, U32]
ST_KINDS = [0, 1, 2, 3, 4]
REC_COUNTS = [0, 5, 120, 121, U32]
REC_NEXTS = [0, 5, 119, 120, 121, U32]
REC_FRAMES = [1, U64, 0]
NS_LIST = [0, 1, 9999, 10000, 10001, 16690000, 18200000, 25040000, 999999999, 10**9, 10**9 + 1, 10**15, 2**53,
           U64 - 1, U64]
FPS_LIST = [0, 1, 9, 10, 11, 599, 600, U32]
CNT_LIST = [0, 1, 120, U32]
FULL_FPS = [599, U32]
CAPS = [0, 1, 2, 3, 5, 9, 10, 11, 40, 70, 95, 96, 97, 98, 100, 200]


def digests():
    out = {}
    f = Fnv(); n = 0
    for kind in SEQ_KINDS:
        st = Stats()
        fr = seq_frames(kind, SEQ_N)
        for i, v in enumerate(fr):
            if i % 7 == 6:
                v = 0
            ok = record(st, v)
            feed_summary(f, ok, summarize(st))
            f.u32(st.count)
            f.u32(st.next)
            n += 1
    out["seq"] = (f.h, n)

    f = Fnv(); n = 0
    for kind in ST_KINDS:
        for c in ST_COUNTS:
            for nx in ST_NEXTS:
                st = Stats(pattern(kind), c, nx)
                feed_summary(f, True, summarize(st))
                n += 1
    out["structs"] = (f.h, n)

    f = Fnv(); n = 0
    for c in REC_COUNTS:
        for nx in REC_NEXTS:
            for fr in REC_FRAMES:
                st = Stats(pattern(0), c, nx)
                ok = record(st, fr)
                f.byte(1 if ok else 0)
                feed_struct(f, st)
                n += 1
    out["record"] = (f.h, n)

    f = Fnv(); n = 0

    def one(cnt, fps, a, p, w, caps):
        nonlocal n
        t = text(cnt, fps, a, p, w).encode()
        for cap in caps:
            if cap == 0:
                f.u64(0)
                n += 1
                continue
            buf = [0x7F] * cap
            keep = min(len(t), cap - 1)
            for i in range(keep):
                buf[i] = t[i]
            buf[keep] = 0
            f.u64(len(t))
            for b in buf:
                f.byte(b)
            n += 1

    for fps in FULL_FPS:
        for a in NS_LIST:
            for p in NS_LIST:
                for w in NS_LIST:
                    one(1, fps, a, p, w, CAPS)
    for cnt in CNT_LIST:
        for fps in FPS_LIST:
            for v in NS_LIST:
                one(cnt, fps, v, v, v, CAPS)
    out["format"] = (f.h, n)
    return out


def arr(name, typ, vals, suffix=""):
    return "const %s %s[] = {%s};" % (typ, name, ", ".join("%d%s" % (v, suffix) for v in vals))


def render():
    d = digests()
    L = [BEGIN]
    L.append("const int kRefSeqN = %d;" % SEQ_N)
    L.append(arr("kRefSeqKinds", "int", SEQ_KINDS))
    L.append(arr("kRefStCounts", "std::uint32_t", ST_COUNTS, "u"))
    L.append(arr("kRefStNexts", "std::uint32_t", ST_NEXTS, "u"))
    L.append(arr("kRefStKinds", "int", ST_KINDS))
    L.append(arr("kRefRecCounts", "std::uint32_t", REC_COUNTS, "u"))
    L.append(arr("kRefRecNexts", "std::uint32_t", REC_NEXTS, "u"))
    L.append(arr("kRefRecFrames", "std::uint64_t", REC_FRAMES, "ull"))
    L.append(arr("kRefNsList", "std::uint64_t", NS_LIST, "ull"))
    L.append(arr("kRefFpsList", "std::uint32_t", FPS_LIST, "u"))
    L.append(arr("kRefCntList", "std::uint32_t", CNT_LIST, "u"))
    L.append(arr("kRefFullFps", "std::uint32_t", FULL_FPS, "u"))
    L.append(arr("kRefCaps", "std::size_t", CAPS, "u"))
    for k in ("seq", "structs", "record", "format"):
        L.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (k, d[k][0]))
        L.append("const std::size_t kRefCases_%s = %d;" % (k, d[k][1]))
    L.append(END)
    return "\n".join(L) + "\n"


def main(argv):
    block = render()
    if len(argv) == 3 and argv[1] == "--check":
        text_ = open(argv[2]).read()
        a, b = text_.index(BEGIN), text_.index(END) + len(END) + 1
        if text_[a:b] != block:
            print("reference block differs; regenerate it", file=sys.stderr)
            return 1
        return 0
    sys.stdout.write(block)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
