#!/usr/bin/env python3
"""Independent reference for core/modarith (not run in CI).

Written from the header text with Python integers, which have no 64-bit limit: mulmod is (a * b) % m, powmod is pow(), modinv is pow(a, -1, m)
after a gcd test, with the header's check order (modulus 0 first, then the function failure). The primality reference is its own
deterministic Miller-Rabin: trial division by the primes below 100, then strong tests to the 7 bases 2, 325, 9375, 28178, 450775, 9780504,
1795265022 (a different set from the 12 prime bases 2 to 37 that the C++ code uses). Every value it evaluates is cross-checked, each time
the script runs, against a second Miller-Rabin with the 12 prime bases, and the values below 2^20 against a sieve; the windows near 2^32
also against trial division. It prints a C++ block with 64-bit digests, counts and known-answer tables that the tests recompute.

  grid    every (a, b, m) over 27 boundary values (0, 1, 2, 3, 4, 5, 7, 10, 10^9 + 7, 2^16 +- 1 and 2^16, 2^31 +- 1 and 2^31,
          2^32 +- 1 and 2^32, 4294967291, 4294967311, 2^63 +- 1 and 2^63, 2^64 - 1, 2^64 - 2, 2^64 - 3, 18446744073709551557) for mulmod and
          powmod (the exponent is the second value); every (a, m) for modinv
  small   mulmod over a, b in 0..15 and m in 0..12; powmod over base 0..15, exp 0..15, m 0..12; modinv over a in 0..15, m in 0..12
  rand    400 seeds, a fixed 64-bit LCG: four values a, b, m, e of random bit length, mulmod(a, b, m), powmod(a, e, m), modinv(a, m)
  prime_small   every n below 2^20 (the digest folds the primes)
  prime_win     every n in [2^32 - 512, 2^32 + 512), [2^63 - 512, 2^63 + 512) and [2^64 - 1024, 2^64)
  prime_table   a table of known primes and composites (Mersenne and Fermat numbers, prime squares and products near 2^32, the largest
                primes below 2^64, UINT64_MAX, the smallest strong pseudoprimes to the first n prime bases)
  prime_psp     every odd composite n below 2^22 that is a strong pseudoprime to base 2, and every Carmichael number below 2^22 (found here
                by a sieve and Korselt's criterion)

  python3 tools/modarith_ref.py            print the generated C++ block
  python3 tools/modarith_ref.py --check F  fail unless the block in F matches
The rand scenario, everything from 2^32 up, and the table are NOT exhaustive.
"""
import math
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/modarith_ref.py)"
END = "// END GENERATED REFERENCE"

M64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
OK, ZERO, NOTINV = 0, 2, 3
RAND_SEEDS = 400
SMALL_SIEVE = 1 << 20
PSP_LIMIT = 1 << 22
BOUNDARY = sorted({0, 1, 2, 3, 4, 5, 7, 10, 1000000007, (1 << 16) - 1, 1 << 16, (1 << 16) + 1, (1 << 31) - 1, 1 << 31, (1 << 31) + 1,
                   (1 << 32) - 1, 1 << 32, (1 << 32) + 1, 4294967291, 4294967311, (1 << 63) - 1, 1 << 63, (1 << 63) + 1, M64, M64 - 1,
                   M64 - 2, 18446744073709551557})
MR7 = [2, 325, 9375, 28178, 450775, 9780504, 1795265022]
MR12 = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37]
SMALL_PRIMES = [p for p in range(2, 100) if all(p % q for q in range(2, p))]


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


def mulmod(a, b, m):
    return (OK, (a * b) % m) if m else (ZERO, 0)


def powmod(a, e, m):
    return (OK, pow(a, e, m)) if m else (ZERO, 0)


def modinv(a, m):
    if m == 0:
        return ZERO, 0
    if m == 1:
        return OK, 0
    if math.gcd(a % m, m) != 1:
        return NOTINV, 0
    return OK, pow(a % m, -1, m)


def strong(n, bases):
    d, s = n - 1, 0
    while d % 2 == 0:
        d //= 2
        s += 1
    for a in bases:
        a %= n
        if a == 0:
            continue
        x = pow(a, d, n)
        if x == 1 or x == n - 1:
            continue
        for _ in range(s - 1):
            x = x * x % n
            if x == n - 1:
                break
        else:
            return False
    return True


def is_prime(n):
    if n < 2:
        return False
    for p in SMALL_PRIMES:
        if n == p:
            return True
        if n % p == 0:
            return False
    if n < 101 * 101:
        return True
    a = strong(n, MR7)
    b = strong(n, MR12)
    assert a == b, n  # two different base sets must agree on every value evaluated
    return a


