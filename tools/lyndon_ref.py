#!/usr/bin/env python3
"""Independent reference for core/lyndon (not run in CI).

Written from the header text with Python integers. Implementations, all different from the C++ code (which is Duval's algorithm, and Duval's
algorithm run over the doubled string for the rotation):
  brute_factor  the factorization by the definition: the first factor is the longest prefix that is a Lyndon word (strictly smaller than each of its
                proper suffixes), then the same for the rest. Used for the self-check on short strings.
  brute_rot     every rotation compared with every other; the smallest index among the smallest rotations. Used for the self-check.
  check_factor  verifies a list of lengths without Duval: they are positive and sum to n, every factor is a Lyndon word (minimal rotation at 0 by the
                two-pointer method, and primitive by the failure function), and neighbours do not increase. A non-increasing run of Lyndon words is THE
                factorization, so this identifies it. Used for the large strings.
  fast_rot      two-pointer minimal rotation, reduced modulo the smallest period (the smallest index of the smallest rotation). Used for large strings.
  duval         Duval's algorithm, only to produce the lengths that check_factor then verifies.
The selfcheck runs these against each other on every string over {0, 1} up to length 12, over {0, 1, 2} up to length 8, and on 3000 random strings.

Folds. A fold is a 64-bit FNV-style digest of, for every string in order: n, k (the number of factors), the k lengths, the start of the least
rotation. "cases" is the number of strings.
  ex2    EXHAUSTIVE over alphabet {0, 255}: every string of length 0 .. 16 (131071 strings; by length, then the first byte changes fastest).
  ex3    EXHAUSTIVE over alphabet {0, 128, 255}: every string of length 0 .. 10 (88573 strings).
  ex6    EXHAUSTIVE over alphabet {0, 1, 127, 128, 254, 255}: every string of length 0 .. 6 (55987 strings).
  all256 EXHAUSTIVE: the empty string, all 256 one-byte strings and all 65536 two-byte strings (65793 strings).
  rs2, rs3, rs256  SAMPLED, fixed seed: 600 random strings of length 17 .. 400 (length 17 + draw % 384) over the alphabets {0, 255}, {0, 128, 255}
         and all 256 byte values (byte = draw % 256).
  big_<name>_<n>   the listed large patterns (see PATTERNS), digest over the same values for the one string.
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Usage: lyndon_ref.py (prints the generated block), lyndon_ref.py --check tests/lyndon/lyndon_tests.cpp
"""
import sys

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
BEGIN = "// BEGIN GENERATED REFERENCE (tools/lyndon_ref.py)"
END = "// END GENERATED REFERENCE"
A2 = [0, 255]
A3 = [0, 128, 255]
A6 = [0, 1, 127, 128, 254, 255]


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def next(self):
        self.x = (self.x * 6364136223846793005 + 1442695040888963407) & M64
        return self.x >> 32


class Fold:
    def __init__(self):
        self.h = FNV_OFFSET
        self.cases = 0

    def add(self, v):
        self.h = ((self.h ^ (v & M64)) * FNV_PRIME) & M64


def is_lyndon_def(w):
    return all(w < w[i:] for i in range(1, len(w)))


def brute_factor(s):
    n = len(s)
    lens = []
    i = 0
    while i < n:
        j = i + 1
        for e in range(i + 1, n + 1):
            if is_lyndon_def(s[i:e]):
                j = e
        lens.append(j - i)
        i = j
    return lens


def brute_rot(s):
    n = len(s)
    if n == 0:
        return 0
    best = 0
    for i in range(1, n):
        if s[i:] + s[:i] < s[best:] + s[:best]:
            best = i
    return best


def smallest_period_dividing(w):
    n = len(w)
    fail = [0] * (n + 1)
    k = 0
    for i in range(1, n):
        while k and w[i] != w[k]:
            k = fail[k]
        if w[i] == w[k]:
            k += 1
        fail[i + 1] = k
    p = n - fail[n]
    return p if n % p == 0 else n


