#!/usr/bin/env python3
"""Independent reference for core/coins: digests that tests/coins/coins_tests.cpp recomputes from the real functions.

Two implementations, compared with each other by selfcheck(): brute (the definitions: breadth-first search over amounts for the fewest coins, an
enumeration of multiplicity vectors for the multisets, an enumeration of bit masks for the subsets; exact integers) and fast (per residue class of
the coin value, prefix sums and prefix minima; a different formulation from the library's in-place sweeps). The generated block is spliced into the
test file; `--check` regenerates it and compares.

Every case folds (FNV-1a over 64-bit words): m, the m coin values, n, then min_count[0 .. n], ways[0 .. n], subset_ways[0 .. n] (kUnreachable =
0xFFFFFFFF, counts modulo 998244353). A group folds the cases in the order given below; where a group lists several n for one coin list, every n
folds as its own case (the tables are prefixes of the table at the largest n).

Groups:
  g4    EXHAUSTIVE: every ordered coin list of length 0 .. 5 over {1, 2, 3, 4} (1365 lists, by length, then lexicographic), every n = 0 .. 30.
  g3    EXHAUSTIVE: every ordered coin list of length 6 over {1, 2, 3} (729 lists), every n = 0 .. 30.
  g2    EXHAUSTIVE: every ordered coin list of length 7 over {1, 2} (128 lists), every n = 0 .. 30.
  p64   EXHAUSTIVE: every ordered coin list of length 1 and 2 over 1 .. 64 (4160 lists) at n = 130 (the test also runs every n = 0 .. 130 and
        compares with the prefix of the n = 130 table).
  rsS, rsM, rsF  SAMPLED, fixed seed: 200 random coin lists each, m = 3 + draw % 62, n = draw % 5001, coin values 1 + draw % 6, 1 + draw % 200 and
        the full u32 draw (a draw of 0 becomes 1). Seeds 8001, 8002, 8003.
  big_mix64      one case: 64 coins, n = 2^20 (the list is described in the test; values 1 .. 3000 from seed 9101 with a few special positions).
  big_pow2       one case: the 20 coins 2^0 .. 2^19 at n = 2^20.
  big_small      cases with n = 2^20 for the coin lists {1, 1}, {} (m = 0), {0xFFFFFFFF}, {1048576}, {1048577} (five cases).
  hit_p, hit_p1  one case each at n = 65536: a list of 40 .. 64 coins (hit_list(1172), hit_list(1341): m = 40 + draw % 25, maxv = 1 + draw % 300, coins
                 1 + draw % maxv, the LCG seeded with the number) where an addition in the ways table gives exactly 998244353, respectively 998244352,
                 before the reduction (found by a search over lists; the test checks that its own oracle sees the hit).
  hit_pm1        one case at n = 2^20: pm1_list() (seed 300270; 64 coins of value 1 .. 29), whose ways table has the final entry kMod - 1 = 998244352
                 at the amount 33777 (found by a search; the test checks that entry).
  zero_w         one case: coins {1, 18, 40, 29, 12} at n = 972268, where coins_ways has a reachable positive amount (n itself) whose exact count
                 148631314946321454 is a multiple of 998244353, so the table entry is 0.
  c64            one case: coins 1 .. 64 at n = 4000 (counts far above the modulus before reduction).
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Also printed in the block: how many entries of the exact (unreduced) tables of the case c64 are at least the modulus.
Usage: coins_ref.py (prints the generated block), coins_ref.py --check tests/coins/coins_tests.cpp [NAME...] (with names: only those groups),
       coins_ref.py --rows (prints the hand-row table), coins_ref.py --only NAME... (prints those groups only, for quick runs).
"""
import itertools
import sys
from itertools import accumulate

M64 = (1 << 64) - 1
MOD = 998244353
UNR = 0xFFFFFFFF
BIGN = 1 << 20
BEGIN = "// BEGIN GENERATED REFERENCE (tools/coins_ref.py)"
END = "// END GENERATED REFERENCE"


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def next(self):
        self.x = (self.x * 6364136223846793005 + 1442695040888963407) & M64
        return self.x >> 32


class Fold:
    def __init__(self):
        self.h = 0xCBF29CE484222325
        self.cases = 0

    def add(self, v):
        self.h = ((self.h ^ v) * 0x100000001B3) & M64


