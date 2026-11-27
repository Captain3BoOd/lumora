#!/usr/bin/env python3
"""Independent reference for core/palin (not run in CI).

Written from the header text with Python integers. Three implementations, all different from the C++ code:
  brute   every substring s[i .. j] is tested against its reverse; the radii are counts of palindromic substrings per center (by the header's
          definition), so no expansion and no mirror reuse is involved. Used for the self-check on short strings.
  expand  expand around every center (quadratic, simple). Used for the exhaustive and the sampled groups.
  linear  Manacher on the string with a separator between all bytes (a different formulation from the C++ code) and a palindromic tree (eertree) for
          the count and the longest palindrome. Used for the large groups; the self-check compares linear with brute and expand, and for every large
          group the sum of the radii is compared with the eertree count and the longest palindrome with the eertree's.
The selfcheck runs brute, expand and linear against each other on every binary string up to length 12 and on 3000 random strings up to length 120.

Folds. A fold is a 64-bit FNV-style digest of, for every string in order: n, odd[0 .. n), even[0 .. n), the start and the length of the longest
palindromic substring (first among the longest) and the count (u64). "cases" is the number of strings.
  ex2    EXHAUSTIVE over alphabet {0, 255}: every string of length 0 .. 16 (131071 strings; by length, then the first byte changes fastest).
  ex3    EXHAUSTIVE over alphabet {0, 128, 255}: every string of length 0 .. 10 (88573 strings).
  ex6    EXHAUSTIVE over alphabet {0, 1, 127, 128, 254, 255}: every string of length 0 .. 6 (55987 strings).
  all256 EXHAUSTIVE: the empty string, all 256 one-byte strings and all 65536 two-byte strings (65793 strings).
  rs2, rs3, rs256  SAMPLED, fixed seed: 600 random strings of length 17 .. 400 (length 17 + draw % 384) over the alphabets {0, 255}, {0, 128, 255}
         and all 256 byte values (byte = draw % 256).
  big_<name>_<n>   the listed large patterns (see PATTERNS), digest over the same values for the one string.
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Usage: palin_ref.py (prints the generated block), palin_ref.py --check tests/palin/palin_tests.cpp
"""
import sys

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
BEGIN = "// BEGIN GENERATED REFERENCE (tools/palin_ref.py)"
END = "// END GENERATED REFERENCE"
MAXN = 262144
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


