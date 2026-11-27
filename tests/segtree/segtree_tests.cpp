// Tests for core/segtree. Evidence label: unit tests only. Every comparison is exact (modulo 2^64).
// The block between BEGIN and END GENERATED REFERENCE comes from tools/segtree_ref.py, an independent Python implementation that is NOT a segment
// tree (a plain list with loops over the range). Regenerate it with: python3 tools/segtree_ref.py
// The tests also carry a naive oracle that shares no code with core/segtree/segtree.cpp: a std::vector with a loop over the range per operation.
// What is checked, and what is not: the storage CONTENTS are unspecified in the header, so no test looks at them after a success (successes are
// checked only through later sums). After a failed seg_init or seg_range_add the whole buffer that was passed is compared byte for byte with a
// copy taken before the call (the header says the storage is untouched on any failure); after a failed seg_range_sum only *out is checked.
// Buffers are heap buffers of exactly 8 * n words (or the shorter buffer for ScratchTooSmall), so AddressSanitizer sees any access past the end.
// Max-size coverage (n = 65535 and 65536) is SAMPLED (random operations checked against the oracle), not exhaustive. Time and the O(log n)
// bound are not tested.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "segtree.h"

using namespace lumora::segtree;

namespace {

// BEGIN GENERATED REFERENCE (tools/segtree_ref.py)
const std::uint32_t kRefExhMaxN = 12, kRefPairMaxN = 6, kRefRandSeeds = 400, kRefRandOps = 40, kRefRandMaxN = 300;
const std::uint32_t kRefBoundSizes[] = {1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1000};
const std::uint64_t kRefDigest_exh = 0x7D3E6A5C9DFDE8D0ull;
const std::size_t kRefCases_exh = 3637, kRefOps_exh = 158728;
const std::uint64_t kRefDigest_bound = 0x3299CAE0FA7978EEull;
const std::size_t kRefCases_bound = 23, kRefOps_bound = 27979;
const std::uint64_t kRefDigest_rand = 0x706E3F01166C906Dull;
const std::size_t kRefCases_rand = 400, kRefOps_rand = 16000;
// END GENERATED REFERENCE

constexpr std::uint64_t kM64 = ~0ull;
constexpr std::uint64_t kSentinel = 0xDEADBEEFCAFEF00Dull;
constexpr std::uint32_t kOver = kMaxLen + 1u;
constexpr std::uint32_t kU32Max = 0xFFFFFFFFu;
constexpr std::uint64_t kGold = 0x9E3779B97F4A7C15ull;

std::uint32_t code(SegResult r) { return static_cast<std::uint32_t>(r); }

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  std::size_t ops = 0;
  void add(std::uint64_t v) { h = (h ^ v) * 0x100000001B3ull; }
};

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  }
  std::uint64_t next64() {
    const std::uint64_t hi = next();
    const std::uint64_t lo = next();
    return (hi << 32) | lo;
  }
};

std::vector<std::uint64_t> fixed_values(std::uint32_t n) {
  std::vector<std::uint64_t> v(n);
  for (std::uint32_t i = 0; i < n; ++i) v[i] = i % 5u == 3u ? kM64 : kGold * (static_cast<std::uint64_t>(i) + 1u);
  return v;
}

// ---- oracle: a plain vector, loops over the range ----
struct Oracle {
  std::vector<std::uint64_t> a;
  explicit Oracle(const std::vector<std::uint64_t>& v) : a(v) {}
  bool valid(std::uint32_t l, std::uint32_t r) const { return l <= r && r <= a.size(); }
  void add(std::uint32_t l, std::uint32_t r, std::uint64_t d) {
    for (std::uint32_t i = l; i < r; ++i) a[i] += d;
  }
  std::uint64_t sum(std::uint32_t l, std::uint32_t r) const {
    std::uint64_t s = 0;
    for (std::uint32_t i = l; i < r; ++i) s += a[i];
    return s;
  }
};

std::vector<std::uint64_t> snapshot(const std::uint64_t* p, std::uint32_t words) { return std::vector<std::uint64_t>(p, p + words); }
bool same(const std::vector<std::uint64_t>& s, const std::uint64_t* p) { return std::memcmp(s.data(), p, sizeof(std::uint64_t) * s.size()) == 0; }

// An exact heap buffer of 8 * n words (null for n = 0), refilled before every init with a prefill that alternates between two patterns.
struct Store {
  std::uint32_t n;
  std::uint32_t words;
  std::unique_ptr<std::uint64_t[]> p;
  explicit Store(std::uint32_t n_) : n(n_), words(8u * n_) {
    if (words != 0u) p.reset(new std::uint64_t[words]);
  }
  std::uint64_t* get() { return p.get(); }
  void prefill(std::uint32_t which) {
    for (std::uint32_t i = 0; i < words; ++i) p[i] = which % 2u == 0u ? 0xA5A5A5A5A5A5A5A5ull : 0ull;
  }
};

