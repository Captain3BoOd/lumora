#!/usr/bin/env python3
"""Independent reference for core/utf8 (not run in CI).

Written from the header text with Python integers. Well-formedness is a table of byte ranges per lead byte (the Unicode table of
well-formed sequences), decoding uses shifts and masks, and an offset is the lead byte of the first ill-formed sequence. The codecs module
is not used to produce any value. It runs every 1-byte and 2-byte input, every 3-byte input whose third byte is in a grid, every 4-byte
input whose third and fourth bytes are in the grid (the 3-byte and 4-byte parts are NOT exhaustive), every scalar value through encode, a
table of buffer sizes, and fixed ill-formed vectors, and prints a C++ block with 64-bit digests that the tests recompute.

  python3 tools/utf8_ref.py            print the generated C++ block
  python3 tools/utf8_ref.py --check F  fail unless the block in F matches
  python3 tools/utf8_ref.py --codecs   extra: compare with Python's strict utf-8 codec; prints what it covered and any offset differences
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/utf8_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = 2**64
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3

OK, NULL, TOO_LARGE, SMALL, MALFORMED = 0, 1, 2, 3, 4
KMAX_CP = 0x10FFFF
GRID = [0x00, 0x7F, 0x80, 0xBF, 0xC0]
EXTRA_CPS = [0x110000, 0x110001, 0x1FFFFF, 0x80000000, 0xFFFFFFFF]
CAP_CPS = [0, 0x7F, 0x80, 0x7FF, 0x800, 0xD7FF, 0xD800, 0xDFFF, 0xE000, 0xFFFF, 0x10000, 0x10FFFF, 0x110000, 0xFFFFFFFF]
CAPS = [0, 1, 2, 3, 4, 5]

# lead byte -> list of (lo, hi) ranges for the continuation bytes
def ranges(b):
    if 0xC2 <= b <= 0xDF:
        return [(0x80, 0xBF)]
    if b == 0xE0:
        return [(0xA0, 0xBF), (0x80, 0xBF)]
    if b == 0xED:
        return [(0x80, 0x9F), (0x80, 0xBF)]
    if 0xE1 <= b <= 0xEF:
        return [(0x80, 0xBF)] * 2
    if b == 0xF0:
        return [(0x90, 0xBF), (0x80, 0xBF), (0x80, 0xBF)]
    if 0xF1 <= b <= 0xF3:
        return [(0x80, 0xBF)] * 3
    if b == 0xF4:
        return [(0x80, 0x8F), (0x80, 0xBF), (0x80, 0xBF)]
    return None


def seq(b, i):
    """(length, code point) of the well-formed sequence at b[i], or (0, 0)."""
    first = b[i]
    if first < 0x80:
        return 1, first
    r = ranges(first)
    if r is None or i + 1 + len(r) > len(b):
        return 0, 0
    cp = first & (0x7F >> (len(r) + 1))
    for k, (lo, hi) in enumerate(r):
        c = b[i + 1 + k]
        if not lo <= c <= hi:
            return 0, 0
        cp = (cp << 6) | (c & 0x3F)
    return len(r) + 1, cp


def validate(b):
    i = 0
    while i < len(b):
        l, _ = seq(b, i)
        if l == 0:
            return MALFORMED, i
        i += l
    return OK, len(b)


def decode(b):
    if len(b) == 0:
        return MALFORMED, 0, 0
    l, cp = seq(b, 0)
    if l == 0:
        return MALFORMED, 0, 0
    return OK, cp, l


def encode(cp, cap, dst_null=False):
    """(status, written bytes) for encode_one."""
    if dst_null and cap > 0:
        return NULL, b""
    if cp > KMAX_CP or 0xD800 <= cp <= 0xDFFF:
        return MALFORMED, b""
    if cp <= 0x7F:
        out = bytes([cp])
    elif cp <= 0x7FF:
        out = bytes([0xC0 | (cp >> 6), 0x80 | (cp & 0x3F)])
    elif cp <= 0xFFFF:
        out = bytes([0xE0 | (cp >> 12), 0x80 | ((cp >> 6) & 0x3F), 0x80 | (cp & 0x3F)])
    else:
        out = bytes([0xF0 | (cp >> 18), 0x80 | ((cp >> 12) & 0x3F), 0x80 | ((cp >> 6) & 0x3F), 0x80 | (cp & 0x3F)])
    if len(out) > cap:
        return SMALL, b""
    return OK, out


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.n = 0

    def add(self, v):
        self.n += 1
        self.h = ((self.h ^ v) * FNV_PRIME) % M64


def pack(b):
    vs, off = validate(b)
    ds, cp, ln = decode(b)
    return vs | (off << 4) | (ds << 8) | (ln << 12) | (cp << 16)


def inputs():
    for b0 in range(256):
        yield 1, bytes([b0])
    for b0 in range(256):
        for b1 in range(256):
            yield 2, bytes([b0, b1])
    for b0 in range(256):
        for b1 in range(256):
            for b2 in GRID:
                yield 3, bytes([b0, b1, b2])
    for b0 in range(256):
        for b1 in range(256):
            for b2 in GRID:
                for b3 in GRID:
                    yield 4, bytes([b0, b1, b2, b3])


# ill-formed classes (each starts at offset 0 of the class) and the contexts they are placed in
CLASSES = [
    "C0 80", "C1 BF", "E0 80 80", "E0 9F BF", "ED A0 80", "ED BF BF", "F0 80 80 80", "F0 8F BF BF", "F4 90 80 80", "F4 BF BF BF",
    "F5 80 80 80", "FF", "FE", "80", "BF", "C2", "C2 41", "E1 80", "E1 80 41", "E0 A0", "E0 A0 41", "F1 80 80", "F1 80 80 41",
    "F0 90 80", "F0 90 80 41", "F4 8F BF", "ED 9F", "E1 41 80", "C2 C2 80", "E2 82 41", "F8 88 80 80 80", "C0", "E0", "F0", "F4",
]
PREFIXES = ["", "41", "41 42 E2 82 AC"]
SUFFIXES = ["", "5A", "C3 A9 5A"]
VALID = ["", "00", "41", "7F", "C2 80", "DF BF", "E0 A0 80", "E1 80 80", "EC BF BF", "ED 80 80", "ED 9F BF", "EE 80 80", "EF BF BF", "EF BF BE",
         "EF BB BF", "F0 90 80 80", "F1 80 80 80", "F3 BF BF BF", "F4 80 80 80", "F4 8F BF BF", "E2 82 AC", "41 C3 A9 E2 82 AC F0 9F 98 80 5A"]


def hexb(s):
    return bytes(int(x, 16) for x in s.split())


def vectors():
    out = []
    for v in VALID:
        out.append(hexb(v))
    for c in CLASSES:
        for p in PREFIXES:
            for s in SUFFIXES:
                out.append(hexb((p + " " + c + " " + s).strip()))
    return out


def render():
    f = {k: Fold() for k in ("l1", "l2", "l3", "l4", "enc", "cap", "vec")}
    for ln, b in inputs():
        f["l%d" % ln].add(pack(b))
    for cp in list(range(0, KMAX_CP + 1)) + EXTRA_CPS:
        st, out = encode(cp, 4)
        f["enc"].add(st | (len(out) << 4) | (int.from_bytes(out.ljust(4, b"\0"), "little") << 8))
    for cp in CAP_CPS:
        for cap in CAPS:
            for dn in ([False, True] if cap == 0 else [False]):
                st, out = encode(cp, cap, dn)
                f["cap"].add(st | (len(out) << 4) | (int(dn) << 8) | (cap << 12))
    vecs = vectors()
    for b in vecs:
        f["vec"].add(pack(b))
    out = [BEGIN]
    out.append("const std::uint32_t kRefGrid[] = {" + ", ".join("0x%02Xu" % g for g in GRID) + "};")
    out.append("const std::uint32_t kRefExtraCps[] = {" + ", ".join("0x%Xu" % c for c in EXTRA_CPS) + "};")
    out.append("const std::uint32_t kRefCapCps[] = {" + ", ".join("0x%Xu" % c for c in CAP_CPS) + "};")
    out.append("const std::uint32_t kRefCaps[] = {" + ", ".join("%du" % c for c in CAPS) + "};")
    for k in ("l1", "l2", "l3", "l4", "enc", "cap", "vec"):
        out.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (k, f[k].h))
        out.append("const std::size_t kRefCases_%s = %d;" % (k, f[k].n))
    out.append("struct RefVector { std::uint32_t n; std::uint8_t bytes[16]; std::uint32_t offset; };")
    out.append("const RefVector kRefVectors[] = {")
    for b in vecs:
        st, off = validate(b)
        out.append("    {%du, {%s}, %du}," % (len(b), ", ".join("0x%02X" % x for x in b) if b else "0", off))
    out.append("};")
    out.append(END)
    return "\n".join(out) + "\n"


def codecs_check():
    ok = bad_valid = off_diff = 0
    cnt = 0
    examples = []
    for ln, b in inputs():
        cnt += 1
        try:
            b.decode("utf-8")
            py_ok, py_off = True, len(b)
        except UnicodeDecodeError as e:
            py_ok, py_off = False, e.start
        st, off = validate(b)
        if (st == OK) != py_ok:
            bad_valid += 1
            continue
        if st != OK and off != py_off:
            off_diff += 1
            if len(examples) < 5:
                examples.append((b.hex(" "), off, py_off))
    for b in vectors():
        cnt += 1
        try:
            b.decode("utf-8")
            py_ok, py_off = True, len(b)
        except UnicodeDecodeError as e:
            py_ok, py_off = False, e.start
        st, off = validate(b)
        if (st == OK) != py_ok:
            bad_valid += 1
        elif st != OK and off != py_off:
            off_diff += 1
            if len(examples) < 5:
                examples.append((b.hex(" "), off, py_off))
    enc_bad = 0
    for cp in range(0, KMAX_CP + 1):
        st, out = encode(cp, 4)
        try:
            exp = chr(cp).encode("utf-8")
        except UnicodeEncodeError:
            exp = None
        if (st == OK and out != exp) or (st != OK and exp is not None):
            enc_bad += 1
    print("codecs: inputs compared %d, validity disagreements %d, offset differences %d (first: %s), encode disagreements over all scalars %d"
          % (cnt, bad_valid, off_diff, examples, enc_bad))
    return 0


def main(argv):
    if len(argv) == 2 and argv[1] == "--codecs":
        return codecs_check()
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
