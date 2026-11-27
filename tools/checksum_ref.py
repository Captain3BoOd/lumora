#!/usr/bin/env python3
"""Independent reference for core/checksum (not run in CI).

Written from the header comments with Python integers and no table: CRC-32 is the reflected bitwise loop (polynomial 0xEDB88320,
finished values are un-xored on entry and xored on exit), Adler-32 is the per-byte modulo 65521 loop. zlib is not used to produce any
value. It builds fixed inputs (all 0xFF, all 0x00, an LCG stream, a counter pattern) and prints a C++ block with FNV-1a 64 digests that the
tests recompute, plus a few vectors.

  python3 tools/checksum_ref.py            print the generated C++ block
  python3 tools/checksum_ref.py --check F  fail unless the block in F matches
  python3 tools/checksum_ref.py --zlib     extra: compare the same cases with Python's zlib (valid Adler starts only); prints what it covered
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/checksum_ref.py)"
END = "// END GENERATED REFERENCE"

M32 = 2**32
M64 = 2**64
BASE = 65521
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
LCG_MUL = 6364136223846793005
LCG_ADD = 1442695040888963407

KINDS = [0, 1, 2, 3]  # 0: all 0xFF, 1: all 0x00, 2: LCG bytes, 3: counter pattern
LENS = list(range(0, 81)) + [127, 128, 129, 255, 256, 257, 1000, 4096, 5550, 5551, 5552, 5553, 5554, 11103, 11104, 11105, 16656]
LONG_LENS = [65521, 65536, 70000]
LONG_KINDS = [0, 2]
CRC_STARTS = [0, 0xFFFFFFFF, 0x12345678]
ADLER_STARTS = [1, 0, 0xFFFFFFFF, 0xFFF0FFF0, 65521, 65521 << 16]
CH_KINDS = [2, 0]
CH_LENS = list(range(0, 41)) + [100, 5551, 5552, 5553, 11104, 11105, 20000]
CH_CHUNKS = [1, 2, 3, 7, 64, 5552]
VECTORS = [b"123456789", b"Wikipedia", b"The quick brown fox jumps over the lazy dog", b"a", b"abc"]


class Fnv:
    def __init__(self):
        self.h = FNV_OFFSET
        self.n = 0

    def u32(self, v):
        v %= M32
        self.n += 1
        for i in range(4):
            self.h = ((self.h ^ ((v >> (8 * i)) & 0xFF)) * FNV_PRIME) % M64


def data(kind, n):
    if kind == 0:
        return bytes([0xFF]) * n
    if kind == 1:
        return bytes(n)
    if kind == 2:
        x = (n * 2654435761 + 7) % M64
        out = bytearray()
        for _ in range(n):
            x = (x * LCG_MUL + LCG_ADD) % M64
            out.append((x >> 32) & 0xFF)
        return bytes(out)
    return bytes((i * 131 + n) & 0xFF for i in range(n))


def crc_update(crc, buf):
    c = (crc ^ 0xFFFFFFFF) % M32
    for b in buf:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
    return c ^ 0xFFFFFFFF


def adler_update(adler, buf):
    s1 = (adler & 0xFFFF) % BASE
    s2 = ((adler >> 16) & 0xFFFF) % BASE
    for b in buf:
        s1 = (s1 + b) % BASE
        s2 = (s2 + s1) % BASE
    return (s2 << 16) | s1


def chain(fn, start, buf, chunk):
    v = start
    for i in range(0, len(buf), chunk):
        v = fn(v, buf[i:i + chunk])
    return v


def cases(cb_crc, cb_adler):
    """Visit every case in the order the tests use."""
    for b in range(256):
        cb_crc("single", crc_update(0, bytes([b])))
        cb_adler("single", adler_update(1, bytes([b])))
    for kind in KINDS:
        for n in LENS:
            d = data(kind, n)
            for s in CRC_STARTS:
                cb_crc("main", crc_update(s, d))
            for s in ADLER_STARTS:
                cb_adler("main", adler_update(s, d))
    for kind in LONG_KINDS:
        for n in LONG_LENS:
            d = data(kind, n)
            cb_crc("main", crc_update(CRC_STARTS[0], d))
            cb_adler("main", adler_update(ADLER_STARTS[0], d))
    for kind in CH_KINDS:
        for n in CH_LENS:
            d = data(kind, n)
            for c in CH_CHUNKS:
                cb_crc("chunk", chain(crc_update, 0, d, c))
                cb_adler("chunk", chain(adler_update, 1, d, c))


def render():
    fn = {(a, k): Fnv() for a in ("crc", "adler") for k in ("single", "main", "chunk")}
    cases(lambda k, v: fn[("crc", k)].u32(v), lambda k, v: fn[("adler", k)].u32(v))
    out = [BEGIN]
    out.append("const std::uint32_t kRefKinds = %d;" % len(KINDS))
    out.append("const std::uint32_t kRefLens[] = {" + ", ".join("%du" % n for n in LENS) + "};")
    out.append("const std::uint32_t kRefLongLens[] = {" + ", ".join("%du" % n for n in LONG_LENS) + "};")
    out.append("const std::uint32_t kRefCrcStarts[] = {" + ", ".join("0x%08Xu" % s for s in CRC_STARTS) + "};")
    out.append("const std::uint32_t kRefAdlerStarts[] = {" + ", ".join("0x%08Xu" % s for s in ADLER_STARTS) + "};")
    out.append("const std::uint32_t kRefChLens[] = {" + ", ".join("%du" % n for n in CH_LENS) + "};")
    out.append("const std::uint32_t kRefChChunks[] = {" + ", ".join("%du" % n for n in CH_CHUNKS) + "};")
    for a in ("crc", "adler"):
        for k in ("single", "main", "chunk"):
            f = fn[(a, k)]
            out.append("const std::uint64_t kRefDigest_%s_%s = 0x%016Xull;" % (a, k, f.h))
            out.append("const std::size_t kRefCases_%s_%s = %d;" % (a, k, f.n))
    out.append("struct RefVector { const char* text; std::uint32_t crc; std::uint32_t adler; };")
    out.append("const RefVector kRefVectors[] = {")
    for v in VECTORS:
        out.append('    {"%s", 0x%08Xu, 0x%08Xu},' % (v.decode(), crc_update(0, v), adler_update(1, v)))
    out.append("};")
    out.append(END)
    return "\n".join(out) + "\n"


def zlib_check():
    import zlib
    cnt = {"crc": 0, "adler": 0, "adler_skipped_invalid_start": 0}
    bad = 0

    def ccrc(k, v):
        pass

    for b in range(256):
        assert zlib.crc32(bytes([b]), 0) == crc_update(0, bytes([b])); cnt["crc"] += 1
        assert zlib.adler32(bytes([b]), 1) == adler_update(1, bytes([b])); cnt["adler"] += 1
    for kind in KINDS:
        for n in LENS:
            d = data(kind, n)
            for s in CRC_STARTS:
                assert zlib.crc32(d, s) == crc_update(s, d); cnt["crc"] += 1
            for s in ADLER_STARTS:
                if (s & 0xFFFF) >= BASE or (s >> 16) >= BASE:
                    cnt["adler_skipped_invalid_start"] += 1
                    continue
                assert zlib.adler32(d, s) == adler_update(s, d); cnt["adler"] += 1
    for kind in LONG_KINDS:
        for n in LONG_LENS:
            d = data(kind, n)
            assert zlib.crc32(d, 0) == crc_update(0, d); cnt["crc"] += 1
            assert zlib.adler32(d, 1) == adler_update(1, d); cnt["adler"] += 1
    for v in VECTORS:
        assert zlib.crc32(v) == crc_update(0, v) and zlib.adler32(v) == adler_update(1, v)
        cnt["crc"] += 1; cnt["adler"] += 1
    print("zlib agrees:", cnt)
    return 0


def main(argv):
    if len(argv) == 2 and argv[1] == "--zlib":
        return zlib_check()
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