def trial(n):
    if n < 2:
        return False
    f = 2
    while f * f <= n:
        if n % f == 0:
            return False
        f += 1
    return True


def fold_result(f, code, out):
    f.cases += 1
    f.ops += 1
    f.add(code)
    if code == OK:
        f.add(out)


def grid_scenario():
    f = Fold()
    for a in BOUNDARY:
        for b in BOUNDARY:
            for m in BOUNDARY:
                fold_result(f, *mulmod(a, b, m))
                fold_result(f, *powmod(a, b, m))
    for a in BOUNDARY:
        for m in BOUNDARY:
            fold_result(f, *modinv(a, m))
    return f


def small_scenario():
    f = Fold()
    for m in range(13):
        for a in range(16):
            for b in range(16):
                fold_result(f, *mulmod(a, b, m))
                fold_result(f, *powmod(a, b, m))
            fold_result(f, *modinv(a, m))
    return f


def rand_scenario():
    f = Fold()
    for seed in range(1, RAND_SEEDS + 1):
        g = Lcg(seed * 104729 + 7)
        v = []
        for _ in range(4):
            shift = g.next() % 64
            hi = g.next()
            lo = g.next()
            v.append((((hi << 32) | lo) & M64) >> shift)
        a, b, m, e = v
        fold_result(f, *mulmod(a, b, m))
        fold_result(f, *powmod(a, e, m))
        fold_result(f, *modinv(a, m))
    return f


def sieve(limit):
    s = bytearray([1]) * limit
    s[0] = s[1] = 0
    for i in range(2, int(limit ** 0.5) + 1):
        if s[i]:
            s[i * i::i] = bytearray(len(s[i * i::i]))
    return s


def prime_small_scenario():
    f = Fold()
    s = sieve(SMALL_SIEVE)
    for n in range(SMALL_SIEVE):
        p = is_prime(n)
        assert p == bool(s[n]), n
        f.cases += 1
        f.ops += 1
        if p:
            f.add(n)
    return f


def prime_win_scenario():
    f = Fold()
    for lo, hi in (((1 << 32) - 512, (1 << 32) + 512), ((1 << 63) - 512, (1 << 63) + 512), ((1 << 64) - 1024, 1 << 64)):
        for n in range(lo, hi):
            p = is_prime(n)
            if lo == (1 << 32) - 512:
                assert p == trial(n), n
            f.cases += 1
            f.ops += 1
            f.add((n << 1) | (1 if p else 0))
    return f


def largest_primes_below_2_64(count):
    out, n = [], M64
    while len(out) < count:
        if is_prime(n):
            out.append(n)
        n -= 1
    return out


def table():
    """(n, is_prime) pairs. Composites built from known factors are composite by construction; the rest rely on the double Miller-Rabin."""
    t = []
    def put(n, expect=None):
        assert 0 <= n <= M64
        p = is_prime(n)
        if expect is not None:
            assert p == expect, n
        t.append((n, p))
    put(M64, False)
    for p in (4294967291, 4294967279):
        put(p * 4294967291, False)
    put(65521 * 65521, False)
    put(65537 * 65537, False)
    put(65521 * 4294967291, False)
    put(4294967291 * 3, False)
    for k in range(2, 64):
        put((1 << k) - 1)
    put((1 << 64) - 59, True)
    for k in range(6):
        put((1 << (1 << k)) + 1)
    for n in (2047, 1373653, 25326001, 3215031751, 2152302898747, 3474749660383, 341550071728321, 3825123056546413051):
        put(n, False)  # the smallest odd composites that are strong pseudoprimes to the first 1 to 11 prime bases (OEIS A014233)
    for n in largest_primes_below_2_64(12):
        put(n, True)
    for n in (4294967291, 4294967311, (1 << 61) - 1, (1 << 63) - 25, 1000000007, 998244353, 10 ** 18 + 3):
        put(n, True)
    for n in (561, 41041, 825265, 321197185, 5394826801, 232250619601, 9746347772161, 1436697831295441, 60977817398996785):
        put(n, False)  # Carmichael numbers
    seen, out = set(), []
    for n, p in t:
        if n not in seen:
            seen.add(n)
            out.append((n, p))
    return out


def spf_sieve(limit):
    spf = list(range(limit))
    primes = [p for p in range(2, int(limit ** 0.5) + 1) if all(p % q for q in range(2, int(p ** 0.5) + 1))]
    for p in reversed(primes):
        spf[p * p::p] = [p] * len(spf[p * p::p])
    return spf


