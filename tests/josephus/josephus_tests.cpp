// Tests for core/josephus. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/josephus_ref.py, an independent Python program (the literal circle
//      with list.pop, and the recurrence written independently with Python integers; compared with each other by the script). The tests re-run every
//      case against the real functions and recompute the 64-bit digest (n, k, the removed numbers, or n, k, step, value for sampled steps) and the count.
//  (2) ORACLES inside the test, sharing no code with core/josephus/josephus.cpp: a literal simulation (vector, erase the person at (pos + k - 1) mod size)
//      and an order-statistic simulation (Fenwick tree over the people still in the circle, O(n log n)), both producing the whole removal order.
//  (3) CLOSED FORMS and properties: k = 1 removes 0, 1, 2, .. in order (survivor n - 1); k = 2 leaves 2 * (n - 2^a) (2^a the largest power of two not above
//      n); the first removed is (k - 1) mod n; the removal order is a permutation whose last element is the survivor; the check-order matrix.
// After EVERY failure *out is compared with a sentinel (nothing is written).
// Extents (labelled): EXHAUSTIVE whole removal order against the vector oracle and the digest: n = 1..100 with every k = 1..300 (this contains n = 1..64
// with k = 1..2n + 1), and n = 1..40 with 14 larger k (2n, 2n + 1, 3n - 1, 3n, 3n + 1, 301, 1000, 2^16, 2^16 + 1, 2^31 - 1, 2^31, 2^31 + 1, 2^32 - 2,
// 2^32 - 1). WHOLE order against the Fenwick oracle and the digest: n = 1000, 4096, 4097 with 10 values of k (1, 2, 3, 7, n - 1, n, n + 1, 2n + 1, 2^31,
// 2^32 - 1). PERMUTATION property: all n = 1..300 and eight seeded n in 301..2048. WHOLE order at n = 65535, 65536, 65537 (one case per n, same 10 values of k) against the
// Fenwick oracle and the digest; SAMPLED steps against the Fenwick oracle and the digest at n = 2^20 - 1, 2^20, 2^20 + 1 (steps 0, 1, 2, n / 2, n - 2, n - 1). SAMPLED steps at the maximum: n = 2^24 (k = 1, 2, 3, 7, n - 1, n, n + 1, 2^31, 2^32 - 1) and n = 2^24 - 1 (k = 3, 7, n - 1,
// n, n + 1, 2^32 - 1), steps 0, 1, 2, n / 2, n - 2, n - 1, against the digest and the closed forms where they exist (k = 1, k = 2 survivor, step 0).
// The large cases carry the tag [big]. What is NOT done: Windows is CI-only; no timing assertion; the WHOLE removal order is not checked at n above 65537 (it costs
// n^2 / 2 steps of the library: 2 * 10^9 at n = 65537, about 12 s per k, so 2^20 is sampled); no Fenwick oracle at 2^24; no other n at 2^24 than the two listed.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "josephus.h"

using namespace lumora::josephus;

// BEGIN GENERATED REFERENCE (tools/josephus_ref.py)
struct RefRow {
  const char* name;
  std::size_t cases;
  std::uint64_t digest;
};
constexpr RefRow kRef[] = {
    {"ex_a", 1515000, 0x8CD08BD914D1C1B5ull},
    {"ex_b", 11476, 0xE43E0F13A40DEDF4ull},
    {"fen_s", 91930, 0xC26D598C052ACCBBull},
    {"whole_a", 655350, 0x0E9D8131848036CFull},
    {"whole_b", 655360, 0x35991B2E9D11350Eull},
    {"whole_c", 655370, 0x6A586FBE79D9270Bull},
    {"fen_l", 180, 0xF0AB23F76E94C5CEull},
    {"max_a", 54, 0x33A83BAAA972FD4Eull},
    {"max_b", 36, 0x407F3A164EFC3EF7ull},
};
// END GENERATED REFERENCE

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Vec = std::vector<u32>;

constexpr u32 kSent = 0xA5A5A5A5u;
constexpr u32 kMax32 = 0xFFFFFFFFu;

struct Lcg {
  u64 x;
  explicit Lcg(u64 seed) : x(seed) {}
  u32 next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(x >> 32);
  }
};

struct Fold {
  u64 h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  void one(u64 v) { h = (h ^ v) * 0x100000001B3ull; }
};

struct Acc {
  bool ok = true;
  std::string first;
  template <class A = int, class B = int, class C = int>
  void expect(bool cond, const char* what, A a = 0, B b = 0, C c = 0) {
    if (!cond && ok) {
      ok = false;
      first = std::string(what) + " (" + std::to_string(static_cast<u64>(a)) + ", " + std::to_string(static_cast<u64>(b)) + ", " + std::to_string(static_cast<u64>(c)) + ")";
    }
  }
};

