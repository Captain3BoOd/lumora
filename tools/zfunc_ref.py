#!/usr/bin/env python3
"""Reference for core/zfunc (independent of the C++ code).

Small strings: the Z array and the border array come straight from their definitions (every candidate length is compared), no scan with a window and no
fall-back chain. Large strings: separate linear scans written differently from the C++ ones (the border array is built from the Z array). The digest of a
string set is FNV-1a 64 over the little-endian bytes of every u32 of z then pi, string after string.

  python3 tools/zfunc_ref.py --digests   print the digest table that tests/zfunc/zfunc_tests.cpp holds
  python3 tools/zfunc_ref.py --check     verify the table in the test file and run the self-check (brute force against the linear versions)
  python3 tools/zfunc_ref.py --selfcheck only the self-check
"""
import itertools, random, re, struct, sys, os

def z_def(s):
    n = len(s)
    out = []
    for i in range(n):
        k = 0
        while i + k < n and s[k] == s[i + k]:
            k += 1
        out.append(k)
    return out

def pi_def(s):
    n = len(s)
    out = []
    for i in range(n):
        best = 0
        for k in range(i, 0, -1):  # proper borders of s[0..i] have length below i + 1
            if s[:k] == s[i + 1 - k:i + 1]:
                best = k
                break
        out.append(best)
    return out

def z_lin(s):
    n = len(s)
    z = [0] * n
    if n:
        z[0] = n
    l = r = 0
    for i in range(1, n):
        if i < r:
            z[i] = min(r - i, z[i - l])
        while i + z[i] < n and s[z[i]] == s[i + z[i]]:
            z[i] += 1
        if i + z[i] > r:
            l, r = i, i + z[i]
    return z

def pi_from_z(z):
    n = len(z)
    pi = [0] * n
    for i in range(1, n):
        j = z[i] - 1
        while j >= 0 and pi[i + j] == 0:
            pi[i + j] = j + 1
            j -= 1
    return pi

def fnv(h, vals):
    for v in vals:
        for b in struct.pack('<I', v):
            h ^= b
            h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h

BASIS = 0xcbf29ce484222325

def strings(alpha, maxlen):
    for n in range(maxlen + 1):
        for t in itertools.product(alpha, repeat=n):
            yield bytes(t)

def digest_set(it, small=True):
    h = BASIS
    c = 0
    for s in it:
        h = fnv(h, z_def(s) if small else z_lin(s))
        h = fnv(h, pi_def(s) if small else pi_from_z(z_lin(s)))
        c += 1
    return h, c

def lcg(seed, n, k):
    x = seed
    out = bytearray()
    for _ in range(n):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        out.append((x >> 16) % k)
    return bytes(out)

def fib(n):
    a, b = b'0', b'01'
    while len(b) < n:
        a, b = b, b + a
    return bytes(c - 48 for c in b[:n])

def large():
    N = 262144
    blk = lcg(77, 1000, 256)
    return [
        ('rep0', bytes(N)),
        ('alt', bytes([0 if i % 2 == 0 else 255 for i in range(N - 1)])),
        ('zeros_one', bytes(N - 1) + b'\x01'),
        ('one_zeros', b'\x01' + bytes(N - 1)),
        ('fib', fib(N)),
        ('rand2', lcg(11, N, 2)),
        ('rand256', lcg(12, N, 256)),
        ('block1000', (blk * 300)[:N]),
    ]

def sets():
    yield 'bin16', strings([0, 255], 16), True
    yield 'ter10', strings([0, 128, 255], 10), True
    yield 'six6', strings([0, 1, 127, 128, 254, 255], 6), True
    yield 'pairs', itertools.chain([b''], ([a] and bytes([a]) for a in range(256)), (bytes([a, b]) for a in range(256) for b in range(256))), True

def table():
    rows = []
    for name, it, small in sets():
        h, c = digest_set(it, small)
        rows.append((name, c, h))
    for name, s in large():
        h, c = digest_set([s], False)
        rows.append((name, 1, h))
    return rows

def selfcheck():
    rnd = random.Random(5)
    for _ in range(30000):
        n = rnd.randint(0, 30)
        k = rnd.choice([1, 2, 3, 256])
        s = bytes(rnd.randrange(k) for _ in range(n))
        z = z_def(s)
        assert z == z_lin(s), s
        assert pi_def(s) == pi_from_z(z_lin(s)), s
    print('selfcheck ok: 30000 random strings, definitions equal the linear versions')

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
        t = open(os.path.join(here, '..', 'tests', 'zfunc', 'zfunc_tests.cpp')).read()
        got = dict((m.group(1), (int(m.group(2)), int(m.group(3), 16))) for m in re.finditer(r'// digest (\w+) count (\d+) (0x[0-9a-f]{16})', t))
        want = dict((n, (c, h)) for n, c, h in table())
        assert got == want, 'digest table in the test file is not current'
        print('check ok: digest table matches', len(want), 'entries')
    else:
        print(__doc__)
        sys.exit(2)
