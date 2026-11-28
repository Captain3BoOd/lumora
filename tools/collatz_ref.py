#!/usr/bin/env python3
"""Independent reference for core/collatz (Python standard library only; no code shared with the C++ library).

The trajectory is run with unbounded Python integers (traj): x -> x // 2 for even x, x -> 3 x + 1 for odd x, until x = 1. The call is Overflow as soon as an odd x
has 3 x + 1 > 2^64 - 1 (decided on the big-integer value, not with the library's constant). For the large exhaustive range a different method is used: the table
method (table_walk) computes steps and peak for every n by following the trajectory only until it drops below n and then reading the stored result for that smaller
value; the `check` command cross-checks it against traj on every n up to 2^16 and on seeded samples, and against the inverse tree (inverse_tree: from 1 upwards
through the predecessors 2 m and (m - 1) / 3 when m = 4 mod 6) for every value up to 2^20.
Digests are FNV-1a over 64-bit words (fold(h, w) = (h ^ w) * 0x100000001b3 mod 2^64, start 0xcbf29ce484222325). A result is folded as: n, the result code
(0 Ok, 3 Overflow), then steps and peak when Ok.
Commands: check, emit (prints the C++ constants for the test), header (prints the header example table check), overflow_table (for each odd start from T - 66 to T:
which odd update, counted from 1, first has 3 x + 1 above 2^64 - 1, after how many steps, and the odd value x it happens at).
"""
import sys
from array import array

M64 = (1 << 64) - 1
FNV0 = 0xCBF29CE484222325
OK, OVERFLOW = 0, 3
T = 0x5555555555555555  # smallest odd x with 3 x + 1 = 2^64
NMAX = 1 << 22
NBIG = 1 << 24


def fold(h, w):
    return ((h ^ (w & M64)) * 0x100000001B3) & M64


def splitmix_at(pos):
    z = (0x9E3779B97F4A7C15 * (pos + 1)) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def traj(n):
    """Returns (code, steps, peak) with unbounded integers."""
    x, s, p = n, 0, n
    while x != 1:
        if x % 2 == 0:
            x //= 2
        else:
            y = 3 * x + 1
            if y > M64:
                return OVERFLOW, 0, 0
            x = y
            if x > p:
                p = x
        s += 1
    return OK, s, p


def fold_result(h, n, r):
    h = fold(h, n)
    h = fold(h, r[0])
    if r[0] == OK:
        h = fold(h, r[1])
        h = fold(h, r[2])
    return h


def table_walk(top):
    """steps and peak for 1..top by the stop-below-n table method; also returns the digests at 2^22 and top."""
    st = array('Q', [0]) * (top + 1)
    pk = array('Q', [0]) * (top + 1)
    st[1], pk[1] = 0, 1
    h = fold(fold(fold(FNV0, 1), OK), 0)
    h = fold(h, 1)
    h22 = None
    mx = {}
    best = (0, 1)
    if top >= NMAX and NMAX == 1:
        h22 = h
    for n in range(2, top + 1):
        x, s, p = n, 0, n
        while x >= n:
            if x & 1:
                x = 3 * x + 1
                if x > p:
                    p = x
            else:
                x >>= 1
            s += 1
        s += st[x]
        if pk[x] > p:
            p = pk[x]
        st[n], pk[n] = s, p
        h = fold(fold(fold(fold(h, n), OK), s), p)
        if s > best[0]:
            best = (s, n)
        if n == NMAX:
            h22 = h
            mx[22] = best
    mx[24] = best
    table_walk.max_steps = mx
    table_walk.pk = pk
    return st, pk, h22, h