void finish(const Acc& acc) {
  INFO(acc.first);
  CHECK(acc.ok);
}

void check_row(const char* name, const Fold& f) {
  bool found = false;
  for (const RefRow& row : kRef) {
    if (std::string(row.name) != name) continue;
    found = true;
    INFO(name);
    CHECK(f.cases == row.cases);
    CHECK(f.h == row.digest);
  }
  CHECK(found);
}

// ---- calls with the sentinel -----------------------------------------------------------------------------------------------------------------

u32 surv(Acc& acc, u32 n, u32 k) {
  u32 out = kSent;
  const JosResult r = josephus_survivor(n, k, &out);
  acc.expect(r == JosResult::Ok, "survivor result", n, k);
  return out;
}

u32 rem(Acc& acc, u32 n, u32 k, u32 step) {
  u32 out = kSent;
  const JosResult r = josephus_removed(n, k, step, &out);
  acc.expect(r == JosResult::Ok, "removed result", n, k, step);
  return out;
}

// ---- ORACLES (no shared code with the library) -----------------------------------------------------------------------------------------------

// The literal circle: a vector of the people; the count moves k - 1 places from the current index; that person leaves; the count continues at the same
// index (the next person).
Vec sim_order(u32 n, u32 k) {
  Vec people(n);
  for (u32 i = 0; i < n; ++i) people[i] = i;
  Vec order;
  order.reserve(n);
  u64 pos = 0;
  while (!people.empty()) {
    pos = (pos + k - 1u) % people.size();
    order.push_back(people[pos]);
    people.erase(people.begin() + static_cast<std::ptrdiff_t>(pos));
  }
  return order;
}

// The same circle with an order-statistic tree: a Fenwick tree counts the people still present; the person of rank pos is found by descent.
Vec fen_order(u32 n, u32 k) {
  Vec tree(static_cast<std::size_t>(n) + 1u, 0u);
  for (u32 i = 1; i <= n; ++i) {
    tree[i] += 1u;
    const u32 j = i + (i & (~i + 1u));
    if (j <= n) tree[j] += tree[i];
  }
  u32 top = 1;
  while (top * 2u <= n) top *= 2u;
  Vec order;
  order.reserve(n);
  u64 pos = 0;
  for (u64 size = n; size > 0; --size) {
    pos = (pos + k - 1u) % size;
    u32 want = static_cast<u32>(pos) + 1u;
    u32 at = 0;
    for (u32 step = top; step > 0; step >>= 1) {
      if (at + step <= n && tree[at + step] < want) {
        at += step;
        want -= tree[at];
      }
    }
    order.push_back(at);  // 1-based rank at + 1 is the person number at
    for (u32 i = at + 1u; i <= n; i += (i & (~i + 1u))) tree[i] -= 1u;
  }
  return order;
}

u32 two_power_not_above(u32 n) {
  u32 a = 1;
  while (a <= n / 2u) a *= 2u;
  return a;
}

u32 k2_survivor(u32 n) { return 2u * (n - two_power_not_above(n)); }

Vec kl(u32 n) {
  Vec out;
  for (const u64 v : {u64{1}, u64{2}, u64{3}, u64{7}, u64{n} - 1u, u64{n}, u64{n} + 1u, 2u * u64{n} + 1u, u64{1} << 31, u64{kMax32}}) {
    bool seen = false;
    for (const u32 w : out) seen = seen || w == v;
    if (v >= 1 && v <= kMax32 && !seen) out.push_back(static_cast<u32>(v));
  }
  return out;
}

Vec kb(u32 n) {
  Vec out;
  const u64 cand[] = {2u * u64{n}, 2u * u64{n} + 1u, 3u * u64{n} - 1u, 3u * u64{n}, 3u * u64{n} + 1u, 301, 1000, 65536, 65537, (u64{1} << 31) - 1u, u64{1} << 31, (u64{1} << 31) + 1u, u64{kMax32} - 1u, u64{kMax32}};
  for (const u64 v : cand) {
    bool seen = false;
    for (const u32 w : out) seen = seen || w == v;
    if (v >= 1 && v <= kMax32 && !seen) out.push_back(static_cast<u32>(v));
  }
  for (std::size_t i = 0; i < out.size(); ++i)
    for (std::size_t j = i + 1; j < out.size(); ++j)
      if (out[j] < out[i]) {
        const u32 t = out[i];
        out[i] = out[j];
        out[j] = t;
      }
  return out;
}

