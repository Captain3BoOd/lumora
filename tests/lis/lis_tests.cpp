// Tests for core/lis. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/lis_ref.py, an independent Python program (every index subset,
//      a quadratic dynamic programme and a Fenwick-tree method, compared with each other by the script, with exact integers). The tests re-run every
//      sequence against the real functions and recompute the 64-bit digest (n, L, the count modulo 2^64, the canonical index list) and the number
//      of sequences.
//  (2) ORACLE inside the test, sharing no code with core/lis/lis.cpp: every index subset for sequences up to 12 elements, the quadratic dynamic
//      programme with the canonical list found by plain scans up to 400 elements, and a Fenwick tree over the value ranks (longest length and
//      number of ways, wrapping modulo 2^64) with patience tails for the starts for the large sequences.
//  (3) CONSTANTS: the hand rows, the true counts 2^63, 2^64 (residue 0) and 3 * 2^63 (residue 2^63), the closed forms at the limit and the
//      check-order matrix, written from the header.
// Output buffers carry sentinels: out_idx has 8 entries before and 16 after, and the entries L .. n - 1 must stay unchanged on success; out_len and
// out_count must stay unchanged on failure. Scratch contents are never examined.
// Extents: sequences over {0, 1, 2} up to length 10, over four values (0, 1, 0x80000000, 0xFFFFFFFF) up to length 8, every permutation of 0 .. n - 1
// for n up to 8 and the empty, one-element and two-element sequences over 0 .. 255 are EXHAUSTIVE; 600 sequences of length 17 to 400 per alphabet are
// fixed-seed SAMPLES; the large sequences are the listed patterns (SAMPLED patterns). No timing assertion: the step bound is argued in docs/lis.md.
// What is NOT done: Windows is CI-only; nothing is asserted about scratch contents.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lis.h"

using namespace lumora::lis;

// BEGIN GENERATED REFERENCE (tools/lis_ref.py)
const std::uint64_t kRefDigest_all256 = 0xAC5E90FD3BC51D84ULL;
const std::size_t kRefCases_all256 = 65793;
const std::uint64_t kRefDigest_big_alt01_262144 = 0xACA0E9C823A16E7AULL;
const std::size_t kRefCases_big_alt01_262144 = 1;
const std::uint64_t kRefDigest_big_altext_262144 = 0x49EA399CCA94906AULL;
const std::size_t kRefCases_big_altext_262144 = 1;
const std::uint64_t kRefDigest_big_const0_262144 = 0x5C9D544AFE86154AULL;
const std::size_t kRefCases_big_const0_262144 = 1;
const std::uint64_t kRefDigest_big_constmax_262143 = 0x157DC17D8560EEB2ULL;
const std::size_t kRefCases_big_constmax_262143 = 1;
const std::uint64_t kRefDigest_big_dec_262144 = 0x5C9D544AFE86154AULL;
const std::size_t kRefCases_big_dec_262144 = 1;
const std::uint64_t kRefDigest_big_dup2_262144 = 0xBDA59DF3E7612FB7ULL;
const std::size_t kRefCases_big_dup2_262144 = 1;
const std::uint64_t kRefDigest_big_dup3_262143 = 0x985F00FA56B87F9AULL;
const std::size_t kRefCases_big_dup3_262143 = 1;
const std::uint64_t kRefDigest_big_inc_262143 = 0x0B708F92DB35AE9DULL;
const std::size_t kRefCases_big_inc_262143 = 1;
const std::uint64_t kRefDigest_big_inc_262144 = 0x00F29D015EB72E04ULL;
const std::size_t kRefCases_big_inc_262144 = 1;
const std::uint64_t kRefDigest_big_pairs_262144 = 0xBDA59DF3E7612FB7ULL;
const std::size_t kRefCases_big_pairs_262144 = 1;
const std::uint64_t kRefDigest_big_perm_262144 = 0x396B8B65DE844B9CULL;
const std::size_t kRefCases_big_perm_262144 = 1;
const std::uint64_t kRefDigest_big_rand16_262144 = 0xCDDF6D600668B6D7ULL;
const std::size_t kRefCases_big_rand16_262144 = 1;
const std::uint64_t kRefDigest_big_rand2_262144 = 0x91EFB6498B857FA3ULL;
const std::size_t kRefCases_big_rand2_262144 = 1;
const std::uint64_t kRefDigest_big_rand32_1000 = 0x222B254180F10312ULL;
const std::size_t kRefCases_big_rand32_1000 = 1;
const std::uint64_t kRefDigest_big_rand32_262143 = 0xB5187CA8A77A3C2EULL;
const std::size_t kRefCases_big_rand32_262143 = 1;
const std::uint64_t kRefDigest_big_rand32_262144 = 0x6513B11CDC8C660CULL;
const std::size_t kRefCases_big_rand32_262144 = 1;
const std::uint64_t kRefDigest_big_saw1000_262144 = 0x2FE58CAB4E2269EBULL;
const std::size_t kRefCases_big_saw1000_262144 = 1;
const std::uint64_t kRefDigest_big_saw512_262142 = 0x188DC1F8F908A490ULL;
const std::size_t kRefCases_big_saw512_262142 = 1;
const std::uint64_t kRefDigest_ex3 = 0xF34796687373D663ULL;
const std::size_t kRefCases_ex3 = 9841;
const std::uint64_t kRefDigest_ex4 = 0x51E1CED57F61AEFEULL;
const std::size_t kRefCases_ex4 = 5461;
const std::uint64_t kRefDigest_pairs63 = 0x8AC4E09F482B9A04ULL;
const std::size_t kRefCases_pairs63 = 1;
const std::uint64_t kRefDigest_pairs64 = 0x33E5A4B36F78EF77ULL;
const std::size_t kRefCases_pairs64 = 1;
const std::uint64_t kRefDigest_pairs65 = 0x6539151A57A884ACULL;
const std::size_t kRefCases_pairs65 = 1;
const std::uint64_t kRefDigest_perm = 0x81DB1D1832E20D33ULL;
const std::size_t kRefCases_perm = 5914;
const std::uint64_t kRefDigest_rs16 = 0xE6E62B4FB4253DB5ULL;
const std::size_t kRefCases_rs16 = 600;
const std::uint64_t kRefDigest_rs32 = 0x4A002666FE9493EBULL;
const std::size_t kRefCases_rs32 = 600;
const std::uint64_t kRefDigest_rs4 = 0xDEE58BA7A58ECACBULL;
const std::size_t kRefCases_rs4 = 600;
const std::uint64_t kRefDigest_wrap3 = 0x6BE3070B20C8C2CCULL;
const std::size_t kRefCases_wrap3 = 1;
// END GENERATED REFERENCE