def fast_rot(w):
    n = len(w)
    if n == 0:
        return 0
    i, j, k = 0, 1, 0
    while i < n and j < n and k < n:
        a, b = w[(i + k) % n], w[(j + k) % n]
        if a == b:
            k += 1
            continue
        if a > b:
            i += k + 1
        else:
            j += k + 1
        if i == j:
            j += 1
        k = 0
    return min(i, j) % smallest_period_dividing(w)


def is_lyndon_fast(w):
    return smallest_period_dividing(w) == len(w) and fast_rot(w) == 0


def duval(s):
    n = len(s)
    lens = []
    i = 0
    while i < n:
        j, k = i + 1, i
        while j < n and s[k] <= s[j]:
            k = i if s[k] < s[j] else k + 1
            j += 1
        ln = j - k
        while i <= k:
            lens.append(ln)
            i += ln
    return lens


def check_factor(s, lens):
    if sum(lens) != len(s) or any(x <= 0 for x in lens):
        return False
    i = 0
    prev = None
    for ln in lens:
        w = s[i:i + ln]
        if not is_lyndon_fast(w):
            return False
        if prev is not None and w > prev:
            return False
        prev = w
        i += ln
    return True


def values(s, lens, rot):
    return [len(s), len(lens)] + lens + [rot]


def fold_string(f, s, small=True):
    if small:
        lens = duval(s)
        rot = fast_rot(s)
    else:
        lens = duval(s)
        assert check_factor(s, lens)
        rot = fast_rot(s)
    for v in values(s, lens, rot):
        f.add(v)
    f.cases += 1


def strings(alpha, maxlen):
    k = len(alpha)
    for L in range(maxlen + 1):
        for c in range(k ** L):
            s = []
            x = c
            for _ in range(L):
                s.append(alpha[x % k])
                x //= k
            yield s


def pat_rep(n, b):
    return [b] * n


def pat_alt(n, a, b):
    return [a if i % 2 == 0 else b for i in range(n)]


def pat_per3(n):
    return [97 + i % 3 for i in range(n)]


def pat_fib(n):
    a, b = [98], [97]
    while len(b) < n:
        a, b = b, b + a
    return b[:n]


def pat_rand(n, seed, k):
    g = Lcg(seed)
    return [(97 + g.next() % 2) if k == 2 else g.next() % 256 for _ in range(n)]


def pat_lynd(n):
    return [0] * (n - 1) + [1]


def pat_revlynd(n):
    return [1] + [0] * (n - 1)


def pat_desc(n):
    return [255 - i % 256 for i in range(n)]


def pat_block(n, rot, seed):
    g = Lcg(seed)
    blk = [g.next() % 256 for _ in range(512)]
    s = [blk[i % 512] for i in range(n)]
    return s[rot:] + s[:rot]


def pat_rotated(base, rot):
    return base[rot:] + base[:rot]


PATTERNS = [
    ("rep0_262144", lambda: pat_rep(262144, 0)),
    ("rep0_262143", lambda: pat_rep(262143, 0)),
    ("rep0_262142", lambda: pat_rep(262142, 0)),
    ("rep255_262144", lambda: pat_rep(262144, 255)),
    ("alt_262144", lambda: pat_alt(262144, 0, 255)),
    ("alt_262143", lambda: pat_alt(262143, 0, 255)),
    ("alt_262142", lambda: pat_alt(262142, 0, 255)),
    ("altab_262144", lambda: pat_alt(262144, 97, 98)),
    ("per3_262144", lambda: pat_per3(262144)),
    ("per3_262143", lambda: pat_per3(262143)),
    ("per3_262142", lambda: pat_per3(262142)),
    ("fib_262144", lambda: pat_fib(262144)),
    ("fib_262143", lambda: pat_fib(262143)),
    ("fib_262142", lambda: pat_fib(262142)),
    ("fibrot_262144", lambda: pat_rotated(pat_fib(262144), 100003)),
    ("lynd_262144", lambda: pat_lynd(262144)),
    ("revlynd_262144", lambda: pat_revlynd(262144)),
    ("desc_262144", lambda: pat_desc(262144)),
    ("ends_262144", lambda: [1] + [0] * 262142 + [2]),
    ("block512_262144", lambda: pat_block(262144, 0, 6001)),
    ("block512rot_262144", lambda: pat_block(262144, 300, 6001)),
    ("rand2_262144", lambda: pat_rand(262144, 5003, 2)),
    ("rand2_262143", lambda: pat_rand(262143, 5004, 2)),
    ("rand256_262144", lambda: pat_rand(262144, 5005, 256)),
    ("rand256_262143", lambda: pat_rand(262143, 5006, 256)),
    ("rep0_32768", lambda: pat_rep(32768, 0)),
    ("rand256_1000", lambda: pat_rand(1000, 5007, 256)),
]


