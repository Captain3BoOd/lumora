// Tests for core/palin. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/palin_ref.py, an independent Python program (every substring
//      against its reverse, expansion around centers, a separator-string Manacher and a palindromic tree, compared with each other by the script).
//      The tests re-run every string against the real functions and recompute the 64-bit digest (n, odd, even, longest start and length, count)
//      and the number of strings.
//  (2) ORACLE inside the test, sharing no code with core/palin/palin.cpp: expansion around every center for the radii, a palindromic tree
//      (Eertree) for the count and the longest palindrome, and for strings up to 24 bytes every substring against its reverse. The radii of the
//      large strings are checked through the digests and through sum(odd + even) = the tree's count.
//  (3) CONSTANTS: the hand rows (ties, palindromes touching the ends, bytes 0 and 255 next to each other), the check-order matrix, and the closed
//      forms for one repeated byte are written from the header.
// Output buffers carry 8 sentinel entries before and 16 after; on success nothing outside the stated range may change, on failure nothing may
// change at all. Scratch is 2 * n words and its contents are never examined.
// Extents: alphabets {0, 255} up to length 16, {0, 128, 255} up to length 10 and six representative bytes up to length 6 are EXHAUSTIVE, and so
// are all one-byte and two-byte strings over all 256 values; 600 strings of length 17 to 400 per alphabet are fixed-seed SAMPLES; the large strings
// are the listed patterns (SAMPLED patterns). No timing assertion: the linear bound is argued in docs/palin.md.
// What is NOT done: Windows is CI-only; nothing is asserted about scratch contents or about overlapping buffers.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "palin.h"

using namespace lumora::palin;

// BEGIN GENERATED REFERENCE (tools/palin_ref.py)
const std::uint64_t kRefDigest_all256 = 0xACAA3EF1A2F8F9F5ULL;
const std::size_t kRefCases_all256 = 65793;
const std::uint64_t kRefDigest_big_alt_262142 = 0x2442C117182D064EULL;
const std::size_t kRefCases_big_alt_262142 = 1;
const std::uint64_t kRefDigest_big_alt_262143 = 0x9EC006909C456FEDULL;
const std::size_t kRefCases_big_alt_262143 = 1;
const std::uint64_t kRefDigest_big_alt_262144 = 0xBB266C564EF308E2ULL;
const std::size_t kRefCases_big_alt_262144 = 1;
const std::uint64_t kRefDigest_big_altab_262144 = 0xBB266C564EF308E2ULL;
const std::size_t kRefCases_big_altab_262144 = 1;
const std::uint64_t kRefDigest_big_ends_262144 = 0x220DA6A238A7D38EULL;
const std::size_t kRefCases_big_ends_262144 = 1;
const std::uint64_t kRefDigest_big_fib_262142 = 0xE46581D187FCA1B7ULL;
const std::size_t kRefCases_big_fib_262142 = 1;
const std::uint64_t kRefDigest_big_fib_262143 = 0x80F1A1A7B5A65E23ULL;
const std::size_t kRefCases_big_fib_262143 = 1;
const std::uint64_t kRefDigest_big_fib_262144 = 0x710169077621C985ULL;
const std::size_t kRefCases_big_fib_262144 = 1;
const std::uint64_t kRefDigest_big_pal_262143 = 0xBAFFB477D9CD5A09ULL;
const std::size_t kRefCases_big_pal_262143 = 1;
const std::uint64_t kRefDigest_big_pal_262144 = 0xFDD5BEC807426265ULL;
const std::size_t kRefCases_big_pal_262144 = 1;
const std::uint64_t kRefDigest_big_palflipend_262144 = 0x7710704DF121AF72ULL;
const std::size_t kRefCases_big_palflipend_262144 = 1;
const std::uint64_t kRefDigest_big_palflipmid_262144 = 0x6D9A0512A9ADBE2BULL;
const std::size_t kRefCases_big_palflipmid_262144 = 1;
const std::uint64_t kRefDigest_big_per3_262142 = 0x3DBD636FD9490136ULL;
const std::size_t kRefCases_big_per3_262142 = 1;
const std::uint64_t kRefDigest_big_per3_262143 = 0xA1FCDBFECA8A5163ULL;
const std::size_t kRefCases_big_per3_262143 = 1;
const std::uint64_t kRefDigest_big_per3_262144 = 0x2CAFF9E2920F30CCULL;
const std::size_t kRefCases_big_per3_262144 = 1;
const std::uint64_t kRefDigest_big_rand256_1000 = 0x89637316136B11A7ULL;
const std::size_t kRefCases_big_rand256_1000 = 1;
const std::uint64_t kRefDigest_big_rand256_262143 = 0xFC4A863FDB95B2F0ULL;
const std::size_t kRefCases_big_rand256_262143 = 1;
const std::uint64_t kRefDigest_big_rand256_262144 = 0x0505A7862FA7301BULL;
const std::size_t kRefCases_big_rand256_262144 = 1;
const std::uint64_t kRefDigest_big_rand2_262143 = 0x1BE6BE3FA8F47FACULL;
const std::size_t kRefCases_big_rand2_262143 = 1;
const std::uint64_t kRefDigest_big_rand2_262144 = 0xBAD55338CEB21386ULL;
const std::size_t kRefCases_big_rand2_262144 = 1;
const std::uint64_t kRefDigest_big_rep0_262142 = 0x8E7BDA851C7EFB65ULL;
const std::size_t kRefCases_big_rep0_262142 = 1;
const std::uint64_t kRefDigest_big_rep0_262143 = 0x0F3535B4BA776FEDULL;
const std::size_t kRefCases_big_rep0_262143 = 1;
const std::uint64_t kRefDigest_big_rep0_262144 = 0xF30F9653C17C13F5ULL;
const std::size_t kRefCases_big_rep0_262144 = 1;
const std::uint64_t kRefDigest_big_rep0_32768 = 0xCF8EA501AF02D3F5ULL;
const std::size_t kRefCases_big_rep0_32768 = 1;
const std::uint64_t kRefDigest_big_rep255_262144 = 0xF30F9653C17C13F5ULL;
const std::size_t kRefCases_big_rep255_262144 = 1;
const std::uint64_t kRefDigest_ex2 = 0x0B8904676E043E8DULL;
const std::size_t kRefCases_ex2 = 131071;
const std::uint64_t kRefDigest_ex3 = 0x1F4D5D79B3BBEF6BULL;
const std::size_t kRefCases_ex3 = 88573;
const std::uint64_t kRefDigest_ex6 = 0x53605FF086985171ULL;
const std::size_t kRefCases_ex6 = 55987;
const std::uint64_t kRefDigest_rs2 = 0xA9A71F4C7CAB1A79ULL;
const std::size_t kRefCases_rs2 = 600;
const std::uint64_t kRefDigest_rs256 = 0xFB468E631865353DULL;
const std::size_t kRefCases_rs256 = 600;
const std::uint64_t kRefDigest_rs3 = 0xF0269C37D0CE189FULL;
const std::size_t kRefCases_rs3 = 600;
// END GENERATED REFERENCE