namespace {

using Seq = std::vector<std::uint32_t>;
constexpr std::uint32_t kSent = 0xA5A5A5A5u;
constexpr std::uint64_t kSent64 = 0xA5A5A5A5A5A5A5A5ull;
constexpr std::size_t kGb = 8, kGa = 16;

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  void add(std::uint64_t v) { h = (h ^ v) * 0x100000001B3ull; }
};

struct Tally {
  std::size_t bad = 0;
  std::string first;
  void fail(const std::string& s) {
    if (bad == 0) first = s;
    ++bad;
  }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

struct Guarded {
  std::vector<std::uint32_t> buf;
  explicit Guarded(std::size_t n) : buf(kGb + n + kGa, kSent) {}
  std::uint32_t* p() { return buf.data() + kGb; }
  bool guards_ok(std::size_t n) const {
    for (std::size_t i = 0; i < kGb; ++i)
      if (buf[i] != kSent) return false;
    for (std::size_t i = 0; i < kGa; ++i)
      if (buf[kGb + n + i] != kSent) return false;
    return true;
  }
  bool untouched() const {
    for (std::uint32_t v : buf)
      if (v != kSent) return false;
    return true;
  }
};

// ---------------------------------------------------------------- the oracle (no code shared with the implementation)

struct Answer {
  std::uint32_t len = 0;
  std::uint64_t count = 1;
  std::vector<std::uint32_t> idx;
};

// Every index subset (n <= 12): the longest strictly increasing ones, how many and the lexicographically smallest index list.
Answer oracle_subsets(const Seq& a) {
  const std::size_t n = a.size();
  Answer best;
  best.len = 0;
  best.count = 0;
  bool have = false;
  for (std::uint32_t mask = 0; mask < (1u << n); ++mask) {
    std::vector<std::uint32_t> idx;
    for (std::uint32_t i = 0; i < n; ++i)
      if ((mask >> i) & 1u) idx.push_back(i);
    bool ok = true;
    for (std::size_t k = 0; k + 1 < idx.size(); ++k) ok = ok && a[idx[k]] < a[idx[k + 1]];
    if (!ok) continue;
    if (!have || idx.size() > best.len) {
      have = true;
      best.len = static_cast<std::uint32_t>(idx.size());
      best.count = 1;
      best.idx = idx;
    } else if (idx.size() == best.len) {
      ++best.count;
      if (idx < best.idx) best.idx = idx;
    }
  }
  return best;
}

// The quadratic dynamic programme (n <= 400); the canonical list by plain scans with the header's rule.
Answer oracle_quad(const Seq& a) {
  const std::size_t n = a.size();
  Answer r;
  if (n == 0) return r;
  std::vector<std::uint32_t> L(n, 1), R(n, 1);
  std::vector<std::uint64_t> C(n, 1);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < i; ++j)
      if (a[j] < a[i]) {
        if (L[j] + 1 > L[i]) {
          L[i] = L[j] + 1;
          C[i] = C[j];
        } else if (L[j] + 1 == L[i]) {
          C[i] += C[j];
        }
      }
  for (std::size_t i = n; i-- > 0;)
    for (std::size_t j = i + 1; j < n; ++j)
      if (a[j] > a[i] && R[j] + 1 > R[i]) R[i] = R[j] + 1;
  r.len = *std::max_element(L.begin(), L.end());
  r.count = 0;
  for (std::size_t i = 0; i < n; ++i)
    if (L[i] == r.len) r.count += C[i];
  std::uint32_t need = r.len;
  bool first = true;
  std::size_t prev = 0;
  for (std::size_t from = 0; need > 0; --need) {
    for (std::size_t j = from; j < n; ++j) {
      if (R[j] == need && (first || a[j] > a[prev])) {
        r.idx.push_back(static_cast<std::uint32_t>(j));
        prev = j;
        first = false;
        from = j + 1;
        break;
      }
    }
  }
  return r;
}

