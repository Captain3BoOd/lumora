#!/usr/bin/env python3
"""Independent reference for core/monostack: digests that tests/monostack/monostack_tests.cpp recomputes from the real functions.

Two implementations, compared with each other by selfcheck(): brute (the definitions: for every i scan left and right; every range for the
rectangle; every window by a plain scan) and fast (monotonic stack and deque, the methods the library uses, written again here). The generated
block is spliced into the test file; `--check` regenerates it and compares.

Every case folds (FNV-1a over 64-bit words): n, left[0 .. n), right[0 .. n) (kNone = 0xFFFFFFFF), area, l, r, then for every window size k in
ks(n): k and out[0 .. n - k + 1). ks(n): n <= 12: 1 .. n; n <= 400: {1, 2, 3, 5, 8, n // 4, n // 2, n - 2, n - 1, n} (those in 1 .. n); above 400:
{1, 2, 3, 1000, n // 2, n - 1, n} (those in 1 .. n).

Groups:
  t3     EXHAUSTIVE over values {0, 1, 2}: every array of length 0 .. 11 (by length, then the first value changes fastest).
  t4     EXHAUSTIVE over values {0, 1, 2, 3}: length 0 .. 9.
  e4     EXHAUSTIVE over values {0, 1, 0x80000000, 0xFFFFFFFF}: length 0 .. 8.
  perm   EXHAUSTIVE: every permutation of 0 .. n-1 for n = 0 .. 9 (lexicographic order for each n).
  all256 EXHAUSTIVE: the empty array, all one-element arrays and all two-element arrays over 0 .. 255 (65793).
  rs3, rs16, rs32  SAMPLED, fixed seed: 600 random arrays of length 10 .. 400 (length 10 + draw % 391) over the values draw % 3, draw % 16 and
         the full u32 draw (seeds 7003, 7016, 7032).
  big_<name>_<n>   the listed large arrays (see PATTERNS), one case each.
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Usage: monostack_ref.py (prints the generated block), monostack_ref.py --check tests/monostack/monostack_tests.cpp,
       monostack_ref.py --rows (prints the hand-row table), monostack_ref.py --only NAME... (prints those groups only, for quick runs).
"""
import itertools
import sys
from collections import deque

M64 = (1 << 64) - 1
NONE = 0xFFFFFFFF
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
BEGIN = "// BEGIN GENERATED REFERENCE (tools/monostack_ref.py)"
END = "// END GENERATED REFERENCE"
V4 = [0, 1, 0x80000000, 0xFFFFFFFF]
BIGN = 1048576


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


