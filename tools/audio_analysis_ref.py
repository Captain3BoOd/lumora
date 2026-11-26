#!/usr/bin/env python3
"""Independent reference for core/audio/analysis (not run in CI).

Builds the Q15 cosine and Hann tables with math.cos (cross-checked against a 50-digit decimal evaluation),
implements the spectrum as a recursive decimation-in-time FFT (the C++ code is iterative and in place), and
prints expected outputs for fixed fixtures as a C++ block for the tests.

  python3 tools/audio_analysis_ref.py                      print the reference block for the tests
  python3 tools/audio_analysis_ref.py --check F            fail unless the reference block in F matches
  python3 tools/audio_analysis_ref.py --emit-tables        print the table block embedded in analysis.cpp
  python3 tools/audio_analysis_ref.py --check-tables F     fail unless the table block in F matches
"""
import math
import sys
from decimal import Decimal, getcontext

BEGIN = "// BEGIN GENERATED REFERENCE (tools/audio_analysis_ref.py)"
END = "// END GENERATED REFERENCE"
TBEGIN = "// BEGIN GENERATED TABLES (tools/audio_analysis_ref.py --emit-tables)"
TEND = "// END GENERATED TABLES"
M64 = 2**64 - 1
SIZES = [64, 128, 256, 512, 1024]


def rnd(x):
    """Round half away from zero."""
    return int(math.floor(x + 0.5)) if x >= 0 else -int(math.floor(-x + 0.5))


def make_tables():
    c, h = [], []
    for j in range(1024):
        a = 2 * math.pi * j / 1024
        c.append(max(-32767, min(32767, rnd(32768 * math.cos(a)))))
        h.append(min(32767, rnd(32768 * (0.5 - 0.5 * math.cos(a)))))
    return c, h


def decimal_tables():
    getcontext().prec = 60
    # pi by Machin's formula
    def arctan_inv(n):
        x = Decimal(1) / n
        total, term, k = x, x, 1
        while abs(term) > Decimal(10) ** -58:
            term = -term * x * x
            k += 2
            total += term / k
        return total
    pi = 4 * (4 * arctan_inv(5) - arctan_inv(239))

    def cos_dec(a):
        a = a % (2 * pi)
        x2 = a * a
        total, term, k = Decimal(1), Decimal(1), 0
        while abs(term) > Decimal(10) ** -58:
            k += 2
            term = -term * x2 / (k * (k - 1))
            total += term
        return total

    def rd(v):
        s = 1 if v >= 0 else -1
        return s * int((abs(v) + Decimal("0.5")).to_integral_value(rounding="ROUND_FLOOR"))

    c, h = [], []
    for j in range(1024):
        cs = cos_dec(2 * pi * j / 1024)
        c.append(max(-32767, min(32767, rd(32768 * cs))))
        h.append(min(32767, rd(32768 * (Decimal("0.5") - Decimal("0.5") * cs))))
    return c, h


C, H = make_tables()
DC, DH = decimal_tables()
assert C == DC and H == DH, "double tables differ from the 50-digit evaluation"


def butterfly(a, b, t):
    wr, wi = C[t], -C[(t + 768) % 1024]
    tre = (b[0] * wr - b[1] * wi + 16384) >> 15
    tim = (b[0] * wi + b[1] * wr + 16384) >> 15
    return ((a[0] + tre) >> 1, (a[1] + tim) >> 1), ((a[0] - tre) >> 1, (a[1] - tim) >> 1)