// Fenwick tree over the value ranks (longest length, number of ways) for the large sequences; the starts by patience tails on negated values.
Answer oracle_fenwick(const Seq& a) {
  const std::size_t n = a.size();
  Answer r;
  if (n == 0) return r;
  Seq sorted = a;
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  const std::size_t m = sorted.size();
  std::vector<std::uint32_t> bl(m + 1, 0);
  std::vector<std::uint64_t> bc(m + 1, 0);
  std::uint32_t top = 0;
  std::uint64_t total = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t rank = static_cast<std::size_t>(std::lower_bound(sorted.begin(), sorted.end(), a[i]) - sorted.begin()) + 1;
    std::uint32_t ql = 0;
    std::uint64_t qc = 1;
    for (std::size_t q = rank - 1; q > 0; q -= q & (~q + 1)) {
      if (bl[q] > ql) {
        ql = bl[q];
        qc = bc[q];
      } else if (bl[q] == ql && ql > 0) {
        qc += bc[q];
      }
    }
    const std::uint32_t li = ql + 1;
    const std::uint64_t ci = qc;
    for (std::size_t q = rank; q <= m; q += q & (~q + 1)) {
      if (bl[q] < li) {
        bl[q] = li;
        bc[q] = ci;
      } else if (bl[q] == li) {
        bc[q] += ci;
      }
    }
    if (li > top) {
      top = li;
      total = ci;
    } else if (li == top) {
      total += ci;
    }
  }
  std::vector<std::int64_t> tails;
  std::vector<std::uint32_t> R(n);
  for (std::size_t i = n; i-- > 0;) {
    const std::int64_t key = -static_cast<std::int64_t>(a[i]);
    const std::size_t p = static_cast<std::size_t>(std::lower_bound(tails.begin(), tails.end(), key) - tails.begin());
    R[i] = static_cast<std::uint32_t>(p + 1);
    if (p == tails.size())
      tails.push_back(key);
    else
      tails[p] = key;
  }
  r.len = top;
  r.count = total;
  std::size_t j = 0, prev = 0;
  bool first = true;
  for (std::uint32_t need = top; need > 0; --need) {
    while (!(R[j] == need && (first || a[j] > a[prev]))) ++j;
    r.idx.push_back(static_cast<std::uint32_t>(j));
    prev = j;
    first = false;
    ++j;
  }
  return r;
}

Answer oracle(const Seq& a) {
  if (a.size() <= 12) return oracle_subsets(a);
  if (a.size() <= 400) return oracle_quad(a);
  return oracle_fenwick(a);
}