namespace {

using Str = std::vector<std::uint8_t>;
constexpr std::uint32_t kSent = 0xA5A5A5A5u;
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

// Palindromic tree: the number of palindromic substring occurrences and the first (smallest start) longest one.
struct Tree {
  std::uint64_t count = 0;
  std::uint32_t start = 0, len = 0;
};

Tree eertree(const Str& s) {
  const std::size_t n = s.size();
  // node 0: length -1 (imaginary root), node 1: length 0 (empty). Edges are kept as linked lists per node.
  std::vector<std::int64_t> length{-1, 0};
  std::vector<std::uint32_t> link{0, 0}, depth{0, 0}, first_end{0, 0}, head{0, 0};  // head: index into the edge arrays + 1 (0 = none)
  std::vector<std::uint32_t> edge_to, edge_next;
  std::vector<std::uint8_t> edge_byte;
  auto find = [&](std::uint32_t node, std::uint8_t c) -> std::uint32_t {
    for (std::uint32_t e = head[node]; e != 0; e = edge_next[e - 1])
      if (edge_byte[e - 1] == c) return edge_to[e - 1];
    return 0;
  };
  auto reaches = [&](std::uint32_t node, std::size_t i) {
    const std::int64_t l = length[node];
    const std::int64_t j = static_cast<std::int64_t>(i) - 1 - l;
    return j >= 0 && s[static_cast<std::size_t>(j)] == s[i];
  };
  std::uint32_t last = 1;
  Tree t;
  for (std::size_t i = 0; i < n; ++i) {
    std::uint32_t cur = last;
    while (!reaches(cur, i)) cur = link[cur];
    std::uint32_t nxt = find(cur, s[i]);
    if (nxt == 0) {
      nxt = static_cast<std::uint32_t>(length.size());
      length.push_back(length[cur] + 2);
      first_end.push_back(static_cast<std::uint32_t>(i));
      head.push_back(0);
      if (length[nxt] == 1) {
        link.push_back(1);
      } else {
        std::uint32_t q = link[cur];
        while (!reaches(q, i)) q = link[q];
        link.push_back(find(q, s[i]));
      }
      depth.push_back(depth[link[nxt]] + 1);
      edge_to.push_back(nxt);
      edge_byte.push_back(s[i]);
      edge_next.push_back(head[cur]);
      head[cur] = static_cast<std::uint32_t>(edge_to.size());
    }
    last = nxt;
    t.count += depth[last];
  }
  for (std::size_t v = 2; v < length.size(); ++v) {
    const std::uint32_t ln = static_cast<std::uint32_t>(length[v]);
    const std::uint32_t st = first_end[v] + 1 - ln;
    if (ln > t.len || (ln == t.len && st < t.start)) {
      t.len = ln;
      t.start = st;
    }
  }
  return t;
}

// Expansion around every center (odd and even).
void expand(const Str& s, std::vector<std::uint32_t>& odd, std::vector<std::uint32_t>& even) {
  const std::size_t n = s.size();
  odd.assign(n, 0);
  even.assign(n, 0);
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t k = 1;
    while (k <= i && i + k < n && s[i - k] == s[i + k]) ++k;
    odd[i] = static_cast<std::uint32_t>(k);
    k = 0;
    while (k < i && i + k < n && s[i - k - 1] == s[i + k]) ++k;
    even[i] = static_cast<std::uint32_t>(k);
  }
}