def inverse_tree(bound):
    """depth (steps) and peak for every value <= bound whose whole trajectory stays <= bound, built from 1 by predecessors."""
    depth = {1: 0}
    peak = {1: 1}
    stack = [1]
    while stack:
        m = stack.pop()
        preds = [2 * m]
        if m % 6 == 4 and (m - 1) // 3 > 1:
            preds.append((m - 1) // 3)
        for c in preds:
            if c <= bound and c not in depth:
                depth[c] = depth[m] + 1
                peak[c] = max(c, peak[m])
                stack.append(c)
    return depth, peak


def edge_set():
    return list(range(T - 4096, T + 4097))


def halving_set():
    return [2 * m for m in range(T - 4096, T + 4097)]


def shifted_set():
    out = []
    for a in (1, 3, 5, 7, 9, 15, 17, 27, 31, 63):
        for k in range(64):
            v = a << k
            if v > M64:
                break
            for w in (v - 1, v, v + 1):
                if 1 <= w <= M64:
                    out.append(w)
    out.append(M64)
    out.append(M64 - 1)
    return out


def random_set():
    out = []
    pos = 1000
    for b in range(1, 65):
        for _ in range(3000):
            out.append((splitmix_at(pos) >> (64 - b)) | (1 << (b - 1)))
            pos += 1
    return out


def uniform_set():
    return [splitmix_at(2000000 + i) for i in range(200000)]


def below40_set():
    out = []
    for i in range(200000):
        v = splitmix_at(3000000 + i) >> 24
        out.append(v if v != 0 else 1)
    return out


def first_overflow_index(n):
    """1-based index of the odd update at which the call overflows, 0 when the call is Ok."""
    x, odd = n, 0
    while x != 1:
        if x % 2 == 0:
            x //= 2
        else:
            odd += 1
            if 3 * x + 1 > M64:
                return odd
            x = 3 * x + 1
    return 0


def first_overflow_set():
    """Scan T - 2^18 .. T + 2^18 upwards; keep the first 64 starts for each overflowing odd-update index j (1, 2, 3, ...). Returns the list and the per-j counts."""
    cnt = {}
    out = []
    for n in range(T - (1 << 18), T + (1 << 18) + 1):
        j = first_overflow_index(n)
        if j > 0 and cnt.get(j, 0) < 64:
            cnt[j] = cnt.get(j, 0) + 1
            out.append(n)
    return out, cnt


def digest_set(lst):
    h = FNV0
    ok = ov = 0
    best = (0, 0)
    for n in lst:
        r = traj(n)
        if r[0] == OK and r[1] > best[0]:
            best = (r[1], n)
        h = fold_result(h, n, r)
        if r[0] == OK:
            ok += 1
        else:
            ov += 1
    digest_set.best = best
    return h, ok, ov, len(lst)


def cmd_check():
    st, pk, h22, h24 = table_walk(1 << 16)
    for n in range(1, (1 << 16) + 1):
        r = traj(n)
        assert r == (OK, st[n], pk[n]), n
    st, pk, _, _ = table_walk(NMAX)
    for i in range(20000):
        n = 1 + splitmix_at(i) % NMAX
        assert traj(n) == (OK, st[n], pk[n]), n
    depth22, _ = inverse_tree(NMAX)
    assert len(depth22) == sum(1 for n in range(1, NMAX + 1) if table_walk.pk[n] <= NMAX)
    depth, peak = inverse_tree(1 << 20)
    cnt = 0
    for n in range(1, (1 << 20) + 1):
        r = traj(n)
        if r[2] <= (1 << 20):
            assert depth.get(n) == r[1] and peak.get(n) == r[2], n
            cnt += 1
        else:
            assert n not in depth, n
    assert cnt == len(depth)
    # the boundary facts of the contract
    assert 3 * (T - 2) + 1 == M64 - 5 and T - 2 == 6148914691236517203
    assert 3 * T + 1 == 1 << 64 and T == 6148914691236517205
    # T - 2 is the largest odd x whose first step 3 x + 1 fits, but the trajectory still overflows two steps later: 3 x + 1 = 2^64 - 6, half of it is the odd
    # 2^63 - 3, and 3 * (2^63 - 3) + 1 > 2^64 - 1. T itself overflows at the first step. So both are Overflow, and so are the doubles 2 * (T - 2) and 2 * T.
    assert (3 * (T - 2) + 1) // 2 == (1 << 63) - 3 and 3 * ((1 << 63) - 3) + 1 > M64
    for n in (T - 2, T, 2 * (T - 2), 2 * T):
        assert traj(n) == (OVERFLOW, 0, 0), n
    # the largest odd start in the window T - 4096 .. T + 4096 that is Ok is T - 68
    assert traj(T - 68)[0] == OK and all(traj(n)[0] == OVERFLOW for n in range(T - 66, T + 4097, 2))
    assert traj(7887663552367)[0] == OVERFLOW and traj(1 << 63) == (OK, 63, 1 << 63)
    print("check ok; inverse-tree nodes up to 2^20:", cnt)


HEADER = [  # n, steps, peak from the header
    (1, 0, 1), (2, 1, 2), (3, 7, 16), (6, 8, 16), (7, 16, 52), (27, 111, 9232), (97, 118, 9232), (871, 178, 190996),
    (6171, 261, 975400), (77031, 350, 21933016), (837799, 524, 2974984576), (63728127, 949, 966616035460),
    (670617279, 986, 966616035460), (9780657630, 1132, 319497287463520), (75128138247, 1228, 319497287463520),
    (989345275647, 1348, 1219624271099764), (80867137596217, 1662, 1180174841128253392),
    (942488749153153, 1862, 1180174841128253392), (13194139533312, 49, 13194139533312),
    (4294967295, 451, 3706040377703680), ((1 << 62) + 1, 481, 13835058055282163716), (1 << 63, 63, 1 << 63)]
HEADER_OVF = [7887663552367, (1 << 63) - 1, (1 << 63) + 1, M64, M64 - 1]


def cmd_header():
    for n, s, p in HEADER:
        assert traj(n) == (OK, s, p), n
    for n in HEADER_OVF:
        assert traj(n)[0] == OVERFLOW, n
    print("header examples reproduced:", len(HEADER), "Ok,", len(HEADER_OVF), "Overflow")


def cmd_emit():
    _, _, h22, h24 = table_walk(NBIG)
    print("constexpr u64 kDigest22 = 0x%016Xull;" % h22)
    print("constexpr u64 kDigest24 = 0x%016Xull;" % h24)
    print("constexpr u32 kMaxSteps22 = %du;  // first reached at n = %d" % table_walk.max_steps[22])
    print("constexpr u32 kMaxSteps24 = %du;  // first reached at n = %d" % table_walk.max_steps[24])
    fo, cnt = first_overflow_set()
    jmax = max(cnt)
    print("constexpr u32 kFoJmax = %du;" % jmax)
    print("constexpr u32 kFoCounts[%d] = {%s};" % (jmax, ", ".join("%du" % cnt.get(j, 0) for j in range(1, jmax + 1))))
    nodes = {}
    for B in (NMAX, NBIG):
        nodes[B] = sum(1 for n in range(1, B + 1) if table_walk.pk[n] <= B)
    print("constexpr u64 kNodes22 = %du;" % nodes[NMAX])
    print("constexpr u64 kNodes24 = %du;" % nodes[NBIG])
    for name, lst in (("Edge", edge_set()), ("Halving", halving_set()), ("Shifted", shifted_set()), ("Random", random_set()), ("Uniform", uniform_set()), ("Below40", below40_set()), ("FirstOverflow", fo)):
        h, ok, ov, n = digest_set(lst)
        print("constexpr u64 kDigest%s = 0x%016Xull;" % (name, h))
        print("constexpr u64 kCount%sOk = %du;" % (name, ok))
        print("constexpr u64 kCount%sOverflow = %du;" % (name, ov))
        print("constexpr u64 kCount%sTotal = %du;" % (name, n))
        print("constexpr u32 kMaxSteps%s = %du;  // first reached at n = %d" % ((name,) + digest_set.best))
    r = traj(T - 68)
    print("// n=%d code=%d steps=%d peak=%d" % (T - 68, r[0], r[1], r[2]))


def cmd_overflow_table():
    print("start n | T - n | overflowing odd update (1 = the first) | steps taken before it | odd x there")
    for n in range(T - 66, T + 1, 2):
        x, odd, steps = n, 0, 0
        while True:
            if x % 2:
                odd += 1
                if 3 * x + 1 > M64:
                    break
                x = 3 * x + 1
            else:
                x //= 2
            steps += 1
        print("%d | %d | %d | %d | %d" % (n, T - n, odd, steps, x))


if __name__ == "__main__":
    {"check": cmd_check, "emit": cmd_emit, "header": cmd_header, "overflow_table": cmd_overflow_table}[sys.argv[1]]()
