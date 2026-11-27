#!/usr/bin/env python3
"""Independent model of core/fenwick: a plain list of elements, sums by direct addition modulo 2^64 (no tree logic).

The operation streams come from the xorshift32 generator mirrored in tests/fenwick/fenwick_tests.cpp. Prints the digests pasted into the
test. Two stream kinds: wrap (any int64 deltas, add and sums) and search (non-negative elements, first_prefix_at_least).
"""
M32, M64 = 0xFFFFFFFF, (1 << 64) - 1
FNV_OFF, FNV_PRIME = 0xCBF29CE484222325, 0x100000001B3
I64_MAX, I64_MIN = (1 << 63) - 1, -(1 << 63)


def signed(v):
    v &= M64
    return v - (1 << 64) if v >> 63 else v


class Fnv:
    def __init__(self):
        self.h = FNV_OFF

    def add(self, v):
        v &= M64
        for k in range(8):
            self.h ^= (v >> (8 * k)) & 0xFF
            self.h = (self.h * FNV_PRIME) & M64


def xs(s):
    s ^= (s << 13) & M32
    s ^= s >> 17
    s ^= (s << 5) & M32
    return s & M32


def run_wrap(seed, steps):
    f, st = Fnv(), xs(seed)
    n = 1 + st % 40
    el = [0] * n
    for _ in range(steps):
        st = xs(st); kind = st % 8
        st = xs(st); i = st % n
        st = xs(st); c = st % 6
        st = xs(st); lo = st
        st = xs(st); hi = st
        delta = [1, -1, signed((hi << 32) | lo), I64_MAX, I64_MIN, lo % 2001 - 1000][c]
        st = xs(st); p = st % (n + 1)
        st = xs(st); q = st
        if kind < 4:
            el[i] = (el[i] + delta) & M64
            f.add(1)
        elif kind < 6:
            f.add(sum(el[:p]) & M64)
        elif kind == 6:
            e = p + q % (n + 1 - p)
            f.add(sum(el[p:e]) & M64)
        else:
            f.add(el[i])
    return f.h


def run_search(seed, steps):
    f, st = Fnv(), xs(seed)
    n = 1 + st % 40
    el = [0] * n
    for _ in range(steps):
        st = xs(st); kind = st % 4
        st = xs(st); i = st % n
        st = xs(st); d = st % 50
        st = xs(st); t = st
        if kind < 2:
            el[i] += d
            f.add(1)
        else:
            total = sum(el)
            target = t % (total + 4) - 2
            k = n + 1
            run = 0
            if target <= 0:
                k = 0
            else:
                for j in range(1, n + 1):
                    run += el[j - 1]
                    if run >= target:
                        k = j
                        break
            f.add(k)
    return f.h


SEEDS = [1, 2, 3, 7, 11, 12345, 0xDEADBEEF, 0x9E3779B9, 42, 2024, 99991, 31337]

if __name__ == "__main__":
    print("// wrap streams")
    for s in SEEDS:
        print("{0x%08X, 0x%016XULL}," % (s, run_wrap(s, 600)))
    print("// search streams")
    for s in SEEDS:
        print("{0x%08X, 0x%016XULL}," % (s, run_search(s, 300)))