// Runs the operations against the real functions and the oracle. Returns mismatches through *bad.
struct Run {
  Store st;
  Oracle orc;
  std::size_t* bad;
  Fold* f;
  std::uint32_t init_count = 0;
  Run(const std::vector<std::uint64_t>& vals, std::size_t* bad_, Fold* f_) : st(static_cast<std::uint32_t>(vals.size())), orc(vals), bad(bad_), f(f_) {
    st.prefill(init_count++);
    const std::uint64_t* vp = vals.empty() ? nullptr : vals.data();
    if (seg_init(vp, st.n, st.get(), st.words) != SegResult::Ok) ++*bad;
  }
  void add(std::uint32_t l, std::uint32_t r, std::uint64_t d) {
    const bool ok = orc.valid(l, r);
    std::vector<std::uint64_t> before;
    if (!ok && st.words != 0u) before = snapshot(st.get(), st.words);
    const SegResult r1 = seg_range_add(st.get(), st.words, st.n, l, r, d);
    if (ok) {
      orc.add(l, r, d);
      if (r1 != SegResult::Ok) ++*bad;
    } else {
      if (r1 != SegResult::OutOfRange) ++*bad;
      if (st.words != 0u && std::memcmp(before.data(), st.get(), sizeof(std::uint64_t) * st.words) != 0) ++*bad;
    }
    ++f->ops;
    f->add(code(r1));
  }
  void sum(std::uint32_t l, std::uint32_t r) {
    std::uint64_t out = kSentinel;
    const SegResult r1 = seg_range_sum(st.get(), st.words, st.n, l, r, &out);
    if (orc.valid(l, r)) {
      if (r1 != SegResult::Ok || out != orc.sum(l, r)) ++*bad;
    } else if (r1 != SegResult::OutOfRange || out != kSentinel) {
      ++*bad;
    }
    ++f->ops;
    f->add(code(r1));
    if (r1 == SegResult::Ok) f->add(out);
  }
};

}  // namespace

TEST_CASE("header anchors", "[segtree]") {
  CHECK(kMaxLen == 65536u);
  CHECK(code(SegResult::Ok) == 0u);
  CHECK(code(SegResult::NullArgument) == 1u);
  CHECK(code(SegResult::TooLarge) == 2u);
  CHECK(code(SegResult::ScratchTooSmall) == 3u);
  CHECK(code(SegResult::OutOfRange) == 4u);
}

TEST_CASE("exhaustive: n in 0..12, every range for sum and add, every single add then every sum, every ordered pair of adds through n = 6", "[sweep][segtree]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t n = 0; n <= kRefExhMaxN; ++n) {
    const std::vector<std::uint64_t> vals = fixed_values(n);
    ++f.cases;
    {
      Run run(vals, &bad, &f);
      for (std::uint32_t l = 0; l < n + 2u; ++l) {
        for (std::uint32_t r = 0; r < n + 2u; ++r) run.sum(l, r);
      }
    }
    for (std::uint64_t d : {std::uint64_t{1}, kM64}) {
      for (std::uint32_t l = 0; l < n + 2u; ++l) {
        for (std::uint32_t r = 0; r < n + 2u; ++r) {
          ++f.cases;
          Run run(vals, &bad, &f);
          run.st.prefill(static_cast<std::uint32_t>(f.cases));
          const bool ok = run.orc.valid(l, r);
          // re-init with the other prefill, then the add and all sums
          const std::uint64_t* vp = vals.empty() ? nullptr : vals.data();
          if (seg_init(vp, n, run.st.get(), run.st.words) != SegResult::Ok) ++bad;
          run.add(l, r, d);
          if (ok) {
            for (std::uint32_t l2 = 0; l2 < n + 2u; ++l2) {
              for (std::uint32_t r2 = 0; r2 < n + 2u; ++r2) run.sum(l2, r2);
            }
          }
        }
      }
    }
  }
  for (std::uint32_t n = 0; n <= kRefPairMaxN; ++n) {
    const std::vector<std::uint64_t> vals = fixed_values(n);
    for (std::uint32_t l1 = 0; l1 <= n; ++l1) {
      for (std::uint32_t r1 = l1; r1 <= n; ++r1) {
        for (std::uint32_t l2 = 0; l2 <= n; ++l2) {
          for (std::uint32_t r2 = l2; r2 <= n; ++r2) {
            ++f.cases;
            Run run(vals, &bad, &f);
            run.add(l1, r1, (std::uint64_t{1} << 63) + 1u);
            run.add(l2, r2, 3u);
            for (std::uint32_t l3 = 0; l3 <= n; ++l3) {
              for (std::uint32_t r3 = l3; r3 <= n; ++r3) run.sum(l3, r3);
            }
          }
        }
      }
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_exh);
  CHECK(f.ops == kRefOps_exh);
  CHECK(f.h == kRefDigest_exh);
}