// Every substring against its reverse (short strings only): count and the first longest.
Tree brute(const Str& s) {
  Tree t;
  const std::size_t n = s.size();
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = i; j < n; ++j) {
      bool pal = true;
      for (std::size_t a = i, b = j; a < b; ++a, --b)
        if (s[a] != s[b]) pal = false;
      if (!pal) continue;
      ++t.count;
      const std::uint32_t ln = static_cast<std::uint32_t>(j - i + 1);
      if (ln > t.len) {
        t.len = ln;
        t.start = static_cast<std::uint32_t>(i);
      }
    }
  return t;
}

// ---------------------------------------------------------------- one string against everything

// Runs all three functions on s; compares with the oracle (radii by expansion only when `small`); folds n, odd, even, start, len, count.
void check_string(const Str& s, bool small, Fold& f, Tally& t) {
  const std::size_t n = s.size();
  const std::string tag = "n=" + std::to_string(n);
  const std::uint8_t* sp = n ? s.data() : nullptr;
  Guarded odd(n), even(n);
  std::vector<std::uint32_t> scratch(2 * n + 1, 0xDEADBEEFu);
  const std::uint32_t sw = static_cast<std::uint32_t>(2 * n);
  std::uint32_t start = kSent, len = kSent;
  std::uint64_t count = 0xFEEDFACEFEEDFACEull;
  const PalinResult r1 = palin_radii(static_cast<std::uint32_t>(n), sp, odd.p(), even.p());
  const PalinResult r2 = palin_longest(static_cast<std::uint32_t>(n), sp, scratch.data(), sw, &start, &len);
  const PalinResult r3 = palin_count(static_cast<std::uint32_t>(n), sp, scratch.data(), sw, &count);
  if (r1 != PalinResult::Ok || r2 != PalinResult::Ok || r3 != PalinResult::Ok) {
    t.fail(tag + ": not Ok");
    return;
  }
  if (!odd.guards_ok(n) || !even.guards_ok(n)) t.fail(tag + ": guard entry written");
  const Tree tr = eertree(s);
  if (small) {
    std::vector<std::uint32_t> eo, ee;
    expand(s, eo, ee);
    for (std::size_t i = 0; i < n; ++i) {
      if (odd.p()[i] != eo[i]) t.fail(tag + ": odd differs");
      if (even.p()[i] != ee[i]) t.fail(tag + ": even differs");
    }
  }
  if (n <= 24) {
    const Tree b = brute(s);
    if (b.count != tr.count || b.len != tr.len || b.start != tr.start) t.fail(tag + ": oracle disagrees with brute force");
  }
  std::uint64_t sum = 0;
  f.add(n);
  for (std::size_t i = 0; i < n; ++i) {
    f.add(odd.p()[i]);
    sum += odd.p()[i];
    if (odd.p()[i] < 1) t.fail(tag + ": odd below 1");
  }
  for (std::size_t i = 0; i < n; ++i) {
    f.add(even.p()[i]);
    sum += even.p()[i];
  }
  if (n > 0 && even.p()[0] != 0) t.fail(tag + ": even[0] is not 0");
  if (sum != tr.count) t.fail(tag + ": sum of radii differs from the tree count");
  if (count != tr.count) t.fail(tag + ": count differs");
  if (len != tr.len || (n > 0 && start != tr.start) || (n == 0 && start != 0)) t.fail(tag + ": longest differs");
  f.add(start);
  f.add(len);
  f.add(count);
  ++f.cases;
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.h == digest);
}

