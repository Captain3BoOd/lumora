#!/usr/bin/env python3
"""Independent reference archives for core/package/zip_audit (not run in CI).

Builds small ZIP archives with struct and zlib only, never with the code under test. The valid
archives are also opened with Python's zipfile as a second opinion. Every expected verdict and
offset below is written by hand from the rules in core/package/zip_audit.h.

  python3 tools/zip_audit_ref.py            print the generated C++ block
  python3 tools/zip_audit_ref.py --check F  fail unless the block in F matches
"""
import io
import struct
import sys
import zipfile
import zlib

TIME, DATE = 0x6000, 0x5A21
BEGIN = "// BEGIN GENERATED REFERENCE (tools/zip_audit_ref.py)"
END = "// END GENERATED REFERENCE"


def deflate(raw):
    c = zlib.compressobj(6, zlib.DEFLATED, -15)
    return c.compress(raw) + c.flush()


def local(name, method, crc, csize, usize, flags=0, extra=b""):
    nb = name.encode()
    return struct.pack("<IHHHHHIIIHH", 0x04034B50, 20, flags, method, TIME, DATE, crc, csize,
                       usize, len(nb), len(extra)) + nb + extra


def central(name, method, crc, csize, usize, offset, flags=0, mode=0o100644, extra=b"",
            comment=b""):
    nb = name.encode()
    ext = (mode << 16) | (0x10 if name.endswith("/") else 0)
    return struct.pack("<IHHHHHHIIIHHHHHII", 0x02014B50, 0x031E, 20, flags, method, TIME, DATE,
                       crc, csize, usize, len(nb), len(extra), len(comment), 0, 0, ext,
                       offset) + nb + extra + comment


def eocd(count, cd_size, cd_off, comment=b""):
    return struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, count, count, cd_size, cd_off,
                       len(comment)) + comment


def build(entries, comment=b"", prefix=b""):
    """entries: (name, method, raw_bytes). Returns (bytes, info dict)."""
    body = bytearray(prefix)
    cds = []
    offs = []
    for name, method, raw in entries:
        stored = raw if method == 0 else deflate(raw)
        crc = zlib.crc32(raw) & 0xFFFFFFFF
        off = len(body)
        offs.append(off)
        body += local(name, method, crc, len(stored), len(raw)) + stored
        mode = 0o040755 if name.endswith("/") else 0o100644
        cds.append(central(name, method, crc, len(stored), len(raw), off, mode=mode))
    cd_off = len(body)
    cd = b"".join(cds)
    eocd_off = cd_off + len(cd)
    out = bytes(body) + cd + eocd(len(entries), len(cd), cd_off, comment)
    return out, {"offs": offs, "cd_off": cd_off, "cd_size": len(cd), "eocd": eocd_off,
                 "end_of_data": cd_off}


BASIC = [("a.txt", 0, b"hello\n"), ("dir/", 0, b""), ("dir/b.bin", 8, bytes(range(50)) * 4)]


def cases():
    out = []
    basic, info = build(BASIC)
    with zipfile.ZipFile(io.BytesIO(basic)) as z:  # second opinion: valid for a stock reader
        assert z.testzip() is None
        assert z.read("a.txt") == b"hello\n" and z.read("dir/b.bin") == bytes(range(50)) * 4
    out.append(("valid_basic", basic, "Ok", 0, 0))

    empty = eocd(0, 0, 0)
    with zipfile.ZipFile(io.BytesIO(empty)) as z:
        assert z.namelist() == []
    out.append(("valid_empty", empty, "Ok", 0, 0))

    withc, _ = build(BASIC, comment=b"made by hand")
    with zipfile.ZipFile(io.BytesIO(withc)) as z:
        assert z.comment == b"made by hand"
    out.append(("valid_comment", withc, "Ok", 0, 0))

    # Junk in front, every offset shifted so the archive is internally consistent.
    pre, pinfo = build(BASIC, prefix=b"JUNK!")
    out.append(("prepended", pre, "PrependedData", 5, 0))

    out.append(("appended", basic + b"tail", "TrailingData", len(basic), 0))

    two, tinfo = build(BASIC, comment=struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 0, 0, 0, 0, 0))
    out.append(("second_end_record_in_comment", two, "MultipleEndRecords", tinfo["eocd"] + 22, 0))

    # Central says b.txt, local says a.txt.
    mm, minfo = build([("a.txt", 0, b"hello\n")])
    mm = bytearray(mm)
    mm[30] = ord("b")  # local name byte (header is 30 bytes)
    out.append(("local_name_mismatch", bytes(mm), "HeaderMismatch", 0, 0))

    # Entry 2's local header sits inside entry 1's data (overlap bomb shape).
    d1 = bytes(100)
    buf = bytearray()
    buf += local("a", 0, zlib.crc32(d1) & 0xFFFFFFFF, 100, 100) + d1  # data at 31..131
    d2 = b"12345"
    h2 = local("b", 0, zlib.crc32(d2) & 0xFFFFFFFF, 5, 5) + d2
    buf[40:40 + len(h2)] = h2
    cd_off = len(buf)
    cd = (central("a", 0, zlib.crc32(d1) & 0xFFFFFFFF, 100, 100, 0) +
          central("b", 0, zlib.crc32(d2) & 0xFFFFFFFF, 5, 5, 40))
    ov = bytes(buf) + cd + eocd(2, len(cd), cd_off)
    out.append(("overlap_inside_first_entry", ov, "Overlap", 40, 1))

    # Seven stray bytes between the two entries.
    e1 = local("a", 0, zlib.crc32(b"x") & 0xFFFFFFFF, 1, 1) + b"x"
    e2 = local("b", 0, zlib.crc32(b"y") & 0xFFFFFFFF, 1, 1) + b"y"
    buf = e1 + b"STRAYXX" + e2
    cd = (central("a", 0, zlib.crc32(b"x") & 0xFFFFFFFF, 1, 1, 0) +
          central("b", 0, zlib.crc32(b"y") & 0xFFFFFFFF, 1, 1, len(e1) + 7))
    gap = buf + cd + eocd(2, len(cd), len(buf))
    out.append(("gap_between_entries", gap, "Gap", len(e1), 1))

    # ZIP64 locator and record present, but no end-record field holds a sentinel.
    z64, zinfo = build(BASIC)
    cd_start = zinfo["cd_off"]
    rec_off = zinfo["eocd"]
    rec = struct.pack("<IQHHIIQQQQ", 0x06064B50, 44, 45, 45, 0, 0, 3, 3, zinfo["cd_size"], cd_start)
    loc = struct.pack("<IIQI", 0x07064B50, 0, rec_off, 1)
    z64 = z64[:zinfo["eocd"]] + rec + loc + z64[zinfo["eocd"]:]
    out.append(("zip64_locator_without_sentinel", z64, "Zip64Mismatch", zinfo["eocd"] + 56, 0))
    return out


def render():
    lines = [BEGIN]
    lines.append("struct RefCase {")
    lines.append("  const char* name;")
    lines.append("  const char* hex;")
    lines.append("  const char* verdict;")
    lines.append("  std::uint64_t offset;")
    lines.append("  std::size_t index;")
    lines.append("};")
    lines.append("const RefCase kRefCases[] = {")
    for name, data, verdict, offset, index in cases():
        lines.append("    {\"%s\",\n     \"%s\",\n     \"%s\", %du, %du}," %
                     (name, data.hex(), verdict, offset, index))
    lines.append("};")
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
