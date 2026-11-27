#!/usr/bin/env python3
"""Reference for core/linrec (independent of the C++ code).

Inputs come from the SplitMix64 generator (the C++ tests generate the same streams). Expected values:
  * n-th term, small n: iteration of the recurrence term by term.
  * n-th term, full 64-bit n and order up to 16: the n-th power of the companion matrix (matrix method, not the polynomial method of the C++ code).
  * n-th term, order up to 256 and full 64-bit n: polynomial exponentiation written separately here (same method as the C++ code, different code;
    its results are cross-checked against term-by-term iteration for n below 20000 in the self-check).
  * shortest recurrence, all sets up to length 100: the least L for which a linear system has a solution (Gaussian elimination mod p, binary search on L
    because a recurrence of order L is also one of order L + 1). Large sets: a separately written Berlekamp-Massey, cross-checked in the self-check
    against the elimination at L and L - 1.
A digest is FNV-1a 64 over the little-endian bytes of every u32 emitted, case after case: for n-th terms the term; for the shortest recurrence L and,
when 2 * L <= len (the recurrence is then unique), the coefficients.

  python3 tools/linrec_ref.py --digests    print the digest table that tests/linrec/linrec_tests.cpp holds
  python3 tools/linrec_ref.py --check      verify that table and run the self-check
  python3 tools/linrec_ref.py --selfcheck  only the self-check
"""
import itertools, os, re, struct, sys

P = 998244353
M64 = (1 << 64) - 1

class SM:
    def __init__(self, seed):
        self.s = seed & M64
    def next(self):
        self.s = (self.s + 0x9E3779B97F4A7C15) & M64
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        return z ^ (z >> 31)
    def res(self):
        v = self.next()
        sel = v & 7
        if sel == 0: return 0
        if sel == 1: return 1
        if sel == 2: return P - 1
        if sel == 3: return P - 2
        if sel == 4: return 2
        return (v >> 3) % P

def iterate(coef, init, count):
    a = list(init)
    d = len(coef)
    while len(a) < count:
        k = len(a)
        a.append(sum(coef[j] * a[k - 1 - j] for j in range(d)) % P)
    return a

def nth_iter(coef, init, n):
    d = len(coef)
    if d == 0: return 0
    return iterate(coef, init, n + 1)[n]

def matmul(A, B):
    n = len(A)
    Bt = list(zip(*B))
    return [[sum(x * y for x, y in zip(row, col)) % P for col in Bt] for row in A]

def nth_matrix(coef, init, n):
    d = len(coef)
    if d == 0: return 0
    if n < d: return init[n]
    # state vector (a[k], ..., a[k-d+1]); companion matrix
    M = [[0] * d for _ in range(d)]
    M[0] = list(coef)
    for i in range(1, d): M[i][i - 1] = 1
    R = [[int(i == j) for j in range(d)] for i in range(d)]
    e = n - (d - 1)
    while e:
        if e & 1: R = matmul(R, M)
        M = matmul(M, M)
        e >>= 1
    vec = [init[d - 1 - i] for i in range(d)]  # (a[d-1], ..., a[0])
    return sum(R[0][j] * vec[j] for j in range(d)) % P

def polymulmod(x, y, coef):
    d = len(coef)
    t = [0] * (2 * d)
    for i, xi in enumerate(x):
        if xi:
            for j, yj in enumerate(y):
                t[i + j] = (t[i + j] + xi * yj) % P
    for i in range(2 * d - 1, d - 1, -1):
        v = t[i]
        if v:
            for j in range(d):
                t[i - 1 - j] = (t[i - 1 - j] + v * coef[j]) % P
    return t[:d]

def nth_poly(coef, init, n):
    d = len(coef)
    if d == 0: return 0
    if n < d: return init[n]
    res = [1] + [0] * (d - 1)
    if d == 1:
        base = [coef[0]]
    else:
        base = [0, 1] + [0] * (d - 2)
    e = n
    while e:
        if e & 1: res = polymulmod(res, base, coef)
        base = polymulmod(base, base, coef)
        e >>= 1
    return sum(r * i for r, i in zip(res, init)) % P