def psp_lists():
    s = sieve(PSP_LIMIT)
    spf = spf_sieve(PSP_LIMIT)
    spsp, carm = [], []
    for n in range(9, PSP_LIMIT, 2):
        if s[n]:
            continue
        d, k = n - 1, 0
        while d % 2 == 0:
            d //= 2
            k += 1
        x = pow(2, d, n)
        if x == 1 or x == n - 1 or any(pow(x, 1 << i, n) == n - 1 for i in range(1, k)):
            spsp.append(n)
        if pow(2, n - 1, n) == 1:  # a Carmichael number passes the Fermat test to base 2
            m, ok = n, True
            while m > 1:
                p = spf[m]
                m //= p
                if m % p == 0 or (n - 1) % (p - 1) != 0:
                    ok = False
                    break
            if ok:
                carm.append(n)
    return spsp, carm


def render():
    out = [BEGIN]
    out.append("const std::uint64_t kRefBoundary[] = {" + ", ".join("0x%016Xull" % v for v in BOUNDARY) + "};")
    out.append("const std::uint32_t kRefRandSeeds = %d, kRefSmallSieve = %d, kRefPspLimit = %d;" % (RAND_SEEDS, SMALL_SIEVE, PSP_LIMIT))
    for name, fn in (("grid", grid_scenario), ("small", small_scenario), ("rand", rand_scenario),
                     ("prime_small", prime_small_scenario), ("prime_win", prime_win_scenario)):
        f = fn()
        out.append("const std::uint64_t kRefDigest_%s = 0x%016Xull;" % (name, f.h))
        out.append("const std::size_t kRefCases_%s = %d, kRefOps_%s = %d;" % (name, f.cases, name, f.ops))
    spsp, carm = psp_lists()
    f = Fold()
    for n in spsp:
        f.add((1 << 40) | n)
    for n in carm:
        f.add((2 << 40) | n)
    out.append("const std::uint64_t kRefDigest_psp = 0x%016Xull;" % f.h)
    out.append("const std::size_t kRefCount_spsp2 = %d, kRefCount_carmichael = %d;" % (len(spsp), len(carm)))
    t = table()
    out.append("const KV kRefTable[] = {")
    for n, p in t:
        out.append("    {0x%016Xull, 0, 0, %d, 0}," % (n, 1 if p else 0))
    out.append("};")
    mul = [(M64, M64, M64), (M64, M64, M64 - 1), (M64 - 1, M64 - 1, M64), (1 << 63, 2, (1 << 63) + 1), ((1 << 63) + 1, (1 << 63) + 1, M64),
           (M64, 2, 1 << 63), (123456789012345678, 987654321098765432, 18446744073709551557), (4294967296, 4294967296, 4294967297),
           (4294967295, 4294967295, 4294967296), (4294967295, 4294967295, 4294967297), (5, 7, 1), (0, 5, 9), (5, 7, 0), (M64, 1, 10),
           (3, 5, 7), (1 << 32, 1 << 32, M64), (1 << 32, 1 << 32, (1 << 64) - 5)]
    out.append("const KV kRefMul[] = {")
    for a, b, m in mul:
        c, o = mulmod(a, b, m)
        out.append("    {0x%016Xull, 0x%016Xull, 0x%016Xull, %d, 0x%016Xull}," % (a, b, m, c, o))
    out.append("};")
    pw = [(0, 0, 5), (0, 0, 1), (5, 0, 1), (7, 0, 13), (0, 5, 13), (2, 10, 1000), (2, 64, M64), (3, M64, M64), (M64, M64, M64 - 1),
          (2, (1 << 61) - 2, (1 << 61) - 1), (7, 1 << 63, (1 << 63) + 1), (10, 18, 1000000007), (M64 - 2, 12345678901234567890, 18446744073709551557),
          (5, 5, 0), (123456789, 1000000007 - 1, 1000000007), (2, M64, 4294967297)]
    out.append("const KV kRefPow[] = {")
    for a, e, m in pw:
        c, o = powmod(a, e, m)
        out.append("    {0x%016Xull, 0x%016Xull, 0x%016Xull, %d, 0x%016Xull}," % (a, e, m, c, o))
    out.append("};")
    iv = [(3, 7), (0, 7), (7, 7), (14, 7), (2, 4), (5, 1), (0, 1), (5, 0), (1, M64), (M64 - 1, M64), (2, M64), (M64, M64), (1 << 63, M64),
          (3, 18446744073709551557), (12345678901234567, 18446744073709551557), (6, 9), (35, 12), (65537, (1 << 64) - 59), (10, 1 << 63),
          (M64, 1 << 63), (123456789, 1000000007)]
    out.append("const KV kRefInv[] = {")
    for a, m in iv:
        c, o = modinv(a, m)
        out.append("    {0x%016Xull, 0, 0x%016Xull, %d, 0x%016Xull}," % (a, m, c, o))
    out.append("};")
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
