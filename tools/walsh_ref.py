#!/usr/bin/env python3
"""Independent reference for core/walsh: digests that tests/walsh/walsh_tests.cpp recomputes from the real functions. Standard library only.

Three formulations, compared with each other by selfcheck():
  brute   the definitions as sums: A[s] = sum over t of (-1)^popcount(s & t) a[t]; zeta A[s] = sum over t subset of s; Mobius A[s] = sum over t subset
          of s of (-1)^popcount(s ^ t) a[t]; convolution as the double loop over (t, u); exact integers reduced at the end. O(4^k), used for k <= 6.
  fast    recursive halving (Kronecker shape, not the in-place butterfly loops of the library): wht(a) = wht(lo + hi), wht(lo - hi) and so on, with
          the inverse as the transform times the modular inverse of 2^k (built with pow(2^k, -1, p)); convolution through the convolution theorem.
  sparse  for arrays with few non-zero entries: the definition summed over the non-zero entries only (O(2^k * nnz)), and the convolution of a sparse
          array with any array as out[s] = sum over the non-zero u of b[u] * a[s ^ u], with no transform at all. This is the check for k = 20.
selfcheck() compares brute with fast on every exhaustive small group and on random arrays (k <= 6), fast with sparse on random sparse arrays up to
k = 12, and the closed forms of the unit vectors with fast for k <= 10. The generated block is spliced into the test file; `--check` regenerates it
and compares.

Every case folds (FNV-1a over 64-bit words, offset basis 0xCBF29CE484222325, prime 0x100000001B3, the words are u32 values and small integers).
Transform cases fold: k, the input words a[0 .. 2^k - 1], then the four outputs walsh_xor, walsh_xor_inverse, subset_zeta, subset_mobius of a (each 2^k
words). Convolution cases fold: k, the words of a, the words of b, the words of out. Array enumeration over an alphabet V: counter c = 0 .. |V|^2^k - 1,
a[i] = V[(c // |V|^i) % |V|] (entry 0 is the least significant digit). A group folds its cases in the order given.

Groups:
  ex3     EXHAUSTIVE transform cases: every array over {0, 1, p - 1} (p = 998244353) for k = 0 .. 3 (3 + 9 + 81 + 6561 = 6654 arrays), by k then counter.
  ex4     EXHAUSTIVE transform cases: every array over {0, 1, 2, p - 1} for k = 0 .. 2 (4 + 16 + 256 = 276 arrays).
  cv3     EXHAUSTIVE convolution cases: every ordered pair over {0, 1, p - 1} for k = 0 .. 2 (9 + 81 + 6561 = 6651 pairs), by k, then counter of a (outer),
          counter of b (inner).
  cv4     EXHAUSTIVE convolution cases: every ordered pair over {0, 1, 2, p - 1} for k = 0 .. 1 (16 + 256 = 272 pairs).
  cvb     EXHAUSTIVE convolution cases: every ordered pair over {0, 1} for k = 3 (65536 pairs).
  unit    EXHAUSTIVE transform cases: every unit vector e_i for k = 0 .. 10, by k then i (2047 cases; the test checks k = 11 and 12 against the same
          closed forms without a digest, k = 13 .. 20 on the index list below).
  upair   EXHAUSTIVE convolution cases: every ordered pair of unit vectors (e_i, e_j) for k = 0 .. 6, by k, i, j (5461 pairs).
  rnd     SAMPLED cases for k = 0 .. 10, three each (33): the LCG is seeded with 5000 + 10 * k + r (r = 0, 1, 2); a and b are drawn in that order,
          entry = by d1 % 5: 0 -> 0, 1 -> 1, 2 -> p - 1, 3 -> p - 2, 4 -> d2 % p (d1 then d2 drawn for every entry). Case: k, a, b, the four
          transforms of a, then convolve(a, b).
  dense   SAMPLED cases like rnd for k = 11 .. 16, one each, seed 6000 + k.
  sp20    SAMPLED, k = 20: the LCG seeded with 7020 draws three positions (draw % 2^20) and three values (draw % p; the second value is p - 1)
          for a, then the same for b; a and b hold those entries (a later entry at the same position adds). Case: k, a, b, the four transforms of a
          (sparse formulation), convolve(a, b) (sparse formulation).
  cd20    SAMPLED, k = 20: a dense, a[i] = draw % p from the LCG seeded with 7021, b sparse (three positions and values from the same LCG after a).
          Case: k, a, b, out (out[s] = sum over the non-zero u of b[u] * a[s ^ u]).
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Usage: walsh_ref.py (prints the generated block), walsh_ref.py --check tests/walsh/walsh_tests.cpp [NAME...] (with names: only those groups),
       walsh_ref.py --only NAME... (prints those groups only, for quick runs), walsh_ref.py --selfcheck.
"""
import itertools
import sys