def ks(n):
    if n <= 12:
        c = range(1, n + 1)
    elif n <= 400:
        c = (1, 2, 3, 5, 8, n // 4, n // 2, n - 2, n - 1, n)
    else:
        c = (1, 2, 3, 1000, n // 2, n - 1, n)
    return sorted(set(k for k in c if 1 <= k <= n))


def brute_bounds(a):
    n = len(a)
    left, right = [], []
    for i in range(n):
        j = i - 1
        while j >= 0 and not a[j] < a[i]:
            j -= 1
        left.append(j if j >= 0 else NONE)
        j = i + 1
        while j < n and not a[j] <= a[i]:
            j += 1
        right.append(j if j < n else NONE)
    return left, right


def brute_rect(a):
    n = len(a)
    best, bl, br = 0, 0, 0
    for l in range(n):
        m = a[l]
        for r in range(l, n):
            m = min(m, a[r])
            ar = (r - l + 1) * m
            if ar > best:
                best, bl, br = ar, l, r
    return best, bl, br


def brute_window(a, k):
    out = []
    for s in range(len(a) - k + 1):
        w = a[s:s + k]
        out.append(s + w.index(min(w)))
    return out


def fast_bounds(a):
    n = len(a)
    left = [NONE] * n
    right = [NONE] * n
    st = []
    for i in range(n):
        v = a[i]
        while st and a[st[-1]] >= v:
            right[st.pop()] = i
        left[i] = st[-1] if st else NONE
        st.append(i)
    return left, right


def fast_rect(a):
    n = len(a)
    best, bl, br = 0, 0, 0
    st = []
    for i in range(n + 1):
        v = a[i] if i < n else -1
        while st and a[st[-1]] >= v:
            h = a[st.pop()]
            l = st[-1] + 1 if st else 0
            r = i - 1
            ar = (r - l + 1) * h
            if ar > best or (ar == best and (l < bl or (l == bl and r < br))):
                best, bl, br = ar, l, r
        st.append(i)
    return best, bl, br


def fast_window(a, k):
    n = len(a)
    dq = deque()
    out = []
    for i in range(n):
        v = a[i]
        while dq and a[dq[-1]] > v:
            dq.pop()
        dq.append(i)
        if dq[0] <= i - k:
            dq.popleft()
        if i + 1 >= k:
            out.append(dq[0])
    return out


def fold_seq(f, a, mode="fast"):
    n = len(a)
    if mode == "brute":
        left, right = brute_bounds(a)
        area, l, r = brute_rect(a)
        win = brute_window
    else:
        left, right = fast_bounds(a)
        area, l, r = fast_rect(a)
        win = fast_window
    f.add(n)
    for v in left:
        f.add(v)
    for v in right:
        f.add(v)
    f.add(area)
    f.add(l)
    f.add(r)
    for k in ks(n):
        f.add(k)
        for v in win(a, k):
            f.add(v)
    f.cases += 1


def seqs(alpha, maxlen):
    k = len(alpha)
    for L in range(maxlen + 1):
        for c in range(k ** L):
            s = []
            x = c
            for _ in range(L):
                s.append(alpha[x % k])
                x //= k
            yield s


def pat_inc(n):
    return list(range(n))


def pat_dec(n):
    return [n - 1 - i for i in range(n)]


def pat_const(n, v):
    return [v] * n


def pat_alt(n, a, b):
    return [a if i % 2 == 0 else b for i in range(n)]


def pat_saw(n, p):
    return [i % p for i in range(n)]


def pat_organ(n):
    h = n // 2
    return [i if i < h else n - 1 - i for i in range(n)]


def pat_rand(n, seed, mod):
    g = Lcg(seed)
    return [g.next() % mod if mod else g.next() for _ in range(n)]


def pat_perm(n, seed):
    g = Lcg(seed)
    a = list(range(n))
    for i in range(n - 1, 0, -1):
        j = g.next() % (i + 1)
        a[i], a[j] = a[j], a[i]
    return a


PATTERNS = [
    ("inc_1048576", lambda: pat_inc(BIGN)),
    ("dec_1048576", lambda: pat_dec(BIGN)),
    ("const0_1048576", lambda: pat_const(BIGN, 0)),
    ("constmax_1048576", lambda: pat_const(BIGN, 0xFFFFFFFF)),
    ("constmax_1048575", lambda: pat_const(BIGN - 1, 0xFFFFFFFF)),
    ("altext_1048576", lambda: pat_alt(BIGN, 0xFFFFFFFF, 0)),
    ("saw1000_1048576", lambda: pat_saw(BIGN, 1000)),
    ("organ_1048576", lambda: pat_organ(BIGN)),
    ("rand2_1048576", lambda: pat_rand(BIGN, 9011, 2)),
    ("rand32_1048576", lambda: pat_rand(BIGN, 9013, 0)),
    ("rand32_1048575", lambda: pat_rand(BIGN - 1, 9014, 0)),
    ("perm_1048576", lambda: pat_perm(BIGN, 9015)),
]


def selfcheck():
    for alpha, mx in (([0, 1, 2], 7), (V4, 6)):
        for s in seqs(alpha, mx):
            assert fast_bounds(s) == brute_bounds(s), s
            assert fast_rect(s) == brute_rect(s), s
            for k in range(1, len(s) + 1):
                assert fast_window(s, k) == brute_window(s, k), (s, k)
    for n in range(8):
        for p in itertools.permutations(range(n)):
            s = list(p)
            assert fast_bounds(s) == brute_bounds(s) and fast_rect(s) == brute_rect(s), s
            for k in range(1, n + 1):
                assert fast_window(s, k) == brute_window(s, k), (s, k)
    g = Lcg(4242)
    for _ in range(300):
        n = 1 + g.next() % 70
        mod = (2, 3, 5, 0)[g.next() % 4]
        s = [g.next() % mod if mod else g.next() for _ in range(n)]
        assert fast_bounds(s) == brute_bounds(s), s
        assert fast_rect(s) == brute_rect(s), s
        for k in {1, 2, n, max(1, n // 2), g.next() % n + 1}:
            assert fast_window(s, k) == brute_window(s, k), (s, k)
    # closed forms for the large arrays
    n = BIGN
    assert fast_rect(pat_const(n, 0xFFFFFFFF)) == (n * 0xFFFFFFFF, 0, n - 1)
    assert fast_rect(pat_const(n, 0)) == (0, 0, 0)
    assert fast_rect(pat_inc(n)) == (max((n - i) * i for i in range(n)), n // 2, n - 1)


def group_names():
    return ["t3", "t4", "e4", "perm", "all256", "rs3", "rs16", "rs32"] + ["big_" + p[0] for p in PATTERNS]


def build_group(name):
    f = Fold()
    if name == "t3":
        for s in seqs([0, 1, 2], 11):
            fold_seq(f, s)
    elif name == "t4":
        for s in seqs([0, 1, 2, 3], 9):
            fold_seq(f, s)
    elif name == "e4":
        for s in seqs(V4, 8):
            fold_seq(f, s)
    elif name == "perm":
        for n in range(10):
            for p in itertools.permutations(range(n)):
                fold_seq(f, list(p))
    elif name == "all256":
        fold_seq(f, [])
        for a in range(256):
            fold_seq(f, [a])
        for c in range(65536):
            fold_seq(f, [c % 256, c // 256])
    elif name in ("rs3", "rs16", "rs32"):
        mod, seed = {"rs3": (3, 7003), "rs16": (16, 7016), "rs32": (0, 7032)}[name]
        g = Lcg(seed)
        for _ in range(600):
            n = 10 + g.next() % 391
            s = [(g.next() % mod if mod else g.next()) for _ in range(n)]
            fold_seq(f, s)
    else:
        for pn, make in PATTERNS:
            if "big_" + pn == name:
                fold_seq(f, make())
                return f
        raise KeyError(name)
    return f


def block(names=None):
    names = names or group_names()
    out = {n: build_group(n) for n in names}
    L = [BEGIN]
    for k in sorted(out):
        L.append("const std::uint64_t kRefDigest_%s = 0x%016XULL;" % (k, out[k].h))
        L.append("const std::size_t kRefCases_%s = %d;" % (k, out[k].cases))
    L.append(END)
    return "\n".join(L) + "\n"


ROWS = [
    ("empty-like single", [5]),
    ("single zero", [0]),
    ("two equal", [3, 3]),
    ("two up", [1, 2]),
    ("two down", [2, 1]),
    ("classic histogram", [2, 1, 5, 6, 2, 3]),
    ("equal run", [4, 4, 4, 4]),
    ("equal plateau between lows", [1, 5, 5, 5, 1]),
    ("zeros inside", [3, 0, 3, 0, 3]),
    ("all zero", [0, 0, 0]),
    ("max values", [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF]),
    ("extremes alternate", [0xFFFFFFFF, 0, 0xFFFFFFFF, 0]),
    ("area tie wide vs tall", [6, 6, 6, 1, 9, 9, 1, 18]),
    ("area tie same l", [4, 2, 4, 6, 1, 8]),
    ("area tie equal height blocks", [3, 3, 1, 3, 3]),
    ("tie between two blocks of 12", [4, 4, 4, 1, 6, 6, 1, 3, 3, 3, 3]),
    ("increasing", [1, 2, 3, 4, 5]),
    ("decreasing", [5, 4, 3, 2, 1]),
    ("valley", [5, 3, 1, 3, 5]),
    ("mountain", [1, 3, 5, 3, 1]),
    ("leftmost minimum ties", [2, 1, 1, 3, 1, 1, 2]),
    ("window ties at the front", [7, 7, 7, 2, 2, 9]),
    ("repeated pattern", [1, 3, 1, 3, 1, 3, 1]),
    ("large values near the top", [0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFE, 0x80000000, 0xFFFFFFFF]),
]


def rows():
    out = []
    for name, a in ROWS:
        left, right = brute_bounds(a)
        area, l, r = brute_rect(a)
        wins = [(k, brute_window(a, k)) for k in range(1, len(a) + 1)]
        out.append((name, a, left, right, area, l, r, wins))
    return out


def cvals(v):
    return "{" + ", ".join(("kNone" if x == NONE else "%du" % x if x < 0x80000000 else "0x%Xu" % x) for x in v) + "}"


def rows_text():
    L = []
    for name, a, left, right, area, l, r, wins in rows():
        w = ", ".join("{%d, %s}" % (k, cvals(o)) for k, o in wins)
        L.append('    {"%s", %s, %s, %s, %dull, %du, %du, {%s}},' % (name, cvals(a), cvals(left), cvals(right), area, l, r, w))
    return "\n".join(L)


def main():
    argv = sys.argv[1:]
    if argv[:1] == ["--rows"]:
        print(rows_text())
        return 0
    if argv[:1] == ["--only"]:
        sys.stdout.write(block(argv[1:]))
        return 0
    selfcheck()
    blk = block()
    if len(argv) >= 2 and argv[0] == "--check":
        txt = open(argv[1]).read()
        i, j = txt.find(BEGIN), txt.find(END)
        if i < 0 or j < 0:
            print("no generated block in", argv[1])
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
