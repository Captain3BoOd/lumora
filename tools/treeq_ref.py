#!/usr/bin/env python3
"""Independent reference for core/treeq (not run in CI).

Written from the header text with Python integers. kNoParent is written NONE = 0xFFFFFFFF. A forest is valid when every parent is NONE or below n,
no node is its own parent and walking up from any node reaches a root within n steps. Two implementations, both different from the C++ code:
  brute  depth and size by walking every node's ancestors; lca by intersecting two ancestor lists; the diameter from the table of ALL pair distances
         (Floyd-Warshall on the undirected tree edges), longest finite distance, smallest pair (a, b) with a <= b.
  bfs    for large forests: adjacency lists, BFS from every root, double sweep for the eccentricities (ecc(v) = max(d(v, x), d(v, y)) for the two ends
         x, y of a longest path of v's tree), a = smallest node whose ecc is the maximum, b = smallest node at distance D from a.
The selfcheck compares bfs with brute on every parent array over up to 5 nodes and on 3000 fixed-seed random forests of up to 300 nodes (brute
distances there come from walking ancestors, not Floyd-Warshall).

Folds. A fold is a 64-bit FNV-style digest of the values below, in order; "cases" is the number of folded arrays or forests.
  all_n     (n = 0..6) EVERY parent array with entries NONE or 0 .. n - 1 (valid and invalid together, (n + 1)^n arrays; entry n stands for NONE, the
            entry of parent[0] changes fastest). Result code of depth_size (Ok 0, BadParent 4). For a valid array also: depth[0 .. n), size[0 .. n), the
            diameter (len, then a and b when n > 0), and the lca of every pair (u, v), u then v ascending. Exhaustive ONLY up to 6 nodes.
  rnd_n     (n = 7..10) SAMPLED: 2000 uniform random arrays per n (entry draw % (n + 1), entry n = NONE), same values as above.
  rf_n      (n = 7..10) SAMPLED: 2000 random valid forests per n (random labelling, each node a root with probability 1/4 or a child of an earlier node).
  big_<p>   n = 65536 for shallow patterns, 8192 for the deep ones (and the stated sizes): depth[0 .. n), size[0 .. n), the diameter (len, a, b) and the lca of the queries listed in QUERIES.
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Usage: treeq_ref.py (prints the generated block), treeq_ref.py --check tests/treeq/treeq_tests.cpp
"""
import sys

M64 = (1 << 64) - 1
NONE = 0xFFFFFFFF
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
BEGIN = "// BEGIN GENERATED REFERENCE (tools/treeq_ref.py)"
END = "// END GENERATED REFERENCE"
MAXN = 65536
DEEPN = 8192  # deep patterns (chains) use 8192 nodes in the suite to keep it fast; the 65536-node chain is checked for depth/size/diameter only


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


def valid(par):
    n = len(par)
    for v, p in enumerate(par):
        if p != NONE and (p >= n or p == v):
            return False
    for v in range(n):
        x, steps = v, 0
        while par[x] != NONE:
            x = par[x]
            steps += 1
            if steps > n:
                return False
    return True


def ancestors(par, v):
    L = [v]
    while par[L[-1]] != NONE:
        L.append(par[L[-1]])
    return L


def brute(par, pairs):
    """depth, size, lca list for the (u, v) pairs, (len, a, b). Floyd-Warshall for the diameter."""
    n = len(par)
    depth = [len(ancestors(par, v)) - 1 for v in range(n)]
    size = [0] * n
    for v in range(n):
        for x in ancestors(par, v):
            size[x] += 1
    res = []
    for u, v in pairs:
        au, av = ancestors(par, u), set(ancestors(par, v))
        r = NONE
        for x in au:
            if x in av:
                r = x
                break
        res.append(r)
    INF = 1 << 30
    d = [[INF] * n for _ in range(n)]
    for v in range(n):
        d[v][v] = 0
        if par[v] != NONE:
            d[v][par[v]] = d[par[v]][v] = 1
    for k in range(n):
        for i in range(n):
            for j in range(n):
                if d[i][k] + d[k][j] < d[i][j]:
                    d[i][j] = d[i][k] + d[k][j]
    best, ba, bb = 0, 0, 0
    for a in range(n):
        for b in range(a, n):
            if d[a][b] < INF and d[a][b] > best:
                best, ba, bb = d[a][b], a, b
    return depth, size, res, (best, ba, bb)


