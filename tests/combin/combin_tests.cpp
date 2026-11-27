// Tests for core/combin. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/combin_ref.py, an independent Python implementation (arbitrary
//      precision integers and math.comb, cross-checked by the script against Pascal's triangle and binary-searched tables). The tests re-run every
//      scenario against the real functions and recompute the 64-bit digest and the counts.
//  (2) ORACLES inside the test that share no code with core/combin/combin.cpp: (a) Pascal's triangle with a saturation flag (rows 0..141) for the
//      small grids; (b) for every case, an exact big integer check: the numerator P (product of the factors, each formed as an exact integer
//      that may need 65 bits) and the denominator Q (a factorial) are built in a vector-of-limbs big integer; the result is Overflow exactly when
//      P > (2^64 - 1) * Q, and a success is checked as value * Q == P. Where the smaller symmetric argument is above 70 the expected result is
//      Overflow (C(N, kk) with N >= 2 kk is at least C(2 kk, kk), above 2^64 - 1 from kk = 34, checked directly up to 70 and by Pascal below).
//  (3) CONSTANTS: the header values and the hand rows were computed once with Python integers; the null and untouched-output rows are written from
//      the header text.
// After every failure the output is compared with the sentinel that was passed (the header promises "untouched").
// What is NOT done: no timing test (every loop in the implementation has a fixed bound, argued in the docs); Windows is CI-only; the grids are NOT
// exhaustive over all u64 arguments (the extents are named in each test and in docs/combin.md).
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "combin.h"

using lumora::combin::CombResult;
using lumora::combin::binom_u64;
using lumora::combin::catalan_u64;
using lumora::combin::factorial_u64;
using lumora::combin::multichoose_u64;