// Every string of length 0 .. maxlen over `alpha` (by length, then the first byte changes fastest): the same order as tools/palin_ref.py.
Fold run_exhaustive(const std::vector<std::uint8_t>& alpha, std::size_t maxlen, Tally& t, std::size_t& total) {
  Fold f;
  total = 0;
  const std::size_t k = alpha.size();
  std::size_t pw = 1;
  for (std::size_t L = 0; L <= maxlen; ++L) {
    for (std::size_t c = 0; c < pw; ++c) {
      Str s(L);
      std::size_t x = c;
      for (std::size_t i = 0; i < L; ++i) {
        s[i] = alpha[x % k];
        x /= k;
      }
      check_string(s, true, f, t);
      ++total;
    }
    pw *= k;
  }
  return f;
}

// ---------------------------------------------------------------- the large patterns (the same as tools/palin_ref.py)

Str pat_rep(std::size_t n, std::uint8_t b) { return Str(n, b); }

Str pat_alt(std::size_t n, std::uint8_t a, std::uint8_t b) {
  Str s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = i % 2 == 0 ? a : b;
  return s;
}

Str pat_per3(std::size_t n) {
  Str s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<std::uint8_t>(97 + i % 3);
  return s;
}

Str pat_fib(std::size_t n) {
  Str a{98}, b{97};  // s1 = "b", s2 = "a", s_k = s_(k-1) + s_(k-2)
  while (b.size() < n) {
    Str c = b;
    c.insert(c.end(), a.begin(), a.end());
    a = b;
    b = c;
  }
  b.resize(n);
  return b;
}

Str pat_pal(std::size_t n, std::uint64_t seed, long flip) {
  Lcg g(seed);
  Str half((n + 1) / 2);
  for (std::uint8_t& c : half) c = static_cast<std::uint8_t>(g.next() % 256);
  Str s = half;
  for (std::size_t i = half.size(); i > 0; --i) {
    if (n % 2 == 1 && i == half.size()) continue;
    s.push_back(half[i - 1]);
  }
  if (flip >= 0) s[static_cast<std::size_t>(flip)] = static_cast<std::uint8_t>(s[static_cast<std::size_t>(flip)] + 1);
  return s;
}

Str pat_ends(std::size_t n) {
  Str s(n, 0);
  s[0] = 1;
  s[n - 1] = 2;
  return s;
}

Str pat_rand(std::size_t n, std::uint64_t seed, int k) {
  Lcg g(seed);
  Str s(n);
  for (std::uint8_t& c : s) c = k == 2 ? static_cast<std::uint8_t>(97 + g.next() % 2) : static_cast<std::uint8_t>(g.next() % 256);
  return s;
}

struct BigCase {
  const char* name;
  Str (*make)();
  std::uint64_t digest;
  std::size_t cases;
};

#define BIG(NAME, EXPR) {#NAME, [] { return EXPR; }, kRefDigest_big_##NAME, kRefCases_big_##NAME}

}  // namespace

TEST_CASE("exhaustive: every string of length 0..16 over {0, 255}", "[palin]") {
  Tally t;
  std::size_t total = 0;
  const Fold f = run_exhaustive({0, 255}, 16, t, total);
  expect_digest(f, t, kRefCases_ex2, kRefDigest_ex2);
  CHECK(total == 131071u);
}

TEST_CASE("exhaustive: every string of length 0..10 over {0, 128, 255}", "[palin]") {
  Tally t;
  std::size_t total = 0;
  const Fold f = run_exhaustive({0, 128, 255}, 10, t, total);
  expect_digest(f, t, kRefCases_ex3, kRefDigest_ex3);
  CHECK(total == 88573u);
}

