#!/usr/bin/env python3
"""Independent reference for core/utf16 (not run in CI).

Written from the header text with Python integers: a scan over unit classes (non-surrogate, high D800-DBFF, low DC00-DFFF) and the pair
formula 0x10000 + ((high - D800) << 10) + (low - DC00). The codecs module is not used to produce any value. It runs every 1-unit input,
every (high, low) pair, every pair with one unit from a 24-value alphabet A and the other any value, every pair of a 24-value subset of
highs with any second unit (high-by-any-unit is NOT exhaustive), every string of length 0 to 4 over A, every scalar value through encode,
a table of buffer sizes, and fixed ill-formed vectors, and prints a C++ block with 64-bit digests that the tests recompute.

  python3 tools/utf16_ref.py            print the generated C++ block
  python3 tools/utf16_ref.py --check F  fail unless the block in F matches
  python3 tools/utf16_ref.py --codecs   extra: compare with Python's strict utf-16-le codec; prints what it covered and any differences
"""
import itertools
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/utf16_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = 2**64
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3

OK, NULL, TOO_LARGE, SMALL, MALFORMED = 0, 1, 2, 3, 4
KMAX_CP = 0x10FFFF
ALPHABET = [0x0000, 0x0001, 0x0041, 0x007F, 0x0080, 0x07FF, 0x0800, 0x8000, 0xD7FE, 0xD7FF, 0xD800, 0xD801, 0xDBFE, 0xDBFF, 0xDC00, 0xDC01,
            0xDFFE, 0xDFFF, 0xE000, 0xE001, 0xFEFF, 0xFFFD, 0xFFFE, 0xFFFF]
HIGHS24 = [0xD800, 0xD801, 0xD802, 0xD83D, 0xD87F, 0xD880, 0xD8FF, 0xD900, 0xD93F, 0xD940, 0xD9FF, 0xDA00, 0xDA3F, 0xDA40, 0xDABF, 0xDB00,
           0xDB3F, 0xDB40, 0xDB7F, 0xDB80, 0xDBBF, 0xDBC0, 0xDBFE, 0xDBFF]
EXTRA_CPS = [0x110000, 0x110001, 0x1FFFFF, 0x80000000, 0xFFFFFFFF]
CAP_CPS = [0, 0x41, 0xD7FF, 0xD800, 0xDFFF, 0xE000, 0xFFFF, 0x10000, 0x10FFFF, 0x110000, 0xFFFFFFFF]
CAPS = [0, 1, 2, 3]
STR_MAX = 4


def is_high(u):
    return 0xD800 <= u <= 0xDBFF


def is_low(u):
    return 0xDC00 <= u <= 0xDFFF


def analyze(u):
    """(validate status, offset, decode status, cp, len, count) for the unit list u."""
    n = len(u)
    i = 0
    count = 0
    vs, off = OK, n
    while i < n:
        x = u[i]
        if is_low(x):
            vs, off = MALFORMED, i
            break
        if is_high(x):
            if i + 1 < n and is_low(u[i + 1]):
                i += 2
            else:
                vs, off = MALFORMED, i
                break
        else:
            i += 1
        count += 1
    if vs != OK:
        count = 0
    if n == 0:
        ds, cp, ln = MALFORMED, 0, 0
    else:
        x = u[0]
        if is_low(x):
            ds, cp, ln = MALFORMED, 0, 0
        elif not is_high(x):
            ds, cp, ln = OK, x, 1
        elif n >= 2 and is_low(u[1]):
            ds, cp, ln = OK, 0x10000 + ((x - 0xD800) << 10) + (u[1] - 0xDC00), 2
        else:
            ds, cp, ln = MALFORMED, 0, 0
    return vs, off, ds, cp, ln, count


def encode(cp, cap, dst_null=False):
    if dst_null and cap > 0:
        return NULL, []
    if cp > KMAX_CP or 0xD800 <= cp <= 0xDFFF:
        return MALFORMED, []
    if cp <= 0xFFFF:
        out = [cp]
    else:
        v = cp - 0x10000
        out = [0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF)]
    if len(out) > cap:
        return SMALL, []
    return OK, out


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.n = 0

    def add(self, v):
        self.n += 1
        self.h = ((self.h ^ v) * FNV_PRIME) % M64


def pack(u):
    vs, off, ds, cp, ln, count = analyze(u)
    return vs | (off << 4) | (ds << 12) | (ln << 16) | (cp << 20) | (count << 41)


def inputs():
    for x in range(65536):
        yield "l1", [x]
    for h in range(0xD800, 0xDC00):
        for l in range(0xDC00, 0xE000):
            yield "hl", [h, l]
    for x in range(65536):
        for a in ALPHABET:
            yield "pair", [x, a]
    for a in ALPHABET:
        for x in range(65536):
            yield "pair", [a, x]
    for h in HIGHS24:
        for x in range(65536):
            yield "h24", [h, x]
    for n in range(STR_MAX + 1):
        for t in itertools.product(ALPHABET, repeat=n):
            yield "str", list(t)


