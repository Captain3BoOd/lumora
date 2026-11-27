#!/usr/bin/env python3
"""Independent model of core/spans on a set of integer points (no intervals in the model).

The model stores the covered integers in a Python set, derives the spans from it and applies the capacity rule to the derived list. The
operation stream comes from the xorshift32 generator mirrored in tests/spans/spans_tests.cpp. Prints the digests pasted into the test.
"""
import sys

LO, HI = -40, 80  # coordinates of random operations
M32 = 0xFFFFFFFF
FNV_OFF, FNV_PRIME, M64 = 0xCBF29CE484222325, 0x100000001B3, (1 << 64) - 1


class Fnv:
    def __init__(self):
        self.h = FNV_OFF

    def add(self, v):
        v &= M64
        for k in range(8):
            self.h ^= (v >> (8 * k)) & 0xFF
            self.h = (self.h * FNV_PRIME) & M64


def spans_of(points):
    out = []
    for p in sorted(points):
        if out and out[-1][1] == p:
            out[-1][1] = p + 1
        else:
            out.append([p, p + 1])
    return out


class Model:
    def __init__(self):
        self.points = set()

    def add(self, b, e):
        if b >= e:
            return True
        new = self.points | set(range(b, e))
        if len(spans_of(new)) > 16:
            return False
        self.points = new
        return True

    def remove(self, b, e):
        if b >= e:
            return True
        new = self.points - set(range(b, e))
        if len(spans_of(new)) > 16:
            return False
        self.points = new
        return True


def xorshift(state):
    state ^= (state << 13) & M32
    state ^= state >> 17
    state ^= (state << 5) & M32
    return state & M32


def run(seed, steps):
    m = Model()
    f = Fnv()
    st = seed
    for _ in range(steps):
        st = xorshift(st); kind = st % 16
        st = xorshift(st); a = LO + st % (HI - LO)
        st = xorshift(st); w = st % 3 if kind < 12 else st % 2
        b, e = a, a + w
        if kind < 12:
            ok = m.add(b, e)
        elif kind < 15:
            ok = m.remove(b, e)
        else:
            m.points = set(); ok = True
        f.add(1 if ok else 0)
        sp = spans_of(m.points)
        f.add(len(sp))
        for s in sp:
            f.add(s[0]); f.add(s[1])
        f.add(sum(1 for x in range(-40, 90) if x in m.points))
    return f.h


if __name__ == "__main__":
    seeds = [1, 2, 3, 7, 11, 12345, 0xDEADBEEF, 0x9E3779B9, 42, 2024, 99991, 31337]
    for s in seeds:
        print("{0x%08X, 0x%016XULL}," % (s, run(s, 500)))