TEST_CASE("exhaustive: every string of length 0..6 over the bytes 0, 1, 127, 128, 254, 255", "[palin]") {
  Tally t;
  std::size_t total = 0;
  const Fold f = run_exhaustive({0, 1, 127, 128, 254, 255}, 6, t, total);
  expect_digest(f, t, kRefCases_ex6, kRefDigest_ex6);
  CHECK(total == 55987u);
}

TEST_CASE("exhaustive: the empty string, all one-byte and all two-byte strings over all 256 values", "[palin]") {
  Tally t;
  Fold f;
  check_string(Str{}, true, f, t);
  for (std::size_t a = 0; a < 256; ++a) check_string(Str{static_cast<std::uint8_t>(a)}, true, f, t);
  for (std::size_t c = 0; c < 65536; ++c) check_string(Str{static_cast<std::uint8_t>(c % 256), static_cast<std::uint8_t>(c / 256)}, true, f, t);
  expect_digest(f, t, kRefCases_all256, kRefDigest_all256);
}

// Fixed-seed SAMPLES: 600 random strings of length 17 .. 400 per alphabet (not exhaustive).
TEST_CASE("fixed-seed samples: 600 random strings of length 17..400 over 2, 3 and 256 symbols (sampled, not exhaustive)", "[palin]") {
  const std::uint8_t a2[2] = {0, 255}, a3[3] = {0, 128, 255};
  struct G { const char* name; const std::uint8_t* alpha; std::size_t k; std::uint64_t seed; std::uint64_t digest; std::size_t cases; };
  const G groups[3] = {{"rs2", a2, 2, 3002, kRefDigest_rs2, kRefCases_rs2},
                       {"rs3", a3, 3, 3003, kRefDigest_rs3, kRefCases_rs3},
                       {"rs256", nullptr, 256, 3256, kRefDigest_rs256, kRefCases_rs256}};
  for (const G& g : groups) {
    Lcg r(g.seed);
    Fold f;
    Tally t;
    for (int i = 0; i < 600; ++i) {
      const std::size_t n = 17 + r.next() % 384;
      Str s(n);
      for (std::uint8_t& c : s) {
        const std::uint32_t d = r.next();
        c = g.alpha ? g.alpha[d % g.k] : static_cast<std::uint8_t>(d % 256);
      }
      check_string(s, true, f, t);
    }
    INFO(g.name);
    expect_digest(f, t, g.cases, g.digest);
  }
}

TEST_CASE("large strings: repeated byte, alternating, period 3, Fibonacci, long palindromes with a flipped byte, random (sampled patterns)", "[palin]") {
  const BigCase cases[] = {
      BIG(rep0_262144, pat_rep(262144, 0)),
      BIG(rep0_262143, pat_rep(262143, 0)),
      BIG(rep0_262142, pat_rep(262142, 0)),
      BIG(rep255_262144, pat_rep(262144, 255)),
      BIG(alt_262144, pat_alt(262144, 0, 255)),
      BIG(alt_262143, pat_alt(262143, 0, 255)),
      BIG(alt_262142, pat_alt(262142, 0, 255)),
      BIG(altab_262144, pat_alt(262144, 97, 98)),
      BIG(per3_262144, pat_per3(262144)),
      BIG(per3_262143, pat_per3(262143)),
      BIG(per3_262142, pat_per3(262142)),
      BIG(fib_262144, pat_fib(262144)),
      BIG(fib_262143, pat_fib(262143)),
      BIG(fib_262142, pat_fib(262142)),
      BIG(pal_262144, pat_pal(262144, 5001, -1)),
      BIG(pal_262143, pat_pal(262143, 5002, -1)),
      BIG(palflipmid_262144, pat_pal(262144, 5001, 131072)),
      BIG(palflipend_262144, pat_pal(262144, 5001, 262141)),
      BIG(ends_262144, pat_ends(262144)),
      BIG(rand2_262144, pat_rand(262144, 5003, 2)),
      BIG(rand2_262143, pat_rand(262143, 5004, 2)),
      BIG(rand256_262144, pat_rand(262144, 5005, 256)),
      BIG(rand256_262143, pat_rand(262143, 5006, 256)),
      BIG(rep0_32768, pat_rep(32768, 0)),
      BIG(rand256_1000, pat_rand(1000, 5007, 256)),
  };
  for (const BigCase& c : cases) {
    const Str s = c.make();
    Fold f;
    Tally t;
    check_string(s, s.size() <= 1000, f, t);
    INFO(c.name);
    expect_digest(f, t, c.cases, c.digest);
  }
}

