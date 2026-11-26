#!/usr/bin/env python3
"""Independent reference for core/lifecycle/crash_breaker (not run in CI).

A second implementation in plain Python, written from the rules in core/lifecycle/crash_breaker.h and not from the
C++ code. Logs are Python lists, arithmetic on times is done modulo 2**64 only where a test case builds a time from
another one. It prints a C++ block with the sweep parameters and FNV-1a 64 digests over the full result (return
value, count and all 16 slots for record_crash) of every case, which the tests recompute from the C++ functions.

  python3 tools/crash_breaker_ref.py            print the generated C++ block
  python3 tools/crash_breaker_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/crash_breaker_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = 2**64 - 1
CAP = 16
# Small exhaustive sweeps. Logs: every sequence of up to LOG_LEN entries with values 0..LOG_VAL.
LOG_LEN, LOG_VAL = 4, 6
NOW_MAX = 8
WIN_MAX = 4
# decide sweep: logs up to D_LEN entries with values 0..D_VAL, plus a null log as one more choice.
D_LEN, D_VAL = 3, 2
D_NOW, D_WIN = 4, 3          # now 0..D_NOW, window 1..D_WIN
D_MAXC = 4                   # max_crashes 1..D_MAXC, backend_crashes 1..max_crashes
# Policy validity sweep.
P_MAX = 18
P_WINDOWS = [0, 1, M64]
# Wide random cases.
WIDE_CASES = 30000
NOWS = [0, 1, 50, 100, 2**63, M64 - 1, M64]
WINDOWS = [1, 2, 10, 100, 2**32, M64]
COUNTS = list(range(0, 17)) + [17, 100, 2**32 - 1]
SEED = 0x9E3779B97F4A7C15


def logs(length, val):
    out = [[]]
    for n in range(1, length + 1):
        def rec(prefix):
            if len(prefix) == n:
                out.append(list(prefix))
                return
            for v in range(val + 1):
                prefix.append(v)
                rec(prefix)
                prefix.pop()
        rec([])
    return out


def inside(t, now, window):
    age = 0 if t > now else now - t
    return age < window


def recent(slots, count, now, window):
    n = min(count, CAP)
    return sum(1 for i in range(n) if inside(slots[i], now, window))


def valid(mc, bc, window):
    return 1 <= mc <= CAP and 1 <= bc <= mc and window >= 1


def record(slots, count, now, window):
    """Returns (count, slots) after a successful record_crash."""
    entries = list(slots[:min(count, CAP)])
    entries = [t for t in entries if inside(t, now, window)]
    if len(entries) == CAP:
        entries = entries[1:]
    entries.append(now)
    return len(entries), entries + [0] * (CAP - len(entries))


def decide(rv, rg, mc, bc, gles_available):
    if rv + rg >= mc:
        return 1, 0           # Disable, Vulkan
    if gles_available and rv >= bc:
        return 0, 1           # Run, Gles
    return 0, 0               # Run, Vulkan


class Fnv:
    def __init__(self):
        self.h = 0xcbf29ce484222325

    def byte(self, b):
        self.h ^= b & 0xFF
        self.h = (self.h * 0x100000001b3) % 2**64

    def num(self, v, n):
        for i in range(n):
            self.byte((v >> (8 * i)) & 0xFF)

    def log(self, count, slots):
        self.num(count, 4)
        for s in slots:
            self.num(s, 8)


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


def pad(l):
    return l + [0] * (CAP - len(l))


def digests():
    d = {}
    small = logs(LOG_LEN, LOG_VAL)
    f = Fnv()
    n = 0
    for l in small:
        for now in range(NOW_MAX + 1):
            for window in range(1, WIN_MAX + 1):
                c, s = record(pad(l), len(l), now, window)
                f.log(c, s)
                n += 1
    d["record_small"], d["record_small_cases"] = f.h, n
    f = Fnv()
    n = 0
    for l in small:
        for now in range(NOW_MAX + 1):
            for window in range(0, WIN_MAX + 2):
                f.num(recent(pad(l), len(l), now, window), 4)
                n += 1
    d["recent_small"], d["recent_small_cases"] = f.h, n
    dl = logs(D_LEN, D_VAL)
    opts = [None] + dl           # None is a null pointer
    f = Fnv()
    n = 0
    for v in opts:
        for g in opts:
            for now in range(D_NOW + 1):
                for window in range(1, D_WIN + 1):
                    rv = 0 if v is None else recent(pad(v), len(v), now, window)
                    rg = 0 if g is None else recent(pad(g), len(g), now, window)
                    for mc in range(1, D_MAXC + 1):
                        for bc in range(1, mc + 1):
                            for ga in (0, 1):
                                a, b = decide(rv, rg, mc, bc, ga)
                                f.num(a, 1)
                                f.num(b, 1)
                                n += 1
    d["decide_small"], d["decide_small_cases"] = f.h, n
    f = Fnv()
    n = 0
    for mc in range(0, P_MAX + 1):
        for bc in range(0, P_MAX + 1):
            for w in P_WINDOWS:
                f.num(1 if valid(mc, bc, w) else 0, 1)
                n += 1
    d["valid"], d["valid_cases"] = f.h, n
    r = Rng(SEED)
    f = Fnv()
    fr = Fnv()
    for _ in range(WIDE_CASES):
        now = NOWS[r.next() % len(NOWS)]
        window = WINDOWS[r.next() % len(WINDOWS)]
        count = COUNTS[r.next() % len(COUNTS)]
        slots = []
        for _i in range(CAP):
            k = r.next() % 8
            if k == 0:
                v = 0
            elif k == 1:
                v = now
            elif k == 2:
                v = (now - 1) & M64
            elif k == 3:
                v = (now + 1) & M64
            elif k == 4:
                v = M64
            elif k == 5:
                v = (now - window) & M64
            elif k == 6:
                v = (now - window + 1) & M64
            else:
                v = r.next() % 200
            slots.append(v)
        fr.num(recent(slots, count, now, window), 4)
        c, s = record(slots, count, now, window)
        f.log(c, s)
    d["record_wide"], d["recent_wide"] = f.h, fr.h
    return d


def render():
    d = digests()
    L = [BEGIN]
    L.append("constexpr std::uint32_t kRefLogLen = %d, kRefLogVal = %d, kRefNowMax = %d, kRefWinMax = %d;" % (
        LOG_LEN, LOG_VAL, NOW_MAX, WIN_MAX))
    L.append("constexpr std::uint32_t kRefDLen = %d, kRefDVal = %d, kRefDNow = %d, kRefDWin = %d, kRefDMaxC = %d;" % (
        D_LEN, D_VAL, D_NOW, D_WIN, D_MAXC))
    L.append("constexpr std::uint32_t kRefPMax = %d;" % P_MAX)
    L.append("const std::uint64_t kRefPWindows[] = {%s};" % ", ".join("0x%016Xull" % v for v in P_WINDOWS))
    L.append("constexpr std::size_t kRefWideCases = %d;" % WIDE_CASES)
    L.append("const std::uint64_t kRefNows[] = {%s};" % ", ".join("0x%016Xull" % v for v in NOWS))
    L.append("const std::uint64_t kRefWindows[] = {%s};" % ", ".join("0x%016Xull" % v for v in WINDOWS))
    L.append("const std::uint32_t kRefCounts[] = {%s};" % ", ".join("%du" % v if v < 2**31 else "0x%08Xu" % v for v in COUNTS))
    L.append("constexpr std::uint64_t kRefSeed = 0x%016Xull;" % SEED)
    for k in ("record_small", "recent_small", "decide_small", "valid", "record_wide", "recent_wide"):
        L.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (k, d[k]))
    for k in ("record_small", "recent_small", "decide_small", "valid"):
        L.append("const std::size_t kRefCases_%s = %d;" % (k, d[k + "_cases"]))
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