def bfs_diameter(par):
    n = len(par)
    adj = [[] for _ in range(n)]
    for v in range(n):
        if par[v] != NONE:
            adj[v].append(par[v])
            adj[par[v]].append(v)

    def dist(s):
        d = {s: 0}
        q = [s]
        for x in q:
            for y in adj[x]:
                if y not in d:
                    d[y] = d[x] + 1
                    q.append(y)
        return d
    ecc = [0] * n
    seen = [False] * n
    for r in range(n):
        if seen[r]:
            continue
        d0 = dist(r)
        for v in d0:
            seen[v] = True
        x = max(d0, key=lambda v: (d0[v], -v))
        dx = dist(x)
        y = max(dx, key=lambda v: (dx[v], -v))
        dy = dist(y)
        for v in d0:
            ecc[v] = max(dx[v], dy[v])
    D = max(ecc) if n else 0
    if n == 0:
        return (0, 0, 0)
    if D == 0:
        return (0, 0, 0)
    a = min(v for v in range(n) if ecc[v] == D)
    da = dist(a)
    b = min(v for v in da if da[v] == D)
    return (D, a, b)


def bfs_all(par, pairs):
    """depth, size, lca by marking ancestors with a stamp, (len, a, b): different code path from brute."""
    n = len(par)
    kids = [[] for _ in range(n)]
    roots = []
    for v in range(n):
        if par[v] == NONE:
            roots.append(v)
        else:
            kids[par[v]].append(v)
    depth = [0] * n
    order = []
    for r in roots:
        order.append(r)
    i = 0
    while i < len(order):
        x = order[i]
        i += 1
        for c in kids[x]:
            depth[c] = depth[x] + 1
            order.append(c)
    size = [1] * n
    for x in reversed(order):
        if par[x] != NONE:
            size[par[x]] += size[x]
    res = []
    stamp = [-1] * n
    for qi, (u, v) in enumerate(pairs):
        x = u
        while True:
            stamp[x] = qi
            if par[x] == NONE:
                break
            x = par[x]
        y = v
        r = NONE
        while True:
            if stamp[y] == qi:
                r = y
                break
            if par[y] == NONE:
                break
            y = par[y]
        res.append(r)
    return depth, size, res, bfs_diameter(par)


def fold_forest(f, par, pairs, big=False):
    if big:
        depth, size, res, diam = bfs_all(par, pairs)
    else:
        depth, size, res, diam = brute(par, pairs)
    n = len(par)
    f.add(0)
    for v in depth:
        f.add(v)
    for v in size:
        f.add(v)
    f.add(diam[0])
    if n > 0:
        f.add(diam[1])
        f.add(diam[2])
    for r in res:
        f.add(r)
    f.cases += 1


def all_pairs(n):
    return [(u, v) for u in range(n) for v in range(n)]


def fold_array(f, par):
    n = len(par)
    if not valid(par):
        f.add(4)
        f.cases += 1
        return
    fold_forest(f, par, all_pairs(n))


def array_from_counter(n, c):
    par = []
    for _ in range(n):
        e = c % (n + 1)
        c //= (n + 1)
        par.append(NONE if e == n else e)
    return par


def rnd_forest(rng, n):
    order = list(range(n))
    for i in range(n - 1, 0, -1):
        j = rng.next() % (i + 1)
        order[i], order[j] = order[j], order[i]
    par = [NONE] * n
    for i in range(n):
        if i == 0 or rng.next() % 4 == 0:
            par[order[i]] = NONE
        else:
            par[order[i]] = order[rng.next() % i]
    return par


