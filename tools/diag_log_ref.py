#!/usr/bin/env python3
"""Independent reference for core/diag/diag_log (not run in CI).

Written from the header comments with Python integers: the window is walked backward from next - 1 for `count` slots,
the digest is built from a flat list of 32-bit words. It prints a C++ block with the grids and FNV-1a 64 digests (the
whole object, return values and read results after every call) that the tests recompute.

  python3 tools/diag_log_ref.py            print the generated C++ block
  python3 tools/diag_log_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/diag_log_ref.py)"
END = "// END GENERATED REFERENCE"

CAP = 64
M64 = 2**64 - 1
M32 = 2**32 - 1
U64 = M64
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3


def s32(v):
    v &= M32
    return v - 2**32 if v >= 2**31 else v


class Log:
    def __init__(self):
        self.entries = [(0, 0, 0, 0)] * CAP
        self.count = 0
        self.next = 0
        self.dropped = 0

    def valid(self):
        return self.count <= CAP and self.next < CAP

    def window(self):
        """Slots of the window, oldest first, walked backward from next - 1."""
        out = []
        s = self.next
        for _ in range(self.count):
            s = (s - 1) % CAP
            out.append(s)
        out.reverse()
        return out


def clear(lg):
    lg.entries = [(0, 0, 0, 0)] * CAP
    lg.count = lg.next = lg.dropped = 0


def push(lg, ts, code, a, b):
    if not lg.valid():
        lg.count = 0
        lg.next = 0
    lg.entries[lg.next] = (ts, code, a, b)
    lg.next = (lg.next + 1) % CAP
    if lg.count < CAP:
        lg.count += 1
    else:
        lg.dropped = min(U64, lg.dropped + 1)
    return True


def read(lg, i):
    if not lg.valid() or i >= lg.count:
        return False, None
    return True, lg.entries[lg.window()[i]]


def size(lg):
    return lg.count if lg.valid() else 0


def digest(lg):
    words = []
    if lg.valid():
        for s in lg.window():
            ts, code, a, b = lg.entries[s]
            words += [ts & M32, ts >> 32, code, a & M32, b & M32]
    words += [lg.dropped & M32, lg.dropped >> 32]
    h = FNV_OFFSET
    for w in words:
        for k in range(4):
            h = ((h ^ ((w >> (8 * k)) & 255)) * FNV_PRIME) & M64
    return h


NULL_DIGEST = FNV_OFFSET


def fill(lg, kind):
    for i in range(CAP):
        ts = ((i + 1) * 0x9E3779B97F4A7C15 + kind) & M64
        code = (i * 257 + kind * 13 + 1) & 0xFFFF
        a = s32(((i + 1) * 0x01000193 + kind) & M32)
        b = s32((((i + 7) * 0x85EBCA6B) ^ kind) & M32)
        lg.entries[i] = (ts, code, a, b)


class Fnv:
    def __init__(self):
        self.h = FNV_OFFSET

    def byte(self, b):
        self.h = ((self.h ^ (b & 255)) * FNV_PRIME) & M64

    def u32(self, v):
        v &= M32
        for i in range(4):
            self.byte(v >> (8 * i))

    def u64(self, v):
        self.u32(v & M32)
        self.u32(v >> 32)


def feed_state(f, lg):
    for ts, code, a, b in lg.entries:
        f.u64(ts)
        f.u32(code)
        f.u32(a)
        f.u32(b)
    f.u32(lg.count)
    f.u32(lg.next)
    f.u64(lg.dropped)


def feed_read(f, ok, e):
    f.byte(1 if ok else 0)
    if ok:
        f.u64(e[0])
        f.u32(e[1])
        f.u32(e[2])
        f.u32(e[3])


COUNTS = [0, 1, 2, 3, 10, 63, 64, 65, 255, M32]
NEXTS = [0, 1, 5, 31, 32, 63, 64, 65, M32]
DROPPEDS = [0, 1, U64 - 1, U64, M32, 2**32]
KINDS = [0, 5]
READ_INDEXES = [0, 1, 2, 31, 62, 63, 64, 65, M32]
PUSH_TS = [0, 1, M32, 2**32, U64 - 1, U64, 0x0123456789ABCDEF]
PUSH_CODES = [0, 1, 255, 256, 32767, 32768, 65535]
PUSH_VALS = [0, 1, -1, 2**31 - 1, -(2**31), 12345, -12345]
SEQ_SEEDS = [5, 6, 7, 8]
SEQ_N = 400


def digests():
    # hand-built objects: every operation on a state, whole object hashed after
    hb, nhb = Fnv(), 0
    for kind in KINDS:
        for c in COUNTS:
            for n in NEXTS:
                for d in DROPPEDS:
                    lg = Log()
                    fill(lg, kind)
                    lg.count, lg.next, lg.dropped = c, n, d
                    f = hb
                    f.byte(size(lg) & 255)
                    f.u64(digest(lg))
                    for i in READ_INDEXES:
                        feed_read(f, *read(lg, i))
                    push(lg, 0xAAAA5555AAAA5555, 777, -5, 6)
                    feed_state(f, lg)
                    f.u64(digest(lg))
                    nhb += 1
    # scripted sequences with clears
    sq, nsq = Fnv(), 0
    for seed in SEQ_SEEDS:
        lg = Log()
        x = seed
        for k in range(SEQ_N):
            x = (x * 6364136223846793005 + 1442695040888963407) & M64
            r = x >> 33
            if r % 61 == 0:
                clear(lg)
                sq.byte(2)
            else:
                ts = PUSH_TS[r % 7] if r % 5 == 0 else (r * 1000003) & M64
                code = PUSH_CODES[(r >> 8) % 7] if r % 3 == 0 else (r >> 4) & 0xFFFF
                a = PUSH_VALS[(r >> 12) % 7] if r % 4 == 0 else s32(r >> 3)
                b = PUSH_VALS[(r >> 16) % 7] if r % 2 == 0 else s32(r * 7)
                push(lg, ts, code, a, b)
                sq.byte(1)
            sq.byte(size(lg))
            sq.u64(digest(lg))
            sq.u32(lg.next)
            sq.u64(lg.dropped)
            ri = (r >> 20) % 70
            feed_read(sq, *read(lg, ri))
            nsq += 1
    # saturation of dropped
    st, nst = Fnv(), 0
    lg = Log()
    for i in range(CAP):
        push(lg, i, i, i, i)
    for start in (U64 - 3, U64 - 1, U64):
        lg.dropped = start
        for i in range(5):
            push(lg, 100 + i, i, i, i)
            st.u64(lg.dropped)
            st.u64(digest(lg))
            nst += 1
    return hb.h, nhb, sq.h, nsq, st.h, nst


def arr(name, typ, vals, suffix=""):
    return "const %s %s[] = {%s};" % (typ, name, ", ".join(cpp(v, suffix) for v in vals))


def cpp(v, suffix):
    if suffix == "i32":
        if v == -(2**31):
            return "INT32_MIN"
        if v == 2**31 - 1:
            return "INT32_MAX"
        return str(v)
    if suffix == "u64":
        return "%dull" % v
    return "%du" % v


def render():
    hb, nhb, sq, nsq, st, nst = digests()
    lines = [BEGIN,
             arr("kRefCounts", "std::uint32_t", COUNTS),
             arr("kRefNexts", "std::uint32_t", NEXTS),
             arr("kRefDroppeds", "std::uint64_t", DROPPEDS, "u64"),
             arr("kRefKinds", "std::uint32_t", KINDS),
             arr("kRefReadIndexes", "std::uint32_t", READ_INDEXES),
             arr("kRefPushTs", "std::uint64_t", PUSH_TS, "u64"),
             arr("kRefPushCodes", "std::uint16_t", PUSH_CODES),
             arr("kRefPushVals", "std::int32_t", PUSH_VALS, "i32"),
             arr("kRefSeqSeeds", "std::uint64_t", SEQ_SEEDS),
             "const std::size_t kRefSeqN = %d;" % SEQ_N,
             "const std::uint64_t kRefNullDigest = 0x%016Xull;" % NULL_DIGEST,
             "const std::uint64_t kRefDigest_hand = 0x%016Xull;" % hb,
             "const std::size_t kRefCases_hand = %d;" % nhb,
             "const std::uint64_t kRefDigest_seq = 0x%016Xull;" % sq,
             "const std::size_t kRefCases_seq = %d;" % nsq,
             "const std::uint64_t kRefDigest_sat = 0x%016Xull;" % st,
             "const std::size_t kRefCases_sat = %d;" % nst,
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
