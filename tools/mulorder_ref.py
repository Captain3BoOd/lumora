#!/usr/bin/env python3
"""Independent reference for core/mulorder: digests that tests/mulorder/mulorder_tests.cpp recomputes from the real functions. Standard library only.

Three independent formulations, compared with each other (and with the definitions) by selfcheck():
  brute   the definitions: order(a, m) by multiplying until the product is 1; lambda(m) as the least e > 0 with x^e = 1 for every unit x (e raised from 1; the
          units are tested one after the other and the first failure stops the test of that e); primitive root by scanning g and walking its order against the
          count of units; the existence of a root as lambda(m) = phi(m) (the unit group is cyclic exactly when its exponent equals its size).
  fast    smallest-prime-factor sieve (moduli below 2^20) or division by the sieved primes below 2^16 (larger moduli), Python integers, lambda and phi from the
          prime power formulas; the order as the least DIVISOR of lambda (all divisors enumerated, ascending) whose power is 1; the root as the least g that is
          coprime with m and whose power g^(phi/q) is not 1 for every prime q of phi.
  verify  a certificate check used on the large sets: a claimed order o of a satisfies a^o = 1 and a^(o/q) != 1 for each prime q of o; a claimed lambda has a unit
          witness for each prime q of it (a unit x with x^(lambda/q) != 1, found by search) and x^lambda = 1 for sampled units.
selfcheck() compares brute with fast on every (m, a) of a grid, the root rule against the shape {1, 2, 4, p^k, 2 p^k}, the closed forms of the header examples,
and applies verify to every large set.

Every group folds with FNV-1a over 64-bit words (offset basis 0xCBF29CE484222325, prime 0x100000001B3; every word is a u32 or a small integer). A result is
folded as the status (0 Ok, 3 NotCoprime, 4 NoPrimitiveRoot) and then the value (0 when not Ok).
Groups. "cases" counts the moduli m (or the pairs for ord_*):
  lam_0 .. lam_7    carmichael(m) for m in [1 + r * 2^17, (r + 1) * 2^17], r = 0 .. 7: fold m, lambda.
  root_0 .. root_7  primitive_root(m) for the same ranges: fold m, status, value.
  ord_0 .. ord_3    mult_order(a, m) for m in 1..500, 501..1000, 1001..1500, 1501..2000, a = 0 .. m - 1 and then a = m, m + 1, 2 m - 1 (when below 2^32), 2^32 - 1
                    (each kept once): fold m, a, status, value.
  ord64_0 .. ord64_3  mult_order(a, m) for m = 1 + 1024 r .. 1024 (r + 1), r = 0 .. 3 (so EVERY m = 1 .. 4096) and the 64 stated residues a64(m) (see a64): fold m, a,
                    status, value; the order from the fast formulation (the divisor of lambda); certified in selfcheck by verify_order, and compared with the brute
                    definition on every a64 value for m <= 300.
  lamdef            carmichael(m) for m = 1 .. 3000 against the definition: fold m, lambda.
  rootdef           primitive_root(m) for m = 1 .. 4096 against the definition: fold m, status, value.
  sq_0 .. sq_3      the squares p^2 of the primes p <= 65521 (the primes in ascending order, split in four chunks by index modulo 4), cube_0: all cubes p^3 < 2^32.
  fam_a             2 p^2 and 4 p^2 for every prime p <= 200 and every 4th prime up to 46340 (2 p^2) / 32768 (4 p^2).
  fam_b             all p^k and 2 p^k and 4 p^k (p odd; for p = 2 all 2^k) below 2^32 for p in 2, 3, 5, 7, 11, 13 and k >= 1.
  top               the largest 30 primes below 2^32, the largest 30 primes below 2^31 with 2 p, the largest 30 primes below 2^30 with 4 p, the 15 largest safe primes
                    below 2^32, 998244353, 469762049, 167772161, 1000000007, 1000000009, 2^31 - 1, 65537, 65521 * 65519, and the 64 moduli 2^32 - 64 .. 2^32 - 1.
  carm              the Carmichael numbers 561, 1105, 1729, 2465, 2821, 6601, 8911, 41041, 825265 and the larger ones listed in CARM (all checked by Korselt's
                    criterion by selfcheck), and the largest Chernick number (6k + 1)(12k + 1)(18k + 1) below 2^32.
  umax              2^32 - 1 and all 32 of its divisors.
  hc                the primorial 223092870 times 1 .. 19 (below 2^32) and the eight numbers below 2^32 with the most divisors (1920 down to 1728) listed by HC (found by search over products of the first ten primes with nonincreasing exponents).
  rnd               200 moduli from the LCG (m = draw | 1 for half, draw for the others, then m % 2^32; a zero is replaced by 1) with 4 residues each.
For the big sets (sq_*, cube_0, fam_*, top, carm, umax, hc, rnd) each modulus m folds: m, lambda, primitive_root status and value, then for each a in
A(m) = [1, 2, 3, m - 1, 0, m, m + 1, 2^32 - 1] (values above 2^32 - 1 dropped, a value listed twice kept once): a, status, value.
The LCG is x = x * 6364136223846793005 + 1442695040888963407 (mod 2^64), draw = x >> 32.
Usage: mulorder_ref.py (prints the generated block), mulorder_ref.py --check tests/mulorder/mulorder_tests.cpp [NAME...] (with names: only those groups),
       mulorder_ref.py --only NAME... (prints those groups only), mulorder_ref.py --selfcheck.
"""
import sys
from math import gcd

