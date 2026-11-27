// Tests for core/monostack. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/monostack_ref.py, an independent Python program (definitions by
//      plain scans and a monotonic stack and deque written again, compared with each other by the script). The tests re-run every array against the
//      real functions and recompute the 64-bit digest (n, left, right, area, l, r and, for every window size in ks(n), k and the window minima) and
//      the number of arrays.
//  (2) ORACLE inside the test, sharing no code with core/monostack/monostack.cpp: up to 400 elements the definitions by plain scans (every range for
//      the rectangle, every window by a scan); above that pointer chains (a neighbour of i is found by jumping along the neighbours of the elements in
//      between), the same chains for the rectangle (a range per bar) and a block method (prefix and suffix minima per block of k) for the windows.
//  (3) CONSTANTS: the hand rows, the closed forms at the limit (2^20 elements: constant 0xFFFFFFFF gives the area 2^20 * (2^32 - 1), increasing,
//      decreasing and constant arrays have closed-form neighbours and windows), the scratch rules and the check-order matrix, written from the header.
// Output buffers carry sentinels: left, right and out have 8 entries before and 16 after, entries after the written ones must stay unchanged on
// success, and on failure every output (including area, l and r) must stay unchanged. Scratch contents are never examined.
// Extents: arrays over {0, 1, 2} up to length 11, over {0, 1, 2, 3} up to length 9, over four values (0, 1, 0x80000000, 0xFFFFFFFF) up to length 8,
// every permutation of 0 .. n - 1 for n up to 9 and the empty, one-element and two-element arrays over 0 .. 255 are EXHAUSTIVE (for every window
// size k from 1 to n when n <= 12); 600 arrays of length 10 to 400 per alphabet are fixed-seed SAMPLES; the large arrays are the listed patterns
// (SAMPLED patterns, a few window sizes each). No timing assertion: the step counts are argued in docs/monostack.md.
// What is NOT done: Windows is CI-only; nothing is asserted about scratch contents or overlapping arrays.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "monostack.h"

using namespace lumora::monostack;

// BEGIN GENERATED REFERENCE (tools/monostack_ref.py)
const std::uint64_t kRefDigest_all256 = 0xA672205B5C512351ULL;
const std::size_t kRefCases_all256 = 65793;
const std::uint64_t kRefDigest_big_altext_1048576 = 0x0F5927018CD62481ULL;
const std::size_t kRefCases_big_altext_1048576 = 1;
const std::uint64_t kRefDigest_big_const0_1048576 = 0xC312192E1BC65470ULL;
const std::size_t kRefCases_big_const0_1048576 = 1;
const std::uint64_t kRefDigest_big_constmax_1048575 = 0x7F533D2FBBC801AFULL;
const std::size_t kRefCases_big_constmax_1048575 = 1;
const std::uint64_t kRefDigest_big_constmax_1048576 = 0x4891CABBEBE7D097ULL;
const std::size_t kRefCases_big_constmax_1048576 = 1;
const std::uint64_t kRefDigest_big_dec_1048576 = 0xADC763BAD2B67C59ULL;
const std::size_t kRefCases_big_dec_1048576 = 1;
const std::uint64_t kRefDigest_big_inc_1048576 = 0x80BF4A7F48463D22ULL;
const std::size_t kRefCases_big_inc_1048576 = 1;
const std::uint64_t kRefDigest_big_organ_1048576 = 0xF31D4490333DDF44ULL;
const std::size_t kRefCases_big_organ_1048576 = 1;
const std::uint64_t kRefDigest_big_perm_1048576 = 0x1F2F3EC31B46058DULL;
const std::size_t kRefCases_big_perm_1048576 = 1;
const std::uint64_t kRefDigest_big_rand2_1048576 = 0x0E01AC7ED0322774ULL;
const std::size_t kRefCases_big_rand2_1048576 = 1;
const std::uint64_t kRefDigest_big_rand32_1048575 = 0xD01541921A1A96CEULL;
const std::size_t kRefCases_big_rand32_1048575 = 1;
const std::uint64_t kRefDigest_big_rand32_1048576 = 0xEC6FEB3CFF5E3876ULL;
const std::size_t kRefCases_big_rand32_1048576 = 1;
const std::uint64_t kRefDigest_big_saw1000_1048576 = 0x02A204D1F01E45C1ULL;
const std::size_t kRefCases_big_saw1000_1048576 = 1;
const std::uint64_t kRefDigest_e4 = 0x54D93D3DAFD56F2BULL;
const std::size_t kRefCases_e4 = 87381;
const std::uint64_t kRefDigest_perm = 0x29861F8A95C402CFULL;
const std::size_t kRefCases_perm = 409114;
const std::uint64_t kRefDigest_rs16 = 0xABD4E3680696D11BULL;
const std::size_t kRefCases_rs16 = 600;
const std::uint64_t kRefDigest_rs3 = 0x8E801CAD2993ECE1ULL;
const std::size_t kRefCases_rs3 = 600;
const std::uint64_t kRefDigest_rs32 = 0x4634BA10EA3CE7CFULL;
const std::size_t kRefCases_rs32 = 600;
const std::uint64_t kRefDigest_t3 = 0xD5EF1353043698A5ULL;
const std::size_t kRefCases_t3 = 265720;
const std::uint64_t kRefDigest_t4 = 0xFCA6ED6BA45AFFEAULL;
const std::size_t kRefCases_t4 = 349525;
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

// A u32 buffer with sentinel guards before and after the n entries the call may write.
struct Guarded {
  std::vector<std::uint32_t> buf;
  std::size_t n = 0;
  explicit Guarded(std::size_t count = 0) { reset(count); }
  void reset(std::size_t count) {
    n = count;
    buf.assign(kGb + count + kGa, kSent);
  }
  std::uint32_t* p() { return buf.data() + kGb; }
  const std::uint32_t* p() const { return buf.data() + kGb; }
  bool guards_ok() const {
    for (std::size_t i = 0; i < kGb; ++i)
      if (buf[i] != kSent) return false;
    for (std::size_t i = 0; i < kGa; ++i)
      if (buf[kGb + n + i] != kSent) return false;
    return true;
  }
  // true when entries from `from` to n - 1 were not written
  bool tail_untouched(std::size_t from) const {
    for (std::size_t i = from; i < n; ++i)
      if (p()[i] != kSent) return false;
    return true;
  }
  bool untouched() const {
    for (std::uint32_t v : buf)
      if (v != kSent) return false;
    return true;
  }
};