Vec kx(u32 n) {
  if (n == kMaxN) return {1u, 2u, 3u, 7u, n - 1u, n, n + 1u, 1u << 31, kMax32};
  return {3u, 7u, n - 1u, n, n + 1u, kMax32};
}

// The whole removal order of the library, one removed() call per step; compared with the oracle order; folded into f.
void whole_case(Acc& acc, Fold& f, u32 n, u32 k, const Vec& oracle) {
  f.one(n);
  f.one(k);
  for (u32 s = 0; s < n; ++s) {
    const u32 got = rem(acc, n, k, s);
    acc.expect(got == oracle[s], "removed vs oracle", n, k, s);
    f.one(got);
    ++f.cases;
  }
  acc.expect(surv(acc, n, k) == oracle[n - 1u], "survivor vs oracle", n, k);
}

void sampled_case(Acc& acc, Fold& f, u32 n, u32 k, u32 s, const Vec& oracle) {
  const u32 got = rem(acc, n, k, s);
  acc.expect(got == oracle[s], "sampled removed vs oracle", n, k, s);
  f.one(n);
  f.one(k);
  f.one(s);
  f.one(got);
  ++f.cases;
}

// ---- the failure matrix: the check order of the header, restated ------------------------------------------------------------------------------

JosResult expected_code(bool has_out, u32 n, u32 k, u32 step, bool removed) {
  if (!has_out) return JosResult::NullArgument;
  if (n == 0 || k == 0) return JosResult::BadArgument;
  if (n > kMaxN) return JosResult::TooLarge;
  if (removed && step >= n) return JosResult::OutOfRange;
  return JosResult::Ok;
}

}  // namespace

TEST_CASE("josephus: hand rows, the closed forms k = 1 and k = 2, the first removed, survivor = last removed", "[josephus]") {
  Acc acc;
  const auto order_is = [&acc](u32 n, u32 k, const Vec& expect, const char* what) {
    for (u32 s = 0; s < n; ++s) acc.expect(rem(acc, n, k, s) == expect[s], what, n, k, s);
    acc.expect(surv(acc, n, k) == expect[n - 1u], what, n, k);
  };
  // The example of the header: k = 2, n = 5 removes 1, 3, 0, 4 and leaves 2.
  order_is(5, 2, {1u, 3u, 0u, 4u, 2u}, "header example");
  // The classic n = 7, k = 3 (1-based 3, 6, 2, 7, 5, 1, 4) and n = 41, k = 3 (the survivor is the 31st person, 30 here).
  order_is(7, 3, {2u, 5u, 1u, 6u, 4u, 0u, 3u}, "n = 7, k = 3");
  acc.expect(surv(acc, 41, 3) == 30u, "n = 41, k = 3");
  order_is(2, 2, {1u, 0u}, "n = 2, k = 2");
  order_is(2, 1, {0u, 1u}, "n = 2, k = 1");
  order_is(6, 4, {3u, 1u, 0u, 2u, 5u, 4u}, "n = 6, k = 4");
  order_is(10, 7, {6u, 3u, 1u, 0u, 2u, 5u, 9u, 4u, 7u, 8u}, "n = 10, k = 7");
  order_is(3, kMax32, {2u, 0u, 1u}, "n = 3, k = UINT32_MAX");
  order_is(4, 1u << 31, {3u, 1u, 0u, 2u}, "n = 4, k = 2^31");
  // n = 1: 0 for every k.
  for (const u32 k : {1u, 2u, 3u, 1000u, 1u << 31, kMax32}) {
    acc.expect(surv(acc, 1, k) == 0u, "n = 1 survivor", k);
    acc.expect(rem(acc, 1, k, 0) == 0u, "n = 1 removed", k);
  }
  // k = 1: removes 0, 1, 2, .. ; the survivor is n - 1. k = 2: the survivor is 2 * (n - 2^a).
  for (u32 n = 1; n <= 3000; ++n) {
    acc.expect(surv(acc, n, 1) == n - 1u, "k = 1 survivor", n);
    acc.expect(surv(acc, n, 2) == k2_survivor(n), "k = 2 survivor", n);
  }
  for (u32 n = 1; n <= 150; ++n)
    for (u32 s = 0; s < n; ++s) acc.expect(rem(acc, n, 1, s) == s, "k = 1 removed", n, s);
  // The first removed is (k - 1) mod n; the survivor is the last removed (all n up to 300, many k).
  for (u32 n = 1; n <= 300; ++n)
    for (const u32 k : kl(n)) {
      acc.expect(rem(acc, n, k, 0) == (k - 1u) % n, "first removed", n, k);
      acc.expect(rem(acc, n, k, n - 1u) == surv(acc, n, k), "survivor is the last removed", n, k);
    }
  finish(acc);
}

