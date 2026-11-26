#!/usr/bin/env python3
"""Independent reference for core/package/capabilities (not run in CI).

A second implementation in plain Python, written from the rules in core/package/capabilities.h and not from
the C++ code. Capabilities are Python sets of names here, not bit masks. It prints a C++ block with the name
table, the numeric values of the enums, and FNV-1a 64 digests over the full result of exhaustive
enumerations, which the tests recompute from the C++ functions.

  python3 tools/capabilities_ref.py            print the generated C++ block
  python3 tools/capabilities_ref.py --check F  fail unless the block in F matches
"""
import itertools
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/capabilities_ref.py)"
END = "// END GENERATED REFERENCE"

NAMES = ["microphone", "notifications", "activity_recognition", "coarse_location"]
VERDICTS = ["Ok", "UnknownCapability", "DuplicateCapability", "TooManyNames"]
STATUSES = ["Available", "NotDeclared", "PermissionDenied", "Unsupported", "TemporarilyUnavailable"]
# Capability values tried in every operation: the four real ones, Count_ and values above it.
CAPS = [0, 1, 2, 3, 4, 5, 31, 32, 255]
HIGH = [0, 0x10, 0x80000000, 0xFFFFFFF0]
POOL = ["microphone", "notifications", "activity_recognition", "coarse_location", "Microphone",
        "microphone ", "", "count_", "microphone\x00x", "microphone\x00"]
MAX_NAMES = [0, 1, 2, 3, 4, 5, 6, 32, 2**64 - 1]


def to_set(mask):
    return {NAMES[i] for i in range(4) if (mask >> i) & 1}


def to_mask(s):
    return sum(1 << NAMES.index(n) for n in s)


class Fnv:
    def __init__(self):
        self.h = 0xcbf29ce484222325

    def byte(self, b):
        self.h ^= b & 0xFF
        self.h = (self.h * 0x100000001b3) % 2**64

    def num(self, v, n):
        for i in range(n):
            self.byte((v >> (8 * i)) & 0xFF)


def parse(names, max_names):
    if len(names) > max_names:
        return 3, max_names, set()
    seen = set()
    for i, n in enumerate(names):
        if n not in NAMES:
            return 1, i, set()
        if n in seen:
            return 2, i, set()
        seen.add(n)
    return 0, 0, seen


def query(declared, granted, supported, active, cap):
    if cap > 3:
        return 1
    n = NAMES[cap]
    if n not in declared:
        return 1
    if n not in granted:
        return 2
    if n not in supported:
        return 3
    if n not in active:
        return 4
    return 0


def update(old_declared, old_granted, new_declared):
    od = to_set(old_declared & 15)
    og = to_set(old_granted & 15)
    nd = to_set(new_declared & 15)
    og = og & od
    if nd <= od:
        return nd, og & nd
    return nd, set()


def digests():
    out = {}
    h = Fnv()
    for d, g, s, a in itertools.product(range(16), repeat=4):
        for c in range(5):
            h.byte(query(to_set(d), to_set(g), to_set(s), to_set(a), c))
    out["query"] = h.h
    # Stray high bits in the record and the platform state must not matter for the four capabilities.
    h = Fnv()
    for d, g in itertools.product(range(16), repeat=2):
        for hi in HIGH:
            for c in CAPS:
                h.byte(query(to_set(d), to_set(g), to_set(0xF), to_set(0xF), c))
                h.byte(query(to_set(d | hi), to_set(g | hi), to_set(5), to_set(3), c))
    out["query_wide"] = h.h
    h = Fnv()
    for d, g, n in itertools.product(range(16), repeat=3):
        for hi in HIGH:
            for hj in HIGH:
                nd, ng = update(d | hi, g | hj, n | hi)
                h.num(to_mask(nd), 4)
                h.num(to_mask(ng), 4)
    out["update"] = h.h
    h = Fnv()
    for d in range(16):
        for g in range(16 * 2):  # granted may carry a stray bit 4 as well
            for c in CAPS:
                # grant
                dec, gr = to_set(d), g
                if c <= 3 and NAMES[c] in dec:
                    gr |= 1 << c
                    ok = 1
                else:
                    ok = 0
                h.byte(ok)
                h.num(gr, 4)
                # revoke
                gr = g
                if c <= 3:
                    gr &= ~(1 << c)
                h.byte(1)
                h.num(gr, 4)
    out["grant_revoke"] = h.h
    h = Fnv()
    count = 0
    for k in range(0, 6):
        for combo in itertools.product(range(len(POOL)), repeat=k):
            names = [POOL[i] for i in combo]
            for m in MAX_NAMES:
                v, i, s = parse(names, m)
                h.byte(v)
                h.num(i, 8)
                h.num(to_mask(s), 4)
                count += 1
    out["parse"] = h.h
    out["parse_cases"] = count
    return out


def render():
    d = digests()
    L = [BEGIN]
    L.append("const char* const kRefNames[4] = {%s};" % ", ".join('"%s"' % n for n in NAMES))
    L.append("// Enum values: DeclareVerdict %s; CapabilityStatus %s." % (
        ", ".join("%s=%d" % (n, i) for i, n in enumerate(VERDICTS)),
        ", ".join("%s=%d" % (n, i) for i, n in enumerate(STATUSES))))
    L.append("const std::uint8_t kRefCaps[] = {%s};" % ", ".join(str(c) for c in CAPS))
    L.append("const std::uint32_t kRefHigh[] = {%s};" % ", ".join("0x%08Xu" % h for h in HIGH))
    L.append("const std::size_t kRefMaxNames[] = {%s};" % ", ".join("%dull" % m if m < 2**32 else "static_cast<std::size_t>(-1)" for m in MAX_NAMES))
    L.append("const std::vector<std::string> kRefPool = {")
    for p in POOL:
        L.append("    " + (("std::string(" + " ".join('"\\x%02x"' % ord(c) for c in p) + ", %d)" % len(p)) if p else "std::string()") + ",")
    L.append("};")
    for k in ("query", "query_wide", "update", "grant_revoke", "parse"):
        L.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (k, d[k]))
    L.append("const std::size_t kRefParseCases = %d;" % d["parse_cases"])
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