def fft_rec(x):
    """x is a list of (re, im); recursive decimation in time with the same rounding per butterfly."""
    m = len(x)
    if m == 1:
        return x
    even = fft_rec(x[0::2])
    odd = fft_rec(x[1::2])
    out = [None] * m
    for j in range(m // 2):
        lo, hi = butterfly(even[j], odd[j], j * (1024 // m))
        out[j], out[j + m // 2] = lo, hi
    return out


def power(samples):
    n = len(samples)
    s = 1024 // n
    x = [((v * H[i * s]) >> 15, 0) for i, v in enumerate(samples)]
    y = fft_rec(x)
    return [y[k][0] ** 2 + y[k][1] ** 2 for k in range(n // 2)]


class Rng:
    """xorshift64*, the same in the tests."""

    def __init__(self, seed):
        self.s = seed

    def next(self):
        x = self.s
        x ^= x >> 12
        x ^= (x << 25) & M64
        x ^= x >> 27
        self.s = x
        return (x * 0x2545F4914F6CDD1D) & M64


def i16(v):
    v &= 0xFFFF
    return v - 65536 if v >= 32768 else v


def fixtures(n):
    f = []
    f.append(("silence", [0] * n))
    f.append(("dc_pos", [32767] * n))
    f.append(("dc_neg", [-32768] * n))
    f.append(("alt", [-32768 if i % 2 == 0 else 32767 for i in range(n)]))
    for pos in (0, 1, n // 2, n - 1):
        f.append(("impulse_%d" % pos, [32767 if i == pos else 0 for i in range(n)]))
    f.append(("impulse_neg_1", [-32768 if i == 1 else 0 for i in range(n)]))
    for b in (1, 2, 5, n // 4, n // 2 - 1):
        f.append(("tone_%d" % b, [(32767 * C[(b * i * (1024 // n)) % 1024]) >> 15 for i in range(n)]))
        f.append(("tone_neg_%d" % b, [-((32768 * C[(b * i * (1024 // n)) % 1024]) >> 15) for i in range(n)]))
    for b in (1, 3, n // 2 - 1):
        f.append(("square_%d" % b, [32767 if (i * b * 2 // n) % 2 == 0 else -32768 for i in range(n)]))
    f.append(("ramp", [-32768 + i * 65535 // (n - 1) for i in range(n)]))
    for seed in (1, 2, 3):
        r = Rng(seed * 0x9E3779B97F4A7C15 & M64)
        f.append(("noise_%d" % seed, [i16(r.next() >> 48) for _ in range(n)]))
    r = Rng(77)
    f.append(("quiet_noise", [i16(r.next() >> 48) >> 8 for _ in range(n)]))
    return f


class Fnv:
    def __init__(self):
        self.h = 0xcbf29ce484222325

    def byte(self, b):
        self.h ^= b & 0xFF
        self.h = (self.h * 0x100000001b3) % 2**64

    def num(self, v, n):
        for i in range(n):
            self.byte((v >> (8 * i)) & 0xFF)


def level(samples):
    return max(abs(v) for v in samples) if samples else 0, sum(v * v for v in samples) % 2**64


def env_step(env, x, att, rel):
    att, rel = min(att, 65536), min(rel, 65536)
    if x == env:
        return env
    coef = att if x > env else rel
    return env + (((x - env) * coef) >> 16)


def band(power_, edges, count, bins):
    """(ok, energies); ok False on violation."""
    if any(edges[i] > edges[i + 1] for i in range(count)) or edges[count] > bins:
        return False, []
    return True, [sum(power_[edges[b]:edges[b + 1]]) for b in range(count)]


def digests():
    out = {}
    h = Fnv()
    cnt = 0
    for n in SIZES:
        for name, s in fixtures(n):
            p = power(s)
            for v in p:
                assert 0 <= v < 2**32
                h.num(v, 4)
            pk, sq = level(s)
            h.num(pk, 4)
            h.num(sq, 8)
            cnt += 1
    out["fixtures"] = (h.h, cnt)
    h = Fnv()
    for v in range(70000):
        h.num(math.isqrt(v), 4)
    ks = list(range(0, 70000, 7)) + [2**16 - 1, 2**16, 2**31 - 1, 2**31, 2**32 - 2, 2**32 - 1]
    r = Rng(5)
    ks += [r.next() >> 32 for _ in range(3000)]
    for k in ks:
        for v in (k * k - 1, k * k, k * k + 1):
            if 0 <= v <= M64:
                h.num(math.isqrt(v), 4)
    h.num(math.isqrt(M64), 4)
    r = Rng(6)
    for _ in range(20000):
        v = r.next() >> (r.next() % 64)
        h.num(math.isqrt(v), 4)
    out["isqrt"] = h.h
    h = Fnv()
    vals = [0, 1, 2, 100, 65535, 65536, 65537, 2**31, 2**32 - 2, 2**32 - 1]
    coefs = [0, 1, 2, 100, 32768, 65535, 65536, 65537, 2**32 - 1]
    cnt = 0
    for env in vals:
        for x in vals:
            for a in coefs:
                for rl in coefs:
                    v = env_step(env, x, a, rl)
                    assert min(env, x) <= v <= max(env, x)
                    h.num(v, 4)
                    cnt += 1
    out["envelope"] = (h.h, cnt)
    h = Fnv()
    r = Rng(9)
    cases = 0
    for _ in range(6000):
        bins = r.next() % 40
        count = r.next() % 6
        pw = [(r.next() >> 33) & 0xFFFFFFFF if r.next() % 4 else 0xFFFFFFFF for _ in range(bins)]
        if r.next() % 3:
            edges = sorted(r.next() % (bins + 1) for _ in range(count + 1))
        else:
            edges = [r.next() % (bins + 3) for _ in range(count + 1)]
        ok, en = band(pw, edges, count, bins)
        h.byte(1 if ok else 0)
        for v in pw:
            h.num(v, 4)
        for v in edges:
            h.num(v, 4)
        for v in en:
            h.num(v, 8)
        cases += 1
    out["band"] = (h.h, cases)
    return out


def arr(name, typ, vals, per=10):
    lines = ["const %s %s[%d] = {" % (typ, name, len(vals))]
    for i in range(0, len(vals), per):
        lines.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    lines.append("};")
    return lines


def tables_block():
    L = [TBEGIN]
    L += arr("kCos", "std::int16_t", C, 12)
    L += arr("kHann", "std::uint16_t", H, 12)
    L.append(TEND)
    return "\n".join(L) + "\n"


def render():
    d = digests()
    L = [BEGIN]
    L += arr("kRefCos", "std::int16_t", C, 12)
    L += arr("kRefHann", "std::uint16_t", H, 12)
    L.append("const std::uint64_t kRefFixtureDigest = 0x%016Xull;" % d["fixtures"][0])
    L.append("const std::size_t kRefFixtureCount = %d;" % d["fixtures"][1])
    L.append("const std::uint64_t kRefIsqrtDigest = 0x%016Xull;" % d["isqrt"])
    L.append("const std::uint64_t kRefEnvelopeDigest = 0x%016Xull;" % d["envelope"][0])
    L.append("const std::size_t kRefEnvelopeCount = %d;" % d["envelope"][1])
    L.append("const std::uint64_t kRefBandDigest = 0x%016Xull;" % d["band"][0])
    L.append("const std::size_t kRefBandCount = %d;" % d["band"][1])
    L.append("struct RefSpectrum64 {")
    L.append("  const char* name;")
    L.append("  std::uint32_t peak;")
    L.append("  std::uint64_t sum_squares;")
    L.append("  std::uint32_t power[32];")
    L.append("};")
    L.append("const RefSpectrum64 kRefSpectra64[] = {")
    for name, s in fixtures(64):
        pk, sq = level(s)
        p = power(s)
        L.append('    {"%s", %du, %dull, {%s}},' % (name, pk, sq, ", ".join("%du" % v for v in p)))
    L.append("};")
    L.append(END)
    return "\n".join(L) + "\n"


def check(block, begin, end, path):
    text = open(path).read()
    a, b = text.index(begin), text.index(end) + len(end) + 1
    if text[a:b] != block:
        print("generated block differs; regenerate it", file=sys.stderr)
        return 1
    return 0


def main(argv):
    if len(argv) == 2 and argv[1] == "--emit-tables":
        sys.stdout.write(tables_block())
        return 0
    if len(argv) == 3 and argv[1] == "--check-tables":
        return check(tables_block(), TBEGIN, TEND, argv[2])
    if len(argv) == 3 and argv[1] == "--check":
        return check(render(), BEGIN, END, argv[2])
    sys.stdout.write(render())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
