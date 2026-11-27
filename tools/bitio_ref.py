#!/usr/bin/env python3
"""Independent reference for core/bitio (not run in CI).

Written from the header comments with Python integers: a buffer is a list of bytes and every field is moved one bit at a time
(bit p of the buffer is bit p % 8 of byte p // 8, value bit i goes to position pos + i). It runs a table of small buffers, a table of
huge caps with a small dummy buffer (only operations that never reach past that buffer are included), and a scripted random
sequence, and prints a C++ block with FNV-1a 64 digests that the tests recompute.

  python3 tools/bitio_ref.py            print the generated C++ block
  python3 tools/bitio_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/bitio_ref.py)"
END = "// END GENERATED REFERENCE"

KMAX = 1 << 28
M32 = 2**32
M64 = 2**64
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
LCG_MUL = 6364136223846793005
LCG_ADD = 1442695040888963407

CAPS = [0, 1, 2, 5, 9]
BACKGROUNDS = 3
VALUES = [0, 1, 0xFFFFFFFF, 0xAAAAAAAA, 0x12345678]
NBITS = list(range(0, 35)) + [0xFFFFFFFF]
LIMIT_CAPS = [0, 1, 2, 3, KMAX - 1, KMAX, KMAX + 1, 0xFFFFFFFF]
LIMIT_POS = [0, 1, 7, 8, 9, 31, 32, 33, 63, 64, 65, 2**31 - 33, 2**31 - 32, 2**31 - 1, 2**31, 2**31 + 1, 2**32 - 1]
LIMIT_NBITS = [0, 1, 2, 31, 32, 33, 0xFFFFFFFF]
DUMMY = 16
N_SEQ = 100
N_STEPS = 40


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


def checksum(buf):
    s = 0
    for b in buf:
        s = (s * 31 + b + 1) % M32
    return s


def valid(cap, pos):
    return cap <= KMAX and pos <= 8 * cap


def write_bits(buf, cap, pos, value, nbits):
    """(ok, new_pos); buf (a list of bytes) is changed in place on success"""
    if not valid(cap, pos) or nbits > 32 or pos + nbits > 8 * cap:
        return False, pos
    for i in range(nbits):
        p = pos + i
        bit = (value >> i) & 1
        if bit:
            buf[p // 8] |= 1 << (p % 8)
        else:
            buf[p // 8] &= ~(1 << (p % 8)) & 255
    return True, pos + nbits


def read_bits(buf, cap, pos, nbits):
    """(ok, new_pos, out)"""
    if not valid(cap, pos) or nbits > 32 or pos + nbits > 8 * cap:
        return False, pos, None
    out = 0
    for i in range(nbits):
        p = pos + i
        out |= ((buf[p // 8] >> (p % 8)) & 1) << i
    return True, pos + nbits, out


def align(cap, pos):
    if not valid(cap, pos):
        return False, pos
    while pos % 8:
        pos += 1
    return True, pos


def bits_left(cap, pos):
    return 8 * cap - pos if valid(cap, pos) else 0


def background(cap, kind):
    if kind == 0:
        return [0] * cap
    if kind == 1:
        return [255] * cap
    return [(i * 37 + 11) & 255 for i in range(cap)]


def positions(cap):
    return range(0, min(8 * cap + 3, 75))


def run_table(stats):
    f = Fnv()
    cases = 0
    for cap in CAPS:
        for bg in range(BACKGROUNDS):
            for pos in positions(cap):
                for nbits in NBITS:
                    for value in VALUES:
                        buf = background(cap, bg)
                        ok, np_ = write_bits(buf, cap, pos, value, nbits)
                        f.u32(1 if ok else 0)
                        f.u32(np_)
                        f.u32(checksum(buf))
                        cases += 1
                        stats["write_ok" if ok else "write_fail"] += 1
                    buf = background(cap, bg)
                    ok, np_, out = read_bits(buf, cap, pos, nbits)
                    f.u32(1 if ok else 0)
                    f.u32(np_)
                    f.u32(out if ok else 0xDEADBEEF)
                    cases += 1
                    stats["read_ok" if ok else "read_fail"] += 1
                ok, np_ = align(cap, pos)
                f.u32(1 if ok else 0)
                f.u32(np_)
                f.u32(bits_left(cap, pos))
                cases += 1
    return f.h, cases


def limit_positions(cap):
    ps = set(LIMIT_POS)
    for d in (-33, -32, -1, 0, 1):
        v = 8 * cap + d
        if 0 <= v < M32:
            ps.add(v)
    return sorted(ps)


def run_limit():
    f = Fnv()
    cases = 0
    for cap in LIMIT_CAPS:
        for pos in limit_positions(cap):
            ok, np_ = align(cap, pos)
            f.u32(1 if ok else 0)
            f.u32(np_)
            f.u32(bits_left(cap, pos))
            cases += 1
            for nbits in LIMIT_NBITS:
                fails = (not valid(cap, pos)) or nbits > 32 or pos + nbits > 8 * cap
                if not (fails or nbits == 0 or pos + nbits <= 8 * DUMMY):
                    continue  # would reach past the dummy buffer
                buf = background(DUMMY, 2)
                ok, np_ = write_bits(buf, cap, pos, 0xFFFFFFFF, nbits)
                f.u32(1 if ok else 0)
                f.u32(np_)
                f.u32(checksum(buf))
                buf = background(DUMMY, 2)
                ok, np_, out = read_bits(buf, cap, pos, nbits)
                f.u32(1 if ok else 0)
                f.u32(np_)
                f.u32(out if ok else 0xDEADBEEF)
                cases += 2
    return f.h, cases


def run_random(stats):
    g = Lcg(81)
    f = Fnv()
    for _ in range(N_SEQ):
        cap = g.u32() % 41
        buf = [g.u32() & 255 for _ in range(cap)]
        pos = 0
        for _ in range(N_STEPS):
            sel = g.u32() % 7
            res = 0
            out = 0
            if sel <= 1:
                nbits = g.u32() % 34
                value = g.u32()
                ok, pos = write_bits(buf, cap, pos, value, nbits)
                res = 1 if ok else 0
                out = value
                stats["rnd_write_ok" if ok else "rnd_write_fail"] += 1
            elif sel <= 3:
                nbits = g.u32() % 34
                ok, pos, out = read_bits(buf, cap, pos, nbits)
                res = 1 if ok else 0
                if not ok:
                    out = 0xDEADBEEF
                stats["rnd_read_ok" if ok else "rnd_read_fail"] += 1
            elif sel == 4:
                ok, pos = align(cap, pos)
                res = 1 if ok else 0
                stats["rnd_align_ok" if ok else "rnd_align_fail"] += 1
            elif sel == 5:
                out = bits_left(cap, pos)
            else:
                pos = g.u32() % (8 * cap + 3)
            f.byte(sel)
            f.u32(res)
            f.u32(out)
            f.u32(pos)
            f.u32(checksum(buf))
    return f.h


STAT_KEYS = ["write_ok", "write_fail", "read_ok", "read_fail", "rnd_write_ok", "rnd_write_fail", "rnd_read_ok", "rnd_read_fail",
             "rnd_align_ok", "rnd_align_fail"]


def render():
    stats = {k: 0 for k in STAT_KEYS}
    d_tab, n_tab = run_table(stats)
    d_lim, n_lim = run_limit()
    d_rnd = run_random(stats)
    out = [BEGIN]
    out.append("const std::uint32_t kRefCaps[] = {" + ", ".join("%du" % c for c in CAPS) + "};")
    out.append("const std::uint32_t kRefValues[] = {" + ", ".join("%du" % v for v in VALUES) + "};")
    out.append("const std::uint32_t kRefLimitCaps[] = {" + ", ".join("%du" % c for c in LIMIT_CAPS) + "};")
    out.append("const std::uint32_t kRefLimitPos[] = {" + ", ".join("%du" % p for p in LIMIT_POS) + "};")
    out.append("const std::uint32_t kRefLimitNbits[] = {" + ", ".join("%du" % n for n in LIMIT_NBITS) + "};")
    out.append("const std::uint64_t kRefDigest_table = 0x%016Xull;" % d_tab)
    out.append("const std::size_t kRefCases_table = %d;" % n_tab)
    out.append("const std::uint64_t kRefDigest_limit = 0x%016Xull;" % d_lim)
    out.append("const std::size_t kRefCases_limit = %d;" % n_lim)
    out.append("const std::uint64_t kRefDigest_random = 0x%016Xull;" % d_rnd)
    for k in STAT_KEYS:
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