// Window sizes checked for an array of n entries (the same function as ks in tools/monostack_ref.py).
Seq ks(std::size_t n) {
  std::vector<std::size_t> c;
  if (n <= 12) {
    for (std::size_t k = 1; k <= n; ++k) c.push_back(k);
  } else if (n <= 400) {
    c = {1, 2, 3, 5, 8, n / 4, n / 2, n - 2, n - 1, n};
  } else {
    c = {1, 2, 3, 1000, n / 2, n - 1, n};
  }
  std::sort(c.begin(), c.end());
  c.erase(std::unique(c.begin(), c.end()), c.end());
  Seq out;
  for (std::size_t k : c)
    if (k >= 1 && k <= n) out.push_back(static_cast<std::uint32_t>(k));
  return out;
}

// ---------------------------------------------------------------- the oracle (no code shared with the implementation)

struct Rect {
  std::uint64_t area = 0;
  std::uint32_t l = 0, r = 0;
};

// By the definitions (n <= 400): scan outwards from every element; every range; every window.
void def_bounds(const Seq& a, Seq& left, Seq& right) {
  const std::size_t n = a.size();
  left.assign(n, kNone);
  right.assign(n, kNone);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i; j > 0; --j)
      if (a[j - 1] < a[i]) {
        left[i] = static_cast<std::uint32_t>(j - 1);
        break;
      }
    for (std::size_t j = i + 1; j < n; ++j)
      if (a[j] <= a[i]) {
        right[i] = static_cast<std::uint32_t>(j);
        break;
      }
  }
}

Rect def_rect(const Seq& a) {
  Rect best;
  const std::size_t n = a.size();
  for (std::size_t l = 0; l < n; ++l) {
    std::uint32_t m = a[l];
    for (std::size_t r = l; r < n; ++r) {
      m = std::min(m, a[r]);
      const std::uint64_t area = (r - l + 1) * m;
      if (area > best.area) {
        best.area = area;
        best.l = static_cast<std::uint32_t>(l);
        best.r = static_cast<std::uint32_t>(r);
      }
    }
  }
  return best;
}

Seq def_window(const Seq& a, std::size_t k) {
  Seq out;
  for (std::size_t s = 0; s + k <= a.size(); ++s) {
    std::size_t idx = s;
    for (std::size_t j = s + 1; j < s + k; ++j)
      if (a[j] < a[idx]) idx = j;
    out.push_back(static_cast<std::uint32_t>(idx));
  }
  return out;
}

// Pointer chains (any n): the neighbour of i is found by jumping along the neighbours of the elements in between.
Seq chain_left(const Seq& a) {
  const std::size_t n = a.size();
  Seq L(n, kNone);
  for (std::size_t i = 1; i < n; ++i) {
    std::uint32_t j = static_cast<std::uint32_t>(i - 1);
    while (j != kNone && a[j] >= a[i]) j = L[j];
    L[i] = j;
  }
  return L;
}

// strict = false: the first element to the right that is smaller or equal; strict = true: the first that is strictly smaller.
Seq chain_right(const Seq& a, bool strict) {
  const std::size_t n = a.size();
  Seq R(n, kNone);
  for (std::size_t i = n; i-- > 0;) {
    std::uint32_t j = i + 1 < n ? static_cast<std::uint32_t>(i + 1) : kNone;
    while (j != kNone && (strict ? a[j] >= a[i] : a[j] > a[i])) j = R[j];
    R[i] = j;
  }
  return R;
}

Rect chain_rect(const Seq& a) {
  const std::size_t n = a.size();
  const Seq L = chain_left(a), R = chain_right(a, true);
  Rect best;
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint32_t l = L[i] == kNone ? 0u : L[i] + 1u;
    const std::uint32_t r = (R[i] == kNone ? static_cast<std::uint32_t>(n) : R[i]) - 1u;
    const std::uint64_t area = static_cast<std::uint64_t>(r - l + 1u) * a[i];
    if (area > best.area || (area == best.area && (l < best.l || (l == best.l && r < best.r)))) {
      best.area = area;
      best.l = l;
      best.r = r;
    }
  }
  return best;
}

// Blocks of k: prefix and suffix minima inside each block (leftmost on ties); a window is a suffix of one block plus a prefix of the next.
Seq block_window(const Seq& a, std::size_t k) {
  const std::size_t n = a.size();
  Seq g(n), h(n), out;
  for (std::size_t i = 0; i < n; ++i) g[i] = (i % k == 0 || a[i] < a[g[i - 1]]) ? static_cast<std::uint32_t>(i) : g[i - 1];
  for (std::size_t i = n; i-- > 0;) h[i] = (i + 1 == n || i % k == k - 1 || a[i] <= a[h[i + 1]]) ? static_cast<std::uint32_t>(i) : h[i + 1];
  for (std::size_t s = 0; s + k <= n; ++s) {
    const std::uint32_t x = h[s], y = g[s + k - 1];
    out.push_back(a[y] < a[x] ? y : x);
  }
  return out;
}

struct Expect {
  Seq left, right;
  Rect rect;
};

Expect oracle(const Seq& a) {
  Expect e;
  if (a.size() <= 400) {
    def_bounds(a, e.left, e.right);
    e.rect = def_rect(a);
  } else {
    e.left = chain_left(a);
    e.right = chain_right(a, false);
    e.rect = chain_rect(a);
  }
  return e;
}

Seq oracle_window(const Seq& a, std::size_t k) { return a.size() <= 400 ? def_window(a, k) : block_window(a, k); }

std::string describe(const Seq& a) {
  std::string s = "n = " + std::to_string(a.size()) + " a =";
  for (std::size_t i = 0; i < a.size() && i < 14; ++i) s += " " + std::to_string(a[i]);
  return s;
}