// Runs lis_run (with exactly 16 * n + 8 scratch words) on a, compares with the oracle and folds the values in the same order as tools/lis_ref.py.
void check_seq(const Seq& a, Fold& f, Tally& t) {
  const std::size_t n = a.size();
  Guarded idx(n);
  std::vector<std::uint32_t> scratch(16 * n + 8, 0x5A5A5A5Au);
  std::uint32_t len = kSent;
  std::uint64_t count = kSent64;
  const std::uint32_t dummy = 0;
  const std::uint32_t* ap = n > 0 ? a.data() : &dummy;
  if (lis_run(static_cast<std::uint32_t>(n), ap, scratch.data(), static_cast<std::uint32_t>(scratch.size()), &len, &count, idx.p()) != LisResult::Ok) {
    t.fail("lis_run not Ok");
    return;
  }
  if (!idx.guards_ok(n)) t.fail("guards of out_idx changed");
  for (std::size_t i = len; i < n; ++i)
    if (idx.p()[i] != kSent) {
      t.fail("out_idx entry at or after L was written");
      break;
    }
  const Answer o = oracle(a);
  if (len != o.len) t.fail("length differs, n = " + std::to_string(n));
  if (count != o.count) t.fail("count differs, n = " + std::to_string(n));
  if (len == o.len && !std::equal(o.idx.begin(), o.idx.end(), idx.p())) t.fail("index list differs, n = " + std::to_string(n));
  f.add(n);
  f.add(len);
  f.add(count);
  for (std::size_t i = 0; i < len && i < n; ++i) f.add(idx.p()[i]);
  ++f.cases;
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.h == digest);
}

// Every sequence of length 0 .. maxlen over `alpha` (by length, then the first value changes fastest): the same order as tools/lis_ref.py.
Fold run_exhaustive(const Seq& alpha, std::size_t maxlen, Tally& t, std::size_t& total) {
  Fold f;
  total = 0;
  const std::size_t k = alpha.size();
  std::size_t pw = 1;
  for (std::size_t L = 0; L <= maxlen; ++L) {
    for (std::size_t c = 0; c < pw; ++c) {
      Seq s(L);
      std::size_t x = c;
      for (std::size_t i = 0; i < L; ++i) {
        s[i] = alpha[x % k];
        x /= k;
      }
      check_seq(s, f, t);
      ++total;
    }
    pw *= k;
  }
  return f;
}

// ---------------------------------------------------------------- the large patterns (the same as tools/lis_ref.py)

Seq pairs(std::size_t k) {
  Seq s;
  for (std::size_t p = 0; p < k; ++p) {
    s.push_back(static_cast<std::uint32_t>(2 * p + 2));
    s.push_back(static_cast<std::uint32_t>(2 * p + 1));
  }
  return s;
}

Seq pat_inc(std::size_t n) {
  Seq s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<std::uint32_t>(i);
  return s;
}

Seq pat_dec(std::size_t n) {
  Seq s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<std::uint32_t>(n - 1 - i);
  return s;
}

Seq pat_const(std::size_t n, std::uint32_t v) { return Seq(n, v); }

Seq pat_alt(std::size_t n, std::uint32_t a, std::uint32_t b) {
  Seq s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = i % 2 == 0 ? a : b;
  return s;
}

Seq pat_saw(std::size_t n, std::size_t p) {
  Seq s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<std::uint32_t>(i % p);
  return s;
}

Seq pat_dup(std::size_t n, std::size_t k) {
  Seq s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<std::uint32_t>(i / k);
  return s;
}

Seq pat_rand(std::size_t n, std::uint64_t seed, std::uint32_t mod) {
  Lcg g(seed);
  Seq s(n);
  for (std::uint32_t& v : s) {
    const std::uint32_t d = g.next();
    v = mod ? d % mod : d;
  }
  return s;
}

Seq pat_perm(std::size_t n, std::uint64_t seed) {
  Lcg g(seed);
  Seq a = pat_inc(n);
  for (std::size_t i = n - 1; i > 0; --i) std::swap(a[i], a[g.next() % (i + 1)]);
  return a;
}

struct BigCase {
  const char* name;
  Seq (*make)();
  std::uint64_t digest;
  std::size_t cases;
};

#define BIG(NAME, EXPR) {#NAME, [] { return EXPR; }, kRefDigest_big_##NAME, kRefCases_big_##NAME}

}  // namespace

TEST_CASE("exhaustive: every sequence of length 0..8 over {0, 1, 2}", "[lis]") {
  Tally t;
  std::size_t total = 0;
  const Fold f = run_exhaustive({0, 1, 2}, 8, t, total);
  expect_digest(f, t, kRefCases_ex3, kRefDigest_ex3);
  CHECK(total == 9841u);
}