TEST_CASE("one repeated byte at the limit: exact radii, longest and the count 34359869440", "[palin]") {
  for (std::uint32_t n : {262144u, 262143u}) {
    const Str s(n, 0xFFu);
    Guarded odd(n), even(n);
    std::vector<std::uint32_t> scratch(2 * static_cast<std::size_t>(n));
    std::uint32_t start = kSent, len = kSent;
    std::uint64_t count = 0;
    REQUIRE(palin_radii(n, s.data(), odd.p(), even.p()) == PalinResult::Ok);
    REQUIRE(palin_longest(n, s.data(), scratch.data(), 2 * n, &start, &len) == PalinResult::Ok);
    REQUIRE(palin_count(n, s.data(), scratch.data(), 2 * n, &count) == PalinResult::Ok);
    bool ok = true;
    for (std::uint32_t i = 0; i < n; ++i) {
      const std::uint32_t left = i + 1, right = n - i;
      if (odd.p()[i] != (left < right ? left : right)) ok = false;
      const std::uint32_t el = i, er = n - i;
      if (even.p()[i] != (el < er ? el : er)) ok = false;
    }
    CHECK(ok);
    CHECK(start == 0u);
    CHECK(len == n);
    CHECK(count == static_cast<std::uint64_t>(n) * (static_cast<std::uint64_t>(n) + 1u) / 2u);
    if (n == 262144u) CHECK(count == 34359869440ull);
  }
}

TEST_CASE("hand rows: ties, palindromes touching the ends, bytes 0 and 255 next to each other", "[palin]") {
  struct Row { const char* name; Str s; std::uint32_t start, len; std::uint64_t count; };
  const Row rows[] = {
      {"ties, first at the start", {97, 98, 97, 120, 121, 122, 99, 100, 99}, 0, 3, 11ull},
      {"ties, first in the middle", {120, 121, 97, 98, 97, 120, 121, 122, 99, 100, 99}, 2, 3, 13ull},
      {"ties, second touches the end", {120, 121, 122, 119, 113, 97, 98, 97, 99, 100, 99}, 5, 3, 13ull},
      {"three even ties", {97, 97, 98, 98, 99, 99}, 0, 2, 9ull},
      {"even tie not at 0", {120, 97, 97, 98, 98, 99, 99}, 1, 2, 10ull},
      {"whole string odd", {97, 98, 99, 98, 97}, 0, 5, 7ull},
      {"whole string even", {97, 98, 98, 97}, 0, 4, 6ull},
      {"two equal bytes", {97, 97}, 0, 2, 3ull},
      {"one byte", {97}, 0, 1, 1ull},
      {"whole aabaa", {97, 97, 98, 97, 97}, 0, 5, 9ull},
      {"whole abcxxxcba", {97, 98, 99, 120, 120, 120, 99, 98, 97}, 0, 9, 15ull},
      {"prefix palindrome is the longest", {97, 98, 97, 120, 121, 122}, 0, 3, 7ull},
      {"suffix palindrome is the longest", {120, 121, 122, 97, 98, 97}, 3, 3, 7ull},
      {"longest touches the start only", {97, 98, 99, 120, 120, 120, 99, 98, 97, 113}, 0, 9, 16ull},
      {"longest touches the end only", {113, 97, 98, 99, 120, 120, 120, 99, 98, 97}, 1, 9, 16ull},
      {"longest inside, ends differ", {113, 97, 98, 99, 120, 99, 98, 97, 122}, 1, 7, 12ull},
      {"bytes 0 255 0", {0, 255, 0}, 0, 3, 4ull},
      {"bytes 255 0 255 255 0 255", {255, 0, 255, 255, 0, 255}, 0, 6, 11ull},
      {"bytes 0 255", {0, 255}, 0, 1, 2ull},
      {"bytes 255 255", {255, 255}, 0, 2, 3ull},
      {"bytes 128 127 128", {128, 127, 128}, 0, 3, 4ull},
      {"bytes 127 128 127 128", {127, 128, 127, 128}, 0, 3, 6ull},
      {"bytes 0 0 255 255 0 0", {0, 0, 255, 255, 0, 0}, 0, 6, 11ull},
      {"bytes 255 0 0 255 255", {255, 0, 0, 255, 255}, 0, 4, 8ull},
  };
  for (const Row& r : rows) {
    INFO(r.name);
    const std::uint32_t n = static_cast<std::uint32_t>(r.s.size());
    std::vector<std::uint32_t> scratch(2 * r.s.size());
    std::uint32_t start = kSent, len = kSent;
    std::uint64_t count = 0;
    REQUIRE(palin_longest(n, r.s.data(), scratch.data(), 2 * n, &start, &len) == PalinResult::Ok);
    REQUIRE(palin_count(n, r.s.data(), scratch.data(), 2 * n, &count) == PalinResult::Ok);
    CHECK(start == r.start);
    CHECK(len == r.len);
    CHECK(count == r.count);
  }
}