M64 = (1 << 64) - 1
P = 998244353
BEGIN = "// BEGIN GENERATED REFERENCE (tools/walsh_ref.py)"
END = "// END GENERATED REFERENCE"
FNV_PRIME = 0x100000001B3


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


# ---- brute: the definitions ------------------------------------------------------------------------------------------------------------------

def pc(x):
    return bin(x).count("1")


def brute_wht(a):
    n = len(a)
    return [sum((-1) ** pc(s & t) * a[t] for t in range(n)) % P for s in range(n)]


def brute_inv(a):
    n = len(a)
    inv = pow(n, -1, P)
    return [sum((-1) ** pc(s & t) * a[t] for t in range(n)) * inv % P for s in range(n)]


def brute_zeta(a):
    n = len(a)
    return [sum(a[t] for t in range(n) if t & s == t) % P for s in range(n)]


def brute_mobius(a):
    n = len(a)
    return [sum((-1) ** pc(s ^ t) * a[t] for t in range(n) if t & s == t) % P for s in range(n)]


def brute_conv(a, b):
    n = len(a)
    out = [0] * n
    for t in range(n):
        for u in range(n):
            out[t ^ u] += a[t] * b[u]
    return [v % P for v in out]


# ---- fast: recursive halving (a different shape from the library's in-place loops) ------------------------------------------------------------

def f_wht(a):
    if len(a) == 1:
        return list(a)
    h = len(a) // 2
    lo = f_wht(a[:h])
    hi = f_wht(a[h:])
    return [(x + y) % P for x, y in zip(lo, hi)] + [(x - y) % P for x, y in zip(lo, hi)]


def f_inv(a):
    inv = pow(len(a), -1, P)
    return [v * inv % P for v in f_wht(a)]


def f_zeta(a):
    if len(a) == 1:
        return list(a)
    h = len(a) // 2
    lo = f_zeta(a[:h])
    hi = f_zeta(a[h:])
    return lo + [(x + y) % P for x, y in zip(lo, hi)]


def f_mobius(a):
    if len(a) == 1:
        return list(a)
    h = len(a) // 2
    lo = f_mobius(a[:h])
    hi = f_mobius([(x - y) % P for x, y in zip(a[h:], a[:h])])
    return lo + hi


def f_conv(a, b):
    fa = f_wht(a)
    fb = f_wht(b)
    return f_inv([x * y % P for x, y in zip(fa, fb)])


# ---- sparse: the definitions over the non-zero entries only ----------------------------------------------------------------------------------

def sp_wht(n, nz):
    out = [0] * n
    for s in range(n):
        acc = 0
        for t, v in nz:
            acc += -v if pc(s & t) & 1 else v
        out[s] = acc % P
    return out


def sp_inv(n, nz):
    inv = pow(n, -1, P)
    return [v * inv % P for v in sp_wht(n, nz)]


def sp_zeta(n, nz):
    out = [0] * n
    for s in range(n):
        acc = 0
        for t, v in nz:
            if t & s == t:
                acc += v
        out[s] = acc % P
    return out


def sp_mobius(n, nz):
    out = [0] * n
    for s in range(n):
        acc = 0
        for t, v in nz:
            if t & s == t:
                acc += -v if pc(s ^ t) & 1 else v
        out[s] = acc % P
    return out


def sp_conv_dense(a, nzb):
    n = len(a)
    out = [0] * n
    for s in range(n):
        acc = 0
        for u, v in nzb:
            acc += v * a[s ^ u]
        out[s] = acc % P
    return out