CLASSES = ["DC00", "DFFF", "D800", "DBFF", "D800 0041", "D800 E000", "D800 D7FF", "DBFF FFFF", "D800 D800", "D800 D800 DC00", "DC00 DC00",
           "DBFF DBFF", "D800 0041 DC00", "DFFF D800", "DBFF 0000"]
PREFIXES = ["", "0041", "0041 D83D DE00"]
SUFFIXES = ["", "005A", "D83D DE00 005A"]
VALID = ["", "0000", "0041", "D7FF", "E000", "FFFF", "FEFF", "FFFE", "D800 DC00", "DBFF DFFF", "D83D DE00", "DBFF DC00", "D800 DFFF",
         "0041 D83D DE00 005A"]


def hexu(s):
    return [int(x, 16) for x in s.split()]


def vectors():
    out = [hexu(v) for v in VALID]
    for c in CLASSES:
        for p in PREFIXES:
            for s in SUFFIXES:
                out.append(hexu((p + " " + c + " " + s).strip()))
    return out


def render():
    f = {k: Fold() for k in ("l1", "hl", "pair", "h24", "str", "enc", "cap", "vec")}
    for k, u in inputs():
        f[k].add(pack(u))
    for cp in list(range(0, KMAX_CP + 1)) + EXTRA_CPS:
        st, out = encode(cp, 2)
        f["enc"].add(st | (len(out) << 4) | ((out[0] if out else 0) << 8) | ((out[1] if len(out) > 1 else 0) << 24))
    for cp in CAP_CPS:
        for cap in CAPS:
            for dn in ([False, True] if cap == 0 else [False]):
                st, out = encode(cp, cap, dn)
                f["cap"].add(st | (len(out) << 4) | (int(dn) << 8) | (cap << 12))
    vecs = vectors()
    for u in vecs:
        f["vec"].add(pack(u))
    out = ["" + BEGIN]
    out.append("const std::uint32_t kRefAlphabet[] = {" + ", ".join("0x%04Xu" % a for a in ALPHABET) + "};")
    out.append("const std::uint32_t kRefHighs24[] = {" + ", ".join("0x%04Xu" % a for a in HIGHS24) + "};")
    out.append("const std::uint32_t kRefExtraCps[] = {" + ", ".join("0x%Xu" % c for c in EXTRA_CPS) + "};")
    out.append("const std::uint32_t kRefCapCps[] = {" + ", ".join("0x%Xu" % c for c in CAP_CPS) + "};")
    out.append("const std::uint32_t kRefCaps[] = {" + ", ".join("%du" % c for c in CAPS) + "};")
    out.append("const std::uint32_t kRefStrMax = %d;" % STR_MAX)
    for k in ("l1", "hl", "pair", "h24", "str", "enc", "cap", "vec"):
        out.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (k, f[k].h))
        out.append("const std::size_t kRefCases_%s = %d;" % (k, f[k].n))
    out.append("struct RefVector { std::uint32_t n; std::uint16_t units[16]; std::uint32_t offset; std::uint32_t count; };")
    out.append("const RefVector kRefVectors[] = {")
    for u in vecs:
        vs, off, ds, cp, ln, count = analyze(u)
        out.append("    {%du, {%s}, %du, %du}," % (len(u), ", ".join("0x%04X" % x for x in u) if u else "0", off, count))
    out.append("};")
    out.append(END)
    return "\n".join(out) + "\n"


def codecs_check():
    cnt = bad_valid = off_diff = 0
    examples = []

    def cmp(u):
        nonlocal cnt, bad_valid, off_diff
        cnt += 1
        raw = b"".join(x.to_bytes(2, "little") for x in u)
        try:
            raw.decode("utf-16-le")
            py_ok, py_off = True, len(u)
        except UnicodeDecodeError as e:
            py_ok, py_off = False, e.start // 2
        vs, off, _, _, _, _ = analyze(u)
        if (vs == OK) != py_ok:
            bad_valid += 1
        elif vs != OK and off != py_off:
            off_diff += 1
            if len(examples) < 5:
                examples.append(([hex(x) for x in u], off, py_off))

    for k, u in inputs():
        cmp(u)
    for u in vectors():
        cmp(u)
    enc_bad = 0
    for cp in range(0, KMAX_CP + 1):
        st, out = encode(cp, 2)
        try:
            exp = chr(cp).encode("utf-16-le")
        except UnicodeEncodeError:
            exp = None
        got = b"".join(x.to_bytes(2, "little") for x in out)
        if (st == OK and got != exp) or (st != OK and exp is not None):
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