TEST_CASE("hand radii: aabaa, abba, a 0/255 string", "[palin]") {
  // aabaa: the longest odd palindrome centered at b is aabaa (radius 3); the two a a pairs are even palindromes centered at indexes 1 and 4.
  const Str s1{97, 97, 98, 97, 97};
  std::uint32_t odd[5], even[5];
  REQUIRE(palin_radii(5, s1.data(), odd, even) == PalinResult::Ok);
  const std::uint32_t eo1[5] = {1, 1, 3, 1, 1}, ee1[5] = {0, 1, 0, 0, 1};
  for (int i = 0; i < 5; ++i) {
    CHECK(odd[i] == eo1[i]);
    CHECK(even[i] == ee1[i]);
  }
  const Str s2{97, 98, 98, 97};
  REQUIRE(palin_radii(4, s2.data(), odd, even) == PalinResult::Ok);
  const std::uint32_t eo2[4] = {1, 1, 1, 1}, ee2[4] = {0, 0, 2, 0};
  for (int i = 0; i < 4; ++i) {
    CHECK(odd[i] == eo2[i]);
    CHECK(even[i] == ee2[i]);
  }
  const Str s3{0, 255, 255, 0, 255};
  REQUIRE(palin_radii(5, s3.data(), odd, even) == PalinResult::Ok);
  const std::uint32_t eo3[5] = {1, 1, 1, 2, 1}, ee3[5] = {0, 0, 2, 0, 0};  // the pair 255 255 grows to 0 255 255 0
  for (int i = 0; i < 5; ++i) {
    CHECK(odd[i] == eo3[i]);
    CHECK(even[i] == ee3[i]);
  }
}