TEST_CASE("josephus: every whole removal order for n = 1..100, k = 1..300 against the vector oracle and the digest", "[josephus]") {
  Acc acc;
  Fold f;
  for (u32 n = 1; n <= 100; ++n)
    for (u32 k = 1; k <= 300; ++k) whole_case(acc, f, n, k, sim_order(n, k));
  finish(acc);
  check_row("ex_a", f);
}

TEST_CASE("josephus: every whole removal order for n = 1..40 and the 14 larger k against the vector oracle and the digest", "[josephus]") {
  Acc acc;
  Fold f;
  for (u32 n = 1; n <= 40; ++n)
    for (const u32 k : kb(n)) whole_case(acc, f, n, k, sim_order(n, k));
  finish(acc);
  check_row("ex_b", f);
}

TEST_CASE("josephus: the removal order is a permutation ending at the survivor", "[josephus]") {
  Acc acc;
  Vec ns;
  for (u32 n = 1; n <= 300; ++n) ns.push_back(n);
  Lcg g(8800);
  for (int i = 0; i < 8; ++i) ns.push_back(301u + g.next() % 1748u);
  for (const u32 n : ns) {
    Vec ks = kl(n);
    ks.push_back(4);
    ks.push_back(5);
    ks.push_back(6);
    for (const u32 k : ks) {
      std::vector<char> seen(n, 0);
      u32 last = 0;
      for (u32 s = 0; s < n; ++s) {
        last = rem(acc, n, k, s);
        acc.expect(last < n, "removed below n", n, k, s);
        if (last < n) {
          acc.expect(seen[last] == 0, "removed twice", n, k, s);
          seen[last] = 1;
        }
      }
      acc.expect(last == surv(acc, n, k), "last removed is the survivor", n, k);
    }
  }
  finish(acc);
}

TEST_CASE("josephus: whole removal order at n = 1000, 4096, 4097 against the Fenwick oracle and the digest", "[josephus]") {
  Acc acc;
  Fold f;
  for (const u32 n : {1000u, 4096u, 4097u})
    for (const u32 k : kl(n)) whole_case(acc, f, n, k, fen_order(n, k));
  finish(acc);
  check_row("fen_s", f);
}

TEST_CASE("josephus: the two oracles agree (vector and Fenwick) on a grid", "[josephus]") {
  Acc acc;
  for (u32 n = 1; n <= 64; ++n)
    for (const u32 k : {1u, 2u, 3u, 5u, 64u, 65u, 1000u, kMax32}) acc.expect(sim_order(n, k) == fen_order(n, k), "oracles", n, k);
  finish(acc);
}

TEST_CASE("josephus: whole removal order at n = 65535 against the Fenwick oracle and the digest", "[josephus][big][whole]") {
  Acc acc;
  Fold f;
  for (const u32 k : kl(65535u)) whole_case(acc, f, 65535u, k, fen_order(65535u, k));
  finish(acc);
  check_row("whole_a", f);
}

TEST_CASE("josephus: whole removal order at n = 65536 against the Fenwick oracle and the digest", "[josephus][big][whole]") {
  Acc acc;
  Fold f;
  for (const u32 k : kl(65536u)) whole_case(acc, f, 65536u, k, fen_order(65536u, k));
  finish(acc);
  check_row("whole_b", f);
}

TEST_CASE("josephus: whole removal order at n = 65537 against the Fenwick oracle and the digest", "[josephus][big][whole]") {
  Acc acc;
  Fold f;
  for (const u32 k : kl(65537u)) whole_case(acc, f, 65537u, k, fen_order(65537u, k));
  finish(acc);
  check_row("whole_c", f);
}

TEST_CASE("josephus: n = 2^20 - 1, 2^20, 2^20 + 1, sampled steps against the Fenwick oracle and the digest", "[josephus][big]") {
  Acc acc;
  Fold f;
  for (const u32 n : {(1u << 20) - 1u, 1u << 20, (1u << 20) + 1u})
    for (const u32 k : kl(n)) {
      const Vec oracle = fen_order(n, k);
      for (const u32 s : {0u, 1u, 2u, n / 2u, n - 2u, n - 1u}) sampled_case(acc, f, n, k, s, oracle);
    }
  finish(acc);
  check_row("fen_l", f);
}