// Runs the three functions on a (scratch of exactly n words), compares with the oracle and folds the values in the same order as
// tools/monostack_ref.py.
void check_arr(const Seq& a, Fold& f, Tally& t) {
  static Guarded left, right, out;
  const std::size_t n = a.size();
  const std::uint32_t dummy = 0;
  const std::uint32_t* ap = n > 0 ? a.data() : &dummy;
  std::vector<std::uint32_t> scratch(n, 0x5A5A5A5Au);
  const std::uint32_t nn = static_cast<std::uint32_t>(n);
  const Expect e = oracle(a);
  f.add(n);

  left.reset(n);
  right.reset(n);
  if (monostack_bounds(ap, nn, scratch.data(), nn, left.p(), right.p()) != StackResult::Ok) {
    t.fail("bounds not Ok, " + describe(a));
    return;
  }
  if (!left.guards_ok() || !right.guards_ok()) t.fail("guards of left or right changed, " + describe(a));
  if (!std::equal(e.left.begin(), e.left.end(), left.p())) t.fail("left differs, " + describe(a));
  if (!std::equal(e.right.begin(), e.right.end(), right.p())) t.fail("right differs, " + describe(a));
  for (std::size_t i = 0; i < n; ++i) f.add(left.p()[i]);
  for (std::size_t i = 0; i < n; ++i) f.add(right.p()[i]);

  std::uint64_t area = kSent64;
  std::uint32_t l = kSent, r = kSent;
  if (monostack_max_rect(ap, nn, scratch.data(), nn, &area, &l, &r) != StackResult::Ok) {
    t.fail("max_rect not Ok, " + describe(a));
    return;
  }
  if (area != e.rect.area || l != e.rect.l || r != e.rect.r) t.fail("rectangle differs, " + describe(a));
  f.add(area);
  f.add(l);
  f.add(r);

  for (std::uint32_t k : ks(n)) {
    out.reset(n);
    if (monostack_window_min(ap, nn, k, scratch.data(), nn, out.p()) != StackResult::Ok) {
      t.fail("window_min not Ok, " + describe(a));
      return;
    }
    const std::size_t w = n - k + 1;
    const Seq want = oracle_window(a, k);
    if (!out.guards_ok() || !out.tail_untouched(w)) t.fail("guards or entries after the windows changed, " + describe(a));
    if (!std::equal(want.begin(), want.end(), out.p())) t.fail("window minima differ, k = " + std::to_string(k) + ", " + describe(a));
    f.add(k);
    for (std::size_t i = 0; i < w; ++i) f.add(out.p()[i]);
  }
  ++f.cases;
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.h == digest);
}

// Every array of length 0 .. maxlen over `alpha` (by length, then the first value changes fastest): the same order as tools/monostack_ref.py.
Fold run_exhaustive(const Seq& alpha, std::size_t maxlen, Tally& t) {
  Fold f;
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
      check_arr(s, f, t);
    }
    pw *= k;
  }
  return f;
}

// ---------------------------------------------------------------- the large patterns (the same as tools/monostack_ref.py)

constexpr std::size_t kBig = 1048576;

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

Seq pat_organ(std::size_t n) {
  Seq s(n);
  const std::size_t h = n / 2;
  for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<std::uint32_t>(i < h ? i : n - 1 - i);
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

struct HandRow {
  const char* name;
  Seq a, left, right;
  std::uint64_t area;
  std::uint32_t l, r;
  std::vector<std::pair<std::uint32_t, Seq>> wins;
};

}  // namespace

TEST_CASE("exhaustive: every array of length 0..11 over {0, 1, 2}", "[monostack]") {
  Tally t;
  const Fold f = run_exhaustive({0, 1, 2}, 11, t);
  expect_digest(f, t, 265720u, kRefDigest_t3);
  CHECK(kRefCases_t3 == 265720u);
}

TEST_CASE("exhaustive: every array of length 0..9 over {0, 1, 2, 3}", "[monostack]") {
  Tally t;
  const Fold f = run_exhaustive({0, 1, 2, 3}, 9, t);
  expect_digest(f, t, 349525u, kRefDigest_t4);
  CHECK(kRefCases_t4 == 349525u);
}

TEST_CASE("exhaustive: every array of length 0..8 over 0, 1, 0x80000000, 0xFFFFFFFF", "[monostack]") {
  Tally t;
  const Fold f = run_exhaustive({0u, 1u, 0x80000000u, 0xFFFFFFFFu}, 8, t);
  expect_digest(f, t, 87381u, kRefDigest_e4);
  CHECK(kRefCases_e4 == 87381u);
}

TEST_CASE("exhaustive: every permutation of 0..n-1 for n = 0..9", "[monostack]") {
  Tally t;
  Fold f;
  for (std::uint32_t n = 0; n <= 9; ++n) {
    Seq p = pat_inc(n);
    do {
      check_arr(p, f, t);
    } while (std::next_permutation(p.begin(), p.end()));
  }
  expect_digest(f, t, 409114u, kRefDigest_perm);
  CHECK(kRefCases_perm == 409114u);
}

TEST_CASE("exhaustive: the empty array, all one-element and all two-element arrays over 0..255", "[monostack]") {
  Tally t;
  Fold f;
  check_arr(Seq{}, f, t);
  for (std::uint32_t a = 0; a < 256; ++a) check_arr(Seq{a}, f, t);
  for (std::uint32_t c = 0; c < 65536; ++c) check_arr(Seq{c % 256, c / 256}, f, t);
  expect_digest(f, t, 65793u, kRefDigest_all256);
  CHECK(kRefCases_all256 == 65793u);
}

TEST_CASE("fixed-seed samples: 600 random arrays of length 10..400 over 3, 16 and 2^32 values (sampled, not exhaustive)", "[monostack]") {
  struct Group {
    std::uint32_t mod;
    std::uint64_t seed;
    std::uint64_t digest;
    std::size_t cases;
  };
  const Group groups[] = {{3, 7003, kRefDigest_rs3, kRefCases_rs3}, {16, 7016, kRefDigest_rs16, kRefCases_rs16}, {0, 7032, kRefDigest_rs32, kRefCases_rs32}};
  for (const Group& gr : groups) {
    Lcg g(gr.seed);
    Tally t;
    Fold f;
    for (int c = 0; c < 600; ++c) {
      const std::size_t n = 10 + g.next() % 391;
      Seq s(n);
      for (std::uint32_t& v : s) {
        const std::uint32_t d = g.next();
        v = gr.mod ? d % gr.mod : d;
      }
      check_arr(s, f, t);
    }
    expect_digest(f, t, 600u, gr.digest);
    CHECK(gr.cases == 600u);
  }
}