# ---- brute: the definitions ------------------------------------------------------------------------------------------------------------------

def brute_min(coins, n):
    out = [UNR] * (n + 1)
    out[0] = 0
    for v in range(1, n + 1):
        best = None
        for c in coins:
            if c <= v and out[v - c] != UNR and (best is None or out[v - c] + 1 < best):
                best = out[v - c] + 1
        if best is not None:
            out[v] = best
    return out


def brute_ways_exact(coins, n):
    """Multisets over the coin kinds: count the multiplicity vectors (t_0, ..., t_{m-1}) with sum t_i * c_i = v."""
    out = [0] * (n + 1)

    def rec(k, rest, total):
        if k == len(coins):
            out[total] += 1
            return
        t = 0
        while total + t * coins[k] <= n:
            rec(k + 1, rest, total + t * coins[k])
            t += 1

    rec(0, 0, 0)
    return out


def brute_subset_exact(coins, n):
    out = [0] * (n + 1)
    m = len(coins)
    for mask in range(1 << m):
        s = sum(coins[i] for i in range(m) if mask >> i & 1)
        if s <= n:
            out[s] += 1
    return out


# ---- fast: residue classes, prefix sums and prefix minima -----------------------------------------------------------------------------------

INF = 1 << 40


def fast_min(coins, n):
    out = [INF] * (n + 1)
    out[0] = 0
    for c in sorted(set(coins)):
        if c > n:
            continue
        for r in range(min(c, n + 1)):
            seg = out[r::c]
            g = [x - j for j, x in enumerate(seg)]
            out[r::c] = [a + j for j, a in enumerate(accumulate(g, min))]
    return [x if x < INF // 2 else UNR for x in out]


def fast_ways_exact(coins, n):
    out = [0] * (n + 1)
    out[0] = 1
    for c in coins:
        if c > n:
            continue
        for r in range(min(c, n + 1)):
            out[r::c] = list(accumulate(out[r::c]))
    return out


def fast_ways(coins, n):
    out = [0] * (n + 1)
    out[0] = 1
    for c in coins:
        if c > n:
            continue
        for r in range(min(c, n + 1)):
            out[r::c] = [x % MOD for x in accumulate(out[r::c])]
    return out


def fast_subset_exact(coins, n):
    out = [0] * (n + 1)
    out[0] = 1
    for c in coins:
        if c <= n:
            out = out[:c] + [a + b for a, b in zip(out[c:], out)]
    return out


def fast_subset(coins, n):
    out = [0] * (n + 1)
    out[0] = 1
    for c in coins:
        if c <= n:
            out = out[:c] + [(a + b) % MOD for a, b in zip(out[c:], out)]
    return out


def tables(coins, n):
    return fast_min(coins, n), fast_ways(coins, n), fast_subset(coins, n)


def fold_tables(f, coins, n, t):
    f.add(len(coins))
    for c in coins:
        f.add(c)
    f.add(n)
    for tab in t:
        for v in range(n + 1):
            f.add(tab[v])
    f.cases += 1


def lists(alpha, lo, hi):
    for ln in range(lo, hi + 1):
        for p in itertools.product(alpha, repeat=ln):
            yield list(p)


def fold_all_n(f, coins, top):
    t = tables(coins, top)
    for n in range(top + 1):
        fold_tables(f, coins, n, tuple(x[:n + 1] for x in t))


def mix64():
    g = Lcg(9101)
    c = [1 + g.next() % 3000 for _ in range(64)]
    c[5] = 0xFFFFFFFF
    c[9] = 1048576
    c[20] = c[0]
    c[30] = 1048577
    c[40] = c[41] = 7
    return c


SMALL_BIG = [[1, 1], [], [0xFFFFFFFF], [1048576], [1048577]]


def sample_list(g, kind):
    m = 3 + g.next() % 62
    n = g.next() % 5001
    if kind == "rsS":
        c = [1 + g.next() % 6 for _ in range(m)]
    elif kind == "rsM":
        c = [1 + g.next() % 200 for _ in range(m)]
    else:
        c = [(g.next() or 1) for _ in range(m)]
    return c, n


HIT_SEEDS = {"hit_p": 1172, "hit_p1": 1341}


def hit_list(seed):
    """A list found by a search: in the ways table of this list one addition has the sum exactly kMod (seed 1172) or kMod - 1 (seed 1341)."""
    g = Lcg(seed)
    m = 40 + g.next() % 25
    maxv = 1 + g.next() % 300
    return [1 + g.next() % maxv for _ in range(m)]


def pm1_list():
    """seed 300270: m = 64 coins; the ways table at n = 2^20 has the entry 998244352 (kMod - 1) at the amount 33777."""
    g = Lcg(300270)
    g.next()
    maxv = 1 + g.next() % 300
    return [1 + g.next() % maxv for _ in range(64)]


ZERO_COINS = [1, 18, 40, 29, 12]
ZERO_N = 972268
ZERO_EXACT = 148631314946321454  # the exact number of multisets, divisible by 998244353 (verified in selfcheck with two exact computations)


def sweep_ways_exact(coins, n):
    """The textbook in-place sweep with exact integers (a third formulation, used only to confirm the witness)."""
    out = [0] * (n + 1)
    out[0] = 1
    for c in coins:
        for v in range(c, n + 1):
            out[v] += out[v - c]
    return out


def group_names():
    return ["g4", "g3", "g2", "p64", "rsS", "rsM", "rsF", "big_mix64", "big_pow2", "big_small", "c64", "hit_p", "hit_p1", "hit_pm1", "zero_w"]


def build_group(name):
    f = Fold()
    if name == "g4":
        for c in lists([1, 2, 3, 4], 0, 5):
            fold_all_n(f, c, 30)
    elif name == "g3":
        for c in lists([1, 2, 3], 6, 6):
            fold_all_n(f, c, 30)
    elif name == "g2":
        for c in lists([1, 2], 7, 7):
            fold_all_n(f, c, 30)
    elif name == "p64":
        for c in lists(range(1, 65), 1, 2):
            fold_tables(f, c, 130, tables(c, 130))
    elif name in ("rsS", "rsM", "rsF"):
        g = Lcg({"rsS": 8001, "rsM": 8002, "rsF": 8003}[name])
        for _ in range(200):
            c, n = sample_list(g, name)
            fold_tables(f, c, n, tables(c, n))
    elif name == "big_mix64":
        c = mix64()
        fold_tables(f, c, BIGN, tables(c, BIGN))
    elif name in HIT_SEEDS:
        c = hit_list(HIT_SEEDS[name])
        fold_tables(f, c, 65536, tables(c, 65536))
    elif name == "hit_pm1":
        c = pm1_list()
        fold_tables(f, c, BIGN, tables(c, BIGN))
    elif name == "zero_w":
        fold_tables(f, ZERO_COINS, ZERO_N, tables(ZERO_COINS, ZERO_N))
    elif name == "big_pow2":
        c = [1 << k for k in range(20)]
        fold_tables(f, c, BIGN, tables(c, BIGN))
    elif name == "big_small":
        for c in SMALL_BIG:
            fold_tables(f, c, BIGN, tables(c, BIGN))
    elif name == "c64":
        c = list(range(1, 65))
        fold_tables(f, c, 4000, tables(c, 4000))
    else:
        raise KeyError(name)
    return f


def exceed_counts():
    c = list(range(1, 65))
    w = fast_ways_exact(c, 4000)
    s = fast_subset_exact(c, 4000)
    return sum(1 for x in w if x >= MOD), sum(1 for x in s if x >= MOD)


def block(names=None):
    L = [BEGIN, "struct RefRow {", "  const char* name;", "  std::size_t cases;", "  std::uint64_t digest;", "};", "constexpr RefRow kRef[] = {"]
    for nm in names or group_names():
        f = build_group(nm)
        L.append('    {"%s", %d, 0x%016Xull},' % (nm, f.cases, f.h))
    L.append("};")
    if names is None:
        a, b = exceed_counts()
        L.append("constexpr std::size_t kC64WaysAtLeastMod = %d;    // entries of the exact (unreduced) tables of the case c64 that are >= 998244353" % a)
        L.append("constexpr std::size_t kC64SubsetsAtLeastMod = %d;" % b)
        L.append("constexpr std::uint64_t kZeroExact = %dull;    // exact count of multisets for coins {1, 18, 40, 29, 12} at n = 972268" % ZERO_EXACT)
    L.append(END)
    return "\n".join(L) + "\n"


# ---- self-check ----------------------------------------------------------------------------------------------------------------------------

ROWS = [
    ("header example: two kinds of value 1", [1, 1], 4),
    ("no coins", [], 5),
    ("one coin", [3], 10),
    ("coin equal to n", [5], 5),
    ("coin above n", [6, 2], 5),
    ("duplicates and others", [2, 2, 3], 12),
    ("greedy fails", [1, 3, 4], 12),
    ("greedy fails 2", [1, 5, 6], 15),
    ("same value three times", [2, 2, 2], 8),
    ("coins 1 2 5", [1, 2, 5], 20),
    ("all above n", [7, 8, 9], 6),
    ("odd coins only", [3, 5], 20),
    ("n = 0", [1, 2, 3], 0),
    ("n = 0 and no coins", [], 0),
    ("big coin value", [0xFFFFFFFF, 2], 10),
    ("order does not matter", [4, 1, 3], 12),
    ("coin 1 twice and 2", [1, 1, 2], 8),
    ("power of two coins", [1, 2, 4, 8], 16),
    ("unreachable gaps", [4, 6], 20),
    ("ten ones", [1] * 10, 12),
]


def rows_text():
    out = []
    for name, c, n in ROWS:
        t = (brute_min(c, n), [x % MOD for x in brute_ways_exact(c, n)], [x % MOD for x in brute_subset_exact(c, n)])
        vals = lambda v: "{" + ", ".join(("kUnreachable" if x == UNR else "0x%Xu" % x if x >= 0x80000000 else "%du" % x) for x in v) + "}"
        out.append('    {"%s", %s, %d, %s, %s, %s},' % (name, vals(c), n, vals(t[0]), vals(t[1]), vals(t[2])))
    return "\n".join(out)


def selfcheck():
    def same(c, n):
        assert fast_min(c, n) == brute_min(c, n), (c, n)
        assert fast_ways_exact(c, n) == brute_ways_exact(c, n), (c, n)
        assert fast_subset_exact(c, n) == brute_subset_exact(c, n), (c, n)
        assert fast_ways(c, n) == [x % MOD for x in brute_ways_exact(c, n)], (c, n)
        assert fast_subset(c, n) == [x % MOD for x in brute_subset_exact(c, n)], (c, n)

    for c in lists([1, 2, 3], 0, 4):
        for n in (0, 1, 7, 15):
            same(c, n)
    g = Lcg(4242)
    for _ in range(120):
        m = g.next() % 7
        c = [1 + g.next() % (6 if g.next() % 2 else 40) for _ in range(m)]
        if g.next() % 5 == 0:
            c.append(0xFFFFFFFF)
        same(c, g.next() % 31)
    for _, c, n in ROWS:
        same(c, n)
    # the witness for a table entry of 0: n itself is reachable and its exact count is a multiple of the modulus
    w1 = fast_ways_exact(ZERO_COINS, ZERO_N)[ZERO_N]
    w2 = sweep_ways_exact(ZERO_COINS, ZERO_N)[ZERO_N]
    assert w1 == w2 == ZERO_EXACT and ZERO_EXACT % MOD == 0 and ZERO_EXACT // MOD > 0
    assert fast_min(ZERO_COINS, ZERO_N)[ZERO_N] != UNR
    # a few closed forms
    assert fast_ways([1] * 5, 9) == [__import__("math").comb(v + 4, 4) % MOD for v in range(10)]
    assert fast_subset([1] * 6, 8) == [__import__("math").comb(6, v) % MOD if v <= 6 else 0 for v in range(9)]


def main():
    argv = sys.argv[1:]
    if argv[:1] == ["--rows"]:
        print(rows_text())
        return 0
    if argv[:1] == ["--only"]:
        sys.stdout.write(block(argv[1:]))
        return 0
    selfcheck()
    if len(argv) >= 3 and argv[0] == "--check":
        # partial check (it takes about two minutes for everything): only the named groups, each row must appear in the test file
        txt = open(argv[1]).read()
        have = set(l.strip() for l in txt.split("\n"))
        rows = [l for l in block(argv[2:]).split("\n") if l.strip().startswith('{"')]
        bad = [r for r in rows if r.strip() not in have]
        print("ok (partial: %s)" % " ".join(argv[2:]) if not bad else "differs: %s" % bad)
        return 1 if bad else 0
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
