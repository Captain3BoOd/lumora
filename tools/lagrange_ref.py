#!/usr/bin/env python3
"""Independent reference for core/lagrange: digests that tests/lagrange/lagrange_tests.cpp recomputes from the real function. Standard library only.

Three formulations, compared with each other by selfcheck():
  brute    the definition: f(x) = sum over i of y_i * prod over j != i of (r - j) / (i - j) with r = x mod p, a modular inverse (pow(d, -1, p)) per factor.
           O(n^2); used for every exhaustive small group and for n <= 256.
  bary     barycentric form with a table of inverse factorials: f(r) = P * sum over i of y_i * (-1)^(n-1-i) / (i! (n-1-i)! (r - i)) with P the product of
           (r - j) and one pow(r - i, -1, p) per node. O(n); not the recurrence of the library (no running fraction, no per-node product chain).
  horner   for the values of polynomials: y_i = f(i) by Horner's rule and the expected value f(r) by Horner's rule (or pow(r, e, p) for a monomial),
           which needs no interpolation at all. This is the check for n = 2^20.
selfcheck() compares brute with bary on every exhaustive small group and on random arrays, and brute with Horner on polynomials. The generated block is
spliced into the test file; `--check` regenerates it and compares.

Every case folds (FNV-1a over 64-bit words, offset basis 0xCBF29CE484222325, prime 0x100000001B3; the words are u32 values and small integers).
Value cases fold: n, the n words of y, x, the result f(r). Polynomial cases fold: n, the number of coefficients, the coefficients (low degree first), then
for every x of the list: x, f(r) (Horner). Monomial cases fold: n, e, then for every x: x, r^e mod p (pow). A group folds its cases in the order given.
The u32 list of x for small n, X(n): 0 .. n + 6, then p - 2, p - 1, p, p + 1, p + n - 1, p + n, p + n + 1, 2p - 1, 2p, 2p + 1, 3p, 4p, 2^32 - 1, 2^32 - 2 (values
above 2^32 - 1 are dropped, a value already listed is kept once). The list for the large n, BX(n): 0, 1, n - 1, n, n + 1, p - 1, p, p + 5, p + n - 1, p + n, 2p,
2^32 - 1, then 18 full u32 draws of the LCG seeded with n.

Groups:
  ex3     EXHAUSTIVE: every y over {0, 1, p - 1} for n = 1 .. 6 (by n, then counter c = 0 .. 3^n - 1 with y[i] = V[(c // 3^i) % 3]), each at every x of X(n).
  ex4     EXHAUSTIVE: every y over {0, 1, 2, p - 1} for n = 1 .. 4, each at every x of X(n).
  poly8   EXHAUSTIVE: every coefficient vector over {0, 1, p - 1} of length n = 1 .. 8 (polynomial of degree below n; same counter order), y_i = f(i), each at every
          x of X(n). Polynomial case; the library result must equal Horner's f(r).
  rnd     SAMPLED value cases, fixed seeds: n = 1 .. 64 and n = 100, 255, 256, 257, 1000, 4096; the LCG seeded with 4000 + n draws y entries
          (draw % 5: 0 -> 0, 1 -> 1, 2 -> p - 1, 3 -> p - 2, 4 -> next draw % p) and then the five values x = draw % n, n, n + draw % 7, draw, p + draw % (n + 3).
  bigpoly the polynomial cases at n = 2^20, 2^20 - 1, 2^19 + 1 (in this order), for the polynomials, in order: ones [1], minus one [p - 1], i [0, 1], i^2 [0, 0, 1], then
          random of degree 0, 1, 2, 3, 7, 40 (the LCG seeded with 9100 + degree, coefficient k = draw % p), each at the 30 values of BX(n).
  bigmono the monomial cases y_i = i^e at the same three n for e = n - 1, n - 2, n / 2, each at the 30 values of BX(n).
  bigrnd  two value cases at n = 2^20: y from the LCG seeded with 9900 (entry = draw % p), x = n + 3 and p + 12345 (the folds include y).
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Usage: lagrange_ref.py (prints the generated block), lagrange_ref.py --check tests/lagrange/lagrange_tests.cpp [NAME...] (with names: only those groups),
       lagrange_ref.py --only NAME... (prints those groups only), lagrange_ref.py --selfcheck.
"""
import sys

M64 = (1 << 64) - 1
P = 998244353
BEGIN = "// BEGIN GENERATED REFERENCE (tools/lagrange_ref.py)"
END = "// END GENERATED REFERENCE"
FNV_PRIME = 0x100000001B3
U32 = (1 << 32) - 1


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


def brute(y, x):
    n = len(y)
    r = x % P
    if r < n:
        return y[r]
    s = 0
    for i in range(n):
        t = y[i]
        for j in range(n):
            if j != i:
                t = t * ((r - j) % P) % P * pow((i - j) % P, -1, P) % P
        s += t
    return s % P


def bary(y, x):
    n = len(y)
    r = x % P
    if r < n:
        return y[r]
    fact = [1] * n
    for i in range(1, n):
        fact[i] = fact[i - 1] * i % P
    invf = [1] * n
    invf[n - 1] = pow(fact[n - 1], -1, P)
    for i in range(n - 1, 0, -1):
        invf[i - 1] = invf[i] * i % P
    prod = 1
    s = 0
    for i in range(n):
        d = r - i
        prod = prod * d % P
        t = y[i] * invf[i] % P * invf[n - 1 - i] % P * pow(d, -1, P) % P
        s += -t if (n - 1 - i) & 1 else t
    return s % P * prod % P