TEST_CASE("boundary sizes (1 to 1000): points 0, 1, 2, n/2, n-1, n, one running array (NOT exhaustive)", "[sweep][segtree]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t n : kRefBoundSizes) {
    std::vector<std::uint32_t> pts;
    for (std::uint32_t p : {0u, 1u, 2u, n / 2u, n - 1u, n}) {
      if (p > n) continue;
      bool seen = false;
      for (std::uint32_t q : pts) seen = seen || q == p;
      if (!seen) pts.push_back(p);
    }
    for (std::size_t i = 1; i < pts.size(); ++i) {  // insertion sort
      for (std::size_t j = i; j > 0 && pts[j - 1] > pts[j]; --j) {
        const std::uint32_t t = pts[j];
        pts[j] = pts[j - 1];
        pts[j - 1] = t;
      }
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> rs;
    for (std::uint32_t a : pts) {
      for (std::uint32_t b : pts) {
        if (a <= b) rs.emplace_back(a, b);
      }
    }
    Run run(fixed_values(n), &bad, &f);
    ++f.cases;
    for (const auto& ab : rs) run.sum(ab.first, ab.second);
    for (const auto& ab : rs) {
      for (std::uint64_t d : {std::uint64_t{1}, kM64, std::uint64_t{1} << 63}) {
        run.add(ab.first, ab.second, d);
        for (const auto& ce : rs) run.sum(ce.first, ce.second);
      }
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_bound);
  CHECK(f.ops == kRefOps_bound);
  CHECK(f.h == kRefDigest_bound);
}

TEST_CASE("pseudo-random operation sequences, n up to 300 (NOT exhaustive)", "[sweep][segtree]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t seed = 1; seed <= kRefRandSeeds; ++seed) {
    Lcg g(static_cast<std::uint64_t>(seed) * 7919u + 13u);
    const std::uint32_t n = g.next() % (kRefRandMaxN + 1u);
    std::vector<std::uint64_t> vals(n);
    for (std::uint32_t i = 0; i < n; ++i) vals[i] = g.next64();
    Run run(vals, &bad, &f);
    ++f.cases;
    for (std::uint32_t k = 0; k < kRefRandOps; ++k) {
      const std::uint32_t kind = g.next() % 2u;
      const std::uint32_t a = g.next() % (n + 1u);
      const std::uint32_t b = g.next() % (n + 1u);
      const std::uint32_t l = a < b ? a : b;
      const std::uint32_t r = a < b ? b : a;
      if (kind == 0u) {
        const std::uint32_t mode = g.next() % 4u;
        std::uint64_t d = 0;
        if (mode == 0u) {
          d = g.next64();
        } else if (mode == 1u) {
          d = 1u;
        } else if (mode == 2u) {
          d = kM64;
        } else {
          d = g.next64() >> 40;
        }
        run.add(l, r, d);
      } else {
        run.sum(l, r);
      }
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_rand);
  CHECK(f.ops == kRefOps_rand);
  CHECK(f.h == kRefDigest_rand);
}

TEST_CASE("hand-checked wrapping", "[segtree]") {
  const std::uint64_t vals[3] = {kM64, 1u, std::uint64_t{1} << 63};
  std::uint64_t st[24];
  std::uint64_t out = kSentinel;
  REQUIRE(seg_init(vals, 3, st, 24) == SegResult::Ok);
  CHECK(seg_range_sum(st, 24, 3, 0, 3, &out) == SegResult::Ok);
  CHECK(out == std::uint64_t{1} << 63);  // (2^64 - 1) + 1 + 2^63 = 2^63 mod 2^64
  CHECK(seg_range_sum(st, 24, 3, 0, 2, &out) == SegResult::Ok);
  CHECK(out == 0u);
  REQUIRE(seg_range_add(st, 24, 3, 0, 3, std::uint64_t{1} << 63) == SegResult::Ok);
  CHECK(seg_range_sum(st, 24, 3, 0, 3, &out) == SegResult::Ok);
  CHECK(out == 0u);  // 2^63 - 1, 2^63 + 1, 0 add up to 2^64
  CHECK(seg_range_sum(st, 24, 3, 2, 3, &out) == SegResult::Ok);
  CHECK(out == 0u);
  CHECK(seg_range_sum(st, 24, 3, 1, 2, &out) == SegResult::Ok);
  CHECK(out == (std::uint64_t{1} << 63) + 1u);
  REQUIRE(seg_range_add(st, 24, 3, 1, 3, kM64) == SegResult::Ok);  // add -1 to elements 1 and 2
  CHECK(seg_range_sum(st, 24, 3, 0, 3, &out) == SegResult::Ok);
  CHECK(out == kM64 - 1u);
  CHECK(seg_range_sum(st, 24, 3, 2, 3, &out) == SegResult::Ok);
  CHECK(out == kM64);
}

TEST_CASE("storage size", "[args][segtree]") {
  std::uint32_t w = 77;
  CHECK(seg_storage_words(0, &w) == SegResult::Ok);
  CHECK(w == 0u);
  CHECK(seg_storage_words(1, &w) == SegResult::Ok);
  CHECK(w == 8u);
  CHECK(seg_storage_words(1000, &w) == SegResult::Ok);
  CHECK(w == 8000u);
  CHECK(seg_storage_words(kMaxLen, &w) == SegResult::Ok);
  CHECK(w == 524288u);
  w = 77;
  CHECK(seg_storage_words(kOver, &w) == SegResult::TooLarge);
  CHECK(seg_storage_words(kU32Max, &w) == SegResult::TooLarge);
  CHECK(w == 77u);
  CHECK(seg_storage_words(5, nullptr) == SegResult::NullArgument);
  CHECK(seg_storage_words(kOver, nullptr) == SegResult::NullArgument);  // Null beats TooLarge
  CHECK(seg_storage_words(kU32Max, nullptr) == SegResult::NullArgument);
}

TEST_CASE("check order of seg_init: Null, TooLarge, ScratchTooSmall; the buffer is untouched on failure", "[args][segtree]") {
  const std::vector<std::uint64_t> vals = fixed_values(10);
  const std::uint64_t* v = vals.data();
  std::unique_ptr<std::uint64_t[]> exact(new std::uint64_t[80]);
  std::unique_ptr<std::uint64_t[]> shorter(new std::uint64_t[79]);
  for (std::uint32_t i = 0; i < 80; ++i) exact[i] = kSentinel + i;
  for (std::uint32_t i = 0; i < 79; ++i) shorter[i] = kSentinel + i;
  const auto before_exact = snapshot(exact.get(), 80);
  const auto before_short = snapshot(shorter.get(), 79);
  CHECK(seg_init(nullptr, 10, exact.get(), 80) == SegResult::NullArgument);
  CHECK(seg_init(v, 10, nullptr, 80) == SegResult::NullArgument);
  CHECK(seg_init(nullptr, 10, nullptr, 80) == SegResult::NullArgument);
  CHECK(seg_init(nullptr, 10, exact.get(), 0) == SegResult::NullArgument);  // Null beats ScratchTooSmall
  CHECK(seg_init(v, 10, nullptr, 0) == SegResult::NullArgument);
  CHECK(seg_init(nullptr, kOver, exact.get(), 80) == SegResult::NullArgument);  // Null beats TooLarge
  CHECK(seg_init(v, kOver, nullptr, 80) == SegResult::NullArgument);
  CHECK(seg_init(nullptr, kU32Max, exact.get(), 80) == SegResult::NullArgument);
  CHECK(seg_init(v, kU32Max, nullptr, 80) == SegResult::NullArgument);
  CHECK(seg_init(nullptr, kU32Max, nullptr, 0) == SegResult::NullArgument);
  CHECK(seg_init(v, kOver, exact.get(), 80) == SegResult::TooLarge);
  CHECK(seg_init(v, kOver, exact.get(), 0) == SegResult::TooLarge);  // TooLarge beats ScratchTooSmall
  CHECK(seg_init(v, kU32Max, exact.get(), 80) == SegResult::TooLarge);
  CHECK(seg_init(v, kU32Max, exact.get(), 0) == SegResult::TooLarge);
  CHECK(seg_init(v, 10, shorter.get(), 79) == SegResult::ScratchTooSmall);
  CHECK(seg_init(v, 10, exact.get(), 0) == SegResult::ScratchTooSmall);
  CHECK(seg_init(v, 10, exact.get(), 79) == SegResult::ScratchTooSmall);
  CHECK(same(before_exact, exact.get()));
  CHECK(same(before_short, shorter.get()));
  // n = 0: both pointers may be null, the need is 0, nothing is written
  CHECK(seg_init(nullptr, 0, nullptr, 0) == SegResult::Ok);
  CHECK(seg_init(nullptr, 0, exact.get(), 80) == SegResult::Ok);
  CHECK(seg_init(v, 0, nullptr, 0) == SegResult::Ok);
  CHECK(same(before_exact, exact.get()));
  // storage_words above the need is accepted (nothing is asserted about words past 8 * n)
  std::unique_ptr<std::uint64_t[]> bigger(new std::uint64_t[180]);
  REQUIRE(seg_init(v, 10, bigger.get(), 180) == SegResult::Ok);
  std::uint64_t out = kSentinel;
  REQUIRE(seg_range_sum(bigger.get(), 180, 10, 0, 10, &out) == SegResult::Ok);
  Oracle o(vals);
  CHECK(out == o.sum(0, 10));
}

TEST_CASE("check order of seg_range_add: Null, TooLarge, ScratchTooSmall, OutOfRange; the buffer is untouched on failure", "[args][segtree]") {
  const std::vector<std::uint64_t> vals = fixed_values(10);
  std::unique_ptr<std::uint64_t[]> exact(new std::uint64_t[80]);
  std::unique_ptr<std::uint64_t[]> shorter(new std::uint64_t[79]);
  REQUIRE(seg_init(vals.data(), 10, exact.get(), 80) == SegResult::Ok);
  for (std::uint32_t i = 0; i < 79; ++i) shorter[i] = kSentinel + i;
  const auto before_exact = snapshot(exact.get(), 80);
  const auto before_short = snapshot(shorter.get(), 79);
  std::uint64_t* e = exact.get();
  CHECK(seg_range_add(nullptr, 80, 10, 2, 5, 1) == SegResult::NullArgument);
  CHECK(seg_range_add(nullptr, 0, 10, 2, 5, 1) == SegResult::NullArgument);   // Null beats ScratchTooSmall
  CHECK(seg_range_add(nullptr, 80, 10, 5, 3, 1) == SegResult::NullArgument);  // Null beats OutOfRange
  CHECK(seg_range_add(nullptr, 80, kOver, 2, 5, 1) == SegResult::NullArgument);  // Null beats TooLarge
  CHECK(seg_range_add(nullptr, 80, kU32Max, 2, 5, 1) == SegResult::NullArgument);
  CHECK(seg_range_add(nullptr, 0, kU32Max, kU32Max, 0, 1) == SegResult::NullArgument);
  CHECK(seg_range_add(e, 80, kOver, 2, 5, 1) == SegResult::TooLarge);
  CHECK(seg_range_add(e, 0, kOver, 5, 3, 1) == SegResult::TooLarge);  // TooLarge beats ScratchTooSmall and OutOfRange
  CHECK(seg_range_add(e, 80, kU32Max, 2, 5, 1) == SegResult::TooLarge);
  CHECK(seg_range_add(e, 0, kU32Max, kU32Max, 0, 1) == SegResult::TooLarge);
  CHECK(seg_range_add(shorter.get(), 79, 10, 2, 5, 1) == SegResult::ScratchTooSmall);
  CHECK(seg_range_add(shorter.get(), 79, 10, 5, 3, 1) == SegResult::ScratchTooSmall);  // ScratchTooSmall beats OutOfRange
  CHECK(seg_range_add(shorter.get(), 79, 10, 0, 11, 1) == SegResult::ScratchTooSmall);
  CHECK(seg_range_add(e, 0, 10, 2, 5, 1) == SegResult::ScratchTooSmall);
  CHECK(seg_range_add(e, 79, 10, 2, 5, 1) == SegResult::ScratchTooSmall);
  CHECK(seg_range_add(e, 80, 10, 5, 3, 1) == SegResult::OutOfRange);  // l above r although r is within n
  CHECK(seg_range_add(e, 80, 10, 10, 9, 1) == SegResult::OutOfRange);
  CHECK(seg_range_add(e, 80, 10, 0, 11, 1) == SegResult::OutOfRange);
  CHECK(seg_range_add(e, 80, 10, 0, kU32Max, 1) == SegResult::OutOfRange);
  CHECK(seg_range_add(e, 80, 10, kU32Max, 10, 1) == SegResult::OutOfRange);
  CHECK(seg_range_add(e, 80, 10, kU32Max, kU32Max, 1) == SegResult::OutOfRange);
  CHECK(seg_range_add(e, 80, 10, 11, 11, 1) == SegResult::OutOfRange);  // empty but past n
  CHECK(seg_range_add(e, 80, 10, 11, 10, 1) == SegResult::OutOfRange);
  CHECK(same(before_exact, e));
  CHECK(same(before_short, shorter.get()));
  // valid empty ranges are Ok everywhere up to n; l = r = n is Ok
  Oracle o(vals);
  CHECK(seg_range_add(e, 80, 10, 0, 0, 5) == SegResult::Ok);
  CHECK(seg_range_add(e, 80, 10, 4, 4, 5) == SegResult::Ok);
  CHECK(seg_range_add(e, 80, 10, 10, 10, 5) == SegResult::Ok);
  for (std::uint32_t l = 0; l <= 10; ++l) {
    for (std::uint32_t r = l; r <= 10; ++r) {
      std::uint64_t out = kSentinel;
      CHECK(seg_range_sum(e, 80, 10, l, r, &out) == SegResult::Ok);
      CHECK(out == o.sum(l, r));
    }
  }
  // n = 0: null storage is fine, only the empty range at 0 is valid
  CHECK(seg_range_add(nullptr, 0, 0, 0, 0, 9) == SegResult::Ok);
  CHECK(seg_range_add(e, 80, 0, 0, 0, 9) == SegResult::Ok);
  CHECK(seg_range_add(nullptr, 0, 0, 0, 1, 9) == SegResult::OutOfRange);
  CHECK(seg_range_add(nullptr, 0, 0, 1, 1, 9) == SegResult::OutOfRange);
  CHECK(seg_range_add(nullptr, 0, 0, 1, 0, 9) == SegResult::OutOfRange);
}

TEST_CASE("check order of seg_range_sum: Null (a null out always), TooLarge, ScratchTooSmall, OutOfRange; *out untouched on failure", "[args][segtree]") {
  const std::vector<std::uint64_t> vals = fixed_values(10);
  std::unique_ptr<std::uint64_t[]> exact(new std::uint64_t[80]);
  std::unique_ptr<std::uint64_t[]> shorter(new std::uint64_t[79]);
  REQUIRE(seg_init(vals.data(), 10, exact.get(), 80) == SegResult::Ok);
  for (std::uint32_t i = 0; i < 79; ++i) shorter[i] = kSentinel + i;
  std::uint64_t* e = exact.get();
  std::uint64_t out = kSentinel;
  CHECK(seg_range_sum(e, 80, 10, 2, 5, nullptr) == SegResult::NullArgument);
  CHECK(seg_range_sum(nullptr, 0, 0, 0, 0, nullptr) == SegResult::NullArgument);  // a null out is an error even for n = 0
  CHECK(seg_range_sum(e, 80, 0, 0, 0, nullptr) == SegResult::NullArgument);
  CHECK(seg_range_sum(e, 80, kOver, 5, 3, nullptr) == SegResult::NullArgument);  // Null beats everything
  CHECK(seg_range_sum(e, 0, kU32Max, kU32Max, 0, nullptr) == SegResult::NullArgument);
  CHECK(seg_range_sum(nullptr, 80, 10, 2, 5, &out) == SegResult::NullArgument);
  CHECK(seg_range_sum(nullptr, 0, 10, 2, 5, &out) == SegResult::NullArgument);
  CHECK(seg_range_sum(nullptr, 80, 10, 5, 3, &out) == SegResult::NullArgument);
  CHECK(seg_range_sum(nullptr, 80, kOver, 2, 5, &out) == SegResult::NullArgument);
  CHECK(seg_range_sum(nullptr, 80, kU32Max, 2, 5, &out) == SegResult::NullArgument);
  CHECK(seg_range_sum(nullptr, 0, kU32Max, kU32Max, 0, &out) == SegResult::NullArgument);
  CHECK(seg_range_sum(e, 80, kOver, 2, 5, &out) == SegResult::TooLarge);
  CHECK(seg_range_sum(e, 0, kOver, 5, 3, &out) == SegResult::TooLarge);
  CHECK(seg_range_sum(e, 80, kU32Max, 2, 5, &out) == SegResult::TooLarge);
  CHECK(seg_range_sum(e, 0, kU32Max, kU32Max, 0, &out) == SegResult::TooLarge);
  CHECK(seg_range_sum(shorter.get(), 79, 10, 2, 5, &out) == SegResult::ScratchTooSmall);
  CHECK(seg_range_sum(shorter.get(), 79, 10, 5, 3, &out) == SegResult::ScratchTooSmall);
  CHECK(seg_range_sum(shorter.get(), 79, 10, 0, 11, &out) == SegResult::ScratchTooSmall);
  CHECK(seg_range_sum(e, 0, 10, 2, 5, &out) == SegResult::ScratchTooSmall);
  CHECK(seg_range_sum(e, 79, 10, 2, 5, &out) == SegResult::ScratchTooSmall);
  CHECK(seg_range_sum(e, 80, 10, 5, 3, &out) == SegResult::OutOfRange);
  CHECK(seg_range_sum(e, 80, 10, 10, 9, &out) == SegResult::OutOfRange);
  CHECK(seg_range_sum(e, 80, 10, 0, 11, &out) == SegResult::OutOfRange);
  CHECK(seg_range_sum(e, 80, 10, 0, kU32Max, &out) == SegResult::OutOfRange);
  CHECK(seg_range_sum(e, 80, 10, kU32Max, 10, &out) == SegResult::OutOfRange);
  CHECK(seg_range_sum(e, 80, 10, kU32Max, kU32Max, &out) == SegResult::OutOfRange);
  CHECK(seg_range_sum(e, 80, 10, 11, 11, &out) == SegResult::OutOfRange);
  CHECK(out == kSentinel);
  // success writes *out, an empty range writes 0, and n = 0 writes 0 with null storage
  CHECK(seg_range_sum(e, 80, 10, 3, 3, &out) == SegResult::Ok);
  CHECK(out == 0u);
  out = kSentinel;
  CHECK(seg_range_sum(e, 80, 10, 10, 10, &out) == SegResult::Ok);
  CHECK(out == 0u);
  out = kSentinel;
  CHECK(seg_range_sum(nullptr, 0, 0, 0, 0, &out) == SegResult::Ok);
  CHECK(out == 0u);
  out = kSentinel;
  CHECK(seg_range_sum(e, 80, 0, 0, 0, &out) == SegResult::Ok);
  CHECK(out == 0u);
  out = kSentinel;
  CHECK(seg_range_sum(nullptr, 0, 0, 0, 1, &out) == SegResult::OutOfRange);
  CHECK(seg_range_sum(nullptr, 0, 0, 1, 0, &out) == SegResult::OutOfRange);
  CHECK(out == kSentinel);
}

TEST_CASE("sizes: one over the limit and UINT32_MAX are TooLarge on every function, with tiny buffers that must not be read", "[max][segtree]") {
  const std::uint64_t one[1] = {kM64};
  std::uint64_t st[1] = {kSentinel};
  std::uint64_t out = kSentinel;
  std::uint32_t w = 77;
  for (std::uint32_t n : {kOver, kU32Max}) {
    CHECK(seg_storage_words(n, &w) == SegResult::TooLarge);
    CHECK(seg_init(one, n, st, 1) == SegResult::TooLarge);
    CHECK(seg_init(one, n, st, kU32Max) == SegResult::TooLarge);
    CHECK(seg_range_add(st, 1, n, 0, 1, 1) == SegResult::TooLarge);
    CHECK(seg_range_add(st, kU32Max, n, 0, 1, 1) == SegResult::TooLarge);
    CHECK(seg_range_sum(st, 1, n, 0, 1, &out) == SegResult::TooLarge);
    CHECK(seg_range_sum(st, kU32Max, n, 0, 1, &out) == SegResult::TooLarge);
  }
  CHECK(w == 77u);
  CHECK(st[0] == kSentinel);
  CHECK(out == kSentinel);
}

TEST_CASE("re-init: a second init over the same storage replaces everything, with a different n and two prefills", "[segtree]") {
  std::size_t bad = 0;
  Fold f;
  const std::uint32_t cap = 50;
  std::unique_ptr<std::uint64_t[]> st(new std::uint64_t[8u * cap]);
  Lcg g(99);
  for (std::uint32_t round = 0; round < 40; ++round) {
    const std::uint32_t n = g.next() % (cap + 1u);
    std::vector<std::uint64_t> vals(n);
    for (std::uint32_t i = 0; i < n; ++i) vals[i] = g.next64();
    for (std::uint32_t i = 0; i < 8u * cap; ++i) st[i] = round % 2u == 0u ? 0xA5A5A5A5A5A5A5A5ull : 0ull;
    if (seg_init(vals.empty() ? nullptr : vals.data(), n, st.get(), 8u * cap) != SegResult::Ok) ++bad;
    Oracle o(vals);
    for (std::uint32_t k = 0; k < 30; ++k) {
      const std::uint32_t a = g.next() % (n + 1u);
      const std::uint32_t b = g.next() % (n + 1u);
      const std::uint32_t l = a < b ? a : b;
      const std::uint32_t r = a < b ? b : a;
      if (k % 2u == 0u) {
        const std::uint64_t d = g.next64();
        if (seg_range_add(st.get(), 8u * cap, n, l, r, d) != SegResult::Ok) ++bad;
        o.add(l, r, d);
      } else {
        std::uint64_t out = kSentinel;
        if (seg_range_sum(st.get(), 8u * cap, n, l, r, &out) != SegResult::Ok || out != o.sum(l, r)) ++bad;
      }
    }
    for (std::uint32_t l = 0; l <= n; ++l) {
      std::uint64_t out = kSentinel;
      if (seg_range_sum(st.get(), 8u * cap, n, l, n, &out) != SegResult::Ok || out != o.sum(l, n)) ++bad;
    }
  }
  (void)f;
  CHECK(bad == 0u);
  // a larger n than the storage holds is refused
  const std::vector<std::uint64_t> big = fixed_values(cap + 1u);
  CHECK(seg_init(big.data(), cap + 1u, st.get(), 8u * cap) == SegResult::ScratchTooSmall);
}

namespace {

// Random operations at one size against the oracle. Most ranges are short or full, a third are random. Sums are compared to the oracle at
// every `sum_stride`-th sum and at every checkpoint (32 sampled ranges, the full range and the 8 elements at the ends).
std::size_t random_run(std::uint32_t n, std::uint32_t ops, std::uint64_t seed, std::uint32_t sum_stride) {
  std::size_t bad = 0;
  Lcg g(seed);
  std::vector<std::uint64_t> vals(n);
  for (std::uint32_t i = 0; i < n; ++i) vals[i] = g.next64();
  Store st(n);
  st.prefill(static_cast<std::uint32_t>(seed));
  if (seg_init(vals.data(), n, st.get(), st.words) != SegResult::Ok) return 1;
  Oracle o(vals);
  auto pick = [&](std::uint32_t* l, std::uint32_t* r) {
    const std::uint32_t t = g.next() % 3u;
    if (t == 0u) {
      const std::uint32_t a = g.next() % (n + 1u);
      const std::uint32_t b = g.next() % (n + 1u);
      *l = a < b ? a : b;
      *r = a < b ? b : a;
    } else if (t == 1u) {
      *l = g.next() % (n + 1u);
      const std::uint32_t len = g.next() % 65u;
      *r = *l + len > n ? n : *l + len;
    } else {
      const std::uint32_t k = g.next() % 4u;
      *l = k == 0u ? 0u : (k == 1u ? n / 2u : (k == 2u ? 0u : n));
      *r = k == 0u ? n : (k == 1u ? n : (k == 2u ? n / 2u : n));
    }
  };
  auto check_sum = [&](std::uint32_t l, std::uint32_t r) {
    std::uint64_t out = kSentinel;
    if (seg_range_sum(st.get(), st.words, n, l, r, &out) != SegResult::Ok || out != o.sum(l, r)) ++bad;
  };
  std::uint32_t sums = 0;
  for (std::uint32_t k = 0; k < ops; ++k) {
    std::uint32_t l = 0;
    std::uint32_t r = 0;
    pick(&l, &r);
    if (g.next() % 2u == 0u) {
      const std::uint32_t mode = g.next() % 4u;
      const std::uint64_t d = mode == 0u ? g.next64() : (mode == 1u ? 1u : (mode == 2u ? kM64 : g.next64() >> 40));
      if (seg_range_add(st.get(), st.words, n, l, r, d) != SegResult::Ok) ++bad;
      o.add(l, r, d);
    } else {
      if (sums++ % sum_stride == 0u) {
        check_sum(l, r);
      } else {
        std::uint64_t out = 0;  // still run the query (it may reorganise the storage), only the comparison is skipped
        if (seg_range_sum(st.get(), st.words, n, l, r, &out) != SegResult::Ok) ++bad;
      }
    }
    if (k % 1000u == 999u || k + 1u == ops) {
      for (int j = 0; j < 32; ++j) {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        pick(&a, &b);
        check_sum(a, b);
      }
      check_sum(0, n);
      for (std::uint32_t j = 0; j < 4u && j < n; ++j) {
        check_sum(j, j + 1u);
        check_sum(n - 1u - j, n - j);
      }
    }
  }
  return bad;
}

}  // namespace

TEST_CASE("random operations against the oracle at n = 1..17, 1023..1025 (sampled)", "[sweep][segtree]") {
  std::size_t bad = 0;
  std::uint64_t seed = 1;
  for (std::uint32_t n : {1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 1023u, 1024u, 1025u}) bad += random_run(n, 3000, seed++, 1);
  CHECK(bad == 0u);
}

TEST_CASE("sizes: n = 65535 and 65536 (the limit, 524,288 words), about 20,000 random operations each, sampled against the oracle", "[max][segtree]") {
  CHECK(random_run(65535u, 20000, 7001, 10) == 0u);
  CHECK(random_run(65536u, 20000, 7002, 10) == 0u);
}

TEST_CASE("sizes: n = 65536 hand-checked totals with exact buffers", "[max][segtree]") {
  const std::uint32_t n = kMaxLen;
  std::vector<std::uint64_t> vals(n, kM64);
  Store st(n);
  REQUIRE(st.words == 524288u);
  REQUIRE(seg_init(vals.data(), n, st.get(), st.words) == SegResult::Ok);
  std::uint64_t out = kSentinel;
  CHECK(seg_range_sum(st.get(), st.words, n, 0, n, &out) == SegResult::Ok);
  CHECK(out == 0u - std::uint64_t{n});  // 65536 * (2^64 - 1) mod 2^64
  CHECK(seg_range_add(st.get(), st.words, n, 0, n, 1) == SegResult::Ok);
  CHECK(seg_range_sum(st.get(), st.words, n, 0, n, &out) == SegResult::Ok);
  CHECK(out == 0u);
  CHECK(seg_range_add(st.get(), st.words, n, n - 1u, n, 5) == SegResult::Ok);
  CHECK(seg_range_add(st.get(), st.words, n, 0, 1, 7) == SegResult::Ok);
  CHECK(seg_range_sum(st.get(), st.words, n, 0, n, &out) == SegResult::Ok);
  CHECK(out == 12u);
  CHECK(seg_range_sum(st.get(), st.words, n, n - 1u, n, &out) == SegResult::Ok);
  CHECK(out == 5u);
  CHECK(seg_range_sum(st.get(), st.words, n, 1, n - 1u, &out) == SegResult::Ok);
  CHECK(out == 0u);
  CHECK(seg_range_sum(st.get(), st.words, n, 0, n + 1u, &out) == SegResult::OutOfRange);
}

TEST_CASE("a null storage is an error for every n above zero, including 1 and 2", "[args][segtree]") {
  const std::uint64_t vals[2] = {5u, 6u};
  std::uint64_t out = kSentinel;
  for (std::uint32_t n : {1u, 2u, 3u}) {
    CHECK(seg_init(vals, n, nullptr, 8u * n) == SegResult::NullArgument);
    CHECK(seg_range_add(nullptr, 8u * n, n, 0, 1, 1) == SegResult::NullArgument);
    CHECK(seg_range_sum(nullptr, 8u * n, n, 0, 1, &out) == SegResult::NullArgument);
  }
  CHECK(out == kSentinel);
}
