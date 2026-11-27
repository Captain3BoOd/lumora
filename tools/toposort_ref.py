#!/usr/bin/env python3
"""Independent reference for core/toposort (not run in CI).

Written from the header text with Python sets: at each step the smallest-numbered node that is not placed and whose predecessors are all
placed comes next; no such node while some node is unplaced is a Cycle. The check order is applied exactly as the header lists it (bad edge,
then scratch size when nodes > 0, then cycle). The scan-based result is cross-checked, every time this script runs, against a brute force
over permutations (the first valid permutation in lexicographic order) for every graph of the exhaustive scenario (n = 1 to 4) and for 300
pseudo-random graphs with 5 nodes. It prints a C++ block with 64-bit digests that the tests recompute.

  exh    every subset of the n*n ordered pairs (self-loops included) as an edge list in pair order, n = 1 to 4 (66,066 graphs), each with
         scratch_words = need, need - 1 and need + 5 where need = 3 * nodes + edges + 1
  rand   400 seeds: 0 to 40 nodes, 5 modes (random edges, a DAG under a random relabelling, a DAG with duplicate edges, a DAG with one bad
         endpoint, a DAG with one self-loop), the same three scratch sizes

  python3 tools/toposort_ref.py            print the generated C++ block
  python3 tools/toposort_ref.py --check F  fail unless the block in F matches
The random scenario is NOT exhaustive.
"""
import itertools
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/toposort_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
OK, BADEDGE, SCRATCH, CYCLE = 0, 3, 4, 5
EXH_MAX_N = 4
RAND_SEEDS = 400


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


def scan_order(n, edges):
    preds = [set() for _ in range(n)]
    for a, b in edges:
        preds[b].add(a)
    placed = []
    done = set()
    while len(placed) < n:
        for v in range(n):
            if v not in done and preds[v] <= done:
                placed.append(v)
                done.add(v)
                break
        else:
            return None
    return placed


def brute_order(n, edges):
    for perm in itertools.permutations(range(n)):
        pos = {v: i for i, v in enumerate(perm)}
        if all(pos[a] < pos[b] for a, b in edges):
            return list(perm)
    return None


def expect(n, flat, scratch_words):
    e = len(flat) // 2
    if any(x >= n for x in flat):
        return BADEDGE, []
    if n == 0:
        return OK, []
    if scratch_words < 3 * n + e + 1:
        return SCRATCH, []
    edges = [(flat[2 * k], flat[2 * k + 1]) for k in range(e)]
    order = scan_order(n, edges)
    return (CYCLE, []) if order is None else (OK, order)


def fold_graph(f, n, flat):
    e = len(flat) // 2
    need = 3 * n + e + 1
    for sw in (need, need - 1, need + 5):
        code, order = expect(n, flat, sw)
        f.ops += 1
        f.add(code << 32)
        for v in order:
            f.add(v)
    f.cases += 1


def exh_scenario():
    f = Fold()
    for n in range(1, EXH_MAX_N + 1):
        pairs = [(i, j) for i in range(n) for j in range(n)]
        for mask in range(1 << (n * n)):
            edges = [pairs[k] for k in range(n * n) if mask >> k & 1]
            if n <= 4:
                a, b = scan_order(n, edges), brute_order(n, edges)
                assert a == b, (n, edges, a, b)
            flat = [x for ab in edges for x in ab]
            fold_graph(f, n, flat)
    return f


def rand_graph(seed):
    g = Lcg(seed)
    n = g.next() % 41
    e = g.next() % (2 * n + 3)
    mode = seed % 5
    perm = list(range(n))
    for i in range(n - 1, 0, -1):
        j = g.next() % (i + 1)
        perm[i], perm[j] = perm[j], perm[i]
    edges = []
    for _ in range(e):
        a, b = g.next(), g.next()
        m = n if n > 0 else 4
        a, b = a % m, b % m
        if mode != 0 and n >= 2:
            if a == b:
                b = (a + 1) % n
            lo, hi = min(a, b), max(a, b)
            a, b = perm[lo], perm[hi]
        edges.append((a, b))
    if mode == 2:
        for k in range(e):
            r = g.next()
            if r % 4 == 0 and k > 0:
                edges[k] = edges[(r >> 8) % k]
    flat = [x for ab in edges for x in ab]
    if mode == 3 and e > 0:
        pos = g.next() % (2 * e)
        flat[pos] = n + g.next() % 3
    if mode == 4 and e > 0 and n > 0:
        pos = g.next() % e
        v = g.next() % n
        flat[2 * pos] = v
        flat[2 * pos + 1] = v
    return n, flat


def rand_scenario():
    f = Fold()
    for seed in range(1, RAND_SEEDS + 1):
        n, flat = rand_graph(seed)
        fold_graph(f, n, flat)
    return f


def cross_check_five():
    g = Lcg(777)
    for _ in range(300):
        edges = [(g.next() % 5, g.next() % 5) for _ in range(g.next() % 9)]
        assert scan_order(5, edges) == brute_order(5, edges)


def render():
    cross_check_five()
    out = [BEGIN]
    out.append("const std::uint32_t kRefExhMaxN = %d, kRefRandSeeds = %d;" % (EXH_MAX_N, RAND_SEEDS))
    for name, fn in (("exh", exh_scenario), ("rand", rand_scenario)):
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