TEST_CASE("exhaustive: every sequence of length 0..6 over 0, 1, 0x80000000, 0xFFFFFFFF", "[lis]") {
  Tally t;
  std::size_t total = 0;
  const Fold f = run_exhaustive({0u, 1u, 0x80000000u, 0xFFFFFFFFu}, 6, t, total);
  expect_digest(f, t, kRefCases_ex4, kRefDigest_ex4);
  CHECK(total == 5461u);
}

TEST_CASE("exhaustive: every permutation of 0..n-1 for n = 0..7", "[lis]") {
  Tally t;
  Fold f;
  for (std::uint32_t n = 0; n <= 7; ++n) {
    Seq p = pat_inc(n);
    do {
      check_seq(p, f, t);
    } while (std::next_permutation(p.begin(), p.end()));
  }
  expect_digest(f, t, kRefCases_perm, kRefDigest_perm);
}

TEST_CASE("exhaustive: the empty sequence, all one-element and all two-element sequences over 0..255", "[lis]") {
  Tally t;
  Fold f;
  check_seq(Seq{}, f, t);
  for (std::uint32_t a = 0; a < 256; ++a) check_seq(Seq{a}, f, t);
  for (std::uint32_t c = 0; c < 65536; ++c) check_seq(Seq{c % 256, c / 256}, f, t);
  expect_digest(f, t, kRefCases_all256, kRefDigest_all256);
}

// Fixed-seed SAMPLES: 600 random sequences of length 17 .. 400 per alphabet (not exhaustive).
TEST_CASE("fixed-seed samples: 600 random sequences of length 17..400 over 4, 16 and 2^32 values (sampled, not exhaustive)", "[lis]") {
  struct G { const char* name; std::uint32_t mod; std::uint64_t seed; std::uint64_t digest; std::size_t cases; };
  const G groups[3] = {{"rs4", 4, 3004, kRefDigest_rs4, kRefCases_rs4},
                       {"rs16", 16, 3016, kRefDigest_rs16, kRefCases_rs16},
                       {"rs32", 0, 3032, kRefDigest_rs32, kRefCases_rs32}};
  for (const G& g : groups) {
    Lcg r(g.seed);
    Fold f;
    Tally t;
    for (int i = 0; i < 600; ++i) {
      const std::size_t n = 17 + r.next() % 384;
      Seq s(n);
      for (std::uint32_t& v : s) {
        const std::uint32_t d = r.next();
        v = g.mod ? d % g.mod : d;
      }
      check_seq(s, f, t);
    }
    INFO(g.name);
    expect_digest(f, t, g.cases, g.digest);
  }
}

TEST_CASE("true counts above 2^63: 63, 64 and 65 pairs (2^63, 2^64 -> 0, 2^65 -> 0) and 63 pairs with a triple (3 * 2^63 -> 2^63)", "[lis]") {
  struct C { const char* name; Seq s; std::uint32_t len; std::uint64_t count; std::uint64_t digest; std::size_t cases; };
  Seq w = pairs(63);
  w.insert(w.end(), {129u, 128u, 127u});
  const C cs[] = {{"63 pairs", pairs(63), 63, 0x8000000000000000ull, kRefDigest_pairs63, kRefCases_pairs63},
                  {"64 pairs", pairs(64), 64, 0ull, kRefDigest_pairs64, kRefCases_pairs64},
                  {"65 pairs", pairs(65), 65, 0ull, kRefDigest_pairs65, kRefCases_pairs65},
                  {"63 pairs and a triple", w, 64, 0x8000000000000000ull, kRefDigest_wrap3, kRefCases_wrap3}};
  for (const C& c : cs) {
    INFO(c.name);
    Fold f;
    Tally t;
    check_seq(c.s, f, t);
    expect_digest(f, t, c.cases, c.digest);
    const std::size_t n = c.s.size();
    Guarded idx(n);
    std::vector<std::uint32_t> scratch(16 * n + 8);
    std::uint32_t len = kSent;
    std::uint64_t count = kSent64;
    REQUIRE(lis_run(static_cast<std::uint32_t>(n), c.s.data(), scratch.data(), static_cast<std::uint32_t>(scratch.size()), &len, &count, idx.p()) ==
            LisResult::Ok);
    CHECK(len == c.len);
    CHECK(count == c.count);
    bool firsts = true;
    for (std::uint32_t p = 0; p < len; ++p) firsts = firsts && idx.p()[p] == 2 * p;  // the first element of every pair
    CHECK(firsts);
  }
}