namespace {

constexpr std::uint64_t kU64 = 0xFFFFFFFFFFFFFFFFull;
constexpr std::uint64_t kSentinel = 0xA5A5A5A55A5A5A5Aull;

// BEGIN GENERATED REFERENCE (tools/combin_ref.py)
const std::uint64_t kRefS[] = {0ull, 1ull, 2ull, 3ull, 4ull, 5ull, 6ull, 7ull, 8ull, 9ull, 10ull, 11ull, 12ull, 13ull, 14ull, 15ull, 16ull, 17ull, 18ull, 19ull, 20ull, 21ull, 22ull, 23ull, 24ull, 25ull, 26ull, 27ull, 28ull, 29ull, 30ull, 31ull, 32ull, 33ull, 34ull, 35ull, 36ull, 37ull, 38ull, 39ull, 40ull, 41ull, 42ull, 43ull, 44ull, 45ull, 46ull, 47ull, 48ull, 49ull, 50ull, 51ull, 52ull, 53ull, 54ull, 55ull, 56ull, 57ull, 58ull, 61ull, 62ull, 63ull, 64ull, 65ull, 66ull, 67ull, 68ull, 69ull, 70ull, 71ull, 72ull, 73ull, 74ull, 75ull, 76ull, 77ull, 78ull, 79ull, 80ull, 81ull, 82ull, 83ull, 86ull, 87ull, 88ull, 89ull, 90ull, 91ull, 92ull, 97ull, 98ull, 100ull, 101ull, 102ull, 105ull, 106ull, 115ull, 116ull, 117ull, 130ull, 131ull, 136ull, 137ull, 138ull, 150ull, 151ull, 164ull, 165ull, 166ull, 177ull, 178ull, 206ull, 207ull, 208ull, 218ull, 219ull, 271ull, 272ull, 273ull, 282ull, 283ull, 376ull, 377ull, 378ull, 386ull, 387ull, 568ull, 569ull, 570ull, 577ull, 578ull, 959ull, 960ull, 961ull, 967ull, 968ull, 1906ull, 1907ull, 1908ull, 1913ull, 1914ull, 4862ull, 4863ull, 4864ull, 4868ull, 4869ull, 18575ull, 18576ull, 18577ull, 18580ull, 18581ull, 145052ull, 145053ull, 145054ull, 145056ull, 145057ull, 4801277ull, 4801278ull, 4801279ull, 4801280ull, 4801281ull, 4294967295ull, 4294967296ull, 4294967297ull, 6074000998ull, 6074000999ull, 6074001000ull, 6074001001ull, 0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL, 0x8000000000000001ULL, 0xFFFFFFFFFFFFFFFCULL, 0xFFFFFFFFFFFFFFFDULL, 0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFFULL};
const std::size_t kRefSizeS = 175;
const std::uint64_t kRefDigest_bsmall = 0x9D06A84D6FEB0F3CULL;
const std::size_t kRefCases_bsmall = 3483;
const std::uint64_t kRefDigest_bgrid = 0x09DC47228342AB7AULL;
const std::size_t kRefCases_bgrid = 30625;
const std::uint64_t kRefDigest_bnear = 0x7C43E2A5DC48F219ULL;
const std::size_t kRefCases_bnear = 6355;
const std::uint64_t kRefDigest_msmall = 0x30650577BFCB71B5ULL;
const std::size_t kRefCases_msmall = 5041;
const std::uint64_t kRefDigest_mgrid = 0xDAEFAF1184C7482FULL;
const std::size_t kRefCases_mgrid = 30625;
const std::uint64_t kRefDigest_mnear = 0x46B6F9473E8ECC17ULL;
const std::size_t kRefCases_mnear = 14268;
const std::uint64_t kRefDigest_fact = 0xF0ACCB823CE6A649ULL;
const std::size_t kRefCases_fact = 25;
const std::uint64_t kRefDigest_cat = 0x1C9B9BD7D2232C5BULL;
const std::size_t kRefCases_cat = 44;
// END GENERATED REFERENCE

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

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Oracle (a): Pascal's triangle with saturation, rows 0..141. Not shared with combin.cpp.
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct Sat {
  std::uint64_t v;
  bool over;
};
constexpr int kRows = 142;
Sat g_pas[kRows][kRows];
void build_pascal() {
  for (int n = 0; n < kRows; ++n)
    for (int k = 0; k < kRows; ++k) g_pas[n][k] = Sat{0, false};
  for (int n = 0; n < kRows; ++n) {
    g_pas[n][0] = Sat{1, false};
    for (int k = 1; k <= n; ++k) {
      const Sat a = g_pas[n - 1][k - 1];
      const Sat b = (k <= n - 1) ? g_pas[n - 1][k] : Sat{0, false};
      Sat s;
      s.over = a.over || b.over || a.v > kU64 - b.v;
      s.v = s.over ? 0 : a.v + b.v;
      g_pas[n][k] = s;
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Oracle (b): big integer, little-endian 32-bit limbs, no fixed size.
// ---------------------------------------------------------------------------------------------------------------------------------------------
typedef std::vector<std::uint32_t> Big;
void trim(Big& a) {
  while (!a.empty() && a.back() == 0) a.pop_back();
}
Big big_u64(std::uint64_t v) {
  Big b;
  b.push_back(static_cast<std::uint32_t>(v & 0xFFFFFFFFull));
  b.push_back(static_cast<std::uint32_t>(v >> 32));
  trim(b);
  return b;
}
Big big_add(const Big& a, const Big& b) {
  Big r;
  std::uint64_t carry = 0;
  for (std::size_t i = 0; i < a.size() || i < b.size() || carry != 0; ++i) {
    const std::uint64_t cur = (i < a.size() ? a[i] : 0u) + static_cast<std::uint64_t>(i < b.size() ? b[i] : 0u) + carry;
    r.push_back(static_cast<std::uint32_t>(cur & 0xFFFFFFFFull));
    carry = cur >> 32;
  }
  trim(r);
  return r;
}
Big big_sub(const Big& a, const Big& b) {  // a >= b
  Big r;
  std::int64_t borrow = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    std::int64_t cur = static_cast<std::int64_t>(a[i]) - (i < b.size() ? static_cast<std::int64_t>(b[i]) : 0) - borrow;
    borrow = 0;
    if (cur < 0) {
      cur += 0x100000000ll;
      borrow = 1;
    }
    r.push_back(static_cast<std::uint32_t>(cur));
  }
  trim(r);
  return r;
}
Big big_mul(const Big& a, const Big& b) {
  Big r(a.size() + b.size() + 1, 0u);
  for (std::size_t i = 0; i < a.size(); ++i) {
    std::uint64_t carry = 0;
    for (std::size_t j = 0; j < b.size() || carry != 0; ++j) {
      const std::uint64_t cur = static_cast<std::uint64_t>(r[i + j]) + static_cast<std::uint64_t>(a[i]) * (j < b.size() ? b[j] : 0u) + carry;
      r[i + j] = static_cast<std::uint32_t>(cur & 0xFFFFFFFFull);
      carry = cur >> 32;
    }
  }
  trim(r);
  return r;
}
int big_cmp(const Big& a, const Big& b) {
  if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
  for (std::size_t i = a.size(); i-- > 0;)
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  return 0;
}

// Expected result for P / Q (Q is nonzero and divides P): fits iff P <= (2^64 - 1) * Q.
bool check_ratio(const Big& p, const Big& q, CombResult res, std::uint64_t value) {
  const bool over = big_cmp(p, big_mul(q, big_u64(kU64))) > 0;
  if (over) return res == CombResult::Overflow;
  if (res != CombResult::Ok) return false;
  return big_cmp(big_mul(big_u64(value), q), p) == 0;
}
Big big_fact(std::uint64_t n) {
  Big q = big_u64(1);
  for (std::uint64_t i = 2; i <= n; ++i) q = big_mul(q, big_u64(i));
  return q;
}
// C(N, kk) = prod_{i=1..kk} (N - kk + i) / kk!, with N = nbig (exact, up to 65 bits) and kk <= 70.
bool check_choose(const Big& nbig, std::uint64_t kk, CombResult res, std::uint64_t value) {
  if (kk > 70) return res == CombResult::Overflow;
  Big p = big_u64(1);
  const Big base = big_sub(nbig, big_u64(kk));
  for (std::uint64_t i = 1; i <= kk; ++i) p = big_mul(p, big_add(base, big_u64(i)));
  return check_ratio(p, big_fact(kk), res, value);
}

std::uint32_t code(CombResult r) { return static_cast<std::uint32_t>(r); }

void fold_res(Fold& f, CombResult r, std::uint64_t v) {
  ++f.cases;
  f.add(code(r));
  if (r == CombResult::Ok) f.add(v);
}

void do_binom(std::uint64_t n, std::uint64_t k, Fold& f, Tally& t) {
  std::uint64_t out = kSentinel;
  const CombResult r = binom_u64(n, k, &out);
  bool ok;
  if (r != CombResult::Ok && r != CombResult::Overflow) ok = false;
  else if (r == CombResult::Overflow && out != kSentinel) ok = false;
  else if (k > n) ok = (r == CombResult::Ok && out == 0);
  else {
    const std::uint64_t kk = (n - k < k) ? n - k : k;
    ok = check_choose(big_u64(n), kk, r, out);
  }
  if (!ok) t.fail("binom(" + std::to_string(n) + ", " + std::to_string(k) + ") gave " + std::to_string(code(r)) + " / " + std::to_string(out));
  fold_res(f, r, out);
}

void do_multi(std::uint64_t n, std::uint64_t k, Fold& f, Tally& t) {
  std::uint64_t out = kSentinel;
  const CombResult r = multichoose_u64(n, k, &out);
  bool ok;
  if (r != CombResult::Ok && r != CombResult::Overflow) ok = false;
  else if (r == CombResult::Overflow && out != kSentinel) ok = false;
  else if (n == 0) ok = (r == CombResult::Ok && out == (k == 0 ? 1u : 0u));
  else {
    const std::uint64_t m = n - 1;
    const std::uint64_t kk = (m < k) ? m : k;
    const Big nbig = big_add(big_u64(m), big_u64(k));  // N = n + k - 1, exact
    ok = check_choose(nbig, kk, r, out);
  }
  if (!ok) t.fail("multichoose(" + std::to_string(n) + ", " + std::to_string(k) + ") gave " + std::to_string(code(r)) + " / " + std::to_string(out));
  fold_res(f, r, out);
}

void do_fact(std::uint32_t n, Fold& f, Tally& t) {
  std::uint64_t out = kSentinel;
  const CombResult r = factorial_u64(n, &out);
  bool ok;
  if (r == CombResult::Overflow && out != kSentinel) ok = false;
  else if (n > 40) ok = (r == CombResult::Overflow);
  else ok = (r == CombResult::Ok || r == CombResult::Overflow) && check_ratio(big_fact(n), big_u64(1), r, out);
  if (!ok) t.fail("factorial(" + std::to_string(n) + ") gave " + std::to_string(code(r)) + " / " + std::to_string(out));
  fold_res(f, r, out);
}

void do_cat(std::uint32_t n, Fold& f, Tally& t) {
  std::uint64_t out = kSentinel;
  const CombResult r = catalan_u64(n, &out);
  bool ok;
  if (r == CombResult::Overflow && out != kSentinel) ok = false;
  else if (n > 40) ok = (r == CombResult::Overflow);
  else {
    Big p = big_u64(1);
    for (std::uint64_t i = 1; i <= n; ++i) p = big_mul(p, big_u64(n + i));
    const Big q = big_mul(big_fact(n), big_u64(static_cast<std::uint64_t>(n) + 1));  // C_n = C(2n, n) / (n + 1) = P / (n! (n + 1))
    ok = (r == CombResult::Ok || r == CombResult::Overflow) && check_ratio(p, q, r, out);
  }
  if (!ok) t.fail("catalan(" + std::to_string(n) + ") gave " + std::to_string(code(r)) + " / " + std::to_string(out));
  fold_res(f, r, out);
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.h == digest);
}

}  // namespace

TEST_CASE("list sizes match the reference", "[combin]") {
  CHECK(sizeof kRefS / sizeof kRefS[0] == kRefSizeS);
}

// EXHAUSTIVE: n in 0..80, k in 0..n+2; every result also against Pascal's triangle with saturation.
TEST_CASE("binom: n 0..80, every k, against Pascal", "[combin]") {
  build_pascal();
  Fold f;
  Tally t;
  for (std::uint64_t n = 0; n <= 80; ++n)
    for (std::uint64_t k = 0; k <= n + 2; ++k) {
      do_binom(n, k, f, t);
      std::uint64_t out = kSentinel;
      const CombResult r = binom_u64(n, k, &out);
      if (k > n) {
        CHECK((r == CombResult::Ok && out == 0));
      } else {
        const Sat s = g_pas[n][k];
        CHECK((s.over ? r == CombResult::Overflow : (r == CombResult::Ok && out == s.v)));
      }
    }
  expect_digest(f, t, kRefCases_bsmall, kRefDigest_bsmall);
}

// EXHAUSTIVE over S x S (S: 0..40, around 2^32, 2^63, 2^64 - 1, and the largest fitting n for each k and the largest fitting k for each m, +1, +2).
TEST_CASE("binom: S x S grid", "[combin]") {
  Fold f;
  Tally t;
  for (std::size_t i = 0; i < kRefSizeS; ++i)
    for (std::size_t j = 0; j < kRefSizeS; ++j) do_binom(kRefS[i], kRefS[j], f, t);
  expect_digest(f, t, kRefCases_bgrid, kRefDigest_bgrid);
}

// EXHAUSTIVE over S x j: k = n - j for j = 0..40 (the symmetric side, k near n).
TEST_CASE("binom: k near n", "[combin]") {
  Fold f;
  Tally t;
  for (std::size_t i = 0; i < kRefSizeS; ++i)
    for (std::uint64_t j = 0; j <= 40; ++j)
      if (kRefS[i] >= j) do_binom(kRefS[i], kRefS[i] - j, f, t);
  expect_digest(f, t, kRefCases_bnear, kRefDigest_bnear);
}

// EXHAUSTIVE: n, k in 0..70, against Pascal (row n + k - 1 up to 139).
TEST_CASE("multichoose: n, k 0..70, against Pascal", "[combin]") {
  build_pascal();
  Fold f;
  Tally t;
  for (std::uint64_t n = 0; n <= 70; ++n)
    for (std::uint64_t k = 0; k <= 70; ++k) {
      do_multi(n, k, f, t);
      std::uint64_t out = kSentinel;
      const CombResult r = multichoose_u64(n, k, &out);
      if (n == 0) {
        CHECK((r == CombResult::Ok && out == (k == 0 ? 1u : 0u)));
      } else {
        const Sat s = g_pas[n + k - 1][k];
        CHECK((s.over ? r == CombResult::Overflow : (r == CombResult::Ok && out == s.v)));
      }
    }
  expect_digest(f, t, kRefCases_msmall, kRefDigest_msmall);
}

TEST_CASE("multichoose: S x S grid (both orders)", "[combin]") {
  Fold f;
  Tally t;
  for (std::size_t i = 0; i < kRefSizeS; ++i)
    for (std::size_t j = 0; j < kRefSizeS; ++j) do_multi(kRefS[i], kRefS[j], f, t);
  expect_digest(f, t, kRefCases_mgrid, kRefDigest_mgrid);
}

// EXHAUSTIVE over S x j, both orders: multichoose(n, j) and multichoose(j, n).
TEST_CASE("multichoose: one argument small, both orders", "[combin]") {
  Fold f;
  Tally t;
  for (std::size_t i = 0; i < kRefSizeS; ++i)
    if (kRefS[i] >= 1)
      for (std::uint64_t j = 0; j <= 40; ++j) {
        do_multi(kRefS[i], j, f, t);
        do_multi(j, kRefS[i], f, t);
      }
  expect_digest(f, t, kRefCases_mnear, kRefDigest_mnear);
}

TEST_CASE("factorial: 0..22, 2^31, 2^32 - 1", "[combin]") {
  Fold f;
  Tally t;
  for (std::uint32_t n = 0; n <= 22; ++n) do_fact(n, f, t);
  do_fact(2147483648u, f, t);
  do_fact(0xFFFFFFFFu, f, t);
  expect_digest(f, t, kRefCases_fact, kRefDigest_fact);
}

TEST_CASE("catalan: 0..40, 1000, 2^31, 2^32 - 1", "[combin]") {
  Fold f;
  Tally t;
  for (std::uint32_t n = 0; n <= 40; ++n) do_cat(n, f, t);
  do_cat(1000u, f, t);
  do_cat(2147483648u, f, t);
  do_cat(0xFFFFFFFFu, f, t);
  expect_digest(f, t, kRefCases_cat, kRefDigest_cat);
}

#define WANT(CALL, ER, EV) \
  do { \
    std::uint64_t o_ = kSentinel; \
    const CombResult r_ = CALL; \
    CHECK(r_ == (ER)); \
    CHECK(o_ == (EV)); \
  } while (0)

// CONSTANTS: the values the header states, and hand rows computed once with Python integers.
TEST_CASE("header values and hand rows (constants)", "[combin]") {
  WANT(binom_u64(67, 33, &o_), CombResult::Ok, 14226520737620288370ull);
  WANT(binom_u64(68, 34, &o_), CombResult::Overflow, kSentinel);
  WANT(binom_u64(kU64, 1, &o_), CombResult::Ok, kU64);
  WANT(binom_u64(kU64, 2, &o_), CombResult::Overflow, kSentinel);
  WANT(binom_u64(kU64, kU64 - 1, &o_), CombResult::Ok, kU64);
  WANT(binom_u64(kU64, 0, &o_), CombResult::Ok, 1);
  WANT(binom_u64(kU64, kU64, &o_), CombResult::Ok, 1);
  WANT(binom_u64(5, 7, &o_), CombResult::Ok, 0);
  WANT(binom_u64(0, 0, &o_), CombResult::Ok, 1);
  WANT(binom_u64(0, kU64, &o_), CombResult::Ok, 0);
  WANT(binom_u64(1ull << 32, 2, &o_), CombResult::Ok, 9223372034707292160ull);
  WANT(binom_u64((1ull << 32) + 1ull, 2, &o_), CombResult::Ok, 9223372039002259456ull);
  WANT(multichoose_u64(0, 0, &o_), CombResult::Ok, 1);
  WANT(multichoose_u64(0, 9, &o_), CombResult::Ok, 0);
  WANT(multichoose_u64(kU64, 0, &o_), CombResult::Ok, 1);
  WANT(multichoose_u64(kU64, 1, &o_), CombResult::Ok, kU64);
  WANT(multichoose_u64(1, kU64, &o_), CombResult::Ok, 1);
  WANT(multichoose_u64(2, kU64 - 1ull, &o_), CombResult::Ok, kU64);
  WANT(multichoose_u64(2, kU64, &o_), CombResult::Overflow, kSentinel);  // C(2^64, 1) = 2^64: the factor needs 65 bits
  WANT(multichoose_u64(kU64, kU64, &o_), CombResult::Overflow, kSentinel);
  WANT(multichoose_u64(3, kU64, &o_), CombResult::Overflow, kSentinel);
  WANT(multichoose_u64(kU64, 2, &o_), CombResult::Overflow, kSentinel);
  WANT(factorial_u64(0, &o_), CombResult::Ok, 1);
  WANT(factorial_u64(20, &o_), CombResult::Ok, 2432902008176640000ull);
  WANT(factorial_u64(21, &o_), CombResult::Overflow, kSentinel);
  WANT(catalan_u64(0, &o_), CombResult::Ok, 1);
  WANT(catalan_u64(1, &o_), CombResult::Ok, 1);
  WANT(catalan_u64(5, &o_), CombResult::Ok, 42);
  WANT(catalan_u64(36, &o_), CombResult::Ok, 11959798385860453492ull);
  WANT(catalan_u64(37, &o_), CombResult::Overflow, kSentinel);
}

TEST_CASE("null out: NullArgument first, even for values that overflow", "[combin]") {
  CHECK(binom_u64(5, 2, nullptr) == CombResult::NullArgument);
  CHECK(binom_u64(kU64, 2, nullptr) == CombResult::NullArgument);
  CHECK(binom_u64(5, 7, nullptr) == CombResult::NullArgument);
  CHECK(multichoose_u64(0, 0, nullptr) == CombResult::NullArgument);
  CHECK(multichoose_u64(kU64, kU64, nullptr) == CombResult::NullArgument);
  CHECK(factorial_u64(3, nullptr) == CombResult::NullArgument);
  CHECK(factorial_u64(100, nullptr) == CombResult::NullArgument);
  CHECK(catalan_u64(3, nullptr) == CombResult::NullArgument);
  CHECK(catalan_u64(100, nullptr) == CombResult::NullArgument);
}

TEST_CASE("Overflow leaves the output untouched", "[combin]") {
  std::uint64_t o = kSentinel;
  CHECK(binom_u64(100, 50, &o) == CombResult::Overflow);
  CHECK(o == kSentinel);
  CHECK(binom_u64(kU64, 40, &o) == CombResult::Overflow);
  CHECK(o == kSentinel);
  CHECK(multichoose_u64(1ull << 40, 3, &o) == CombResult::Overflow);
  CHECK(o == kSentinel);
  CHECK(factorial_u64(0xFFFFFFFFu, &o) == CombResult::Overflow);
  CHECK(o == kSentinel);
  CHECK(catalan_u64(0xFFFFFFFFu, &o) == CombResult::Overflow);
  CHECK(o == kSentinel);
  CHECK(catalan_u64(37, &o) == CombResult::Overflow);
  CHECK(o == kSentinel);
}