TEST_CASE("oracles agree: definitions against chains and blocks on random arrays (so the large cases rest on checked oracles)", "[monostack]") {
  Lcg g(8123);
  for (int c = 0; c < 400; ++c) {
    const std::size_t n = 1 + g.next() % 150;
    const std::uint32_t mod = (c % 4 == 0) ? 2 : (c % 4 == 1) ? 3 : (c % 4 == 2) ? 8 : 0;
    Seq s(n);
    for (std::uint32_t& v : s) {
      const std::uint32_t d = g.next();
      v = mod ? d % mod : d;
    }
    Seq dl, dr;
    def_bounds(s, dl, dr);
    CHECK(dl == chain_left(s));
    CHECK(dr == chain_right(s, false));
    const Rect a = def_rect(s), b = chain_rect(s);
    CHECK(a.area == b.area);
    CHECK(a.l == b.l);
    CHECK(a.r == b.r);
    for (std::size_t k : ks(n)) CHECK(def_window(s, k) == block_window(s, k));
  }
}

TEST_CASE("large arrays: increasing, decreasing, constant, alternating, sawtooth, organ pipe, random (sampled patterns)", "[monostack]") {
  const BigCase cases[] = {
      BIG(inc_1048576, pat_inc(kBig)),
      BIG(dec_1048576, pat_dec(kBig)),
      BIG(const0_1048576, pat_const(kBig, 0)),
      BIG(constmax_1048576, pat_const(kBig, 0xFFFFFFFFu)),
      BIG(constmax_1048575, pat_const(kBig - 1, 0xFFFFFFFFu)),
      BIG(altext_1048576, pat_alt(kBig, 0xFFFFFFFFu, 0)),
      BIG(saw1000_1048576, pat_saw(kBig, 1000)),
      BIG(organ_1048576, pat_organ(kBig)),
      BIG(rand2_1048576, pat_rand(kBig, 9011, 2)),
      BIG(rand32_1048576, pat_rand(kBig, 9013, 0)),
      BIG(rand32_1048575, pat_rand(kBig - 1, 9014, 0)),
      BIG(perm_1048576, pat_perm(kBig, 9015)),
  };
  for (const BigCase& bc : cases) {
    INFO(bc.name);
    Tally t;
    Fold f;
    check_arr(bc.make(), f, t);
    expect_digest(f, t, bc.cases, bc.digest);
  }
}

TEST_CASE("closed forms at the limit: constant, increasing and decreasing arrays of 1048576 entries", "[monostack]") {
  const std::size_t n = kBig;
  const std::uint32_t nn = static_cast<std::uint32_t>(n);
  std::vector<std::uint32_t> scratch(n), left(n), right(n), out(n);
  std::uint64_t area = 0;
  std::uint32_t l = 0, r = 0;

  {  // constant 0xFFFFFFFF: the widest and tallest rectangle there is
    const Seq a = pat_const(n, 0xFFFFFFFFu);
    REQUIRE(monostack_max_rect(a.data(), nn, scratch.data(), nn, &area, &l, &r) == StackResult::Ok);
    CHECK(area == (1ull << 20) * 0xFFFFFFFFull);
    CHECK(area == 4503599626321920ull);
    CHECK(l == 0u);
    CHECK(r == nn - 1);
    REQUIRE(monostack_bounds(a.data(), nn, scratch.data(), nn, left.data(), right.data()) == StackResult::Ok);
    bool ok = true;
    for (std::size_t i = 0; i < n; ++i) ok = ok && left[i] == kNone && right[i] == (i + 1 < n ? i + 1 : kNone);
    CHECK(ok);
    REQUIRE(monostack_window_min(a.data(), nn, nn, scratch.data(), nn, out.data()) == StackResult::Ok);
    CHECK(out[0] == 0u);
    REQUIRE(monostack_window_min(a.data(), nn, 1000, scratch.data(), nn, out.data()) == StackResult::Ok);
    ok = true;
    for (std::size_t s = 0; s + 1000 <= n; ++s) ok = ok && out[s] == s;
    CHECK(ok);
  }
  {  // constant 0: area 0 and the range (0, 0)
    const Seq a = pat_const(n, 0);
    REQUIRE(monostack_max_rect(a.data(), nn, scratch.data(), nn, &area, &l, &r) == StackResult::Ok);
    CHECK(area == 0u);
    CHECK(l == 0u);
    CHECK(r == 0u);
  }
  {  // increasing 0 .. n-1: left[i] = i - 1, no right bound, window minima at the window start
    const Seq a = pat_inc(n);
    REQUIRE(monostack_bounds(a.data(), nn, scratch.data(), nn, left.data(), right.data()) == StackResult::Ok);
    bool ok = true;
    for (std::size_t i = 0; i < n; ++i) ok = ok && left[i] == (i == 0 ? kNone : static_cast<std::uint32_t>(i - 1)) && right[i] == kNone;
    CHECK(ok);
    REQUIRE(monostack_window_min(a.data(), nn, 4096, scratch.data(), nn, out.data()) == StackResult::Ok);
    ok = true;
    for (std::size_t s = 0; s + 4096 <= n; ++s) ok = ok && out[s] == s;
    CHECK(ok);
    REQUIRE(monostack_max_rect(a.data(), nn, scratch.data(), nn, &area, &l, &r) == StackResult::Ok);
    CHECK(area == (1ull << 38));  // (n - i) * i is largest at i = n / 2
    CHECK(l == nn / 2);
    CHECK(r == nn - 1);
  }
  {  // decreasing n-1 .. 0: no left bound, right[i] = i + 1, window minima at the window end
    const Seq a = pat_dec(n);
    REQUIRE(monostack_bounds(a.data(), nn, scratch.data(), nn, left.data(), right.data()) == StackResult::Ok);
    bool ok = true;
    for (std::size_t i = 0; i < n; ++i) ok = ok && left[i] == kNone && right[i] == (i + 1 < n ? static_cast<std::uint32_t>(i + 1) : kNone);
    CHECK(ok);
    REQUIRE(monostack_window_min(a.data(), nn, 4096, scratch.data(), nn, out.data()) == StackResult::Ok);
    ok = true;
    for (std::size_t s = 0; s + 4096 <= n; ++s) ok = ok && out[s] == s + 4095;
    CHECK(ok);
    REQUIRE(monostack_max_rect(a.data(), nn, scratch.data(), nn, &area, &l, &r) == StackResult::Ok);
    CHECK(area == (1ull << 38));  // (r + 1) * (n - 1 - r) is largest at r = n / 2 - 1
    CHECK(l == 0u);
    CHECK(r == nn / 2 - 1);
  }
}