M64 = (1 << 64) - 1
BEGIN = "// BEGIN GENERATED REFERENCE (tools/mulorder_ref.py)"
END = "// END GENERATED REFERENCE"
FNV_PRIME = 0x100000001B3
U32 = (1 << 32) - 1
LIM = 1 << 20


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


# ---- sieve ---------------------------------------------------------------------------------------------------------------------------------------

_spf = None
_primes = None


def init():
    global _spf, _primes
    if _spf is not None:
        return
    spf = list(range(LIM + 1))
    i = 2
    while i * i <= LIM:
        if spf[i] == i:
            for j in range(i * i, LIM + 1, i):
                if spf[j] == j:
                    spf[j] = i
        i += 1
    _spf = spf
    _primes = [p for p in range(2, 1 << 16) if spf[p] == p]


def is_prime(n):
    if n < 2:
        return False
    if n <= LIM:
        return _spf[n] == n
    for p in _primes:
        if p * p > n:
            return True
        if n % p == 0:
            return False
    return True


def fac(m):
    """Prime factorisation of m >= 1 as [(p, e)] ascending."""
    init()
    out = []
    if m <= LIM:
        while m > 1:
            p = _spf[m]
            e = 0
            while m % p == 0:
                m //= p
                e += 1
            out.append((p, e))
        return out
    for p in _primes:
        if p * p > m:
            break
        if m % p == 0:
            e = 0
            while m % p == 0:
                m //= p
                e += 1
            out.append((p, e))
    if m > 1:
        out.append((m, 1))
    return out


def lcm(a, b):
    return a // gcd(a, b) * b


def lam_phi(m):
    lam, phi = 1, 1
    for p, e in fac(m):
        if p == 2:
            lp = 1 if e == 1 else (2 if e == 2 else 1 << (e - 2))
            ph = 1 << (e - 1)
        else:
            lp = p ** (e - 1) * (p - 1)
            ph = lp
        lam = lcm(lam, lp)
        phi *= ph
    return lam, phi


def divisors(n):
    ds = [1]
    for p, e in fac(n):
        ds = [d * p ** i for d in ds for i in range(e + 1)]
    return sorted(ds)


def order_fast(a, m):
    """None when gcd(a, m) != 1, else the order: the least divisor of lambda whose power of a is 1 (m = 1: 1)."""
    if m == 1:
        return 1
    a %= m
    if gcd(a, m) != 1:
        return None
    lam, _ = lam_phi(m)
    for d in divisors(lam):
        if pow(a, d, m) == 1:
            return d
    raise AssertionError("no order")


