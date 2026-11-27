#!/usr/bin/env python3
"""Independent reference for core/globmatch (not run in CI).

Written from the header text. The pattern is parsed into tokens (a backslash makes the next byte literal; a backslash with no next byte is
BadPattern, found while parsing the WHOLE pattern, before any matching). The match is a dynamic programme over the tokens. Every result is
cross-checked, every time this script runs, against a second implementation: the pattern translated to a regular expression over bytes
('?' is any byte, '*' is any run, a literal is escaped) and applied with fullmatch and DOTALL. It prints a C++ block with 64-bit digests
that the tests recompute.

  exh    every pattern and every text of length 0 to 4 over the 5 symbols a b ? * backslash (781 x 781 pairs; wildcards in the text are
         ordinary bytes)
  bytes  every pattern and every text of length 0 to 3 over 7 byte values 0x00 0x7F 0x80 0xFF ? * backslash (400 x 400 pairs)
  rand   400 seeds: a pattern of 0 to 30 symbols and a text that is random (mode 0), built from the pattern (mode 1) or built from the
         pattern with one byte changed (mode 2), fixed 64-bit LCG

  python3 tools/globmatch_ref.py            print the generated C++ block
  python3 tools/globmatch_ref.py --check F  fail unless the block in F matches
The rand scenario is NOT exhaustive.
"""
import itertools
import re
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/globmatch_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
OK, BADPATTERN = 0, 3
STAR, ANY, LIT = 0, 1, 2
RAND_SEEDS = 400
EXH_ALPHA = [ord("a"), ord("b"), ord("?"), ord("*"), ord("\\")]
BYTE_ALPHA = [0x00, 0x7F, 0x80, 0xFF, ord("?"), ord("*"), ord("\\")]
SYM = [ord("a"), ord("b"), ord("c"), ord("?"), ord("*"), ord("\\"), 0x00, 0xFF, ord("?"), ord("*")]
TS = [ord("a"), ord("b"), ord("c"), 0x00, 0xFF, ord("\\"), ord("*"), ord("?")]


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


def tokens(pat):
    out = []
    i = 0
    while i < len(pat):
        c = pat[i]
        if c == ord("\\"):
            if i + 1 >= len(pat):
                return None
            out.append((LIT, pat[i + 1]))
            i += 2
        elif c == ord("*"):
            out.append((STAR, 0))
            i += 1
        elif c == ord("?"):
            out.append((ANY, 0))
            i += 1
        else:
            out.append((LIT, c))
            i += 1
    return out


def dp_match(toks, text):
    m = len(text)
    row = [False] * (m + 1)
    row[0] = True
    for kind, b in toks:
        new = [False] * (m + 1)
        if kind == STAR:
            seen = False
            for j in range(m + 1):
                seen = seen or row[j]
                new[j] = seen
        else:
            for j in range(1, m + 1):
                if row[j - 1] and (kind == ANY or text[j - 1] == b):
                    new[j] = True
        row = new
    return row[m]


def regex_for(toks):
    parts = []
    for kind, b in toks:
        parts.append(b".*" if kind == STAR else b"." if kind == ANY else re.escape(bytes([b])))
    return re.compile(b"".join(parts), re.DOTALL)


def code_and_match(pat, text, rx_cache={}):
    toks = tokens(pat)
    if toks is None:
        return BADPATTERN, False
    key = bytes(pat)
    if key not in rx_cache:
        rx_cache[key] = regex_for(toks)
    a = dp_match(toks, text)
    b = rx_cache[key].fullmatch(bytes(text)) is not None
    assert a == b, (pat, text)
    return OK, a


def fold_pair(f, pat, text):
    code, match = code_and_match(pat, text)
    f.ops += 1
    f.cases += 1
    f.add((code << 32) | (1 if match else 0))


def strings(alpha, maxlen):
    for n in range(maxlen + 1):
        for t in itertools.product(alpha, repeat=n):
            yield list(t)


def pair_scenario(alpha, maxlen):
    f = Fold()
    texts = list(strings(alpha, maxlen))
    for p in strings(alpha, maxlen):
        for t in texts:
            fold_pair(f, p, t)
    return f


def rand_case(seed):
    g = Lcg(seed * 7919 + 13)
    plen = g.next() % 31
    pat = [SYM[g.next() % 10] for _ in range(plen)]
    mode = g.next() % 3
    text = []
    if mode == 0:
        tlen = g.next() % 41
        text = [TS[g.next() % 8] for _ in range(tlen)]
    else:
        i = 0
        while i < plen:
            c = pat[i]
            if c == ord("\\"):
                if i + 1 < plen:
                    text.append(pat[i + 1])
                    i += 2
                else:
                    i += 1
            elif c == ord("*"):
                k = g.next() % 4
                for _ in range(k):
                    text.append(TS[g.next() % 8])
                i += 1
            elif c == ord("?"):
                text.append(TS[g.next() % 8])
                i += 1
            else:
                text.append(c)
                i += 1
        if mode == 2 and text:
            pos = g.next() % len(text)
            text[pos] = TS[g.next() % 8]
    return pat, text


def rand_scenario():
    f = Fold()
    for seed in range(1, RAND_SEEDS + 1):
        pat, text = rand_case(seed)
        fold_pair(f, pat, text)
    return f


def render():
    out = [BEGIN]
    out.append("const std::uint32_t kRefRandSeeds = %d;" % RAND_SEEDS)
    for name, fn in (
        ("exh", lambda: pair_scenario(EXH_ALPHA, 4)),
        ("bytes", lambda: pair_scenario(BYTE_ALPHA, 3)),
        ("rand", rand_scenario),
    ):
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