TEST_CASE("hand rows: ties, plateaus, zeros, extremes, equal minima", "[monostack]") {
  const std::vector<HandRow> rows = {
    {"empty-like single", {5u}, {kNone}, {kNone}, 5ull, 0u, 0u, {{1, {0u}}}},
    {"single zero", {0u}, {kNone}, {kNone}, 0ull, 0u, 0u, {{1, {0u}}}},
    {"two equal", {3u, 3u}, {kNone, kNone}, {1u, kNone}, 6ull, 0u, 1u, {{1, {0u, 1u}}, {2, {0u}}}},
    {"two up", {1u, 2u}, {kNone, 0u}, {kNone, kNone}, 2ull, 0u, 1u, {{1, {0u, 1u}}, {2, {0u}}}},
    {"two down", {2u, 1u}, {kNone, kNone}, {1u, kNone}, 2ull, 0u, 0u, {{1, {0u, 1u}}, {2, {1u}}}},
    {"classic histogram", {2u, 1u, 5u, 6u, 2u, 3u}, {kNone, kNone, 1u, 2u, 1u, 4u}, {1u, kNone, 4u, 4u, kNone, kNone}, 10ull, 2u, 3u, {{1, {0u, 1u, 2u, 3u, 4u, 5u}}, {2, {1u, 1u, 2u, 4u, 4u}}, {3, {1u, 1u, 4u, 4u}}, {4, {1u, 1u, 4u}}, {5, {1u, 1u}}, {6, {1u}}}},
    {"equal run", {4u, 4u, 4u, 4u}, {kNone, kNone, kNone, kNone}, {1u, 2u, 3u, kNone}, 16ull, 0u, 3u, {{1, {0u, 1u, 2u, 3u}}, {2, {0u, 1u, 2u}}, {3, {0u, 1u}}, {4, {0u}}}},
    {"equal plateau between lows", {1u, 5u, 5u, 5u, 1u}, {kNone, 0u, 0u, 0u, kNone}, {4u, 2u, 3u, 4u, kNone}, 15ull, 1u, 3u, {{1, {0u, 1u, 2u, 3u, 4u}}, {2, {0u, 1u, 2u, 4u}}, {3, {0u, 1u, 4u}}, {4, {0u, 4u}}, {5, {0u}}}},
    {"zeros inside", {3u, 0u, 3u, 0u, 3u}, {kNone, kNone, 1u, kNone, 3u}, {1u, 3u, 3u, kNone, kNone}, 3ull, 0u, 0u, {{1, {0u, 1u, 2u, 3u, 4u}}, {2, {1u, 1u, 3u, 3u}}, {3, {1u, 1u, 3u}}, {4, {1u, 1u}}, {5, {1u}}}},
    {"all zero", {0u, 0u, 0u}, {kNone, kNone, kNone}, {1u, 2u, kNone}, 0ull, 0u, 0u, {{1, {0u, 1u, 2u}}, {2, {0u, 1u}}, {3, {0u}}}},
    {"max values", {kNone, kNone, kNone}, {kNone, kNone, kNone}, {1u, 2u, kNone}, 12884901885ull, 0u, 2u, {{1, {0u, 1u, 2u}}, {2, {0u, 1u}}, {3, {0u}}}},
    {"extremes alternate", {kNone, 0u, kNone, 0u}, {kNone, kNone, 1u, kNone}, {1u, 3u, 3u, kNone}, 4294967295ull, 0u, 0u, {{1, {0u, 1u, 2u, 3u}}, {2, {1u, 1u, 3u}}, {3, {1u, 1u}}, {4, {1u}}}},
    {"area tie wide vs tall", {6u, 6u, 6u, 1u, 9u, 9u, 1u, 18u}, {kNone, kNone, kNone, kNone, 3u, 3u, kNone, 6u}, {1u, 2u, 3u, 6u, 5u, 6u, kNone, kNone}, 18ull, 0u, 2u, {{1, {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u}}, {2, {0u, 1u, 3u, 3u, 4u, 6u, 6u}}, {3, {0u, 3u, 3u, 3u, 6u, 6u}}, {4, {3u, 3u, 3u, 3u, 6u}}, {5, {3u, 3u, 3u, 3u}}, {6, {3u, 3u, 3u}}, {7, {3u, 3u}}, {8, {3u}}}},
    {"area tie same l", {4u, 2u, 4u, 6u, 1u, 8u}, {kNone, kNone, 1u, 2u, kNone, 4u}, {1u, 4u, 4u, 4u, kNone, kNone}, 8ull, 0u, 3u, {{1, {0u, 1u, 2u, 3u, 4u, 5u}}, {2, {1u, 1u, 2u, 4u, 4u}}, {3, {1u, 1u, 4u, 4u}}, {4, {1u, 4u, 4u}}, {5, {4u, 4u}}, {6, {4u}}}},
    {"area tie equal height blocks", {3u, 3u, 1u, 3u, 3u}, {kNone, kNone, kNone, 2u, 2u}, {1u, 2u, kNone, 4u, kNone}, 6ull, 0u, 1u, {{1, {0u, 1u, 2u, 3u, 4u}}, {2, {0u, 2u, 2u, 3u}}, {3, {2u, 2u, 2u}}, {4, {2u, 2u}}, {5, {2u}}}},
    {"tie between two blocks of 12", {4u, 4u, 4u, 1u, 6u, 6u, 1u, 3u, 3u, 3u, 3u}, {kNone, kNone, kNone, kNone, 3u, 3u, kNone, 6u, 6u, 6u, 6u}, {1u, 2u, 3u, 6u, 5u, 6u, kNone, 8u, 9u, 10u, kNone}, 12ull, 0u, 2u, {{1, {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u}}, {2, {0u, 1u, 3u, 3u, 4u, 6u, 6u, 7u, 8u, 9u}}, {3, {0u, 3u, 3u, 3u, 6u, 6u, 6u, 7u, 8u}}, {4, {3u, 3u, 3u, 3u, 6u, 6u, 6u, 7u}}, {5, {3u, 3u, 3u, 3u, 6u, 6u, 6u}}, {6, {3u, 3u, 3u, 3u, 6u, 6u}}, {7, {3u, 3u, 3u, 3u, 6u}}, {8, {3u, 3u, 3u, 3u}}, {9, {3u, 3u, 3u}}, {10, {3u, 3u}}, {11, {3u}}}},
    {"increasing", {1u, 2u, 3u, 4u, 5u}, {kNone, 0u, 1u, 2u, 3u}, {kNone, kNone, kNone, kNone, kNone}, 9ull, 2u, 4u, {{1, {0u, 1u, 2u, 3u, 4u}}, {2, {0u, 1u, 2u, 3u}}, {3, {0u, 1u, 2u}}, {4, {0u, 1u}}, {5, {0u}}}},
    {"decreasing", {5u, 4u, 3u, 2u, 1u}, {kNone, kNone, kNone, kNone, kNone}, {1u, 2u, 3u, 4u, kNone}, 9ull, 0u, 2u, {{1, {0u, 1u, 2u, 3u, 4u}}, {2, {1u, 2u, 3u, 4u}}, {3, {2u, 3u, 4u}}, {4, {3u, 4u}}, {5, {4u}}}},
    {"valley", {5u, 3u, 1u, 3u, 5u}, {kNone, kNone, kNone, 2u, 3u}, {1u, 2u, kNone, kNone, kNone}, 6ull, 0u, 1u, {{1, {0u, 1u, 2u, 3u, 4u}}, {2, {1u, 2u, 2u, 3u}}, {3, {2u, 2u, 2u}}, {4, {2u, 2u}}, {5, {2u}}}},
    {"mountain", {1u, 3u, 5u, 3u, 1u}, {kNone, 0u, 1u, 0u, kNone}, {4u, 3u, 3u, 4u, kNone}, 9ull, 1u, 3u, {{1, {0u, 1u, 2u, 3u, 4u}}, {2, {0u, 1u, 3u, 4u}}, {3, {0u, 1u, 4u}}, {4, {0u, 4u}}, {5, {0u}}}},
    {"leftmost minimum ties", {2u, 1u, 1u, 3u, 1u, 1u, 2u}, {kNone, kNone, kNone, 2u, kNone, kNone, 5u}, {1u, 2u, 4u, 4u, 5u, kNone, kNone}, 7ull, 0u, 6u, {{1, {0u, 1u, 2u, 3u, 4u, 5u, 6u}}, {2, {1u, 1u, 2u, 4u, 4u, 5u}}, {3, {1u, 1u, 2u, 4u, 4u}}, {4, {1u, 1u, 2u, 4u}}, {5, {1u, 1u, 2u}}, {6, {1u, 1u}}, {7, {1u}}}},
    {"window ties at the front", {7u, 7u, 7u, 2u, 2u, 9u}, {kNone, kNone, kNone, kNone, kNone, 4u}, {1u, 2u, 3u, 4u, kNone, kNone}, 21ull, 0u, 2u, {{1, {0u, 1u, 2u, 3u, 4u, 5u}}, {2, {0u, 1u, 3u, 3u, 4u}}, {3, {0u, 3u, 3u, 3u}}, {4, {3u, 3u, 3u}}, {5, {3u, 3u}}, {6, {3u}}}},
    {"repeated pattern", {1u, 3u, 1u, 3u, 1u, 3u, 1u}, {kNone, 0u, kNone, 2u, kNone, 4u, kNone}, {2u, 2u, 4u, 4u, 6u, 6u, kNone}, 7ull, 0u, 6u, {{1, {0u, 1u, 2u, 3u, 4u, 5u, 6u}}, {2, {0u, 2u, 2u, 4u, 4u, 6u}}, {3, {0u, 2u, 2u, 4u, 4u}}, {4, {0u, 2u, 2u, 4u}}, {5, {0u, 2u, 2u}}, {6, {0u, 2u}}, {7, {0u}}}},
    {"large values near the top", {0xFFFFFFFEu, kNone, 0xFFFFFFFEu, 0x80000000u, kNone}, {kNone, 0u, kNone, kNone, 3u}, {2u, 2u, 3u, kNone, kNone}, 12884901882ull, 0u, 2u, {{1, {0u, 1u, 2u, 3u, 4u}}, {2, {0u, 2u, 3u, 3u}}, {3, {0u, 3u, 3u}}, {4, {3u, 3u}}, {5, {3u}}}},
  };
  CHECK(rows.size() == 24u);
  for (const HandRow& h : rows) {
    INFO(h.name);
    const std::uint32_t n = static_cast<std::uint32_t>(h.a.size());
    std::vector<std::uint32_t> scratch(n);
    Guarded left(n), right(n), out(n);
    REQUIRE(monostack_bounds(h.a.data(), n, scratch.data(), n, left.p(), right.p()) == StackResult::Ok);
    CHECK(std::equal(h.left.begin(), h.left.end(), left.p()));
    CHECK(std::equal(h.right.begin(), h.right.end(), right.p()));
    CHECK(left.guards_ok());
    CHECK(right.guards_ok());
    std::uint64_t area = 0;
    std::uint32_t l = 0, r = 0;
    REQUIRE(monostack_max_rect(h.a.data(), n, scratch.data(), n, &area, &l, &r) == StackResult::Ok);
    CHECK(area == h.area);
    CHECK(l == h.l);
    CHECK(r == h.r);
    CHECK(h.wins.size() == n);
    for (const auto& w : h.wins) {
      out.reset(n);
      REQUIRE(monostack_window_min(h.a.data(), n, w.first, scratch.data(), n, out.p()) == StackResult::Ok);
      CHECK(std::equal(w.second.begin(), w.second.end(), out.p()));
      CHECK(out.guards_ok());
      CHECK(out.tail_untouched(w.second.size()));
    }
  }
}