def root_fast(m):
    """None when there is no primitive root, else the least one (m = 1: 0)."""
    if m == 1:
        return 0
    lam, phi = lam_phi(m)
    if lam != phi:
        return None
    qs = [p for p, _ in fac(phi)]
    for g in range(1, m):
        if gcd(g, m) == 1 and all(pow(g, phi // q, m) != 1 for q in qs):
            return g
    raise AssertionError("cyclic but no generator")


# ---- brute definitions ---------------------------------------------------------------------------------------------------------------------------

def order_brute(a, m):
    if m == 1:
        return 1
    a %= m
    if gcd(a, m) != 1:
        return None
    x, e = a, 1
    while x != 1:
        x = x * a % m
        e += 1
    return e


def lam_brute(m):
    if m == 1:
        return 1
    units = [x for x in range(1, m) if gcd(x, m) == 1]
    e = 1
    while True:
        ok = True
        for x in units:
            if pow(x, e, m) != 1:
                ok = False
                break
        if ok:
            return e
        e += 1


def root_brute(m):
    if m == 1:
        return 0
    phi = sum(1 for x in range(1, m) if gcd(x, m) == 1)
    if lam_brute(m) != phi:
        return None
    for g in range(1, m):
        if gcd(g, m) == 1 and order_brute(g, m) == phi:
            return g
    raise AssertionError("cyclic but no generator")


# ---- groups --------------------------------------------------------------------------------------------------------------------------------------

def stat(v):
    return (3 if False else 0, v)


def ord_words(F, a, m):
    o = order_fast(a, m)
    F.one(a)
    if o is None:
        F.one(3)
        F.one(0)
    else:
        F.one(0)
        F.one(o)


def g_lam(r):
    init()
    F = Fold()
    for m in range(1 + r * (1 << 17), (r + 1) * (1 << 17) + 1):
        F.one(m)
        F.one(lam_phi(m)[0])
        F.cases += 1
    return F


def g_root(r):
    init()
    F = Fold()
    for m in range(1 + r * (1 << 17), (r + 1) * (1 << 17) + 1):
        F.one(m)
        g = root_fast(m)
        F.one(4 if g is None else 0)
        F.one(0 if g is None else g)
        F.cases += 1
    return F


def a_list(m):
    out = []
    for v in (m - 0, m + 1, 2 * m - 1, U32):
        if v <= U32 and v not in out:
            out.append(v)
    return list(range(m)) + out


def g_ord(lo, hi):
    def f():
        init()
        F = Fold()
        for m in range(lo, hi + 1):
            for a in a_list(m):
                F.one(m)
                o = order_brute(a, m)
                F.one(a)
                F.one(3 if o is None else 0)
                F.one(0 if o is None else o)
                F.cases += 1
        return F
    return f


def a64(m):
    """64 distinct stated residues for the modulus m (u32 values): 0 .. 7, m - 2 .. m + 2, 2 m - 1, 2 m, 2 m + 1, 2^32 - 1, 2^32 - 2, 2^31 - 1, 2^31, 2^31 + 1
    (those inside u32, a value listed twice kept once), then LCG draws (seed 0x6D6F ^ m; the draw itself for even i, draw % m for odd i, i = 0, 1, ..) not yet
    listed, until there are 64."""
    out = []
    for v in (0, 1, 2, 3, 4, 5, 6, 7, m - 2, m - 1, m, m + 1, m + 2, 2 * m - 1, 2 * m, 2 * m + 1, U32, U32 - 1, (1 << 31) - 1, 1 << 31, (1 << 31) + 1):
        if 0 <= v <= U32 and v not in out:
            out.append(v)
    g = Lcg(0x6D6F ^ m)
    i = 0
    while len(out) < 64:
        d = g.next()
        v = d if i % 2 == 0 else d % m
        i += 1
        if v not in out:
            out.append(v)
    return out[:64]


def g_ord64(lo, hi):
    def f():
        init()
        F = Fold()
        for m in range(lo, hi + 1):
            for a in a64(m):
                F.one(m)
                ord_words(F, a, m)
                F.cases += 1
        return F
    return f


def g_lamdef():
    F = Fold()
    for m in range(1, 3001):
        F.one(m)
        F.one(lam_brute(m))
        F.cases += 1
    return F


def g_rootdef():
    F = Fold()
    for m in range(1, 4097):
        g = root_brute(m)
        F.one(m)
        F.one(4 if g is None else 0)
        F.one(0 if g is None else g)
        F.cases += 1
    return F


def a_big(m):
    out = []
    for v in (1, 2, 3, m - 1, 0, m, m + 1, U32):
        if 0 <= v <= U32 and v not in out:
            out.append(v)
    return out


def big_fold(ms):
    init()
    F = Fold()
    for m in ms:
        F.one(m)
        F.one(lam_phi(m)[0])
        g = root_fast(m)
        F.one(4 if g is None else 0)
        F.one(0 if g is None else g)
        for a in a_big(m):
            ord_words(F, a, m)
        F.cases += 1
    return F


def primes_upto(n):
    init()
    return [p for p in _primes if p <= n]


def prev_primes(limit, count):
    out = []
    n = limit
    while len(out) < count:
        if is_prime(n):
            out.append(n)
        n -= 1
    return out


def set_sq(c):
    ps = primes_upto(65521)
    return [p * p for i, p in enumerate(ps) if i % 4 == c and p * p <= U32]


def set_cube():
    return [p ** 3 for p in primes_upto(1626) if p ** 3 <= U32]


def set_fam_a():
    ps = primes_upto(46340)
    out = []
    for i, p in enumerate(ps):
        if p <= 200 or i % 4 == 0:
            if 2 * p * p <= U32:
                out.append(2 * p * p)
            if 4 * p * p <= U32:
                out.append(4 * p * p)
    return out


def set_fam_b():
    out = []
    for p in (2, 3, 5, 7, 11, 13):
        k = 1
        while p ** k <= U32:
            for c in (1, 2, 4):
                v = c * p ** k
                if v <= U32 and v not in out:
                    out.append(v)
            k += 1
    return sorted(out)


def safe_primes_below(limit, count):
    out = []
    n = limit
    while len(out) < count:
        if n % 2 == 1 and is_prime(n) and is_prime((n - 1) // 2):
            out.append(n)
        n -= 1
    return out


def set_top():
    init()
    out = list(prev_primes(U32, 30))
    out += [2 * p for p in prev_primes((1 << 31) - 1, 30)]
    out += [4 * p for p in prev_primes((1 << 30) - 1, 30)]
    out += safe_primes_below(U32, 15)
    out += [998244353, 469762049, 167772161, 1000000007, 1000000009, (1 << 31) - 1, 65537, 65521 * 65519]
    out += list(range((1 << 32) - 64, 1 << 32))
    seen, res = set(), []
    for v in out:
        if v not in seen:
            seen.add(v)
            res.append(v)
    return res


CARM = [1024651, 1033669, 1050985, 1152271, 5481451, 10024561, 118901521, 172947529, 216821881, 228842209]


def chernick_top():
    best = None
    for k in range(1, 500):
        a, b, c = 6 * k + 1, 12 * k + 1, 18 * k + 1
        if a * b * c <= U32 and is_prime(a) and is_prime(b) and is_prime(c):
            best = a * b * c
    return best


def set_carm():
    return [561, 1105, 1729, 2465, 2821, 6601, 8911, 41041, 825265] + CARM + [chernick_top()]


def set_umax():
    return divisors(U32)


HC = [4190266080, 3491888400, 3859455600, 3675672000, 2793510720, 4073869800, 3724680960, 3430627200]


def set_hc():
    out = []
    for k in range(1, 20):
        v = 223092870 * k
        if v <= U32:
            out.append(v)
    return out + [v for v in HC if v <= U32]


def set_rnd():
    g = Lcg(20261128)
    out = []
    for i in range(200):
        d = g.next()
        m = (d | 1) if i % 2 == 0 else d
        out.append(m if m else 1)
    return out


GROUPS = []
for _r in range(8):
    GROUPS.append(("lam_%d" % _r, (lambda r: lambda: g_lam(r))(_r)))
for _r in range(8):
    GROUPS.append(("root_%d" % _r, (lambda r: lambda: g_root(r))(_r)))
for _i, (_lo, _hi) in enumerate([(1, 500), (501, 1000), (1001, 1500), (1501, 2000)]):
    GROUPS.append(("ord_%d" % _i, g_ord(_lo, _hi)))
for _i in range(4):
    GROUPS.append(("ord64_%d" % _i, g_ord64(1 + _i * 1024, (_i + 1) * 1024)))
GROUPS.append(("lamdef", g_lamdef))
GROUPS.append(("rootdef", g_rootdef))
for _c in range(4):
    GROUPS.append(("sq_%d" % _c, (lambda c: lambda: big_fold(set_sq(c)))(_c)))
GROUPS.append(("cube_0", lambda: big_fold(set_cube())))
GROUPS.append(("fam_a", lambda: big_fold(set_fam_a())))
GROUPS.append(("fam_b", lambda: big_fold(set_fam_b())))
GROUPS.append(("top", lambda: big_fold(set_top())))
GROUPS.append(("carm", lambda: big_fold(set_carm())))
GROUPS.append(("umax", lambda: big_fold(set_umax())))
GROUPS.append(("hc", lambda: big_fold(set_hc())))
GROUPS.append(("rnd", lambda: big_fold(set_rnd())))
BIG_SETS = {"sq_0": lambda: set_sq(0), "sq_1": lambda: set_sq(1), "sq_2": lambda: set_sq(2), "sq_3": lambda: set_sq(3), "cube_0": set_cube,
            "fam_a": set_fam_a, "fam_b": set_fam_b, "top": set_top, "carm": set_carm, "umax": set_umax, "hc": set_hc, "rnd": set_rnd}


# ---- verification --------------------------------------------------------------------------------------------------------------------------------

def verify_order(a, m, o):
    if m == 1:
        return o == 1
    a %= m
    if pow(a, o, m) != 1:
        return False
    return all(pow(a, o // q, m) != 1 for q, _ in fac(o))


def verify_lambda(m, lam, rng):
    if m == 1:
        return lam == 1
    units = []
    while len(units) < 20:
        x = rng.next() % m
        if x and gcd(x, m) == 1:
            units.append(x)
    if not all(pow(x, lam, m) == 1 for x in units):
        return False
    for q, _ in fac(lam):
        found = False
        for t in range(1, 100000):
            x = (rng.next() % (m - 1)) + 1
            if gcd(x, m) == 1 and pow(x, lam // q, m) != 1:
                found = True
                break
        if not found:
            # no witness among the random draws: take the structured search, the units 2, 3, 5, ... and -1
            for x in list(range(2, 2000)) + [m - 1]:
                if gcd(x, m) == 1 and pow(x, lam // q, m) != 1:
                    found = True
                    break
        if not found:
            return False
    return True


def verify_root(m, g):
    if m == 1:
        return g == 0
    lam, phi = lam_phi(m)
    if lam != phi or not verify_order(g, m, phi):
        return False
    return all(not (gcd(h, m) == 1 and verify_order(h, m, phi)) for h in range(1, g))


def selfcheck():
    init()
    # brute against fast
    for m in range(1, 301):
        for a in a_list(m):
            assert order_brute(a, m) == order_fast(a, m), (a, m)
        assert lam_brute(m) == lam_phi(m)[0], m
        assert root_brute(m) == root_fast(m), m
    for m in range(301, 801):
        assert lam_brute(m) == lam_phi(m)[0], m
        assert root_brute(m) == root_fast(m), m
    for m in (1000, 1001, 1024, 1155, 1500, 1999, 2000):
        for a in a_list(m)[::17]:
            assert order_brute(a, m) == order_fast(a, m), (a, m)
    # the root exists exactly for 1, 2, 4, p^k, 2 p^k (p odd)
    for m in range(1, 1 << 16):
        fs = fac(m)
        shape = m in (1, 2, 4) or (len(fs) == 1 and fs[0][0] != 2) or (len(fs) == 2 and fs[0] == (2, 1))
        lam, phi = lam_phi(m)
        assert (lam == phi) == shape, m
    # the header examples
    ex_lam = {1: 1, 2: 1, 4: 2, 8: 2, 16: 4, 15: 4, 24: 2, 100: 20, 561: 80, 1000: 100}
    for m, v in ex_lam.items():
        assert lam_phi(m)[0] == v and lam_brute(m) == v
    ex_ord = [(2, 7, 3), (3, 7, 6), (2, 9, 6), (10, 7, 6), (2, 1, 1), (5, 13, 4), (7, 1000, 20), (1, 5, 1), (4, 15, 2), (2, 101, 100), (3, 100, 20)]
    for a, m, v in ex_ord:
        assert order_fast(a, m) == v and order_brute(a, m) == v
    ex_root = {1: 0, 2: 1, 3: 2, 4: 3, 5: 2, 6: 5, 7: 3, 9: 2, 10: 3, 14: 3, 17: 3, 18: 5, 25: 2, 27: 2, 101: 2, 998: 7}
    for m, v in ex_root.items():
        assert root_fast(m) == v and root_brute(m) == v
    for m in (8, 15, 100):
        assert root_fast(m) is None and root_brute(m) is None
    # 998244353 = 119 * 2^23 + 1 is prime; 3 is its least primitive root, shown from the definition
    p = 998244353
    assert is_prime(p) and fac(p - 1) == [(2, 23), (7, 1), (17, 1)]
    assert root_fast(p) == 3 and verify_root(p, 3)
    assert pow(1, (p - 1) // 2, p) == 1 and pow(2, (p - 1) // 2, p) == 1
    assert all(pow(3, (p - 1) // q, p) != 1 for q in (2, 7, 17)) and pow(3, p - 1, p) == 1
    # the stated residues: brute against fast for m <= 300; certificates for every m <= 4096
    for m in range(1, 301):
        assert len(set(a64(m))) == 64
        for a in a64(m):
            assert order_brute(a, m) == order_fast(a, m), (a, m)
    for m in range(1, 4097):
        vs = a64(m)
        assert len(vs) == 64 and len(set(vs)) == 64 and all(0 <= v <= U32 for v in vs), m
        for a in vs:
            o = order_fast(a, m)
            if o is None:
                assert gcd(a % m, m) != 1 and m > 1
            else:
                assert verify_order(a, m, o), (a, m)
    # Korselt for the Carmichael numbers
    for n in set_carm():
        fs = fac(n)
        assert len(fs) >= 3 and all(e == 1 for _, e in fs) and all((n - 1) % (q - 1) == 0 for q, _ in fs), n
    assert U32 == 3 * 5 * 17 * 257 * 65537
    # certificates on every large set
    rng = Lcg(99)
    for name, fn in BIG_SETS.items():
        ms = fn()
        for m in ms:
            lam, _ = lam_phi(m)
            assert verify_lambda(m, lam, rng), (name, m)
            g = root_fast(m)
            if g is None:
                assert lam != lam_phi(m)[1]
            else:
                assert verify_root(m, g), (name, m, g)
            for a in a_big(m):
                o = order_fast(a, m)
                if o is None:
                    assert gcd(a % m, m) != 1 and m > 1
                else:
                    assert verify_order(a, m, o), (name, m, a)
    # the divisor-richest numbers in HC are among the top by divisor count below 2^32 (a spot check of the listed values by count)
    counts = [len(divisors(v)) for v in HC if v <= U32]
    assert counts == [1920, 1920, 1800, 1792, 1792, 1728, 1728, 1728], counts


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