def selfcheck():
    for alpha, mx in (([0, 1], 12), ([0, 1, 2], 8)):
        for s in strings(alpha, mx):
            lens = brute_factor(s)
            assert lens == duval(s), s
            assert check_factor(s, lens), s
            assert brute_rot(s) == fast_rot(s), s
    g = Lcg(31337)
    for _ in range(3000):
        n = g.next() % 121
        k = (2, 3, 4, 256)[g.next() % 4]
        s = [g.next() % k for _ in range(n)]
        lens = duval(s)
        assert check_factor(s, lens), s
        if n <= 30:
            assert lens == brute_factor(s), s
        if n <= 60:
            assert brute_rot(s) == fast_rot(s), s
    for _ in range(300):
        n = 1 + g.next() % 60
        p = 1 + g.next() % 6
        blk = [g.next() % 2 for _ in range(p)]
        s = [blk[i % p] for i in range(n)]
        assert brute_rot(s) == fast_rot(s), s
    assert pat_fib(7) == [97, 98, 97, 97, 98, 97, 98]
    assert duval([97, 98, 97, 98]) == [2, 2] and fast_rot([98, 97, 98, 97]) == 1


def build():
    out = {}
    for name, alpha, mx in (("ex2", A2, 16), ("ex3", A3, 10), ("ex6", A6, 6)):
        f = Fold()
        for s in strings(alpha, mx):
            fold_string(f, s)
        out[name] = f
    f = Fold()
    fold_string(f, [])
    for a in range(256):
        fold_string(f, [a])
    for c in range(65536):
        fold_string(f, [c % 256, c // 256])
    out["all256"] = f
    for name, alpha, seed in (("rs2", A2, 3002), ("rs3", A3, 3003), ("rs256", None, 3256)):
        g = Lcg(seed)
        f = Fold()
        for _ in range(600):
            n = 17 + g.next() % 384
            s = [(alpha[g.next() % len(alpha)] if alpha else g.next() % 256) for _ in range(n)]
            fold_string(f, s)
        out[name] = f
    for name, make in PATTERNS:
        s = make()
        f = Fold()
        fold_string(f, s, small=False)
        out["big_" + name] = f
    return out


def block():
    selfcheck()
    out = build()
    L = [BEGIN]
    for k in sorted(out):
        L.append("const std::uint64_t kRefDigest_%s = 0x%016XULL;" % (k, out[k].h))
        L.append("const std::size_t kRefCases_%s = %d;" % (k, out[k].cases))
    L.append(END)
    return "\n".join(L) + "\n"


def main():
    blk = block()
    if len(sys.argv) >= 3 and sys.argv[1] == "--check":
        txt = open(sys.argv[2]).read()
        i, j = txt.find(BEGIN), txt.find(END)
        if i < 0 or j < 0:
            print("no generated block in", sys.argv[2])
            return 1
        have = [l for l in txt[i:j + len(END)].split("\n") if l.strip()]
        want = [l for l in blk.split("\n") if l.strip()]
        if have != want:
            print("generated block differs")
            return 1
        print("ok")
        return 0
    sys.stdout.write(blk)
    return 0


if __name__ == "__main__":
    sys.exit(main())
