// Tests for core/fenwick. Evidence label: unit tests only. The long streams are checked against digests printed by tools/fenwick_ref.py, an
// independent model on a plain list of elements.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "fenwick.h"

using namespace lumora::fenwick;

namespace {

constexpr std::int64_t kMax = INT64_MAX;
constexpr std::int64_t kMin = INT64_MIN;
constexpr std::uint64_t kFnvOff = 0xCBF29CE484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001B3ULL;

void fnv_add(std::uint64_t& h, std::uint64_t v) {
  for (int k = 0; k < 8; ++k) {
    h ^= (v >> (8 * k)) & 0xFF;
    h *= kFnvPrime;
  }
}

std::uint32_t xorshift(std::uint32_t s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

std::int64_t wrap_signed(std::uint64_t v) {
  if (v <= static_cast<std::uint64_t>(kMax)) return static_cast<std::int64_t>(v);
  return -static_cast<std::int64_t>(~v) - 1;
}

struct Case {
  std::uint32_t seed;
  std::uint64_t digest;
};

const Case kWrap[] = {
{0x00000001, 0xAFB5C24BF7A6A5F8ULL},
{0x00000002, 0xD8D447DE6E397C99ULL},
{0x00000003, 0x5495374BB42E3805ULL},
{0x00000007, 0x21D4BCF5DB1CC0B1ULL},
{0x0000000B, 0x6DE9C4CD680273E1ULL},
{0x00003039, 0xC34E694E42087215ULL},
{0xDEADBEEF, 0x65C49C3E1E8DFC25ULL},
{0x9E3779B9, 0x4E836DF4D96F428AULL},
{0x0000002A, 0x90CA4953A387009CULL},
{0x000007E8, 0x22D26E062CE04FC5ULL},
{0x00018697, 0xC4291CD549F2B87DULL},
{0x00007A69, 0xFCD294BDCAAA41B1ULL},
};

const Case kSearch[] = {
{0x00000001, 0x8E7EC3B3B89D6106ULL},
{0x00000002, 0x241696DA808CFAA4ULL},
{0x00000003, 0x241B9A9062247FF8ULL},
{0x00000007, 0xDDED07B752104FEEULL},
{0x0000000B, 0xAB07645D21A8745CULL},
{0x00003039, 0x8E16292B182C4AAEULL},
{0xDEADBEEF, 0x6D9B928FAB60BC13ULL},
{0x9E3779B9, 0xAFD03EC86AC4A99EULL},
{0x0000002A, 0x063315F123F50C91ULL},
{0x000007E8, 0xD54BDAEA555ED0B2ULL},
{0x00018697, 0xA99BCC04AA64453BULL},
{0x00007A69, 0xB86F2415818215B8ULL},
};

std::int64_t sum_range(const std::vector<std::int64_t>& tree, std::uint32_t n, std::uint32_t b, std::uint32_t e) {
  std::int64_t out = 12345;
  REQUIRE(range_sum(tree.data(), n, b, e, &out));
  return out;
}

std::uint64_t run_wrap(std::uint32_t seed, int steps) {
  std::uint64_t h = kFnvOff;
  std::uint32_t st = xorshift(seed);
  const std::uint32_t n = 1 + st % 40;
  std::vector<std::int64_t> tree(n);
  REQUIRE(init(tree.data(), n));
  for (int s = 0; s < steps; ++s) {
    st = xorshift(st);
    const std::uint32_t kind = st % 8;
    st = xorshift(st);
    const std::uint32_t i = st % n;
    st = xorshift(st);
    const std::uint32_t c = st % 6;
    st = xorshift(st);
    const std::uint32_t lo = st;
    st = xorshift(st);
    const std::uint32_t hi = st;
    std::int64_t delta = 0;
    switch (c) {
      case 0: delta = 1; break;
      case 1: delta = -1; break;
      case 2: delta = wrap_signed((static_cast<std::uint64_t>(hi) << 32) | lo); break;
      case 3: delta = kMax; break;
      case 4: delta = kMin; break;
      default: delta = static_cast<std::int64_t>(lo % 2001) - 1000; break;
    }
    st = xorshift(st);
    const std::uint32_t p = st % (n + 1);
    st = xorshift(st);
    const std::uint32_t q = st;
    std::int64_t r = 0;
    if (kind < 4) {
      REQUIRE(add(tree.data(), n, i, delta));
      fnv_add(h, 1);
    } else if (kind < 6) {
      REQUIRE(prefix_sum(tree.data(), n, p, &r));
      fnv_add(h, static_cast<std::uint64_t>(r));
    } else if (kind == 6) {
      const std::uint32_t e = p + q % (n + 1 - p);
      fnv_add(h, static_cast<std::uint64_t>(sum_range(tree, n, p, e)));
    } else {
      fnv_add(h, static_cast<std::uint64_t>(sum_range(tree, n, i, i + 1)));
    }
  }
  return h;
}

std::uint64_t run_search(std::uint32_t seed, int steps) {
  std::uint64_t h = kFnvOff;
  std::uint32_t st = xorshift(seed);
  const std::uint32_t n = 1 + st % 40;
  std::vector<std::int64_t> tree(n);
  REQUIRE(init(tree.data(), n));
  std::int64_t total = 0;
  for (int s = 0; s < steps; ++s) {
    st = xorshift(st);
    const std::uint32_t kind = st % 4;
    st = xorshift(st);
    const std::uint32_t i = st % n;
    st = xorshift(st);
    const std::uint32_t d = st % 50;
    st = xorshift(st);
    const std::uint32_t t = st;
    if (kind < 2) {
      REQUIRE(add(tree.data(), n, i, static_cast<std::int64_t>(d)));
      total += d;
      fnv_add(h, 1);
    } else {
      const std::int64_t target = static_cast<std::int64_t>(t % static_cast<std::uint64_t>(total + 4)) - 2;
      std::uint32_t k = 777;
      REQUIRE(first_prefix_at_least(tree.data(), n, target, &k));
      fnv_add(h, k);
    }
  }
  return h;
}

}  // namespace

TEST_CASE("empty and invalid trees", "[fenwick]") {
  std::int64_t out = 77;
  std::uint32_t k = 77;
  CHECK(init(nullptr, 0));
  CHECK_FALSE(init(nullptr, 1));
  CHECK_FALSE(init(nullptr, kMaxSize + 1));
  CHECK_FALSE(add(nullptr, 0, 0, 1));
  CHECK(prefix_sum(nullptr, 0, 0, &out));
  CHECK(out == 0);
  CHECK_FALSE(prefix_sum(nullptr, 0, 1, &out));
  CHECK(range_sum(nullptr, 0, 0, 0, &out));
  CHECK_FALSE(range_sum(nullptr, 0, 0, 1, &out));
  CHECK(first_prefix_at_least(nullptr, 0, 5, &k));
  CHECK(k == 1);  // n + 1: nothing reaches 5
  CHECK(first_prefix_at_least(nullptr, 0, 0, &k));
  CHECK(k == 0);
  std::int64_t dummy[1] = {99};
  out = 77;
  k = 77;
  CHECK_FALSE(prefix_sum(dummy, kMaxSize + 1, 0, &out));
  CHECK_FALSE(add(dummy, kMaxSize + 1, 0, 1));
  CHECK_FALSE(range_sum(dummy, kMaxSize + 1, 0, 0, &out));
  CHECK_FALSE(first_prefix_at_least(dummy, kMaxSize + 1, 1, &k));
  CHECK(out == 77);
  CHECK(k == 77);
  CHECK(dummy[0] == 99);
  CHECK_FALSE(prefix_sum(dummy, 1, 0, nullptr));
  CHECK_FALSE(range_sum(dummy, 1, 0, 0, nullptr));
  CHECK_FALSE(first_prefix_at_least(dummy, 1, 1, nullptr));
}

TEST_CASE("failed calls change nothing", "[fenwick]") {
  std::vector<std::int64_t> t(5);
  REQUIRE(init(t.data(), 5));
  for (std::uint32_t i = 0; i < 5; ++i) REQUIRE(add(t.data(), 5, i, static_cast<std::int64_t>(i + 1)));
  const std::vector<std::int64_t> before = t;
  std::int64_t out = 55;
  CHECK_FALSE(add(t.data(), 5, 5, 9));
  CHECK_FALSE(add(t.data(), 5, 0xFFFFFFFFu, 9));
  CHECK(t == before);
  CHECK_FALSE(prefix_sum(t.data(), 5, 6, &out));
  CHECK_FALSE(range_sum(t.data(), 5, 3, 2, &out));
  CHECK_FALSE(range_sum(t.data(), 5, 0, 6, &out));
  CHECK_FALSE(range_sum(t.data(), 5, 6, 6, &out));
  CHECK(out == 55);
}

TEST_CASE("small hand cases", "[fenwick]") {
  std::vector<std::int64_t> t(8);
  REQUIRE(init(t.data(), 8));
  const std::int64_t vals[8] = {3, 1, 4, 1, 5, 9, 2, 6};
  for (std::uint32_t i = 0; i < 8; ++i) REQUIRE(add(t.data(), 8, i, vals[i]));
  std::int64_t run = 0;
  for (std::uint32_t c = 0; c <= 8; ++c) {
    std::int64_t out = -1;
    REQUIRE(prefix_sum(t.data(), 8, c, &out));
    CHECK(out == run);
    if (c < 8) run += vals[c];
  }
  for (std::uint32_t b = 0; b <= 8; ++b) {
    for (std::uint32_t e = b; e <= 8; ++e) {
      std::int64_t want = 0;
      for (std::uint32_t k = b; k < e; ++k) want += vals[k];
      CHECK(sum_range(t, 8, b, e) == want);
    }
  }
  REQUIRE(add(t.data(), 8, 3, -1));  // element 3 becomes 0
  CHECK(sum_range(t, 8, 3, 4) == 0);
  CHECK(sum_range(t, 8, 0, 8) == 30);
  REQUIRE(init(t.data(), 8));
  CHECK(sum_range(t, 8, 0, 8) == 0);
}

TEST_CASE("sums wrap modulo 2^64", "[fenwick]") {
  std::vector<std::int64_t> t(4);
  REQUIRE(init(t.data(), 4));
  REQUIRE(add(t.data(), 4, 0, kMax));
  REQUIRE(add(t.data(), 4, 1, 1));
  CHECK(sum_range(t, 4, 0, 2) == kMin);
  REQUIRE(add(t.data(), 4, 2, kMin));
  CHECK(sum_range(t, 4, 0, 3) == 0);
  CHECK(sum_range(t, 4, 2, 3) == kMin);
  REQUIRE(add(t.data(), 4, 2, kMin));  // element 2 wraps to 0
  CHECK(sum_range(t, 4, 2, 3) == 0);
  REQUIRE(add(t.data(), 4, 3, kMin));
  REQUIRE(add(t.data(), 4, 3, -1));
  CHECK(sum_range(t, 4, 3, 4) == kMax);
}

TEST_CASE("first_prefix_at_least hand cases", "[fenwick]") {
  std::vector<std::int64_t> t(6);
  REQUIRE(init(t.data(), 6));
  const std::int64_t vals[6] = {0, 2, 0, 3, 0, 5};
  for (std::uint32_t i = 0; i < 6; ++i) REQUIRE(add(t.data(), 6, i, vals[i]));
  struct Q { std::int64_t target; std::uint32_t want; };
  const Q qs[] = {{-5, 0}, {0, 0}, {1, 2}, {2, 2}, {3, 4}, {5, 4}, {6, 6}, {10, 6}, {11, 7}, {kMax, 7}};
  for (const Q& q : qs) {
    std::uint32_t k = 999;
    REQUIRE(first_prefix_at_least(t.data(), 6, q.target, &k));
    CHECK(k == q.want);
  }
  std::vector<std::int64_t> one(1);
  REQUIRE(init(one.data(), 1));
  std::uint32_t k = 999;
  REQUIRE(first_prefix_at_least(one.data(), 1, 1, &k));
  CHECK(k == 2);
  REQUIRE(add(one.data(), 1, 0, 1));
  REQUIRE(first_prefix_at_least(one.data(), 1, 1, &k));
  CHECK(k == 1);
}

TEST_CASE("init clears a tree that holds old values", "[fenwick]") {
  for (std::uint32_t n = 1; n <= 20; ++n) {
    std::vector<std::int64_t> t(n, 0x1234567);
    REQUIRE(init(t.data(), n));
    for (std::uint32_t i = 0; i < n; ++i) CHECK(t[i] == 0);
    CHECK(sum_range(t, n, 0, n) == 0);
  }
}

TEST_CASE("every size from 1 to 70 matches a naive sum", "[fenwick]") {
  for (std::uint32_t n = 1; n <= 70; ++n) {
    std::vector<std::int64_t> t(n);
    REQUIRE(init(t.data(), n));
    std::vector<std::int64_t> el(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      el[i] = static_cast<std::int64_t>(i * 7 % 11);
      REQUIRE(add(t.data(), n, i, el[i]));
    }
    std::int64_t run = 0;
    for (std::uint32_t c = 0; c <= n; ++c) {
      std::int64_t out = -1;
      REQUIRE(prefix_sum(t.data(), n, c, &out));
      CHECK(out == run);
      std::uint32_t k = 999;
      REQUIRE(first_prefix_at_least(t.data(), n, run, &k));
      std::uint32_t want = 0;
      std::int64_t acc = 0;
      while (acc < run) acc += el[want++];
      CHECK(k == want);
      if (c < n) run += el[c];
    }
    std::uint32_t k = 999;
    REQUIRE(first_prefix_at_least(t.data(), n, run + 1, &k));
    CHECK(k == n + 1);
  }
}

TEST_CASE("long wrap streams match the independent model", "[fenwick]") {
  for (const Case& c : kWrap) CHECK(run_wrap(c.seed, 600) == c.digest);
}

TEST_CASE("long search streams match the independent model", "[fenwick]") {
  for (const Case& c : kSearch) CHECK(run_search(c.seed, 300) == c.digest);
}