TEST_CASE("large sequences: increasing, decreasing, constant, alternating, sawtooth, repeated values, pairs, random (sampled patterns)", "[lis]") {
  const BigCase cases[] = {
      BIG(inc_262144, pat_inc(262144)),
      BIG(inc_262143, pat_inc(262143)),
      BIG(dec_262144, pat_dec(262144)),
      BIG(const0_262144, pat_const(262144, 0)),
      BIG(constmax_262143, pat_const(262143, 0xFFFFFFFFu)),
      BIG(alt01_262144, pat_alt(262144, 0, 1)),
      BIG(altext_262144, pat_alt(262144, 0xFFFFFFFFu, 0)),
      BIG(saw1000_262144, pat_saw(262144, 1000)),
      BIG(saw512_262142, pat_saw(262142, 512)),
      BIG(dup2_262144, pat_dup(262144, 2)),
      BIG(dup3_262143, pat_dup(262143, 3)),
      BIG(pairs_262144, pairs(131072)),
      BIG(rand2_262144, pat_rand(262144, 5011, 2)),
      BIG(rand16_262144, pat_rand(262144, 5012, 16)),
      BIG(rand32_262144, pat_rand(262144, 5013, 0)),
      BIG(rand32_262143, pat_rand(262143, 5014, 0)),
      BIG(perm_262144, pat_perm(262144, 5015)),
      BIG(rand32_1000, pat_rand(1000, 5016, 0)),
  };
  for (const BigCase& c : cases) {
    const Seq s = c.make();
    Fold f;
    Tally t;
    check_seq(s, f, t);
    INFO(c.name);
    expect_digest(f, t, c.cases, c.digest);
  }
}

TEST_CASE("closed forms at the limit: increasing, decreasing, constant and 131072 pairs", "[lis]") {
  const std::uint32_t n = 262144;
  std::vector<std::uint32_t> scratch(16 * static_cast<std::size_t>(n) + 8);
  const std::uint32_t sw = static_cast<std::uint32_t>(scratch.size());
  {
    const Seq a = pat_inc(n);
    Guarded idx(n);
    std::uint32_t len = kSent;
    std::uint64_t count = kSent64;
    REQUIRE(lis_run(n, a.data(), scratch.data(), sw, &len, &count, idx.p()) == LisResult::Ok);
    CHECK(len == n);
    CHECK(count == 1u);
    bool all = true;
    for (std::uint32_t i = 0; i < n; ++i) all = all && idx.p()[i] == i;
    CHECK(all);
    CHECK(idx.guards_ok(n));
  }
  for (int kind = 0; kind < 2; ++kind) {
    const Seq a = kind == 0 ? pat_dec(n) : pat_const(n, 7u);
    Guarded idx(n);
    std::uint32_t len = kSent;
    std::uint64_t count = kSent64;
    REQUIRE(lis_run(n, a.data(), scratch.data(), sw, &len, &count, idx.p()) == LisResult::Ok);
    CHECK(len == 1u);
    CHECK(count == n);
    CHECK(idx.p()[0] == 0u);
    CHECK(idx.p()[1] == kSent);
    CHECK(idx.guards_ok(n));
  }
  {
    const Seq a = pairs(n / 2);
    Guarded idx(n);
    std::uint32_t len = kSent;
    std::uint64_t count = kSent64;
    REQUIRE(lis_run(n, a.data(), scratch.data(), sw, &len, &count, idx.p()) == LisResult::Ok);
    CHECK(len == n / 2);
    CHECK(count == 0u);  // 2^131072 is a multiple of 2^64
    bool firsts = true;
    for (std::uint32_t p = 0; p < n / 2; ++p) firsts = firsts && idx.p()[p] == 2 * p;
    CHECK(firsts);
  }
}