namespace {

// n = 2^24 or 2^24 - 1: the digest and the closed forms.
void max_case(Acc& acc, Fold& f, u32 n) {
  for (const u32 k : kx(n)) {
    for (const u32 s : {0u, 1u, 2u, n / 2u, n - 2u, n - 1u}) {
      const u32 got = rem(acc, n, k, s);
      f.one(n);
      f.one(k);
      f.one(s);
      f.one(got);
      ++f.cases;
      if (s == 0u) acc.expect(got == (k - 1u) % n, "first removed", n, k);
      if (k == 1u) acc.expect(got == s, "k = 1 removed", n, s);
      if (k == 2u && s == n - 1u) acc.expect(got == k2_survivor(n), "k = 2 survivor", n);
    }
    const u32 sv = surv(acc, n, k);
    acc.expect(sv == rem(acc, n, k, n - 1u), "survivor is the last removed", n, k);
    if (k == 1u) acc.expect(sv == n - 1u, "k = 1 survivor", n);
    if (k == 2u) acc.expect(sv == k2_survivor(n), "k = 2 survivor (survivor call)", n);
  }
}

}  // namespace

TEST_CASE("josephus: n = 2^24, sampled steps against the digest and the closed forms", "[josephus][big]") {
  Acc acc;
  Fold f;
  max_case(acc, f, kMaxN);
  finish(acc);
  check_row("max_a", f);
}

TEST_CASE("josephus: n = 2^24 - 1, sampled steps against the digest and the closed forms", "[josephus][big]") {
  Acc acc;
  Fold f;
  max_case(acc, f, kMaxN - 1u);
  finish(acc);
  check_row("max_b", f);
}

TEST_CASE("josephus: failures, check order and *out untouched", "[josephus]") {
  Acc acc;
  const u32 ns[] = {0u, 1u, 2u, 5u, kMaxN, kMaxN + 1u, kMax32};
  const u32 ks[] = {0u, 1u, 3u, kMax32};
  const u32 steps[] = {0u, 1u, 5u, kMaxN - 1u, kMaxN, kMax32};
  for (const u32 n : ns)
    for (const u32 k : ks)
      for (const u32 step : steps)
        for (const bool has_out : {false, true}) {
          // survivor ignores step
          {
            u32 out = kSent;
            const JosResult want = expected_code(has_out, n, k, step, false);
            if (want != JosResult::Ok || n <= 5) {
              const JosResult r = josephus_survivor(n, k, has_out ? &out : nullptr);
              acc.expect(r == want, "survivor code", n, k, step);
              if (want != JosResult::Ok) acc.expect(out == kSent, "survivor wrote on failure", n, k, step);
              else acc.expect(out == sim_order(n, k)[n - 1u], "survivor value", n, k, step);
            }
          }
          {
            u32 out = kSent;
            const JosResult want = expected_code(has_out, n, k, step, true);
            if (want != JosResult::Ok || n <= 5) {
              const JosResult r = josephus_removed(n, k, step, has_out ? &out : nullptr);
              acc.expect(r == want, "removed code", n, k, step);
              if (want != JosResult::Ok) acc.expect(out == kSent, "removed wrote on failure", n, k, step);
              else acc.expect(out == sim_order(n, k)[step], "removed value", n, k, step);
            }
          }
        }
  // The named precedences, by hand.
  u32 out = kSent;
  CHECK(josephus_survivor(kMaxN + 1u, 0, &out) == JosResult::BadArgument);
  CHECK(josephus_removed(kMaxN + 1u, 0, kMax32, &out) == JosResult::BadArgument);
  CHECK(josephus_removed(kMaxN + 1u, 1, kMax32, &out) == JosResult::TooLarge);
  CHECK(josephus_removed(0, 0, 0, &out) == JosResult::BadArgument);
  CHECK(josephus_removed(0, 5, kMax32, nullptr) == JosResult::NullArgument);
  CHECK(josephus_removed(1, 5, 1, &out) == JosResult::OutOfRange);
  CHECK(josephus_removed(1, 5, 0, &out) == JosResult::Ok);
  CHECK(out == 0u);
  out = kSent;
  CHECK(josephus_removed(5, 2, 5, &out) == JosResult::OutOfRange);
  CHECK(josephus_removed(5, 2, 4, &out) == JosResult::Ok);
  CHECK(out == 2u);
  out = kSent;
  CHECK(josephus_removed(kMaxN, 7, kMaxN, &out) == JosResult::OutOfRange);
  CHECK(out == kSent);
  finish(acc);
}
