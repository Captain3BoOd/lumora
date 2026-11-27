#!/usr/bin/env python3
"""Independent reference for core/decfmt (not run in CI).

Written from the header text with Python integers as the oracle: format is str(int), parse is int() on a string that was first checked for the
form with a regular expression, then range-checked on the integer. Nothing of the C++ (digit loops, the overflow test) is shared. It prints a
C++ block with value tables and 64-bit digests that the tests recompute.

  fmt    boundary values (0, 1, 9, 10, 11, 99, 100, 101, 10^k and its neighbours, 2^k and its neighbours, the u64 and i64 limits) and 120 LCG
         values for u64 and for i64 (both signs), each with every capacity from 0 to length + 1
  small  every byte string of length 0 to 4 over 12 symbols (22,621 strings) through parse_u64 and parse_i64
  bound  15 decimal strings around 2^63 and 2^64 and 10^19, each with 5 prefixes x 4 zero runs x 4 tails, through both parsers
  rand   400 seeds: a string of 1 to 30 symbols, mostly digits, an optional '-', through both parsers

  python3 tools/decfmt_ref.py            print the generated C++ block
  python3 tools/decfmt_ref.py --check F  fail unless the block in F matches
The random scenarios are NOT exhaustive.
"""
import re
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/decfmt_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
I64_MAX = (1 << 63) - 1
I64_MIN = -(1 << 63)
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
OK, TOOLARGE, BTS, INVALID, OVERFLOW = 0, 2, 3, 4, 5

ALPHA = [0x30, 0x31, 0x35, 0x39, 0x2D, 0x2B, 0x20, 0x78, 0x00, 0x80, 0xFF, 0x2E]
SMALL_MAX = 4
BOUND = [
    "0", "1", "9223372036854775807", "9223372036854775808", "9223372036854775809", "18446744073709551615",
    "18446744073709551616", "18446744073709551617", "9999999999999999999", "10000000000000000000",
    "99999999999999999999", "100000000000000000000", "184467440737095516150", "922337203685477580", "1844674407370955161",
]
PREFIX = ["", "-", "+", "--", "x"]
ZEROS = [0, 1, 5, 30]
TAILS = 4  # none, 'x' at the end, 'x' in the middle of the digits, ' ' at the end
RAND_SEEDS = 400
FMT_RAND = 120


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0
        self.ops = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def next(self):
        self.x = (self.x * 6364136223846793005 + 1442695040888963407) & M64
        return self.x >> 32


def u64_values():
    s = {0, 1, 9, 10, 11, 99, 100, 101, M64, M64 - 1}
    for k in range(20):
        for d in (-1, 0, 1):
            v = 10 ** k + d
            if 0 <= v <= M64:
                s.add(v)
    for k in range(65):
        for d in (-1, 0, 1):
            v = (1 << k) + d
            if 0 <= v <= M64:
                s.add(v)
    g = Lcg(12345)
    for _ in range(FMT_RAND):
        hi, lo = g.next(), g.next()
        s.add(((hi << 32) | lo) >> (g.next() % 64))
    return sorted(s)


def i64_values():
    s = {I64_MIN, I64_MAX}
    for v in u64_values():
        if v <= I64_MAX:
            s.add(v)
            s.add(-v)
    s.add(I64_MIN + 1)
    return sorted(s)


def fold_fmt(f, v):
    text = str(v).encode()
    for cap in range(0, len(text) + 2):
        f.ops += 1
        if cap < len(text):
            f.add(BTS << 32)
        else:
            f.add((OK << 32) | len(text))
            for c in text:
                f.add(c)
    f.cases += 1


FORM_U = re.compile(rb"[0-9]+")
FORM_I = re.compile(rb"-?[0-9]+")


def parse_u(s):
    if len(s) > (1 << 20):
        return TOOLARGE, 0
    if not FORM_U.fullmatch(s):
        return INVALID, 0
    v = int(s)
    return (OVERFLOW, 0) if v > M64 else (OK, v)


def parse_i(s):
    if len(s) > (1 << 20):
        return TOOLARGE, 0
    if not FORM_I.fullmatch(s):
        return INVALID, 0
    v = int(s)
    return (OVERFLOW, 0) if v < I64_MIN or v > I64_MAX else (OK, v)


def fold_parse(f, s):
    for fn in (parse_u, parse_i):
        code, v = fn(s)
        f.ops += 1
        f.add(code << 32)
        if code == OK:
            f.add(v)
    f.cases += 1


def fmt_scenario():
    f = Fold()
    for v in u64_values():
        fold_fmt(f, v)
    for v in i64_values():
        fold_fmt(f, v)
    return f


def small_scenario():
    f = Fold()
    cur = [b""]
    fold_parse(f, b"")
    for _ in range(SMALL_MAX):
        nxt = []
        for p in cur:
            for c in ALPHA:
                nxt.append(p + bytes([c]))
        for s in nxt:
            fold_parse(f, s)
        cur = nxt
    return f


def bound_strings():
    out = []
    for s in BOUND:
        for p in PREFIX:
            for z in ZEROS:
                body = p + "0" * z + s
                out.append(body.encode())
                out.append((body + "x").encode())
                mid = len(p) + z + len(s) // 2
                out.append((body[:mid] + "x" + body[mid:]).encode())
                out.append((body + " ").encode())
    return out


def bound_scenario():
    f = Fold()
    for s in bound_strings():
        fold_parse(f, s)
    return f


def rand_scenario():
    f = Fold()
    for seed in range(1, RAND_SEEDS + 1):
        g = Lcg(seed)
        neg = g.next() % 4 == 0
        n = 1 + g.next() % 30
        out = bytearray(b"-" if neg else b"")
        for _ in range(n):
            r = g.next()
            out.append(0x30 + r % 10 if r % 16 != 0 else ALPHA[(r >> 4) % 12])
        fold_parse(f, bytes(out))
    return f


def render():
    out = [BEGIN]
    out.append("const std::uint8_t kRefAlpha[] = {%s};" % ", ".join("0x%02X" % c for c in ALPHA))
    out.append("const std::uint32_t kRefSmallMax = %d, kRefRandSeeds = %d;" % (SMALL_MAX, RAND_SEEDS))
    out.append("const char* const kRefBound[] = {%s};" % ", ".join('"%s"' % s for s in BOUND))
    out.append("const char* const kRefPrefix[] = {%s};" % ", ".join('"%s"' % s for s in PREFIX))
    out.append("const std::uint32_t kRefZeros[] = {%s};" % ", ".join(str(z) for z in ZEROS))
    out.append("const std::uint64_t kRefU[] = {%s};" % ", ".join("%dull" % v for v in u64_values()))
    out.append("const std::int64_t kRefI[] = {%s};" % ", ".join(("%dll" % v) if v != I64_MIN else "(-9223372036854775807ll - 1)" for v in i64_values()))
    for name, fn in (("fmt", fmt_scenario), ("small", small_scenario), ("bound", bound_scenario), ("rand", rand_scenario)):
        f = fn()
        out.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (name, f.h))
        out.append("const std::size_t kRefCases_%s = %d, kRefOps_%s = %d;" % (name, f.cases, name, f.ops))
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