def brute(s):
    n = len(s)
    odd, even = [0] * n, [0] * n
    best = (0, 0)
    count = 0
    for i in range(n):
        for j in range(i, n):
            sub = s[i:j + 1]
            if sub == sub[::-1]:
                count += 1
                ln = j - i + 1
                if ln > best[1]:
                    best = (i, ln)
                if ln % 2 == 1:
                    odd[i + (ln - 1) // 2] += 1
                else:
                    even[i + ln // 2] += 1
    return odd, even, best, count


def expand(s):
    n = len(s)
    odd, even = [0] * n, [0] * n
    for i in range(n):
        k = 1
        while i - k >= 0 and i + k < n and s[i - k] == s[i + k]:
            k += 1
        odd[i] = k
        k = 0
        while i - k - 1 >= 0 and i + k < n and s[i - k - 1] == s[i + k]:
            k += 1
        even[i] = k
    return odd, even


def longest_of(odd, even):
    best = (0, 0)
    for i in range(len(odd)):
        for ln, st in ((2 * odd[i] - 1, i - odd[i] + 1), (2 * even[i], i - even[i])):
            if ln > best[1] or (ln == best[1] and ln > 0 and st < best[0]):
                best = (st, ln)
    return best


def linear(s):
    """Manacher on the string with a separator before, between and after all bytes (-1)."""
    n = len(s)
    t = [-1]
    for c in s:
        t.append(c)
        t.append(-1)
    m = len(t)
    p = [0] * m
    c = r = 0
    for i in range(m):
        k = min(p[2 * c - i], r - i) if i < r else 0
        while i - k - 1 >= 0 and i + k + 1 < m and t[i - k - 1] == t[i + k + 1]:
            k += 1
        p[i] = k
        if i + k > r:
            c, r = i, i + k
    odd = [(p[2 * i + 1] + 1) // 2 for i in range(n)]
    even = [p[2 * i] // 2 for i in range(n)]
    return odd, even


def eertree(s):
    """Palindromic tree: (count of palindromic substring occurrences, (start, length) of the first longest)."""
    ln = [-1, 0]
    link = [0, 0]
    nxt = [{}, {}]
    depth = [0, 0]
    first_end = [0, 0]
    last = 1
    total = 0
    for i, c in enumerate(s):
        cur = last
        while True:
            l = ln[cur]
            if i - 1 - l >= 0 and s[i - 1 - l] == c:
                break
            cur = link[cur]
        if c in nxt[cur]:
            last = nxt[cur][c]
        else:
            node = len(ln)
            ln.append(ln[cur] + 2)
            nxt.append({})
            first_end.append(i)
            if ln[node] == 1:
                link.append(1)
            else:
                q = link[cur]
                while True:
                    l = ln[q]
                    if i - 1 - l >= 0 and s[i - 1 - l] == c:
                        break
                    q = link[q]
                link.append(nxt[q][c])
            depth.append(depth[link[node]] + 1)
            nxt[cur][c] = node
            last = node
        total += depth[last]
    best = (0, 0)
    for v in range(2, len(ln)):
        st = first_end[v] - ln[v] + 1
        if ln[v] > best[1] or (ln[v] == best[1] and st < best[0]):
            best = (st, ln[v])
    return total, best


def values(s, odd, even):
    n = len(s)
    st, ln = longest_of(odd, even)
    return [n] + odd + even + [st, ln, sum(odd) + sum(even)]


def fold_string(f, s, how="expand"):
    if how == "expand":
        odd, even = expand(s)
    else:
        odd, even = linear(s)
    for v in values(s, odd, even):
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
    a, b = [98], [97]  # s1 = "b", s2 = "a", s_k = s_(k-1) + s_(k-2)
    while len(b) < n:
        a, b = b, b + a
    return b[:n]


def pat_pal(n, seed, flip):
    g = Lcg(seed)
    half = [g.next() % 256 for _ in range((n + 1) // 2)]
    s = half + half[::-1][n % 2:]
    if flip is not None:
        s[flip] = (s[flip] + 1) % 256
    return s


def pat_ends(n):
    return [1] + [0] * (n - 2) + [2]


def pat_rand(n, seed, k):
    g = Lcg(seed)
    return [(97 + g.next() % 2) if k == 2 else g.next() % 256 for _ in range(n)]


# (name, builder); the name carries the length.
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
    ("pal_262144", lambda: pat_pal(262144, 5001, None)),
    ("pal_262143", lambda: pat_pal(262143, 5002, None)),
    ("palflipmid_262144", lambda: pat_pal(262144, 5001, 131072)),
    ("palflipend_262144", lambda: pat_pal(262144, 5001, 262141)),
    ("ends_262144", lambda: pat_ends(262144)),
    ("rand2_262144", lambda: pat_rand(262144, 5003, 2)),
    ("rand2_262143", lambda: pat_rand(262143, 5004, 2)),
    ("rand256_262144", lambda: pat_rand(262144, 5005, 256)),
    ("rand256_262143", lambda: pat_rand(262143, 5006, 256)),
    ("rep0_32768", lambda: pat_rep(32768, 0)),
    ("rand256_1000", lambda: pat_rand(1000, 5007, 256)),
]


def selfcheck():
    for s in strings([0, 1], 12):
        o, e, best, count = brute(s)
        o2, e2 = expand(s)
        o3, e3 = linear(s)
        assert (o, e) == (o2, e2) == (o3, e3), s
        assert longest_of(o, e) == best, s
        tot, b2 = eertree(s)
        assert tot == count and b2 == best, (s, tot, count, b2, best)
    g = Lcg(31337)
    for _ in range(3000):
        n = g.next() % 121
        k = (2, 3, 4, 256)[g.next() % 4]
        s = [g.next() % k for _ in range(n)]
        o, e = expand(s)
        assert linear(s) == (o, e), s
        if n <= 40:
            assert (o, e) == brute(s)[:2]
        tot, b2 = eertree(s)
        assert tot == sum(o) + sum(e) and b2 == longest_of(o, e), s
    assert pat_fib(7) == [97, 98, 97, 97, 98, 97, 98], pat_fib(7)


def build():
    out = {}
    f = Fold()
    for s in strings(A2, 16):
        fold_string(f, s)
    out["ex2"] = f
    f = Fold()
    for s in strings(A3, 10):
        fold_string(f, s)
    out["ex3"] = f
    f = Fold()
    for s in strings(A6, 6):
        fold_string(f, s)
    out["ex6"] = f
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
        odd, even = linear(s)
        tot, best = eertree(s)
        assert tot == sum(odd) + sum(even), name
        assert best == longest_of(odd, even), name
        f = Fold()
        for v in values(s, odd, even):
            f.add(v)
        f.cases = 1
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
