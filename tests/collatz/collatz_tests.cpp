// Tests for core/collatz. Expected values come from the header's examples and from FNV-1a 64-bit digests made by tools/collatz_ref.py (Python standard library only:
// unbounded-integer trajectories, the stop-below-n table method for the exhaustive ranges, and an inverse-tree cross-check; see docs/collatz.md), and from two
// test-only oracles of a different method than the library: (1) the shortcut map T(x) = (3 x + 1) / 2 for odd x with its own step accounting and an overflow test
// made by a division, and (2) the inverse tree (from 1 upwards through the predecessors 2 m and (m - 1) / 3 for m = 4 mod 6), which assigns steps and peak without
// iterating forwards. Extents (see docs/collatz.md): the header examples; the check-order matrix; the overflow boundary around 0x5555555555555555; seven digest sets
// (edge window, doubled window, shifted values, 64 bit-length classes of seeded values, 200000 seeded uniform u64, 200000 seeded values below 2^40, a sample varied
// by the odd update at which the call overflows); every n up to 2^24 against both oracles (shortcut oracle and inverse tree) and the digests at 2^22 and 2^24. Result checks are collected in bools and asserted once per case (a loop of millions of values is one assertion).
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "collatz.h"

namespace {
using lumora::collatz::CzResult;
using lumora::collatz::collatz_peak;
using lumora::collatz::collatz_steps;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kFnv0 = 0xcbf29ce484222325ull;
constexpr u64 kCell = 0x1234567812345678ull;
constexpr u64 kMax64 = ~0ull;
constexpr u64 kT = 0x5555555555555555ull;  // 6148914691236517205: the smallest odd x with 3 x + 1 = 2^64
constexpr u64 kNmax = 1ull << 22;
constexpr u64 kNbig = 1ull << 24;

constexpr u64 kDigest22 = 0xC434AB6C15BDB22Bull;
constexpr u64 kDigest24 = 0xC30B1B41BA9799B2ull;
constexpr u32 kMaxSteps22 = 596u;  // first reached at n = 3732423
constexpr u32 kMaxSteps24 = 704u;  // first reached at n = 15733191
constexpr u32 kFoJmax = 102u;
constexpr u32 kFoCounts[102] = {64u, 64u, 64u, 0u, 64u, 0u, 64u, 64u, 0u, 64u, 0u, 64u, 64u, 0u, 64u, 0u, 64u, 0u, 64u, 64u, 0u, 64u, 0u, 64u, 64u, 0u, 64u, 0u, 64u, 0u, 64u, 64u, 0u, 64u, 0u, 64u, 64u, 0u, 64u, 0u, 64u, 0u, 64u, 53u, 0u, 30u, 0u, 57u, 64u, 0u, 37u, 0u, 27u, 64u, 0u, 2u, 0u, 7u, 0u, 1u, 18u, 0u, 0u, 0u, 2u, 15u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 4u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 23u};
constexpr u64 kNodes22 = 1661341u;
constexpr u64 kNodes24 = 6648354u;
constexpr u64 kDigestEdge = 0x29BFED85FD885395ull;
constexpr u64 kCountEdgeOk = 2441u;
constexpr u64 kCountEdgeOverflow = 5752u;
constexpr u64 kCountEdgeTotal = 8193u;
constexpr u32 kMaxStepsEdge = 856u;  // first reached at n = 6148914691236519254
constexpr u64 kDigestHalving = 0x9B12D7086B1D8219ull;
constexpr u64 kCountHalvingOk = 2441u;
constexpr u64 kCountHalvingOverflow = 5752u;
constexpr u64 kCountHalvingTotal = 8193u;
constexpr u32 kMaxStepsHalving = 857u;  // first reached at n = 12297829382473038508
constexpr u64 kDigestShifted = 0x2529C5E4B1084927ull;
constexpr u64 kCountShiftedOk = 1587u;
constexpr u64 kCountShiftedOverflow = 250u;
constexpr u64 kCountShiftedTotal = 1837u;
constexpr u32 kMaxStepsShifted = 759u;  // first reached at n = 2017612633061982209
constexpr u64 kDigestRandom = 0x14B393AF0B4D8A42ull;
constexpr u64 kCountRandomOk = 185930u;
constexpr u64 kCountRandomOverflow = 6070u;
constexpr u64 kCountRandomTotal = 192000u;
constexpr u32 kMaxStepsRandom = 939u;  // first reached at n = 1051610589715613599
constexpr u64 kDigestUniform = 0x24306FE567AA38B3ull;
constexpr u64 kCountUniformOk = 79334u;
constexpr u64 kCountUniformOverflow = 120666u;
constexpr u64 kCountUniformTotal = 200000u;
constexpr u32 kMaxStepsUniform = 1085u;  // first reached at n = 2538342448711463201
constexpr u64 kDigestBelow40 = 0x645E46862FEE9089ull;
constexpr u64 kCountBelow40Ok = 200000u;
constexpr u64 kCountBelow40Overflow = 0u;
constexpr u64 kCountBelow40Total = 200000u;
constexpr u32 kMaxStepsBelow40 = 892u;  // first reached at n = 451151379025
constexpr u64 kDigestFirstOverflow = 0x3E6E4B59836E63FFull;
constexpr u64 kCountFirstOverflowOk = 0u;
constexpr u64 kCountFirstOverflowOverflow = 2069u;
constexpr u64 kCountFirstOverflowTotal = 2069u;
constexpr u32 kMaxStepsFirstOverflow = 0u;  // first reached at n = 0
// n = T - 68 =6148914691236517137 code=0 steps=559 peak=18446744073709551412

u64 fold(u64 h, u64 w) { return (h ^ w) * 0x100000001b3ull; }

u64 splitmix_at(u64 pos) {
  u64 z = 0x9E3779B97F4A7C15ull * (pos + 1u);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// ---- library call: both functions, the cell must stay untouched on a failure and both must return the same code ----
struct Rec {
  u32 code;
  u64 steps;
  u64 peak;
  bool guard_ok;
};
Rec call(u64 n) {
  Rec r{};
  u64 a = kCell;
  u64 b = kCell;
  const CzResult r1 = collatz_steps(n, &a);
  const CzResult r2 = collatz_peak(n, &b);
  r.code = static_cast<u32>(r1);
  r.guard_ok = (r1 == r2);
  if (r1 == CzResult::Ok) {
    r.steps = a;
    r.peak = b;
  } else {
    r.steps = 0u;
    r.peak = 0u;
    if (a != kCell) r.guard_ok = false;
    if (b != kCell) r.guard_ok = false;
  }
  return r;
}

// ---- oracle 1: the shortcut map, overflow decided by a division ----
Rec oracle_shortcut(u64 n) {
  Rec r{};
  r.guard_ok = true;
  const u64 lim = (kMax64 - 1u) / 3u;  // an odd x with 3 x + 1 <= 2^64 - 1 has x <= lim
  u64 x = n;
  u64 s = 0u;
  u64 pk = n;
  while (x != 1u) {
    if (x % 2u == 0u) {
      x /= 2u;
      s += 1u;
    } else {
      if (x > lim) {
        r.code = 3u;
        r.steps = 0u;
        r.peak = 0u;
        return r;
      }
      const u64 nx = x + x / 2u + 1u;  // (3 x + 1) / 2 for odd x
      if (nx * 2u > pk) pk = nx * 2u;   // 3 x + 1 = 2 nx, at most 2^64 - 2
      x = nx;
      s += 2u;
    }
  }
  r.code = 0u;
  r.steps = s;
  r.peak = pk;
  return r;
}

bool same(const Rec& a, const Rec& b) {
  if (a.code != b.code) return false;
  if (a.code == 0u && (a.steps != b.steps || a.peak != b.peak)) return false;
  return true;
}

u32 floor_log2(u64 n) {
  u32 l = 0u;
  while (n > 1u) {
    n >>= 1u;
    ++l;
  }
  return l;
}

// cheap invariants of any Ok result: peak >= n, steps >= floor(log2 n) (a step divides by at most 2), peak >= 1
bool invariants(u64 n, const Rec& r) {
  if (r.code != 0u) return true;
  if (r.peak < n) return false;
  if (r.peak < 1u) return false;
  if (r.steps < floor_log2(n)) return false;
  return true;
}

u64 fold_rec(u64 h, u64 n, const Rec& r) {
  h = fold(h, n);
  h = fold(h, r.code);
  if (r.code == 0u) {
    h = fold(h, r.steps);
    h = fold(h, r.peak);
  }
  return h;
}

struct SetStat {
  u64 digest;
  u64 ok;
  u64 overflow;
  u64 total;
  u32 max_steps;
  bool all_ok;  // guard, oracle and invariants
};
SetStat run_set(const std::vector<u64>& v) {
  SetStat s{kFnv0, 0u, 0u, 0u, 0u, true};
  for (std::size_t i = 0; i < v.size(); ++i) {
    const u64 n = v[i];
    const Rec r = call(n);
    const Rec o = oracle_shortcut(n);
    if (!r.guard_ok) s.all_ok = false;
    if (!same(r, o)) s.all_ok = false;
    if (!invariants(n, r)) s.all_ok = false;
    s.digest = fold_rec(s.digest, n, r);
    s.total += 1u;
    if (r.code == 0u) {
      s.ok += 1u;
      if (r.steps > s.max_steps) s.max_steps = static_cast<u32>(r.steps);
    } else {
      s.overflow += 1u;
    }
  }
  return s;
}

std::vector<u64> edge_set() {
  std::vector<u64> v;
  for (u64 n = kT - 4096u; n <= kT + 4096u; ++n) v.push_back(n);
  return v;
}
std::vector<u64> halving_set() {
  std::vector<u64> v;
  for (u64 m = kT - 4096u; m <= kT + 4096u; ++m) v.push_back(2u * m);
  return v;
}
std::vector<u64> shifted_set() {
  static const u64 mult[10] = {1u, 3u, 5u, 7u, 9u, 15u, 17u, 27u, 31u, 63u};
  std::vector<u64> v;
  for (u32 ai = 0u; ai < 10u; ++ai) {
    const u64 a = mult[ai];
    for (u32 k = 0u; k < 64u; ++k) {
      const u64 x = a << k;
      if ((x >> k) != a) break;
      if (x - 1u >= 1u) v.push_back(x - 1u);
      v.push_back(x);
      if (x != kMax64) v.push_back(x + 1u);
    }
  }
  v.push_back(kMax64);
  v.push_back(kMax64 - 1u);
  return v;
}
std::vector<u64> random_set() {
  std::vector<u64> v;
  u64 pos = 1000u;
  for (u32 b = 1u; b <= 64u; ++b) {
    for (u32 i = 0u; i < 3000u; ++i) {
      v.push_back((splitmix_at(pos) >> (64u - b)) | (1ull << (b - 1u)));
      ++pos;
    }
  }
  return v;
}

std::vector<u64> uniform_set() {
  std::vector<u64> v;
  for (u32 i = 0u; i < 200000u; ++i) v.push_back(splitmix_at(2000000u + i));
  return v;
}
std::vector<u64> below40_set() {
  std::vector<u64> v;
  for (u32 i = 0u; i < 200000u; ++i) {
    const u64 x = splitmix_at(3000000u + i) >> 24u;
    v.push_back(x != 0u ? x : 1u);
  }
  return v;
}

// 1-based index of the odd update at which a call overflows, 0 when it is Ok; made with the shortcut map (each odd x is one shortcut update)
u32 oracle_first_overflow(u64 n) {
  const u64 lim = (kMax64 - 1u) / 3u;
  u64 x = n;
  u32 odd = 0u;
  while (x != 1u) {
    if (x % 2u == 0u) {
      x /= 2u;
    } else {
      ++odd;
      if (x > lim) return odd;
      x = x + x / 2u + 1u;
    }
  }
  return 0u;
}
// scan T - 2^18 .. T + 2^18 upwards and keep the first 64 starts for each overflowing odd-update index; counts[j] for j below 256
std::vector<u64> first_overflow_set(u32* counts) {
  std::vector<u64> v;
  for (u32 j = 0u; j < 256u; ++j) counts[j] = 0u;
  for (u64 n = kT - (1ull << 18u); n <= kT + (1ull << 18u); ++n) {
    const u32 j = oracle_first_overflow(n);
    if (j > 0u && j < 256u && counts[j] < 64u) {
      ++counts[j];
      v.push_back(n);
    }
  }
  return v;
}

struct Ex {
  u64 n;
  u64 steps;
  u64 peak;
};
}  // namespace

TEST_CASE("collatz header examples", "[collatz]") {
  static const Ex ex[] = {
      {1u, 0u, 1u}, {2u, 1u, 2u}, {3u, 7u, 16u}, {6u, 8u, 16u}, {7u, 16u, 52u}, {27u, 111u, 9232u}, {97u, 118u, 9232u}, {871u, 178u, 190996u},
      {6171u, 261u, 975400u}, {77031u, 350u, 21933016u}, {837799u, 524u, 2974984576ull}, {63728127u, 949u, 966616035460ull},
      {670617279u, 986u, 966616035460ull}, {9780657630ull, 1132u, 319497287463520ull}, {75128138247ull, 1228u, 319497287463520ull},
      {989345275647ull, 1348u, 1219624271099764ull}, {80867137596217ull, 1662u, 1180174841128253392ull},
      {942488749153153ull, 1862u, 1180174841128253392ull}, {13194139533312ull, 49u, 13194139533312ull}, {4294967295ull, 451u, 3706040377703680ull},
      {4611686018427387905ull, 481u, 13835058055282163716ull}, {9223372036854775808ull, 63u, 9223372036854775808ull}};
  static const u64 ovf[] = {7887663552367ull, 9223372036854775807ull, 9223372036854775809ull, 18446744073709551615ull, 18446744073709551614ull};
  bool exok = true;
  for (std::size_t i = 0; i < sizeof(ex) / sizeof(ex[0]); ++i) {
    const Rec r = call(ex[i].n);
    if (r.code != 0u || r.steps != ex[i].steps || r.peak != ex[i].peak || !r.guard_ok) exok = false;
  }
  bool ovok = true;
  for (std::size_t i = 0; i < sizeof(ovf) / sizeof(ovf[0]); ++i) {
    const Rec r = call(ovf[i]);
    if (r.code != 3u || !r.guard_ok) ovok = false;
  }
  CHECK(exok);
  CHECK(ovok);
  // 2^63 is 63 halvings and no overflow, 2^k gives k steps and peak 2^k for every k
  bool pw = true;
  for (u32 k = 0u; k < 64u; ++k) {
    const Rec r = call(1ull << k);
    if (r.code != 0u || r.steps != k || r.peak != (1ull << k) || !r.guard_ok) pw = false;
  }
  CHECK(pw);
}

TEST_CASE("collatz check order and untouched cells", "[collatz]") {
  const u64 ns[] = {0u, 1u, 2u, 27u, kT - 68u, kT - 2u, kT, kMax64};
  bool null_ok = true;
  bool zero_ok = true;
  bool ovf_ok = true;
  bool okw = true;
  for (std::size_t i = 0; i < sizeof(ns) / sizeof(ns[0]); ++i) {
    const u64 n = ns[i];
    if (collatz_steps(n, nullptr) != CzResult::NullArgument) null_ok = false;  // null first, even for n = 0 and an overflowing n
    if (collatz_peak(n, nullptr) != CzResult::NullArgument) null_ok = false;
    u64 a = kCell;
    u64 b = kCell;
    const CzResult r1 = collatz_steps(n, &a);
    const CzResult r2 = collatz_peak(n, &b);
    if (n == 0u) {
      if (r1 != CzResult::BadArgument || r2 != CzResult::BadArgument || a != kCell || b != kCell) zero_ok = false;
    } else if (n == kT || n == kT - 2u || n == kMax64) {
      if (r1 != CzResult::Overflow || r2 != CzResult::Overflow || a != kCell || b != kCell) ovf_ok = false;
    } else {
      if (r1 != CzResult::Ok || r2 != CzResult::Ok || a == kCell || b == kCell) okw = false;
    }
  }
  CHECK(null_ok);
  CHECK(zero_ok);
  CHECK(ovf_ok);
  CHECK(okw);
}

TEST_CASE("collatz overflow boundary", "[collatz]") {
  // T = 6148914691236517205 is the smallest odd x with 3 x + 1 = 2^64 (first step overflows). T - 2 = 6148914691236517203 is the largest odd x whose first
  // step fits (3 x + 1 = 2^64 - 6) but its trajectory overflows two steps later (half is the odd 2^63 - 3). The doubles 2 (T - 2) and 2 T reach those values
  // after one halving. All four are Overflow; the largest Ok odd start below T is T - 68.
  const u64 pts[] = {6148914691236517203ull, 6148914691236517205ull, 12297829382473034406ull, 12297829382473034410ull};
  bool all_ovf = true;
  for (std::size_t i = 0; i < 4u; ++i) {
    const Rec r = call(pts[i]);
    if (r.code != 3u || !r.guard_ok) all_ovf = false;
    if (!same(r, oracle_shortcut(pts[i]))) all_ovf = false;
  }
  CHECK(all_ovf);
  const bool consts = (kT == 6148914691236517205ull) && (kT - 2u == 6148914691236517203ull) && (2u * (kT - 2u) == 12297829382473034406ull) &&
                      (2u * kT == 12297829382473034410ull) && (3u * (kT - 2u) + 1u == kMax64 - 5u) && (3u * kT + 1u == 0u);
  CHECK(consts);
  const Rec top = call(kT - 68u);
  const bool top_ok = top.code == 0u && top.steps == 559u && top.peak == 18446744073709551412ull && top.guard_ok;
  CHECK(top_ok);
  // every odd start in T - 66 .. T + 4095 overflows (as does the even number after each), and every odd start just below is looked at against the oracle
  bool above = true;
  for (u64 n = kT - 66u; n <= kT + 4095u; n += 2u) {
    const Rec r = call(n);
    if (r.code != 3u || !r.guard_ok) above = false;
  }
  CHECK(above);
  bool below = true;
  for (u64 n = kT - 4096u; n < kT - 66u; ++n) {
    if (!same(call(n), oracle_shortcut(n))) below = false;
  }
  CHECK(below);
}

TEST_CASE("collatz digest set: edge window", "[collatz]") {
  const SetStat s = run_set(edge_set());
  const bool ok = s.all_ok && s.digest == kDigestEdge && s.ok == kCountEdgeOk && s.overflow == kCountEdgeOverflow && s.total == kCountEdgeTotal &&
                  s.max_steps == kMaxStepsEdge;
  CHECK(ok);
}
TEST_CASE("collatz digest set: doubled window", "[collatz]") {
  const SetStat s = run_set(halving_set());
  const bool ok = s.all_ok && s.digest == kDigestHalving && s.ok == kCountHalvingOk && s.overflow == kCountHalvingOverflow && s.total == kCountHalvingTotal &&
                  s.max_steps == kMaxStepsHalving;
  CHECK(ok);
}
TEST_CASE("collatz digest set: shifted values", "[collatz]") {
  const SetStat s = run_set(shifted_set());
  const bool ok = s.all_ok && s.digest == kDigestShifted && s.ok == kCountShiftedOk && s.overflow == kCountShiftedOverflow && s.total == kCountShiftedTotal &&
                  s.max_steps == kMaxStepsShifted;
  CHECK(ok);
}
TEST_CASE("collatz digest set: seeded bit-length classes", "[collatz]") {
  const SetStat s = run_set(random_set());
  const bool ok = s.all_ok && s.digest == kDigestRandom && s.ok == kCountRandomOk && s.overflow == kCountRandomOverflow && s.total == kCountRandomTotal &&
                  s.max_steps == kMaxStepsRandom;
  CHECK(ok);
}

TEST_CASE("collatz digest set: 200000 seeded uniform u64", "[collatz]") {
  const SetStat s = run_set(uniform_set());
  const bool ok = s.all_ok && s.digest == kDigestUniform && s.ok == kCountUniformOk && s.overflow == kCountUniformOverflow && s.total == kCountUniformTotal &&
                  s.max_steps == kMaxStepsUniform;
  CHECK(ok);
}
TEST_CASE("collatz digest set: 200000 seeded values below 2^40", "[collatz]") {
  const SetStat s = run_set(below40_set());
  const bool ok = s.all_ok && s.digest == kDigestBelow40 && s.ok == kCountBelow40Ok && s.overflow == kCountBelow40Overflow && s.total == kCountBelow40Total &&
                  s.max_steps == kMaxStepsBelow40;
  CHECK(ok);
}
TEST_CASE("collatz digest set: varied first-overflow step", "[collatz]") {
  u32 counts[256];
  const std::vector<u64> v = first_overflow_set(counts);
  bool cnt_ok = true;
  for (u32 j = 1u; j < 256u; ++j) {
    const u32 want = (j <= kFoJmax) ? kFoCounts[j - 1u] : 0u;
    if (counts[j] != want) cnt_ok = false;
  }
  CHECK(cnt_ok);
  const SetStat s = run_set(v);
  const bool ok = s.all_ok && s.digest == kDigestFirstOverflow && s.ok == kCountFirstOverflowOk && s.overflow == kCountFirstOverflowOverflow &&
                  s.total == kCountFirstOverflowTotal;
  CHECK(ok);
}

TEST_CASE("collatz every n up to 2^24 against both oracles and the digests", "[collatz]") {
  // oracle 2: the inverse tree of every value up to 2^24 whose whole trajectory stays at or below 2^24
  std::vector<std::uint16_t> depth(kNbig + 1u, 0u);  // steps + 1, 0 = not in the tree
  std::vector<u32> tpk(kNbig + 1u, 0u);
  std::vector<u32> stack;
  depth[1] = 1u;
  tpk[1] = 1u;
  stack.push_back(1u);
  while (!stack.empty()) {
    const u32 m = stack.back();
    stack.pop_back();
    const u64 c1 = 2ull * m;
    if (c1 <= kNbig && depth[c1] == 0u) {
      depth[c1] = static_cast<std::uint16_t>(depth[m] + 1u);
      tpk[c1] = (c1 > tpk[m]) ? static_cast<u32>(c1) : tpk[m];
      stack.push_back(static_cast<u32>(c1));
    }
    if (m % 6u == 4u && (m - 1u) / 3u > 1u) {
      const u32 c2 = (m - 1u) / 3u;
      if (depth[c2] == 0u) {
        depth[c2] = static_cast<std::uint16_t>(depth[m] + 1u);
        tpk[c2] = (c2 > tpk[m]) ? c2 : tpk[m];
        stack.push_back(c2);
      }
    }
  }
  bool guard = true;
  bool sc = true;
  bool tree = true;
  bool inv = true;
  u64 h = kFnv0;
  u64 h22 = 0u;
  u32 max_steps = 0u;
  u32 max_steps22 = 0u;
  u64 in_tree = 0u;
  u64 in_tree22 = 0u;
  for (u64 n = 1u; n <= kNbig; ++n) {
    const Rec r = call(n);
    if (!r.guard_ok) guard = false;
    if (r.code != 0u) {
      guard = false;
      continue;
    }
    if (!same(r, oracle_shortcut(n))) sc = false;
    if (!invariants(n, r)) inv = false;
    const bool member = depth[n] != 0u;
    if (member != (r.peak <= kNbig)) tree = false;
    if (member) {
      ++in_tree;
      if (r.peak <= kNmax) ++in_tree22;
      if (static_cast<u64>(depth[n] - 1u) != r.steps || static_cast<u64>(tpk[n]) != r.peak) tree = false;
    }
    h = fold(fold(fold(fold(h, n), 0u), r.steps), r.peak);
    if (r.steps > max_steps) max_steps = static_cast<u32>(r.steps);
    if (n == kNmax) {
      h22 = h;
      max_steps22 = max_steps;
    }
  }
  CHECK(guard);
  CHECK(sc);
  CHECK(tree);
  CHECK(inv);
  const bool dig = (h22 == kDigest22) && (max_steps22 == kMaxSteps22) && (h == kDigest24) && (max_steps == kMaxSteps24);
  CHECK(dig);
  const bool nodes = (in_tree == kNodes24) && (in_tree22 == kNodes22);
  CHECK(nodes);
}