TEST_CASE("scratch words, the scratch size boundary and the n = 0 rules", "[monostack]") {
  std::uint32_t words = kSent;
  REQUIRE(monostack_scratch_words(0, &words) == StackResult::Ok);
  CHECK(words == 0u);
  REQUIRE(monostack_scratch_words(1, &words) == StackResult::Ok);
  CHECK(words == 1u);
  REQUIRE(monostack_scratch_words(5, &words) == StackResult::Ok);
  CHECK(words == 5u);
  REQUIRE(monostack_scratch_words(kMaxN, &words) == StackResult::Ok);
  CHECK(words == kMaxN);
  words = kSent;
  CHECK(monostack_scratch_words(kMaxN + 1, &words) == StackResult::TooLarge);
  CHECK(words == kSent);
  CHECK(monostack_scratch_words(0xFFFFFFFFu, &words) == StackResult::TooLarge);
  CHECK(words == kSent);
  CHECK(monostack_scratch_words(5, nullptr) == StackResult::NullArgument);
  CHECK(monostack_scratch_words(kMaxN + 1, nullptr) == StackResult::NullArgument);

  const Seq a = {3, 1, 4, 1, 5, 9, 2, 6};
  const std::uint32_t n = static_cast<std::uint32_t>(a.size());
  std::vector<std::uint32_t> big(n + 5, 0x5A5A5A5Au);
  Guarded left(n), right(n), out(n);
  std::uint64_t area = kSent64;
  std::uint32_t l = kSent, r = kSent;
  // one word short: ScratchTooSmall, nothing written
  CHECK(monostack_bounds(a.data(), n, big.data(), n - 1, left.p(), right.p()) == StackResult::ScratchTooSmall);
  CHECK(left.untouched());
  CHECK(right.untouched());
  CHECK(monostack_max_rect(a.data(), n, big.data(), n - 1, &area, &l, &r) == StackResult::ScratchTooSmall);
  CHECK((area == kSent64 && l == kSent && r == kSent));
  CHECK(monostack_window_min(a.data(), n, 3, big.data(), n - 1, out.p()) == StackResult::ScratchTooSmall);
  CHECK(out.untouched());
  CHECK(monostack_bounds(a.data(), n, big.data(), 0, left.p(), right.p()) == StackResult::ScratchTooSmall);
  // exactly n words and more than n words succeed
  for (std::uint32_t sw : {n, n + 1, n + 5}) {
    left.reset(n);
    right.reset(n);
    out.reset(n);
    REQUIRE(monostack_bounds(a.data(), n, big.data(), sw, left.p(), right.p()) == StackResult::Ok);
    CHECK(left.p()[7] == 6u);
    CHECK(right.p()[2] == 3u);
    REQUIRE(monostack_max_rect(a.data(), n, big.data(), sw, &area, &l, &r) == StackResult::Ok);
    CHECK(area == 10u);
    CHECK(l == 4u);
    CHECK(r == 5u);
    REQUIRE(monostack_window_min(a.data(), n, 3, big.data(), sw, out.p()) == StackResult::Ok);
    CHECK(out.p()[0] == 1u);
    CHECK(out.p()[5] == 6u);
    CHECK(out.tail_untouched(6));
  }
  // n = 0: Ok with every pointer null (outputs of max_rect excepted) and any scratch size and any k; only max_rect writes
  CHECK(monostack_bounds(nullptr, 0, nullptr, 0, nullptr, nullptr) == StackResult::Ok);
  CHECK(monostack_window_min(nullptr, 0, 0, nullptr, 0, nullptr) == StackResult::Ok);
  CHECK(monostack_window_min(nullptr, 0, 7, nullptr, 0, nullptr) == StackResult::Ok);
  CHECK(monostack_window_min(nullptr, 0, 0xFFFFFFFFu, nullptr, 12345, nullptr) == StackResult::Ok);
  area = kSent64;
  l = kSent;
  r = kSent;
  CHECK(monostack_max_rect(nullptr, 0, nullptr, 0, &area, &l, &r) == StackResult::Ok);
  CHECK(area == 0u);
  CHECK(l == 0u);
  CHECK(r == 0u);
  area = kSent64;
  l = kSent;
  r = kSent;
  CHECK(monostack_max_rect(nullptr, 0, nullptr, 99, &area, &l, &r) == StackResult::Ok);
  CHECK((area == 0u && l == 0u && r == 0u));
  // n = 0 with real buffers: nothing is written by bounds and window_min
  left.reset(4);
  right.reset(4);
  out.reset(4);
  CHECK(monostack_bounds(a.data(), 0, big.data(), 0, left.p(), right.p()) == StackResult::Ok);
  CHECK(monostack_window_min(a.data(), 0, 3, big.data(), 0, out.p()) == StackResult::Ok);
  CHECK(left.untouched());
  CHECK(right.untouched());
  CHECK(out.untouched());
  // k = n gives one window, k = 1 gives the identity
  out.reset(n);
  REQUIRE(monostack_window_min(a.data(), n, n, big.data(), n, out.p()) == StackResult::Ok);
  CHECK(out.p()[0] == 1u);
  CHECK(out.tail_untouched(1));
  out.reset(n);
  REQUIRE(monostack_window_min(a.data(), n, 1, big.data(), n, out.p()) == StackResult::Ok);
  bool ident = true;
  for (std::uint32_t i = 0; i < n; ++i) ident = ident && out.p()[i] == i;
  CHECK(ident);
}