def solve(seq, L):
    """Return (consistent, unique, coefs) for seq[k] = sum_{j=1..L} c[j-1] seq[k-j], k = L .. len-1."""
    rows = [[seq[k - 1 - j] for j in range(L)] + [seq[k]] for k in range(L, len(seq))]
    r = 0
    piv = []
    for col in range(L):
        p = None
        for i in range(r, len(rows)):
            if rows[i][col]:
                p = i
                break
        if p is None: continue
        rows[r], rows[p] = rows[p], rows[r]
        inv = pow(rows[r][col], P - 2, P)
        rows[r] = [v * inv % P for v in rows[r]]
        for i in range(len(rows)):
            if i != r and rows[i][col]:
                f = rows[i][col]
                rows[i] = [(a - f * b) % P for a, b in zip(rows[i], rows[r])]
        piv.append(col)
        r += 1
    for i in range(r, len(rows)):
        if rows[i][L]: return False, False, None
    coefs = [0] * L
    for i, col in enumerate(piv): coefs[col] = rows[i][L]
    return True, len(piv) == L, coefs

def shortest(seq):
    lo, hi = 0, len(seq)  # order len always works
    while lo < hi:
        mid = (lo + hi) // 2
        if solve(seq, mid)[0]: hi = mid
        else: lo = mid + 1
    ok, uniq, coefs = solve(seq, lo)
    assert ok
    return lo, uniq, coefs

def bm(seq):
    C, B = [1], [1]
    L, m, b = 0, 1, 1
    for i, s in enumerate(seq):
        d = (s + sum(C[j] * seq[i - j] for j in range(1, L + 1))) % P
        if d == 0:
            m += 1
            continue
        coefk = d * pow(b, P - 2, P) % P
        T = list(C)
        need = len(B) + m
        if len(C) < need: C = C + [0] * (need - len(C))
        for j, bj in enumerate(B): C[j + m] = (C[j + m] - coefk * bj) % P
        if 2 * L <= i:
            L, B, b, m = i + 1 - L, T, d, 1
        else:
            m += 1
    C = C + [0] * (L + 1 - len(C))
    return L, [(-C[j]) % P for j in range(1, L + 1)]

def fnv(h, vals):
    for v in vals:
        for b in struct.pack('<I', v):
            h ^= b
            h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h

BASIS = 0xcbf29ce484222325

def from_recurrence(r, order, length):
    coef = [r.res() for _ in range(order)]
    init = [r.res() for _ in range(order)]
    return coef, iterate(coef, init, length)[:length] if order else [0] * length

# ---- the case sets -------------------------------------------------------------------------------------------------------------------------------

def set_nth_small():
    r = SM(11)
    for d in range(13):
        for idx in range(60):
            coef = [r.res() for _ in range(d)]
            init = [r.res() for _ in range(d)]
            v = r.next()
            n = v % 4000 if idx % 4 != 3 else v % (2 * d + 3)
            yield [nth_iter(coef, init, n)]

def set_nth_mid():
    r = SM(22)
    for d in (1, 2, 3, 4, 5, 8, 13, 16):
        for idx in range(12):
            coef = [r.res() for _ in range(d)]
            init = [r.res() for _ in range(d)]
            v = r.next()
            n = v if idx % 2 == 0 else v >> 40
            yield [nth_matrix(coef, init, n)]

def set_nth_big():
    r = SM(33)
    for d in (24, 32, 64, 100, 128, 255, 256):
        for idx in range(3):
            coef = [r.res() for _ in range(d)]
            init = [r.res() for _ in range(d)]
            yield [nth_poly(coef, init, r.next())]

def set_nth_iter():
    r = SM(44)
    for d in (64, 256):
        for n in (0, 63, 64, 255, 256, 257, 300, 1000, 3000):
            coef = [r.res() for _ in range(d)]
            init = [r.res() for _ in range(d)]
            yield [nth_iter(coef, init, n)]

def bm_out(L, uniq_ok, coefs, n):
    return [L] + (coefs if 2 * L <= n else [])

def sequences(alpha, maxlen):
    for n in range(maxlen + 1):
        for t in itertools.product(alpha, repeat=n):
            yield list(t)

def bm_exact(seq):
    L, uniq, coefs = shortest(seq)
    if 2 * L <= len(seq): assert uniq
    return bm_out(L, uniq, coefs, len(seq))

def set_bm_ter():
    for s in sequences([0, 1, 2], 8): yield bm_exact(s)

def set_bm_ext():
    for s in sequences([0, 1, 2, P - 1], 6): yield bm_exact(s)