# ---- large patterns (name, builder, number of queries) ----
def p_path_inc(n): return [NONE] + [i - 1 for i in range(1, n)]
def p_path_dec(n): return [i + 1 for i in range(n - 1)] + [NONE]
def p_star0(n): return [NONE] + [0] * (n - 1)
def p_star_last(n): return [n - 1] * (n - 1) + [NONE]
def p_binary(n): return [NONE] + [(i - 1) // 2 for i in range(1, n)]
def p_binary_rev(n):
    b = p_binary(n)
    par = [NONE] * n
    for i in range(n):
        if b[i] != NONE:
            par[n - 1 - i] = n - 1 - b[i]
    return par
def p_caterpillar(n):
    s = n // 2
    return [NONE] + [i - 1 for i in range(1, s)] + [j for j in range(n - s)]
def p_all_roots(n): return [NONE] * n
def p_two_paths(n):
    h = n // 2
    return [NONE if i in (0, h) else i - 1 for i in range(n)]
def p_path_small(n):
    h = n // 2
    par = [NONE] + [i - 1 for i in range(1, h)]
    i = h
    while i < n:
        par.append(NONE)
        for k in range(1, 3):
            if i + k < n:
                par.append(i)
        i += 3
    return par
def p_random(n, seed):
    return rnd_forest(Lcg(seed), n)


BIG = [
    ("path_inc", lambda: p_path_inc(DEEPN), 128),
    ("path_dec", lambda: p_path_dec(DEEPN), 128),
    ("star0", lambda: p_star0(MAXN), 65536),
    ("star_last", lambda: p_star_last(MAXN), 65536),
    ("binary", lambda: p_binary(MAXN), 65536),
    ("binary_rev", lambda: p_binary_rev(MAXN), 65536),
    ("caterpillar", lambda: p_caterpillar(DEEPN), 128),
    ("all_roots", lambda: p_all_roots(MAXN), 65536),
    ("two_paths", lambda: p_two_paths(DEEPN), 128),
    ("path_small", lambda: p_path_small(DEEPN), 128),
    ("random_65536", lambda: p_random(MAXN, 7001), 65536),
    ("random_40000", lambda: p_random(40000, 7002), 65536),
    ("random_1000", lambda: p_random(1000, 7003), 65536),
    ("path_inc_8191", lambda: p_path_inc(DEEPN - 1), 128),
    ("star0_65535", lambda: p_star0(65535), 65536),
    ("path_inc_2", lambda: p_path_inc(2), 128),
    ("binary_65535", lambda: p_binary(65535), 65536),
]


def queries(n, q, seed):
    pairs = [(0, n - 1), (n - 1, 0), (n - 1, n - 1)]
    rng = Lcg(seed)
    while len(pairs) < q:
        pairs.append((rng.next() % n, rng.next() % n))
    return pairs[:q]


def lit(v):
    return "%du" % v


def build():
    out = {}
    for n in range(0, 7):
        f = Fold()
        for c in range((n + 1) ** n):
            fold_array(f, array_from_counter(n, c))
        out["all_%d" % n] = f
    for n in range(7, 11):
        rng = Lcg(1000 + n)
        f = Fold()
        for _ in range(2000):
            par = []
            for _ in range(n):
                e = rng.next() % (n + 1)
                par.append(NONE if e == n else e)
            fold_array(f, par)
        out["rnd_%d" % n] = f
        rng = Lcg(2000 + n)
        f = Fold()
        for _ in range(2000):
            fold_array(f, rnd_forest(rng, n))
        out["rf_%d" % n] = f
    for name, mk, q in BIG:
        par = mk()
        f = Fold()
        fold_forest(f, par, queries(len(par), q, 9000 + len(name)), big=True)
        out["big_" + name] = f
    return out


def selfcheck():
    for n in range(1, 6):
        for c in range((n + 1) ** n):
            par = array_from_counter(n, c)
            if not valid(par):
                continue
            pairs = all_pairs(n)
            d1, s1, r1, g1 = brute(par, pairs)
            d2, s2, r2, g2 = bfs_all(par, pairs)
            assert (d1, s1, r1, g1) == (d2, s2, r2, g2), (par, g1, g2)
    rng = Lcg(31337)
    for _ in range(3000):
        n = 1 + rng.next() % 300
        par = rnd_forest(rng, n)
        # cheap independent diameter: all distances by walking ancestors (n <= 300)
        depth = [len(ancestors(par, v)) - 1 for v in range(n)]
        best = (0, 0, 0)
        for a in range(n):
            aa = ancestors(par, a)
            pos = {x: i for i, x in enumerate(aa)}
            for b in range(a, n):
                x, st = b, 0
                while x not in pos and par[x] != NONE:
                    x = par[x]
                    st += 1
                if x in pos:
                    dd = st + pos[x]
                    if dd > best[0]:
                        best = (dd, a, b)
        assert best == bfs_diameter(par), (par, best, bfs_diameter(par))
    # header facts
    assert brute([NONE, 0, 0], all_pairs(3))[3] == (2, 1, 2)
    assert brute([NONE, NONE], all_pairs(2))[3] == (0, 0, 0)


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