TEST_CASE("hand rows: ties, duplicates, the first index is not the smallest value, 0 and 0xFFFFFFFF", "[lis]") {
  struct Row { const char* name; Seq a; std::uint32_t len; std::uint64_t count; std::vector<std::uint32_t> idx; };
  const Row rows[] = {
      {"increasing", {1u, 2u, 3u, 4u}, 4, 1ull, {0, 1, 2, 3}},
      {"decreasing", {4u, 3u, 2u, 1u}, 1, 4ull, {0}},
      {"equal values", {5u, 5u, 5u}, 1, 3ull, {0}},
      {"one", {7u}, 1, 1ull, {0}},
      {"ties lexicographic", {1u, 3u, 2u, 4u}, 3, 2ull, {0, 1, 3}},
      {"first start not index 0", {9u, 1u, 2u, 3u}, 3, 1ull, {1, 2, 3}},
      {"later better start", {3u, 4u, 1u, 2u, 5u}, 3, 2ull, {0, 1, 4}},
      {"two longest same prefix", {1u, 2u, 4u, 3u}, 3, 2ull, {0, 1, 2}},
      {"duplicates chain not", {1u, 1u, 2u, 2u}, 2, 4ull, {0, 2}},
      {"0 and max", {0u, 4294967295u, 0u, 4294967295u}, 2, 3ull, {0, 1}},
      {"max then 0", {4294967295u, 0u, 4294967295u, 0u}, 2, 1ull, {1, 2}},
      {"sign boundary", {2147483647u, 2147483648u, 2147483647u, 2147483648u}, 2, 3ull, {0, 1}},
      {"plateau then rise", {2u, 2u, 2u, 3u}, 2, 3ull, {0, 3}},
      {"zigzag", {1u, 3u, 2u, 4u, 3u, 5u}, 4, 3ull, {0, 1, 3, 5}},
      {"count 6", {2u, 1u, 4u, 3u, 6u, 5u}, 3, 8ull, {0, 2, 4}},
      {"count 3 three choices", {3u, 2u, 1u}, 1, 3ull, {0}},
      {"long tail choice", {1u, 2u, 3u, 1u, 2u, 3u}, 3, 4ull, {0, 1, 2}},
      {"greedy trap", {5u, 6u, 1u, 2u, 3u, 4u}, 4, 1ull, {2, 3, 4, 5}},
      {"start later smaller index", {2u, 1u, 3u, 2u, 4u, 3u}, 3, 4ull, {0, 2, 4}},
      {"not the smallest value list", {3u, 1u, 2u, 0u, 4u}, 3, 1ull, {1, 2, 4}},
      {"late start with same length", {10u, 20u, 1u, 2u}, 2, 2ull, {0, 1}},
      {"equal heads", {4u, 4u, 5u, 6u, 5u, 6u}, 3, 6ull, {0, 2, 3}},
      {"down then up", {6u, 5u, 4u, 1u, 2u, 3u}, 3, 1ull, {3, 4, 5}},
      {"ten rising pairs", {1u, 0u, 3u, 2u, 5u, 4u, 7u, 6u, 9u, 8u}, 5, 32ull, {0, 2, 4, 6, 8}},
  };
  for (const Row& r : rows) {
    INFO(r.name);
    const std::size_t n = r.a.size();
    Guarded idx(n);
    std::vector<std::uint32_t> scratch(16 * n + 8);
    std::uint32_t len = kSent;
    std::uint64_t count = kSent64;
    REQUIRE(lis_run(static_cast<std::uint32_t>(n), r.a.data(), scratch.data(), static_cast<std::uint32_t>(scratch.size()), &len, &count, idx.p()) ==
            LisResult::Ok);
    CHECK(len == r.len);
    CHECK(count == r.count);
    REQUIRE(r.idx.size() == r.len);
    for (std::size_t i = 0; i < r.idx.size(); ++i) CHECK(idx.p()[i] == r.idx[i]);
    for (std::size_t i = r.len; i < n; ++i) CHECK(idx.p()[i] == kSent);
    CHECK(idx.guards_ok(n));
  }
}

TEST_CASE("scratch words, scratch size boundary and the n = 0 rules", "[lis]") {
  std::uint32_t w = kSent;
  CHECK(lis_scratch_words(0, &w) == LisResult::Ok);
  CHECK(w == 8u);
  CHECK(lis_scratch_words(1, &w) == LisResult::Ok);
  CHECK(w == 24u);
  CHECK(lis_scratch_words(5, &w) == LisResult::Ok);
  CHECK(w == 88u);
  CHECK(lis_scratch_words(kMaxLen, &w) == LisResult::Ok);
  CHECK(w == 16u * kMaxLen + 8u);
  const Seq a = {3, 1, 2, 5, 4};
  const std::uint32_t n = 5;
  std::vector<std::uint32_t> scratch(16 * n + 100);
  Guarded idx(n);
  std::uint32_t len = kSent;
  std::uint64_t count = kSent64;
  CHECK(lis_run(n, a.data(), scratch.data(), 16 * n + 7, &len, &count, idx.p()) == LisResult::ScratchTooSmall);
  CHECK(len == kSent);
  CHECK(count == kSent64);
  CHECK(idx.untouched());
  CHECK(lis_run(n, a.data(), scratch.data(), 16 * n + 8, &len, &count, idx.p()) == LisResult::Ok);
  CHECK(len == 3u);
  CHECK(count == 2u);
  CHECK(idx.p()[0] == 1u);
  CHECK(idx.p()[1] == 2u);
  CHECK(idx.p()[2] == 3u);
  len = kSent;
  count = kSent64;
  CHECK(lis_run(n, a.data(), scratch.data(), static_cast<std::uint32_t>(scratch.size()), &len, &count, idx.p()) == LisResult::Ok);  // more is accepted
  CHECK(len == 3u);
  // n = 0: length 0, count 1, nothing written to out_idx; a, scratch and out_idx may be null and the scratch size is not looked at
  len = kSent;
  count = kSent64;
  CHECK(lis_run(0, nullptr, nullptr, 0, &len, &count, nullptr) == LisResult::Ok);
  CHECK(len == 0u);
  CHECK(count == 1u);
  len = kSent;
  count = kSent64;
  Guarded none(0);
  CHECK(lis_run(0, a.data(), scratch.data(), 0, &len, &count, none.p()) == LisResult::Ok);
  CHECK(len == 0u);
  CHECK(count == 1u);
  CHECK(none.untouched());
}