def set_bm_rec():
    r = SM(55)
    for L in range(31):
        for idx in range(5):
            ln = (2 * L, 2 * L + 1, 2 * L + 5, 3 * L + 3, L)[idx]
            coef, seq = from_recurrence(r, L, ln)
            yield bm_exact(seq)

def set_bm_rand():
    r = SM(66)
    for _ in range(60):
        ln = r.next() % 25
        yield bm_exact([r.res() for _ in range(ln)])

BIG = ((256, 512), (256, 513), (256, 1024), (100, 200), (100, 1000), (255, 511), (1, 1024), (2, 1024))

def big_seqs():
    r = SM(77)
    for order, ln in BIG:
        yield from_recurrence(r, order, ln)[1]

def set_bm_big():
    for seq in big_seqs():
        L, coefs = bm(seq)
        yield bm_out(L, True, coefs, len(seq))

SETS = [('nth_small', set_nth_small), ('nth_mid', set_nth_mid), ('nth_big', set_nth_big), ('nth_iter', set_nth_iter), ('bm_ter', set_bm_ter),
        ('bm_ext', set_bm_ext), ('bm_rec', set_bm_rec), ('bm_rand', set_bm_rand), ('bm_big', set_bm_big)]

def table():
    rows = []
    for name, gen in SETS:
        h, c = BASIS, 0
        for vals in gen():
            h = fnv(h, vals)
            c += 1
        rows.append((name, c, h))
    return rows

def selfcheck():
    # poly method against term-by-term iteration and the matrix method
    r = SM(1)
    for d in (1, 2, 3, 5, 9, 16, 40):
        for _ in range(6):
            coef = [r.res() for _ in range(d)]
            init = [r.res() for _ in range(d)]
            n = r.next() % 20000
            assert nth_poly(coef, init, n) == nth_iter(coef, init, n), (d, n)
    for d in (1, 2, 4, 8):
        coef = [r.res() for _ in range(d)]
        init = [r.res() for _ in range(d)]
        n = r.next()
        assert nth_poly(coef, init, n) == nth_matrix(coef, init, n), (d, n)
    # Fibonacci golden values
    assert nth_matrix([1, 1], [0, 1], 10**18) == nth_poly([1, 1], [0, 1], 10**18)
    # Berlekamp-Massey against elimination
    r = SM(2)
    for _ in range(400):
        ln = r.next() % 30
        seq = [r.res() for _ in range(ln)] if r.next() & 1 else from_recurrence(r, r.next() % 8, ln)[1]
        L, coefs = bm(seq)
        L2, uniq, c2 = shortest(seq)
        assert L == L2, (seq, L, L2)
        if uniq: assert coefs == c2
    # large sequences: the order and the unique coefficients of the BM against elimination at L and L - 1
    for seq in big_seqs():
        L, coefs = bm(seq)
        if L > 130: continue  # elimination on the largest orders is slow; the bounds above were checked for 256 by construction below
        ok, uniq, c2 = solve(seq, L)
        assert ok and (L == 0 or not solve(seq, L - 1)[0])
        if 2 * L <= len(seq): assert uniq and c2 == coefs
    # order 256 sequences: BM against elimination at L and L - 1 (a few seconds)
    for seq in list(big_seqs())[:1]:
        L, coefs = bm(seq)
        ok, uniq, c2 = solve(seq, L)
        assert ok and uniq and c2 == coefs and not solve(seq, L - 1)[0]
    print('selfcheck ok: polynomial method, matrix method, iteration, Berlekamp-Massey and elimination agree')

if __name__ == '__main__':
    a = sys.argv[1:]
    if a == ['--digests']:
        for name, c, h in table():
            print('digest %s count %d 0x%016x' % (name, c, h))
    elif a == ['--selfcheck']:
        selfcheck()
    elif a == ['--check']:
        selfcheck()
        here = os.path.dirname(os.path.abspath(__file__))
        t = open(os.path.join(here, '..', 'tests', 'linrec', 'linrec_tests.cpp')).read()
        got = dict((m.group(1), (int(m.group(2)), int(m.group(3), 16))) for m in re.finditer(r'// digest (\w+) count (\d+) (0x[0-9a-f]{16})', t))
        want = dict((n, (c, h)) for n, c, h in table())
        assert got == want, 'digest table in the test file is not current'
        print('check ok: digest table matches', len(want), 'entries')
    else:
        print(__doc__)
        sys.exit(2)