TEST_CASE("check order matrix: null arguments, TooLarge, ScratchTooSmall; nothing written on failure", "[palin]") {
  const Str s = {1, 2, 2, 1, 3};
  const std::uint32_t n = 5;
  Guarded o(n), e(n), st(1), ln(1);
  std::vector<std::uint32_t> scratch(2 * n, 7u);
  std::uint64_t cnt = 0x1234567890ABCDEFull;
  const std::uint64_t cnt_before = cnt;
  std::uint32_t w = kSent;
  // palin_scratch_words
  CHECK(palin_scratch_words(0, nullptr) == PalinResult::NullArgument);
  CHECK(palin_scratch_words(kMaxLen + 1, nullptr) == PalinResult::NullArgument);
  CHECK(palin_scratch_words(kMaxLen + 1, &w) == PalinResult::TooLarge);
  CHECK(w == kSent);
  CHECK(palin_scratch_words(0, &w) == PalinResult::Ok);
  CHECK(w == 0u);
  CHECK(palin_scratch_words(5, &w) == PalinResult::Ok);
  CHECK(w == 10u);
  CHECK(palin_scratch_words(kMaxLen, &w) == PalinResult::Ok);
  CHECK(w == 2u * kMaxLen);
  // palin_radii
  CHECK(palin_radii(n, nullptr, o.p(), e.p()) == PalinResult::NullArgument);
  CHECK(palin_radii(n, s.data(), nullptr, e.p()) == PalinResult::NullArgument);
  CHECK(palin_radii(n, s.data(), o.p(), nullptr) == PalinResult::NullArgument);
  CHECK(palin_radii(kMaxLen + 1, nullptr, o.p(), e.p()) == PalinResult::NullArgument);  // null before TooLarge
  CHECK(palin_radii(kMaxLen + 1, s.data(), o.p(), e.p()) == PalinResult::TooLarge);
  CHECK(palin_radii(0xFFFFFFFFu, s.data(), o.p(), e.p()) == PalinResult::TooLarge);
  CHECK(o.untouched());
  CHECK(e.untouched());
  CHECK(palin_radii(0, nullptr, nullptr, nullptr) == PalinResult::Ok);  // n = 0: nothing is needed, nothing written
  CHECK(palin_radii(0, s.data(), o.p(), e.p()) == PalinResult::Ok);
  CHECK(o.untouched());
  CHECK(e.untouched());
  // palin_longest
  CHECK(palin_longest(n, nullptr, scratch.data(), 2 * n, st.p(), ln.p()) == PalinResult::NullArgument);
  CHECK(palin_longest(n, s.data(), nullptr, 2 * n, st.p(), ln.p()) == PalinResult::NullArgument);
  CHECK(palin_longest(n, s.data(), scratch.data(), 2 * n, nullptr, ln.p()) == PalinResult::NullArgument);
  CHECK(palin_longest(n, s.data(), scratch.data(), 2 * n, st.p(), nullptr) == PalinResult::NullArgument);
  CHECK(palin_longest(0, nullptr, nullptr, 0, nullptr, ln.p()) == PalinResult::NullArgument);  // the outputs are always needed
  CHECK(palin_longest(0, nullptr, nullptr, 0, st.p(), nullptr) == PalinResult::NullArgument);
  CHECK(palin_longest(kMaxLen + 1, nullptr, scratch.data(), 2 * n, st.p(), ln.p()) == PalinResult::NullArgument);
  CHECK(palin_longest(kMaxLen + 1, s.data(), scratch.data(), 2 * n, st.p(), ln.p()) == PalinResult::TooLarge);
  CHECK(palin_longest(kMaxLen + 1, s.data(), scratch.data(), 0, st.p(), ln.p()) == PalinResult::TooLarge);  // TooLarge before ScratchTooSmall
  CHECK(palin_longest(n, s.data(), scratch.data(), 2 * n - 1, st.p(), ln.p()) == PalinResult::ScratchTooSmall);
  CHECK(palin_longest(n, s.data(), scratch.data(), 0, st.p(), ln.p()) == PalinResult::ScratchTooSmall);
  CHECK(st.untouched());
  CHECK(ln.untouched());
  CHECK(palin_longest(0, nullptr, nullptr, 0, st.p(), ln.p()) == PalinResult::Ok);  // n = 0: no scratch, result (0, 0)
  CHECK(st.p()[0] == 0u);
  CHECK(ln.p()[0] == 0u);
  CHECK(st.guards_ok(1));
  CHECK(ln.guards_ok(1));
  st.p()[0] = kSent;
  ln.p()[0] = kSent;
  CHECK(palin_longest(n, s.data(), scratch.data(), 2 * n, st.p(), ln.p()) == PalinResult::Ok);
  CHECK(st.p()[0] == 0u);
  CHECK(ln.p()[0] == 4u);
  CHECK(st.guards_ok(1));
  CHECK(ln.guards_ok(1));
  // palin_count
  CHECK(palin_count(n, nullptr, scratch.data(), 2 * n, &cnt) == PalinResult::NullArgument);
  CHECK(palin_count(n, s.data(), nullptr, 2 * n, &cnt) == PalinResult::NullArgument);
  CHECK(palin_count(n, s.data(), scratch.data(), 2 * n, nullptr) == PalinResult::NullArgument);
  CHECK(palin_count(0, nullptr, nullptr, 0, nullptr) == PalinResult::NullArgument);
  CHECK(palin_count(kMaxLen + 1, nullptr, scratch.data(), 2 * n, &cnt) == PalinResult::NullArgument);
  CHECK(palin_count(kMaxLen + 1, s.data(), scratch.data(), 2 * n, &cnt) == PalinResult::TooLarge);
  CHECK(palin_count(kMaxLen + 1, s.data(), scratch.data(), 0, &cnt) == PalinResult::TooLarge);
  CHECK(palin_count(n, s.data(), scratch.data(), 2 * n - 1, &cnt) == PalinResult::ScratchTooSmall);
  CHECK(palin_count(n, s.data(), scratch.data(), 0, &cnt) == PalinResult::ScratchTooSmall);
  CHECK(cnt == cnt_before);
  CHECK(palin_count(0, nullptr, nullptr, 0, &cnt) == PalinResult::Ok);
  CHECK(cnt == 0u);
  CHECK(palin_count(n, s.data(), scratch.data(), 2 * n, &cnt) == PalinResult::Ok);
  CHECK(cnt == 7u);  // 1 2 2 1 3: five single bytes, "22" and "1221"
}