TEST_CASE("check order matrix: null arguments, TooLarge, BadWindow, ScratchTooSmall; nothing written on failure", "[monostack]") {
  const Seq a = {2, 7, 1, 8};
  const std::uint32_t n = 4;
  std::vector<std::uint32_t> sc(8, 0x5A5A5A5Au);
  Guarded left(8), right(8), out(8);
  std::uint64_t area = kSent64;
  std::uint32_t l = kSent, r = kSent;
  auto rect_untouched = [&] { return area == kSent64 && l == kSent && r == kSent; };
  auto reset_rect = [&] {
    area = kSent64;
    l = kSent;
    r = kSent;
  };
  std::uint32_t words = kSent;

  // monostack_bounds: each pointer null while n > 0
  CHECK(monostack_bounds(nullptr, n, sc.data(), n, left.p(), right.p()) == StackResult::NullArgument);
  CHECK(monostack_bounds(a.data(), n, nullptr, n, left.p(), right.p()) == StackResult::NullArgument);
  CHECK(monostack_bounds(a.data(), n, sc.data(), n, nullptr, right.p()) == StackResult::NullArgument);
  CHECK(monostack_bounds(a.data(), n, sc.data(), n, left.p(), nullptr) == StackResult::NullArgument);
  // null before TooLarge, TooLarge before ScratchTooSmall
  CHECK(monostack_bounds(nullptr, kMaxN + 1, sc.data(), 0, left.p(), right.p()) == StackResult::NullArgument);
  CHECK(monostack_bounds(a.data(), kMaxN + 1, nullptr, 0, left.p(), right.p()) == StackResult::NullArgument);
  CHECK(monostack_bounds(a.data(), kMaxN + 1, sc.data(), 0, left.p(), right.p()) == StackResult::TooLarge);
  CHECK(monostack_bounds(a.data(), 0xFFFFFFFFu, sc.data(), 0, left.p(), right.p()) == StackResult::TooLarge);
  CHECK(monostack_bounds(a.data(), n, sc.data(), n - 1, left.p(), right.p()) == StackResult::ScratchTooSmall);
  CHECK(left.untouched());
  CHECK(right.untouched());

  // monostack_max_rect: the three outputs are required always, a and scratch only while n > 0
  CHECK(monostack_max_rect(a.data(), n, sc.data(), n, nullptr, &l, &r) == StackResult::NullArgument);
  CHECK(monostack_max_rect(a.data(), n, sc.data(), n, &area, nullptr, &r) == StackResult::NullArgument);
  CHECK(monostack_max_rect(a.data(), n, sc.data(), n, &area, &l, nullptr) == StackResult::NullArgument);
  CHECK(monostack_max_rect(a.data(), 0, sc.data(), 0, nullptr, &l, &r) == StackResult::NullArgument);
  CHECK(monostack_max_rect(a.data(), 0, sc.data(), 0, &area, nullptr, &r) == StackResult::NullArgument);
  CHECK(monostack_max_rect(a.data(), 0, sc.data(), 0, &area, &l, nullptr) == StackResult::NullArgument);
  CHECK(monostack_max_rect(nullptr, n, sc.data(), n, &area, &l, &r) == StackResult::NullArgument);
  CHECK(monostack_max_rect(a.data(), n, nullptr, n, &area, &l, &r) == StackResult::NullArgument);
  CHECK(monostack_max_rect(nullptr, kMaxN + 1, sc.data(), 0, &area, &l, &r) == StackResult::NullArgument);
  CHECK(monostack_max_rect(a.data(), kMaxN + 1, sc.data(), 0, nullptr, &l, &r) == StackResult::NullArgument);
  CHECK(monostack_max_rect(a.data(), kMaxN + 1, sc.data(), 0, &area, &l, &r) == StackResult::TooLarge);
  CHECK(monostack_max_rect(a.data(), 0xFFFFFFFFu, sc.data(), 0, &area, &l, &r) == StackResult::TooLarge);
  CHECK(monostack_max_rect(a.data(), n, sc.data(), n - 1, &area, &l, &r) == StackResult::ScratchTooSmall);
  CHECK(rect_untouched());
  reset_rect();

  // monostack_window_min: precedence null > TooLarge > BadWindow > ScratchTooSmall
  CHECK(monostack_window_min(nullptr, n, 2, sc.data(), n, out.p()) == StackResult::NullArgument);
  CHECK(monostack_window_min(a.data(), n, 2, nullptr, n, out.p()) == StackResult::NullArgument);
  CHECK(monostack_window_min(a.data(), n, 2, sc.data(), n, nullptr) == StackResult::NullArgument);
  CHECK(monostack_window_min(nullptr, kMaxN + 1, 0, sc.data(), 0, out.p()) == StackResult::NullArgument);
  CHECK(monostack_window_min(a.data(), kMaxN + 1, 0, sc.data(), 0, out.p()) == StackResult::TooLarge);
  CHECK(monostack_window_min(a.data(), kMaxN + 1, 2, sc.data(), 0, out.p()) == StackResult::TooLarge);
  CHECK(monostack_window_min(a.data(), 0xFFFFFFFFu, 0xFFFFFFFFu, sc.data(), 0, out.p()) == StackResult::TooLarge);
  CHECK(monostack_window_min(a.data(), n, 0, sc.data(), n, out.p()) == StackResult::BadWindow);
  CHECK(monostack_window_min(a.data(), n, 0, sc.data(), 0, out.p()) == StackResult::BadWindow);
  CHECK(monostack_window_min(a.data(), n, n + 1, sc.data(), n, out.p()) == StackResult::BadWindow);
  CHECK(monostack_window_min(a.data(), n, n + 1, sc.data(), 0, out.p()) == StackResult::BadWindow);
  CHECK(monostack_window_min(a.data(), n, 0xFFFFFFFFu, sc.data(), n, out.p()) == StackResult::BadWindow);
  CHECK(monostack_window_min(a.data(), n, 2, sc.data(), n - 1, out.p()) == StackResult::ScratchTooSmall);
  CHECK(monostack_window_min(a.data(), n, n, sc.data(), 0, out.p()) == StackResult::ScratchTooSmall);
  CHECK(out.untouched());
  // k = n and k = n + 1 around the window limit
  CHECK(monostack_window_min(a.data(), n, n, sc.data(), n, out.p()) == StackResult::Ok);
  CHECK(out.p()[0] == 2u);

  // monostack_scratch_words: null out_words before TooLarge
  CHECK(monostack_scratch_words(n, nullptr) == StackResult::NullArgument);
  CHECK(monostack_scratch_words(kMaxN + 1, &words) == StackResult::TooLarge);
  CHECK(words == kSent);
}
