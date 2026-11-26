#!/usr/bin/env python3
"""Independent reference for core/varint (not run in CI).

Written from the header comments with Python integers. Encoding splits the binary string into 7-bit groups, decoding
scans for the first byte without the continuation bit and then classifies the bytes it found, and zigzag uses the
formulas of the header. It prints a C++ block with FNV-1a 64 digests that the tests recompute from the implementation.

  python3 tools/varint_ref.py            print the generated C++ block
  python3 tools/varint_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/varint_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = 2**64 - 1
S64MIN = -(2**63)
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
LCG_MUL = 6364136223846793005
LCG_ADD = 1442695040888963407
RAND_N = 20000


def encode(v):
    bits = bin(v)[2:]
    groups = []
    while bits:
        groups.append(int(bits[-7:], 2))
        bits = bits[:-7]
    if not groups:
        groups = [0]
    return bytes([g | 0x80 for g in groups[:-1]] + [groups[-1]])


def encoded_size(v):
    return len(encode(v))


def decode(data, length):
    """Returns (ok, value, consumed). Only the first `length` bytes exist."""
    if length == 0:
        return (False, 0, 0)
    window = data[:min(length, len(data))][:10]
    end = None
    for i, b in enumerate(window):
        if b < 0x80:
            end = i
            break
    if end is None:
        return (False, 0, 0)
    enc = window[:end + 1]
    if len(enc) == 10 and enc[9] > 1:
        return (False, 0, 0)
    if len(enc) > 1 and enc[-1] == 0:
        return (False, 0, 0)
    value = 0
    for i, b in enumerate(enc):
        value += (b & 0x7F) << (7 * i)
    return (True, value, len(enc))


def zigzag_encode(v):
    return 2 * v if v >= 0 else 2 * (-(v + 1)) + 1


def zigzag_decode(u):
    return u // 2 if u % 2 == 0 else -(u // 2) - 1


class Fnv:
    def __init__(self):
        self.h = FNV_OFFSET
        self.n = 0

    def byte(self, b):
        self.h = ((self.h ^ (b & 255)) * FNV_PRIME) & M64

    def u64(self, v):
        v &= M64
        for i in range(8):
            self.byte(v >> (8 * i))


def to_s64(u):
    u &= M64
    return u - 2**64 if u >= 2**63 else u


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def next(self):
        self.x = (self.x * LCG_MUL + LCG_ADD) & M64
        return self.x

    def u64(self):
        x = self.next()
        return x >> ((x >> 1) & 63)

    def s64(self):
        x = self.next()
        mag = x >> ((x >> 1) & 63)
        neg = (x >> 9) & 1
        return to_s64((-mag) if neg else mag)


V = [0, 1, 2, 126, 127, 128, 129, 255, 256, 16383, 16384, 16385]
for k in range(3, 10):
    V += [2**(7 * k) - 1, 2**(7 * k), 2**(7 * k) + 1]
V += [2**32 - 1, 2**32, 2**32 + 1, 2**63 - 1, 2**63, 2**63 + 1, M64 - 1, M64]
Z = [0, 1, -1, 2, -2, 63, -63, 64, -64, 65, -65, S64MIN, S64MIN + 1, 2**63 - 1, 2**63 - 2]
for k in range(7, 63, 7):
    Z += [2**k, 2**k - 1, 2**k + 1, -(2**k), -(2**k) - 1, -(2**k) + 1]
ALPHA = [0x00, 0x01, 0x7F, 0x80, 0x81, 0xFF]
LONG_N = [9, 10, 11, 12]
LONG_P = [0x80, 0xFF, 0xA5]
LONG_T = [0x00, 0x01, 0x02, 0x7F, 0x80, 0xFF]


def feed_decode(f, data, length):
    ok, value, used = decode(data, length)
    if not ok:
        f.byte(0)
    else:
        f.byte(1)
        f.byte(used)
        f.u64(value)


def all_values():
    vals = list(V) + list(range(0, 2001))
    g = Lcg(21)
    vals += [g.u64() for _ in range(RAND_N)]
    return vals


def digests():
    out = {}
    f = Fnv(); n = 0
    for v in all_values():
        e = encode(v)
        f.byte(len(e))
        for b in e:
            f.byte(b)
        n += 1
    out["encode"] = (f.h, n)
    f = Fnv(); n = 0
    for b0 in range(256):
        feed_decode(f, bytes([b0]), 1); n += 1
    for b0 in range(256):
        for b1 in range(256):
            feed_decode(f, bytes([b0, b1]), 2); n += 1
    for a in ALPHA:
        for b in ALPHA:
            for c in ALPHA:
                feed_decode(f, bytes([a, b, c]), 3); n += 1
    for ln in LONG_N:
        for p in LONG_P:
            for t in LONG_T:
                data = bytes([p] * (ln - 1) + [t])
                for length in (ln, 10, 9):
                    if length > ln:
                        continue
                    feed_decode(f, data, length); n += 1
    for length in range(0, 4):
        feed_decode(f, bytes([0x80, 0x01, 0x05]), length); n += 1
    out["decode"] = (f.h, n)
    f = Fnv(); n = 0
    for v in all_values():
        e = encode(v) + bytes([0x80, 0x80, 0x03])
        feed_decode(f, e, len(e)); n += 1
    out["decode_rt"] = (f.h, n)
    f = Fnv(); n = 0
    zs = list(Z) + list(range(-2000, 2001))
    g = Lcg(22)
    zs += [g.s64() for _ in range(RAND_N)]
    for z in zs:
        f.u64(zigzag_encode(z)); n += 1
    for u in all_values():
        f.u64(zigzag_decode(u)); n += 1
    out["zigzag"] = (f.h, n)
    f = Fnv(); n = 0
    for z in zs:
        e = encode(zigzag_encode(z))
        f.byte(len(e))
        for b in e:
            f.byte(b)
        n += 1
    out["signed"] = (f.h, n)
    return out


def lit_u(v):
    return "%dull" % v


def lit_s(v):
    if v == S64MIN:
        return "INT64_MIN"
    return "%dll" % v


def render():
    d = digests()
    lines = [BEGIN,
             "const std::uint64_t kRefV[] = {%s};" % ", ".join(lit_u(v) for v in V),
             "const std::int64_t kRefZ[] = {%s};" % ", ".join(lit_s(v) for v in Z),
             "const std::uint8_t kRefAlpha[] = {%s};" % ", ".join("0x%02X" % a for a in ALPHA),
             "const std::uint32_t kRefLongN[] = {%s};" % ", ".join("%du" % a for a in LONG_N),
             "const std::uint8_t kRefLongP[] = {%s};" % ", ".join("0x%02X" % a for a in LONG_P),
             "const std::uint8_t kRefLongT[] = {%s};" % ", ".join("0x%02X" % a for a in LONG_T),
             "const std::size_t kRefRandN = %d;" % RAND_N]
    for name in ("encode", "decode", "decode_rt", "zigzag", "signed"):
        h, n = d[name]
        lines.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (name, h))
        lines.append("const std::size_t kRefCases_%s = %d;" % (name, n))
    lines.append(END)
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
