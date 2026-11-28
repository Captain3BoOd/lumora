#!/usr/bin/env python3
"""Independent reference for core/josephus: digests that tests/josephus/josephus_tests.cpp recomputes from the real functions. Standard library only.

Two formulations, compared with each other by selfcheck():
  sim   the literal circle: a list of the people, the count position pos = (pos + k - 1) mod size, the person at pos is removed (list.pop) and the count
        continues at the same index (the next person). O(n^2) with a small constant; used for every whole-order group.
  rec   the recurrence of the library written independently: removed at step s = lift (k - 1) mod (n - s) through the sizes n - s + 1 .. n with
        idx = (idx + k) mod m (Python integers, so no widening question); the survivor is the step n - 1. O(s) per value; used for the sampled steps at large n.
selfcheck() compares sim with rec on every (n, k) of a grid and the closed forms k = 1 (removed step s is s) and k = 2 (survivor 2 * (n - 2^a), 2^a the largest
power of two not above n).

Every group folds with FNV-1a over 64-bit words (offset basis 0xCBF29CE484222325, prime 0x100000001B3; every word is a u32 or a small integer).
Lists (all values below 2^32 are kept as they are; a list is sorted ascending and a value listed twice is kept once):
  KB(n)  = {2n, 2n + 1, 3n - 1, 3n, 3n + 1, 301, 1000, 2^16, 2^16 + 1, 2^31 - 1, 2^31, 2^31 + 1, 2^32 - 2, 2^32 - 1}, ascending, a value listed twice kept once.
  KL(n)  = {1, 2, 3, 7, n - 1, n, n + 1, 2n + 1, 2^31, 2^32 - 1} (order as listed, a value listed twice is kept once).
  KX(n)  = {1, 2, 3, 7, n - 1, n, n + 1, 2^31, 2^32 - 1} for n = 2^24;  {3, 7, n - 1, n, n + 1, 2^32 - 1} for n = 2^24 - 1.
Groups (a "value" is one removed number; the fold of a whole-order group is: n, k, then the n removed numbers in step order; the fold of a sampled group is, for
every sampled step: n, k, step, value):
  ex_a   EXHAUSTIVE whole order: n = 1 .. 100, k = 1 .. 300 (n outer, k inner).
  ex_b   EXHAUSTIVE whole order: n = 1 .. 40, k in KB(n).
  fen_s  whole order: n = 1000, 4096, 4097, k in KL(n).
  whole_a, whole_b, whole_c   whole removal order (the same fold as fen_s) for n = 65535, 65536, 65537 (one group per n), k in KL(n).
  fen_l  SAMPLED steps: n = 2^20 - 1, 2^20, 2^20 + 1, k in KL(n), steps 0, 1, 2, n / 2, n - 2, n - 1.
  max_a  SAMPLED steps: n = 2^24, k in KX(n), steps 0, 1, 2, n / 2, n - 2, n - 1 (the last one is the survivor).
  max_b  the same for n = 2^24 - 1.
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Usage: josephus_ref.py (prints the generated block), josephus_ref.py --check tests/josephus/josephus_tests.cpp [NAME...] (with names: only those groups),
       josephus_ref.py --only NAME... (prints those groups only), josephus_ref.py --selfcheck.
"""
import sys
M64 = (1 << 64) - 1
BEGIN = "// BEGIN GENERATED REFERENCE (tools/josephus_ref.py)"
END = "// END GENERATED REFERENCE"
FNV_PRIME = 0x100000001B3
U32 = (1 << 32) - 1
MAXN = 1 << 24


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

    def one(self, v):
        self.h = ((self.h ^ v) * FNV_PRIME) & M64

    def many(self, arr):
        h = self.h
        for v in arr:
            h = ((h ^ v) * FNV_PRIME) & M64
        self.h = h


def sim_order(n, k):
    people = list(range(n))
    pos = 0
    order = []
    for _ in range(n):
        pos = (pos + k - 1) % len(people)
        order.append(people.pop(pos))
    return order


def rec_removed(n, k, s):
    m0 = n - s
    idx = (k - 1) % m0
    for m in range(m0 + 1, n + 1):
        idx = (idx + k) % m
    return idx


def uniq(vals):
    out = []
    for v in vals:
        if 1 <= v <= U32 and v not in out:
            out.append(v)
    return out


def kb(n):
    return sorted(uniq([2 * n, 2 * n + 1, 3 * n - 1, 3 * n, 3 * n + 1, 301, 1000, 1 << 16, (1 << 16) + 1, (1 << 31) - 1, 1 << 31, (1 << 31) + 1, U32 - 1, U32]))


