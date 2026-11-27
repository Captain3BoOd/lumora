#!/usr/bin/env python3
"""Independent reference for core/ring (not run in CI).

Written from the header comments with Python lists and integers: a data array of 4096 bytes plus head and count, with every
index computed as (head + k) % 4096 one byte at a time (the C++ copies in at most two contiguous segments). It runs a scripted
sequence of operations from a linear congruential generator, and a table of valid and invalid rings, and prints a C++ block with
FNV-1a 64 digests that the tests recompute.

  python3 tools/byte_ring_ref.py            print the generated C++ block
  python3 tools/byte_ring_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/byte_ring_ref.py)"
END = "// END GENERATED REFERENCE"

CAP = 4096
M32 = 2**32
M64 = 2**64
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
LCG_MUL = 6364136223846793005
LCG_ADD = 1442695040888963407
N_OPS = 4000
HEADS = [0, 1, 2047, 4094, 4095, 4096, 4097, 0xFFFFFFFF]
COUNTS = [0, 1, 2048, 4095, 4096, 4097, 0xFFFFFFFF]
NS = [0, 1, 100, 4096, 0xFFFFFFFF]
OFFS = [0, 1, 2048, 4095, 4096, 0xFFFFFFFF]


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


def pattern_data():
    return [(i * 13 + 7) & 255 for i in range(CAP)]


def checksum(seq):
    s = 0
    for b in seq:
        s = (s * 31 + b + 1) % M32
    return s


class Ring:
    def __init__(self, head=0, count=0, data=None):
        self.head = head
        self.count = count
        self.data = list(data) if data is not None else pattern_data()

    def valid(self):
        return self.head < CAP and self.count <= CAP

    def size(self):
        return self.count if self.valid() else 0

    def free(self):
        return CAP - self.count if self.valid() else 0

    def write(self, src, n):
        if not self.valid() or n == 0:
            return 0
        k = min(n, CAP - self.count)
        for i in range(k):
            self.data[(self.head + self.count + i) % CAP] = src[i]
        self.count += k
        return k

    def read(self, n):
        """(count, bytes)"""
        if not self.valid() or n == 0:
            return 0, []
        k = min(n, self.count)
        out = [self.data[(self.head + i) % CAP] for i in range(k)]
        self.head = (self.head + k) % CAP
        self.count -= k
        return k, out

    def peek(self, n, offset):
        if not self.valid() or n == 0 or offset >= self.count:
            return 0, []
        k = min(n, self.count - offset)
        return k, [self.data[(self.head + offset + i) % CAP] for i in range(k)]

    def skip(self, n):
        k, _ = self.read(n)
        return k

    def reset(self):
        self.head = 0
        self.count = 0

    def live(self):
        return [self.data[(self.head + i) % CAP] for i in range(self.count)] if self.valid() else []


def gen_n(g):
    kind = g.u32() % 7
    if kind == 0:
        return 0
    v = g.u32()
    if kind == 1:
        return v % 8
    if kind == 2:
        return v % 64
    if kind == 3:
        return v % 1024
    if kind == 4:
        return v % 5000
    if kind == 5:
        return 4090 + v % 12
    return (M32 - 1 - v % 3)


def gen_offset(g, count):
    kind = g.u32() % 5
    if kind == 0:
        return 0
    v = g.u32()
    if kind == 1:
        return v % 16
    if kind == 2:
        return v % (count + 1)
    if kind == 3:
        return (count + v % 3 - 1) % M32
    return (M32 - 1 - v % 3)


def run_ops():
    f = Fnv()
    g = Lcg(51)
    r = Ring()
    cases = 0
    for i in range(N_OPS):
        sel = g.u32() % 8
        res = 0
        ck = 0
        if sel <= 2:
            n = gen_n(g)
            base = g.u32()
            src = [(base + j * 7) & 255 for j in range(CAP)]
            res = r.write(src, n)
        elif sel <= 4:
            n = gen_n(g)
            res, out = r.read(n)
            ck = checksum(out)
        elif sel == 5:
            n = gen_n(g)
            off = gen_offset(g, r.count)
            res, out = r.peek(n, off)
            ck = checksum(out)
        elif sel == 6:
            res = r.skip(gen_n(g))
        else:
            sub = g.u32() % 16
            if sub == 0:
                r.reset()
            elif sub == 1:
                r.head = g.u32() % CAP
                v = g.u32()
                r.count = v % (CAP + 1) if v % 3 == 0 else 0
            else:
                res = r.size()
                ck = r.free()
        f.byte(sel)
        f.u32(res)
        f.u32(r.head)
        f.u32(r.count)
        f.u32(ck)
        f.u32(checksum(r.live()))
        if i % 64 == 63:
            f.u32(checksum(r.data))
        cases += 1
    return f.h, cases


def run_table():
    f = Fnv()
    cases = 0
    src = [(j * 5 + 3) & 255 for j in range(CAP)]
    for head in HEADS:
        for count in COUNTS:
            def fresh():
                return Ring(head, count)

            r = fresh()
            f.u32(r.size()); f.u32(r.free()); cases += 2
            for n in NS:
                r = fresh()
                res = r.write(src, n)
                f.u32(res); f.u32(r.head); f.u32(r.count); f.u32(checksum(r.data)); cases += 1
            for n in NS:
                r = fresh()
                res, out = r.read(n)
                buf = out + [0xEE] * 4
                f.u32(res); f.u32(r.head); f.u32(r.count); f.u32(checksum(buf[: min(res + 4, CAP)])); cases += 1
            for n in NS:
                for off in OFFS:
                    r = fresh()
                    res, out = r.peek(n, off)
                    buf = out + [0xEE] * 4
                    f.u32(res); f.u32(r.head); f.u32(r.count); f.u32(checksum(buf[: min(res + 4, CAP)])); cases += 1
            for n in NS:
                r = fresh()
                res = r.skip(n)
                f.u32(res); f.u32(r.head); f.u32(r.count); cases += 1
            r = fresh()
            r.reset()
            f.u32(r.head); f.u32(r.count); f.u32(checksum(r.data)); cases += 1
    return f.h, cases


def self_check():
    r = Ring(4090, 0)
    src = list(range(20))
    assert r.write(src, 20) == 20
    assert r.head == 4090 and r.count == 20
    assert r.data[4095] == 5 and r.data[0] == 6 and r.data[13] == 19
    k, out = r.read(7)
    assert k == 7 and out == list(range(7)) and r.head == 1 and r.count == 13
    k, out = r.peek(100, 3)
    assert k == 10 and out == list(range(10, 20)) and r.count == 13
    assert r.skip(100) == 13 and r.count == 0 and r.head == 14
    full = Ring(4095, 4096)
    assert full.write(src, 5) == 0 and full.free() == 0 and full.size() == 4096
    bad = Ring(4096, 0)
    assert bad.size() == 0 and bad.free() == 0 and bad.write(src, 5) == 0 and bad.read(5)[0] == 0
    assert Ring(0, 4097).size() == 0


def render():
    self_check()
    hops, nops = run_ops()
    htab, ntab = run_table()
    def arr(name, vals, typ="std::uint32_t"):
        return "const %s %s[] = {%s};" % (typ, name, ", ".join("%du" % v for v in vals))
    lines = [BEGIN,
             arr("kRefHeads", HEADS), arr("kRefCounts", COUNTS), arr("kRefNs", NS), arr("kRefOffs", OFFS),
             "const std::size_t kRefOps = %d;" % N_OPS,
             "const std::uint64_t kRefDigest_ops = 0x%016Xull;" % hops,
             "const std::size_t kRefCases_ops = %d;" % nops,
             "const std::uint64_t kRefDigest_table = 0x%016Xull;" % htab,
             "const std::size_t kRefCases_table = %d;" % ntab,
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