def horner(coef, x):
    acc = 0
    for c in reversed(coef):
        acc = (acc * x + c) % P
    return acc


def xs_small(n):
    out = list(range(n + 7))
    for v in (P - 2, P - 1, P, P + 1, P + n - 1, P + n, P + n + 1, 2 * P - 1, 2 * P, 2 * P + 1, 3 * P, 4 * P, U32, U32 - 1):
        if v <= U32 and v not in out:
            out.append(v)
    return out


def xs_big(n):
    out = [0, 1, n - 1, n, n + 1, P - 1, P, P + 5, P + n - 1, P + n, 2 * P, U32]
    g = Lcg(n)
    out += [g.next() for _ in range(18)]
    return out


def arrays_over(alpha, n):
    base = len(alpha)
    for c in range(base ** n):
        yield [alpha[(c // base ** i) % base] for i in range(n)]


def value_case(F, y, x, res):
    F.one(len(y))
    F.many(y)
    F.one(x)
    F.one(res)
    F.cases += 1


def g_exh(alpha, nmax):
    F = Fold()
    for n in range(1, nmax + 1):
        xl = xs_small(n)
        for y in arrays_over(alpha, n):
            for x in xl:
                value_case(F, y, x, bary(y, x) if n > 3 else brute(y, x))
    return F


def poly_case(F, n, coef, xl):
    F.one(n)
    F.one(len(coef))
    F.many(coef)
    for x in xl:
        F.one(x)
        F.one(horner(coef, x % P))
    F.cases += 1


def g_poly8():
    F = Fold()
    for n in range(1, 9):
        xl = xs_small(n)
        for coef in arrays_over([0, 1, P - 1], n):
            poly_case(F, n, coef, xl)
    return F


def draw_entry(g):
    d1 = g.next()
    d2 = g.next()
    return [0, 1, P - 1, P - 2, d2 % P][d1 % 5]


def g_rnd():
    F = Fold()
    for n in list(range(1, 65)) + [100, 255, 256, 257, 1000, 4096]:
        g = Lcg(4000 + n)
        y = [draw_entry(g) for _ in range(n)]
        xl = [g.next() % n, n, n + g.next() % 7, g.next(), P + g.next() % (n + 3)]
        xl = [v for v in xl if v <= U32]
        for x in xl:
            value_case(F, y, x, bary(y, x))
    return F


BIGN = [1 << 20, (1 << 20) - 1, (1 << 19) + 1]


def big_specs():
    specs = [[1], [P - 1], [0, 1], [0, 0, 1]]
    for deg in (0, 1, 2, 3, 7, 40):
        g = Lcg(9100 + deg)
        specs.append([g.next() % P for _ in range(deg + 1)])
    return specs


def g_bigpoly():
    F = Fold()
    for n in BIGN:
        xl = xs_big(n)
        for coef in big_specs():
            poly_case(F, n, coef, xl)
    return F


def g_bigmono():
    F = Fold()
    for n in BIGN:
        xl = xs_big(n)
        for e in (n - 1, n - 2, n // 2):
            F.one(n)
            F.one(e)
            for x in xl:
                F.one(x)
                F.one(pow(x % P, e, P))
            F.cases += 1
    return F


def g_bigrnd():
    F = Fold()
    n = 1 << 20
    g = Lcg(9900)
    y = [g.next() % P for _ in range(n)]
    for x in (n + 3, P + 12345):
        value_case(F, y, x, bary(y, x))
    return F


GROUPS = [
    ("ex3", lambda: g_exh([0, 1, P - 1], 6)),
    ("ex4", lambda: g_exh([0, 1, 2, P - 1], 4)),
    ("poly8", g_poly8),
    ("rnd", g_rnd),
    ("bigpoly", g_bigpoly),
    ("bigmono", g_bigmono),
    ("bigrnd", g_bigrnd),
]


def selfcheck():
    for alpha, nmax in (([0, 1, P - 1], 5), ([0, 1, 2, P - 1], 4)):
        for n in range(1, nmax + 1):
            for y in arrays_over(alpha, n):
                for x in xs_small(n):
                    assert brute(y, x) == bary(y, x)
    for n in range(1, 7):
        for y in arrays_over([0, 1, P - 1], n):
            for x in xs_small(n):
                assert brute(y, x) == bary(y, x) == (bary(y, x) if n > 3 else brute(y, x))
    for n in range(1, 9):
        for coef in arrays_over([0, 1, P - 1], n):
            if n >= 6 and sum(coef) % 7:
                continue
            y = [horner(coef, i) for i in range(n)]
            for x in xs_small(n):
                assert brute(y, x) == horner(coef, x % P)
    g = Lcg(77)
    for n in list(range(1, 40)) + [100, 255, 256]:
        y = [draw_entry(g) for _ in range(n)]
        for x in (g.next(), n, n + 1, P + 3, g.next() % n):
            assert brute(y, x) == bary(y, x)
    for n in (257, 1000):
        coef = [g.next() % P for _ in range(41)]
        y = [horner(coef, i) for i in range(n)]
        for x in (n, P - 1, g.next(), 2 ** 32 - 1):
            assert bary(y, x) == horner(coef, x % P)
    # monomials at a moderate n against the interpolation
    n = 300
    for e in (n - 1, n - 2, n // 2):
        y = [pow(i, e, P) for i in range(n)]
        for x in (n, P + n, g.next(), 2 ** 32 - 1):
            assert bary(y, x) == pow(x % P, e, P)


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