TEST_CASE("check order matrix: null arguments, TooLarge, ScratchTooSmall; nothing written on failure", "[lis]") {
  const Seq a = {1, 2, 2, 1, 3};
  const std::uint32_t n = 5;
  std::vector<std::uint32_t> scratch(16 * n + 8, 7u);
  Guarded idx(n);
  std::uint32_t len = kSent;
  std::uint64_t count = kSent64;
  auto untouched = [&] { return len == kSent && count == kSent64 && idx.untouched(); };
  const std::uint32_t sw = 16 * n + 8;
  CHECK(lis_run(n, nullptr, scratch.data(), sw, &len, &count, idx.p()) == LisResult::NullArgument);
  CHECK(lis_run(n, a.data(), nullptr, sw, &len, &count, idx.p()) == LisResult::NullArgument);
  CHECK(lis_run(n, a.data(), scratch.data(), sw, nullptr, &count, idx.p()) == LisResult::NullArgument);
  CHECK(lis_run(n, a.data(), scratch.data(), sw, &len, nullptr, idx.p()) == LisResult::NullArgument);
  CHECK(lis_run(n, a.data(), scratch.data(), sw, &len, &count, nullptr) == LisResult::NullArgument);
  CHECK(lis_run(0, nullptr, nullptr, 0, nullptr, &count, nullptr) == LisResult::NullArgument);  // out_len and out_count are always needed
  CHECK(lis_run(0, nullptr, nullptr, 0, &len, nullptr, nullptr) == LisResult::NullArgument);
  CHECK(lis_run(kMaxLen + 1, nullptr, scratch.data(), sw, &len, &count, idx.p()) == LisResult::NullArgument);  // null before TooLarge
  CHECK(lis_run(kMaxLen + 1, a.data(), nullptr, sw, &len, &count, idx.p()) == LisResult::NullArgument);
  CHECK(lis_run(kMaxLen + 1, a.data(), scratch.data(), sw, &len, &count, nullptr) == LisResult::NullArgument);
  CHECK(lis_run(kMaxLen + 1, a.data(), scratch.data(), sw, nullptr, &count, idx.p()) == LisResult::NullArgument);
  CHECK(lis_run(kMaxLen + 1, a.data(), scratch.data(), sw, &len, &count, idx.p()) == LisResult::TooLarge);  // TooLarge before ScratchTooSmall
  CHECK(lis_run(0xFFFFFFFFu, a.data(), scratch.data(), 0, &len, &count, idx.p()) == LisResult::TooLarge);
  CHECK(lis_run(n, a.data(), scratch.data(), 0, &len, &count, idx.p()) == LisResult::ScratchTooSmall);
  CHECK(lis_run(n, a.data(), scratch.data(), sw - 1, &len, &count, idx.p()) == LisResult::ScratchTooSmall);
  CHECK(untouched());
  std::uint32_t w = kSent;
  CHECK(lis_scratch_words(0, nullptr) == LisResult::NullArgument);
  CHECK(lis_scratch_words(kMaxLen + 1, nullptr) == LisResult::NullArgument);  // null before TooLarge
  CHECK(lis_scratch_words(kMaxLen + 1, &w) == LisResult::TooLarge);
  CHECK(lis_scratch_words(0xFFFFFFFFu, &w) == LisResult::TooLarge);
  CHECK(w == kSent);
}