def kl(n):
    return uniq([1, 2, 3, 7, n - 1, n, n + 1, 2 * n + 1, 1 << 31, U32])


def kx(n):
    if n == MAXN:
        return uniq([1, 2, 3, 7, n - 1, n, n + 1, 1 << 31, U32])
    return uniq([3, 7, n - 1, n, n + 1, U32])


def whole(F, n, k, order):
    F.one(n)
    F.one(k)
    F.many(order)
    F.cases += len(order)


def g_ex_a():
    F = Fold()
    for n in range(1, 101):
        for k in range(1, 301):
            whole(F, n, k, sim_order(n, k))
    return F


def g_ex_b():
    F = Fold()
    for n in range(1, 41):
        for k in kb(n):
            whole(F, n, k, sim_order(n, k))
    return F


def g_fen_s():
    F = Fold()
    for n in (1000, 4096, 4097):
        for k in kl(n):
            whole(F, n, k, sim_order(n, k))
    return F


def sampled(F, n, k, steps):
    for s in steps:
        F.one(n)
        F.one(k)
        F.one(s)
        F.one(rec_removed(n, k, s))
        F.cases += 1


def g_whole(n):
    F = Fold()
    for k in kl(n):
        whole(F, n, k, sim_order(n, k))
    return F


def g_fen_l():
    F = Fold()
    for n in ((1 << 20) - 1, 1 << 20, (1 << 20) + 1):
        for k in kl(n):
            sampled(F, n, k, [0, 1, 2, n // 2, n - 2, n - 1])
    return F


def g_max(n):
    F = Fold()
    for k in kx(n):
        sampled(F, n, k, [0, 1, 2, n // 2, n - 2, n - 1])
    return F


GROUPS = [
    ("ex_a", g_ex_a),
    ("ex_b", g_ex_b),
    ("fen_s", g_fen_s),
    ("whole_a", lambda: g_whole(65535)),
    ("whole_b", lambda: g_whole(65536)),
    ("whole_c", lambda: g_whole(65537)),
    ("fen_l", g_fen_l),
    ("max_a", lambda: g_max(MAXN)),
    ("max_b", lambda: g_max(MAXN - 1)),
]


def selfcheck():
    for n in range(1, 41):
        for k in list(range(1, 131)) + kb(n):
            order = sim_order(n, k)
            assert sorted(order) == list(range(n))
            for s in range(n):
                assert rec_removed(n, k, s) == order[s]
    g = Lcg(5)
    for n in (100, 257, 1000):
        for k in (1, 2, 3, 7, n - 1, n, n + 1, 2 * n + 1, 1 << 31, U32, g.next() or 1):
            order = sim_order(n, k)
            for s in [0, 1, n // 2, n - 2, n - 1] + [g.next() % n for _ in range(4)]:
                assert rec_removed(n, k, s) == order[s]
    for n in range(1, 400):
        assert sim_order(n, 1) == list(range(n))
        a = 1
        while a * 2 <= n:
            a *= 2
        assert sim_order(n, 2)[-1] == 2 * (n - a)
        assert rec_removed(n, 2, n - 1) == 2 * (n - a)
    for n in (1 << 12, (1 << 12) + 5):
        a = 1 << 12
        assert rec_removed(n, 2, n - 1) == 2 * (n - a)


def block(only=None):
    lines = [BEGIN, "struct RefRow {", "  const char* name;", "  std::size_t cases;", "  std::uint64_t digest;", "};", "constexpr RefRow kRef[] = {"]
    for name, fn in GROUPS:
        if only and name not in only:
            continue
        F = fn()
        lines.append('    {"%s", %d, 0x%016Xull},' % (name, F.cases, F.h))
    lines.append("};")
    lines.append(END)
    return "\n".join(lines)


def main(argv):
    if argv and argv[0] == "--selfcheck":
        selfcheck()
        print("selfcheck ok")
        return 0
    if len(argv) >= 2 and argv[0] == "--check":
        text = open(argv[1]).read()
        names = set(argv[2:])
        if not names:
            selfcheck()
        got = block(names or None).split("\n")
        have = text[text.index(BEGIN):text.index(END) + len(END)]
        for row in got:
            if row.startswith('    {"') and row not in have:
                print("MISMATCH:", row)
                return 1
        if not names and block() != have:
            print("MISMATCH: block differs")
            return 1
        print("ok")
        return 0
    if argv and argv[0] == "--only":
        print(block(set(argv[1:])))
        return 0
    selfcheck()
    print(block())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