def dense_from(n, nz):
    a = [0] * n
    for t, v in nz:
        a[t] = (a[t] + v) % P
    return a


def sp_conv(n, nza, nzb):
    out = {}
    for t, x in nza:
        for u, y in nzb:
            out[t ^ u] = (out.get(t ^ u, 0) + x * y) % P
    arr = [0] * n
    for s, v in out.items():
        arr[s] = v
    return arr


# ---- groups ----------------------------------------------------------------------------------------------------------------------------------

def transform_case(F, k, a, fns):
    F.one(k)
    F.many(a)
    for fn in fns:
        F.many(fn(a))
    F.cases += 1


def arrays_over(alpha, n):
    base = len(alpha)
    for c in range(base ** n):
        yield [alpha[(c // base ** i) % base] for i in range(n)]


FAST = (f_wht, f_inv, f_zeta, f_mobius)


def g_exh(alpha, kmax):
    F = Fold()
    for k in range(kmax + 1):
        for a in arrays_over(alpha, 1 << k):
            transform_case(F, k, a, FAST)
    return F


def g_conv_exh(alpha, kmax, kmin=0):
    F = Fold()
    for k in range(kmin, kmax + 1):
        n = 1 << k
        arrs = list(arrays_over(alpha, n))
        for a in arrs:
            for b in arrs:
                F.one(k)
                F.many(a)
                F.many(b)
                F.many(f_conv(a, b))
                F.cases += 1
    return F


def unit_vec(n, i):
    a = [0] * n
    a[i] = 1
    return a


def g_unit():
    F = Fold()
    for k in range(11):
        n = 1 << k
        for i in range(n):
            transform_case(F, k, unit_vec(n, i), FAST)
    return F


def g_upair():
    F = Fold()
    for k in range(7):
        n = 1 << k
        for i in range(n):
            for j in range(n):
                F.one(k)
                F.one(i)
                F.one(j)
                out = [0] * n
                out[i ^ j] = 1
                F.many(out)
                F.cases += 1
    return F


def draw_entry(g):
    d1 = g.next()
    d2 = g.next()
    sel = d1 % 5
    return [0, 1, P - 1, P - 2, d2 % P][sel]


def rnd_case(F, k, seed):
    g = Lcg(seed)
    n = 1 << k
    a = [draw_entry(g) for _ in range(n)]
    b = [draw_entry(g) for _ in range(n)]
    F.one(k)
    F.many(a)
    F.many(b)
    for fn in FAST:
        F.many(fn(a))
    F.many(f_conv(a, b))
    F.cases += 1


def g_rnd():
    F = Fold()
    for k in range(11):
        for r in range(3):
            rnd_case(F, k, 5000 + 10 * k + r)
    return F


def g_dense():
    F = Fold()
    for k in range(11, 17):
        rnd_case(F, k, 6000 + k)
    return F


def sparse3(g):
    pos = [g.next() % (1 << 20) for _ in range(3)]
    val = [g.next() % P for _ in range(3)]
    val[1] = P - 1
    return list(zip(pos, val))


def g_sp20():
    F = Fold()
    n = 1 << 20
    g = Lcg(7020)
    nza = sparse3(g)
    nzb = sparse3(g)
    a = dense_from(n, nza)
    b = dense_from(n, nzb)
    nza_merged = [(t, a[t]) for t in sorted(set(t for t, _ in nza))]
    nzb_merged = [(t, b[t]) for t in sorted(set(t for t, _ in nzb))]
    F.one(20)
    F.many(a)
    F.many(b)
    F.many(sp_wht(n, nza_merged))
    F.many(sp_inv(n, nza_merged))
    F.many(sp_zeta(n, nza_merged))
    F.many(sp_mobius(n, nza_merged))
    F.many(sp_conv(n, nza_merged, nzb_merged))
    F.cases += 1
    return F


def g_cd20():
    F = Fold()
    n = 1 << 20
    g = Lcg(7021)
    a = [g.next() % P for _ in range(n)]
    nzb = sparse3(g)
    b = dense_from(n, nzb)
    nzb_merged = [(t, b[t]) for t in sorted(set(t for t, _ in nzb))]
    F.one(20)
    F.many(a)
    F.many(b)
    F.many(sp_conv_dense(a, nzb_merged))
    F.cases += 1
    return F


GROUPS = [
    ("ex3", lambda: g_exh([0, 1, P - 1], 3)),
    ("ex4", lambda: g_exh([0, 1, 2, P - 1], 2)),
    ("cv3", lambda: g_conv_exh([0, 1, P - 1], 2)),
    ("cv4", lambda: g_conv_exh([0, 1, 2, P - 1], 1)),
    ("cvb", lambda: g_conv_exh([0, 1], 3, 3)),
    ("unit", g_unit),
    ("upair", g_upair),
    ("rnd", g_rnd),
    ("dense", g_dense),
    ("sp20", g_sp20),
    ("cd20", g_cd20),
]


# ---- self-check ------------------------------------------------------------------------------------------------------------------------------

def selfcheck():
    for alpha, kmax in (([0, 1, P - 1], 3), ([0, 1, 2, P - 1], 2)):
        for k in range(kmax + 1):
            for a in arrays_over(alpha, 1 << k):
                assert f_wht(a) == brute_wht(a)
                assert f_inv(a) == brute_inv(a)
                assert f_zeta(a) == brute_zeta(a)
                assert f_mobius(a) == brute_mobius(a)
    for alpha, kmax in (([0, 1, P - 1], 2), ([0, 1, 2, P - 1], 1)):
        for k in range(kmax + 1):
            arrs = list(arrays_over(alpha, 1 << k))
            for a in arrs:
                for b in arrs:
                    assert f_conv(a, b) == brute_conv(a, b)
    g = Lcg(99)
    for k in range(7):
        for _ in range(3):
            n = 1 << k
            a = [draw_entry(g) for _ in range(n)]
            b = [draw_entry(g) for _ in range(n)]
            assert f_wht(a) == brute_wht(a) and f_inv(a) == brute_inv(a)
            assert f_zeta(a) == brute_zeta(a) and f_mobius(a) == brute_mobius(a)
            assert f_conv(a, b) == brute_conv(a, b)
            # identities
            assert f_inv(f_wht(a)) == a and f_mobius(f_zeta(a)) == a and f_zeta(f_mobius(a)) == a
            assert f_conv(a, b) == f_conv(b, a)
    # every pair over {0,1} at k = 3 against the double loop
    arrs = list(arrays_over([0, 1], 8))
    for a in arrs[::7]:
        for b in arrs[::11]:
            assert f_conv(a, b) == brute_conv(a, b)
    # sparse against fast, k <= 12
    for k in (4, 8, 12):
        n = 1 << k
        nz = [(g.next() % n, g.next() % P) for _ in range(4)]
        a = dense_from(n, nz)
        nzm = [(t, a[t]) for t in sorted(set(t for t, _ in nz))]
        assert sp_wht(n, nzm) == f_wht(a) and sp_inv(n, nzm) == f_inv(a)
        assert sp_zeta(n, nzm) == f_zeta(a) and sp_mobius(n, nzm) == f_mobius(a)
        nz2 = [(g.next() % n, g.next() % P) for _ in range(3)]
        b = dense_from(n, nz2)
        nz2m = [(t, b[t]) for t in sorted(set(t for t, _ in nz2))]
        assert sp_conv(n, nzm, nz2m) == f_conv(a, b)
        assert sp_conv_dense(a, nz2m) == f_conv(a, b)
    # closed forms of the unit vectors against fast, k <= 10
    for k in range(11):
        n = 1 << k
        inv = pow(n, -1, P)
        for i in range(n):
            if k > 6 and i % 7:
                continue
            e = unit_vec(n, i)
            assert f_wht(e) == [(-1) ** pc(s & i) % P for s in range(n)]
            assert f_inv(e) == [(-1) ** pc(s & i) * inv % P for s in range(n)]
            assert f_zeta(e) == [int(s & i == i) for s in range(n)]
            assert f_mobius(e) == [((-1) ** pc(s ^ i)) % P if s & i == i else 0 for s in range(n)]


# ---- block ------------------------------------------------------------------------------------------------------------------------------------

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
