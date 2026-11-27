// Tests for core/lyndon. Evidence label: unit tests only. Every comparison is exact.
// Where the expected values come from (three kinds, labelled in the code):
//  (1) DIGESTS: the block between BEGIN and END GENERATED REFERENCE comes from tools/lyndon_ref.py, an independent Python program (the factorization
//      by the definition, every rotation, a verifier for lengths and a two-pointer rotation with the failure function, compared with each other by
//      the script). The tests re-run every string against the real functions and recompute the 64-bit digest (n, k, the k lengths, the least
//      rotation) and the number of strings.
//  (2) ORACLE inside the test, sharing no code with core/lyndon/lyndon.cpp (which is Duval's algorithm): a verifier for the returned lengths (they
//      are positive and sum to n, every factor is a Lyndon word by the two-pointer minimal rotation plus the failure function, neighbours do not
//      increase; a non-increasing run of Lyndon words is the factorization), the same two-pointer rotation reduced modulo the smallest period, and
//      for strings up to 24 bytes the definitions: the longest Lyndon prefix repeatedly, and every rotation against every other.
//  (3) CONSTANTS: the hand rows, the check-order matrix and the closed forms for one repeated byte, a Lyndon word and its rotation, written from the
//      header.
// Output buffers carry 8 sentinel entries before and 16 after; the lens entries at k .. n - 1 must stay untouched on success, and on failure nothing
// may change at all.
// Extents: alphabets {0, 255} up to length 16, {0, 128, 255} up to length 10 and six representative bytes up to length 6 are EXHAUSTIVE, and so
// are all one-byte and two-byte strings over all 256 values; 600 strings of length 17 to 400 per alphabet are fixed-seed SAMPLES; the large strings
// are the listed patterns (SAMPLED patterns). No timing assertion: the linear bound is argued in docs/lyndon.md.
// What is NOT done: Windows is CI-only; nothing is asserted about overlapping buffers.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lyndon.h"

using namespace lumora::lyndon;

// BEGIN GENERATED REFERENCE (tools/lyndon_ref.py)
const std::uint64_t kRefDigest_all256 = 0x85975216293522B7ULL;
const std::size_t kRefCases_all256 = 65793;
const std::uint64_t kRefDigest_big_alt_262142 = 0x091DFEB7C8BD6AD4ULL;
const std::size_t kRefCases_big_alt_262142 = 1;
const std::uint64_t kRefDigest_big_alt_262143 = 0xDE8EFA446BB0118BULL;
const std::size_t kRefCases_big_alt_262143 = 1;
const std::uint64_t kRefDigest_big_alt_262144 = 0x6C5E033BD14D2FB7ULL;
const std::size_t kRefCases_big_alt_262144 = 1;
const std::uint64_t kRefDigest_big_altab_262144 = 0x6C5E033BD14D2FB7ULL;
const std::size_t kRefCases_big_altab_262144 = 1;
const std::uint64_t kRefDigest_big_block512_262144 = 0x8F4A6727D5AEA480ULL;
const std::size_t kRefCases_big_block512_262144 = 1;
const std::uint64_t kRefDigest_big_block512rot_262144 = 0x253ABA34ED134206ULL;
const std::size_t kRefCases_big_block512rot_262144 = 1;
const std::uint64_t kRefDigest_big_desc_262144 = 0xB91B1E336E12F163ULL;
const std::size_t kRefCases_big_desc_262144 = 1;
const std::uint64_t kRefDigest_big_ends_262144 = 0x33F633098CE4A9A2ULL;
const std::size_t kRefCases_big_ends_262144 = 1;
const std::uint64_t kRefDigest_big_fib_262142 = 0xB75DE86380146139ULL;
const std::size_t kRefCases_big_fib_262142 = 1;
const std::uint64_t kRefDigest_big_fib_262143 = 0x78EE045B35F4199CULL;
const std::size_t kRefCases_big_fib_262143 = 1;
const std::uint64_t kRefDigest_big_fib_262144 = 0xA086C83009F16F8AULL;
const std::size_t kRefCases_big_fib_262144 = 1;
const std::uint64_t kRefDigest_big_fibrot_262144 = 0x23C0F0292410C47CULL;
const std::size_t kRefCases_big_fibrot_262144 = 1;
const std::uint64_t kRefDigest_big_lynd_262144 = 0x5C9D544AFE86154AULL;
const std::size_t kRefCases_big_lynd_262144 = 1;
const std::uint64_t kRefDigest_big_per3_262142 = 0x6250F95314CA73C6ULL;
const std::size_t kRefCases_big_per3_262142 = 1;
const std::uint64_t kRefDigest_big_per3_262143 = 0xF3EE9A90768E5656ULL;
const std::size_t kRefCases_big_per3_262143 = 1;
const std::uint64_t kRefDigest_big_per3_262144 = 0x726F6BF0B981F760ULL;
const std::size_t kRefCases_big_per3_262144 = 1;
const std::uint64_t kRefDigest_big_rand256_1000 = 0xF7EA0B9D70AC3080ULL;
const std::size_t kRefCases_big_rand256_1000 = 1;
const std::uint64_t kRefDigest_big_rand256_262143 = 0x63F52F8922E9A6DEULL;
const std::size_t kRefCases_big_rand256_262143 = 1;
const std::uint64_t kRefDigest_big_rand256_262144 = 0x7E9AB048F615DBEEULL;
const std::size_t kRefCases_big_rand256_262144 = 1;
const std::uint64_t kRefDigest_big_rand2_262143 = 0x5D9D85034F927932ULL;
const std::size_t kRefCases_big_rand2_262143 = 1;
const std::uint64_t kRefDigest_big_rand2_262144 = 0x6E2F802E9720E49EULL;
const std::size_t kRefCases_big_rand2_262144 = 1;
const std::uint64_t kRefDigest_big_rep0_262142 = 0x716CA6ECC4E5B991ULL;
const std::size_t kRefCases_big_rep0_262142 = 1;
const std::uint64_t kRefDigest_big_rep0_262143 = 0x45CB15A301BA4FB0ULL;
const std::size_t kRefCases_big_rep0_262143 = 1;
const std::uint64_t kRefDigest_big_rep0_262144 = 0xAC246C3D93D32FB7ULL;
const std::size_t kRefCases_big_rep0_262144 = 1;
const std::uint64_t kRefDigest_big_rep0_32768 = 0x07AEFA7F7430AFB7ULL;
const std::size_t kRefCases_big_rep0_32768 = 1;
const std::uint64_t kRefDigest_big_rep255_262144 = 0xAC246C3D93D32FB7ULL;
const std::size_t kRefCases_big_rep255_262144 = 1;
const std::uint64_t kRefDigest_big_revlynd_262144 = 0xAC246B3D93D32E04ULL;
const std::size_t kRefCases_big_revlynd_262144 = 1;
const std::uint64_t kRefDigest_ex2 = 0xDD63FD103A542414ULL;
const std::size_t kRefCases_ex2 = 131071;
const std::uint64_t kRefDigest_ex3 = 0x767D9048C51E764EULL;
const std::size_t kRefCases_ex3 = 88573;
const std::uint64_t kRefDigest_ex6 = 0x4300CAE15B841FC0ULL;
const std::size_t kRefCases_ex6 = 55987;
const std::uint64_t kRefDigest_rs2 = 0x78CC2E617652A5B5ULL;
const std::size_t kRefCases_rs2 = 600;
const std::uint64_t kRefDigest_rs256 = 0x8633E35625A85552ULL;
const std::size_t kRefCases_rs256 = 600;
const std::uint64_t kRefDigest_rs3 = 0x646E7C7C0849D540ULL;
const std::size_t kRefCases_rs3 = 600;
// END GENERATED REFERENCE

namespace {

using Str = std::vector<std::uint8_t>;
using Lens = std::vector<std::uint32_t>;
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
  const std::uint32_t* p() const { return buf.data() + kGb; }
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

// Lexicographic order of s[a .. a + la) against s[b .. b + lb): a proper prefix is smaller. Returns -1, 0 or 1.
int cmp_range(const Str& s, std::size_t a, std::size_t la, std::size_t b, std::size_t lb) {
  std::size_t i = 0;
  while (i < la && i < lb) {
    if (s[a + i] != s[b + i]) return s[a + i] < s[b + i] ? -1 : 1;
    ++i;
  }
  if (la == lb) return 0;
  return la < lb ? -1 : 1;
}

// By the definition: strictly smaller than each proper non-empty suffix.
bool is_lyndon_def(const Str& s, std::size_t a, std::size_t len) {
  for (std::size_t i = 1; i < len; ++i)
    if (cmp_range(s, a, len, a + i, len - i) >= 0) return false;
  return true;
}

Lens brute_factor(const Str& s) {
  Lens out;
  std::size_t i = 0;
  while (i < s.size()) {
    std::size_t best = 1;
    for (std::size_t e = 1; i + e <= s.size(); ++e)
      if (is_lyndon_def(s, i, e)) best = e;
    out.push_back(static_cast<std::uint32_t>(best));
    i += best;
  }
  return out;
}

std::uint32_t brute_rot(const Str& s) {
  const std::size_t n = s.size();
  std::size_t best = 0;
  for (std::size_t i = 1; i < n; ++i) {
    for (std::size_t k = 0; k < n; ++k) {
      const std::uint8_t a = s[(i + k) % n], b = s[(best + k) % n];
      if (a != b) {
        if (a < b) best = i;
        break;
      }
    }
  }
  return static_cast<std::uint32_t>(best);
}

// Smallest period of w[a .. a + len) that divides len (len if there is none shorter).
std::size_t period_dividing(const Str& w) {
  const std::size_t n = w.size();
  std::vector<std::size_t> fail(n + 1, 0);
  std::size_t k = 0;
  for (std::size_t i = 1; i < n; ++i) {
    while (k > 0 && w[i] != w[k]) k = fail[k];
    if (w[i] == w[k]) ++k;
    fail[i + 1] = k;
  }
  const std::size_t p = n - fail[n];
  return n % p == 0 ? p : n;
}

// The smallest index of the smallest rotation: two-pointer minimal rotation, reduced modulo the smallest period.
std::uint32_t fast_rot(const Str& w) {
  const std::size_t n = w.size();
  if (n == 0) return 0;
  std::size_t i = 0, j = 1, k = 0;
  while (i < n && j < n && k < n) {
    const std::uint8_t a = w[(i + k) % n], b = w[(j + k) % n];
    if (a == b) {
      ++k;
      continue;
    }
    if (a > b)
      i += k + 1;
    else
      j += k + 1;
    if (i == j) ++j;
    k = 0;
  }
  return static_cast<std::uint32_t>((i < j ? i : j) % period_dividing(w));
}

bool is_lyndon_fast(const Str& w) { return period_dividing(w) == w.size() && fast_rot(w) == 0; }

// The lengths are THE factorization when they are positive, sum to n, every factor is a Lyndon word and neighbours do not increase.
bool verify_factor(const Str& s, const std::uint32_t* lens, std::size_t k) {
  std::size_t at = 0, prev_at = 0, prev_len = 0;
  for (std::size_t f = 0; f < k; ++f) {
    const std::size_t ln = lens[f];
    if (ln == 0 || at + ln > s.size()) return false;
    const Str w(s.begin() + static_cast<std::ptrdiff_t>(at), s.begin() + static_cast<std::ptrdiff_t>(at + ln));
    if (!is_lyndon_fast(w)) return false;
    if (f > 0 && cmp_range(s, at, ln, prev_at, prev_len) > 0) return false;
    prev_at = at;
    prev_len = ln;
    at += ln;
  }
  return at == s.size();
}

// Runs both functions on s and compares with the oracle; folds the values in the same order as tools/lyndon_ref.py.
void check_string(const Str& s, Fold& f, Tally& t) {
  const std::size_t n = s.size();
  Guarded lens(n);
  std::uint32_t count = kSent, rot = kSent;
  const std::uint8_t dummy = 0;
  const std::uint8_t* sp = n > 0 ? s.data() : &dummy;
  if (lyndon_factor(static_cast<std::uint32_t>(n), sp, lens.p(), &count) != LyndonResult::Ok) {
    t.fail("lyndon_factor not Ok");
    return;
  }
  if (!lens.guards_ok(n)) t.fail("guards of lens changed");
  if (count > n) {
    t.fail("count above n");
    return;
  }
  for (std::size_t i = count; i < n; ++i)
    if (lens.p()[i] != kSent) {
      t.fail("lens entry at or after k was written");
      break;
    }
  if (!verify_factor(s, lens.p(), count)) t.fail("factorization rejected by the oracle, n = " + std::to_string(n));
  if (n <= 24) {
    const Lens b = brute_factor(s);
    if (b.size() != count || !std::equal(b.begin(), b.end(), lens.p())) t.fail("factorization differs from the definition, n = " + std::to_string(n));
  }
  if (lyndon_min_rotation(static_cast<std::uint32_t>(n), sp, &rot) != LyndonResult::Ok) {
    t.fail("lyndon_min_rotation not Ok");
    return;
  }
  if (rot != fast_rot(s)) t.fail("rotation differs from the oracle, n = " + std::to_string(n));
  if (n <= 24 && rot != brute_rot(s)) t.fail("rotation differs from the definition, n = " + std::to_string(n));
  f.add(n);
  f.add(count);
  for (std::size_t i = 0; i < count; ++i) f.add(lens.p()[i]);
  f.add(rot);
  ++f.cases;
}

void expect_digest(const Fold& f, const Tally& t, std::size_t cases, std::uint64_t digest) {
  INFO(t.first);
  CHECK(t.bad == 0);
  CHECK(f.cases == cases);
  CHECK(f.h == digest);
}

// Every string of length 0 .. maxlen over `alpha` (by length, then the first byte changes fastest): the same order as tools/lyndon_ref.py.
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
      check_string(s, f, t);
      ++total;
    }
    pw *= k;
  }
  return f;
}

// ---------------------------------------------------------------- the large patterns (the same as tools/lyndon_ref.py)

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

Str rotated(const Str& s, std::size_t r) {
  Str o(s.begin() + static_cast<std::ptrdiff_t>(r), s.end());
  o.insert(o.end(), s.begin(), s.begin() + static_cast<std::ptrdiff_t>(r));
  return o;
}

Str pat_lynd(std::size_t n) {
  Str s(n, 0);
  s[n - 1] = 1;
  return s;
}

Str pat_revlynd(std::size_t n) {
  Str s(n, 0);
  s[0] = 1;
  return s;
}

Str pat_desc(std::size_t n) {
  Str s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<std::uint8_t>(255 - i % 256);
  return s;
}

Str pat_ends(std::size_t n) {
  Str s(n, 0);
  s[0] = 1;
  s[n - 1] = 2;
  return s;
}

Str pat_block(std::size_t n, std::size_t rot, std::uint64_t seed) {
  Lcg g(seed);
  Str blk(512);
  for (std::uint8_t& c : blk) c = static_cast<std::uint8_t>(g.next() % 256);
  Str s(n);
  for (std::size_t i = 0; i < n; ++i) s[i] = blk[i % 512];
  return rotated(s, rot);
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

TEST_CASE("exhaustive: every string of length 0..16 over {0, 255}", "[lyndon]") {
  Tally t;
  std::size_t total = 0;
  const Fold f = run_exhaustive({0, 255}, 16, t, total);
  expect_digest(f, t, kRefCases_ex2, kRefDigest_ex2);
  CHECK(total == 131071u);
}

TEST_CASE("exhaustive: every string of length 0..10 over {0, 128, 255}", "[lyndon]") {
  Tally t;
  std::size_t total = 0;
  const Fold f = run_exhaustive({0, 128, 255}, 10, t, total);
  expect_digest(f, t, kRefCases_ex3, kRefDigest_ex3);
  CHECK(total == 88573u);
}

TEST_CASE("exhaustive: every string of length 0..6 over the bytes 0, 1, 127, 128, 254, 255", "[lyndon]") {
  Tally t;
  std::size_t total = 0;
  const Fold f = run_exhaustive({0, 1, 127, 128, 254, 255}, 6, t, total);
  expect_digest(f, t, kRefCases_ex6, kRefDigest_ex6);
  CHECK(total == 55987u);
}

TEST_CASE("exhaustive: the empty string, all one-byte and all two-byte strings over all 256 values", "[lyndon]") {
  Tally t;
  Fold f;
  check_string(Str{}, f, t);
  for (std::size_t a = 0; a < 256; ++a) check_string(Str{static_cast<std::uint8_t>(a)}, f, t);
  for (std::size_t c = 0; c < 65536; ++c) check_string(Str{static_cast<std::uint8_t>(c % 256), static_cast<std::uint8_t>(c / 256)}, f, t);
  expect_digest(f, t, kRefCases_all256, kRefDigest_all256);
}

// Fixed-seed SAMPLES: 600 random strings of length 17 .. 400 per alphabet (not exhaustive).
TEST_CASE("fixed-seed samples: 600 random strings of length 17..400 over 2, 3 and 256 symbols (sampled, not exhaustive)", "[lyndon]") {
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
      check_string(s, f, t);
    }
    INFO(g.name);
    expect_digest(f, t, g.cases, g.digest);
  }
}

TEST_CASE("large strings: repeated byte, alternating, period 3, Fibonacci, Lyndon words, blocks, random (sampled patterns)", "[lyndon]") {
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
      BIG(fibrot_262144, rotated(pat_fib(262144), 100003)),
      BIG(lynd_262144, pat_lynd(262144)),
      BIG(revlynd_262144, pat_revlynd(262144)),
      BIG(desc_262144, pat_desc(262144)),
      BIG(ends_262144, pat_ends(262144)),
      BIG(block512_262144, pat_block(262144, 0, 6001)),
      BIG(block512rot_262144, pat_block(262144, 300, 6001)),
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
    check_string(s, f, t);
    INFO(c.name);
    expect_digest(f, t, c.cases, c.digest);
  }
}

TEST_CASE("closed forms at the limit: one repeated byte, a Lyndon word and its flipped form", "[lyndon]") {
  for (std::uint32_t n : {262144u, 262143u}) {
    const Str s(n, 0xFFu);
    Guarded lens(n);
    std::uint32_t k = kSent, rot = kSent;
    REQUIRE(lyndon_factor(n, s.data(), lens.p(), &k) == LyndonResult::Ok);
    CHECK(k == n);
    bool ones = true;
    for (std::uint32_t i = 0; i < n; ++i) ones = ones && lens.p()[i] == 1u;
    CHECK(ones);
    CHECK(lens.guards_ok(n));
    REQUIRE(lyndon_min_rotation(n, s.data(), &rot) == LyndonResult::Ok);
    CHECK(rot == 0u);
  }
  {
    const std::uint32_t n = 262144;
    const Str s = pat_lynd(n);  // 0 ... 0 1 is one Lyndon word
    Guarded lens(n);
    std::uint32_t k = kSent, rot = kSent;
    REQUIRE(lyndon_factor(n, s.data(), lens.p(), &k) == LyndonResult::Ok);
    CHECK(k == 1u);
    CHECK(lens.p()[0] == n);
    CHECK(lens.p()[1] == kSent);
    REQUIRE(lyndon_min_rotation(n, s.data(), &rot) == LyndonResult::Ok);
    CHECK(rot == 0u);
  }
  {
    const std::uint32_t n = 262144;
    const Str s = pat_revlynd(n);  // 1 0 ... 0: the factors are "1" and then n - 1 single zeros
    Guarded lens(n);
    std::uint32_t k = kSent, rot = kSent;
    REQUIRE(lyndon_factor(n, s.data(), lens.p(), &k) == LyndonResult::Ok);
    CHECK(k == n);  // "1" then n - 1 single zeros (equal neighbours)
    CHECK(lens.p()[0] == 1u);
    CHECK(lens.p()[n - 1] == 1u);
    REQUIRE(lyndon_min_rotation(n, s.data(), &rot) == LyndonResult::Ok);
    CHECK(rot == 1u);
  }
}

TEST_CASE("hand rows: ties, periodic strings, runs, bytes 0 and 255 next to each other", "[lyndon]") {
  struct Row { const char* name; Str s; Lens lens; std::uint32_t rot; };
  const Row rows[] = {
      {"abab", {97, 98, 97, 98}, {2, 2}, 0},
      {"baba", {98, 97, 98, 97}, {1, 2, 1}, 1},
      {"aab", {97, 97, 98}, {3}, 0},
      {"aba", {97, 98, 97}, {2, 1}, 2},
      {"ba", {98, 97}, {1, 1}, 1},
      {"aa", {97, 97}, {1, 1}, 0},
      {"abc", {97, 98, 99}, {3}, 0},
      {"cba", {99, 98, 97}, {1, 1, 1}, 2},
      {"0 255 0", {0, 255, 0}, {2, 1}, 2},
      {"255 0", {255, 0}, {1, 1}, 1},
      {"0 255 255 0", {0, 255, 255, 0}, {3, 1}, 3},
      {"ababa", {97, 98, 97, 98, 97}, {2, 2, 1}, 4},
      {"abaab", {97, 98, 97, 97, 98}, {2, 3}, 2},
      {"one byte", {97}, {1}, 0},
      {"255 255 0 0", {255, 255, 0, 0}, {1, 1, 1, 1}, 2},
      {"0 0 255 255 0 0 255 255", {0, 0, 255, 255, 0, 0, 255, 255}, {4, 4}, 0},
      {"128 127 128 127", {128, 127, 128, 127}, {1, 2, 1}, 1},
      {"aabaab", {97, 97, 98, 97, 97, 98}, {3, 3}, 0},
      {"bab aab", {98, 97, 98, 97, 97, 98}, {1, 2, 3}, 3},
      {"zyx abc", {122, 121, 120, 97, 98, 99}, {1, 1, 1, 3}, 3},
      {"a b a a b a a b", {97, 98, 97, 97, 98, 97, 97, 98}, {2, 3, 3}, 2},
      {"255 0 255 0 0", {255, 0, 255, 0, 0}, {1, 2, 1, 1}, 3},
      {"aaab aaab aaa", {97, 97, 97, 98, 97, 97, 97, 98, 97, 97, 97}, {4, 4, 1, 1, 1}, 8},
      {"0 1 0 1 0 0", {0, 1, 0, 1, 0, 0}, {2, 2, 1, 1}, 4},
  };
  for (const Row& r : rows) {
    INFO(r.name);
    Guarded lens(r.s.size());
    std::uint32_t k = kSent, rot = kSent;
    REQUIRE(lyndon_factor(static_cast<std::uint32_t>(r.s.size()), r.s.data(), lens.p(), &k) == LyndonResult::Ok);
    REQUIRE(k == r.lens.size());
    for (std::size_t i = 0; i < r.lens.size(); ++i) CHECK(lens.p()[i] == r.lens[i]);
    for (std::size_t i = r.lens.size(); i < r.s.size(); ++i) CHECK(lens.p()[i] == kSent);
    CHECK(lens.guards_ok(r.s.size()));
    REQUIRE(lyndon_min_rotation(static_cast<std::uint32_t>(r.s.size()), r.s.data(), &rot) == LyndonResult::Ok);
    CHECK(rot == r.rot);
  }
}

TEST_CASE("check order matrix: null arguments, TooLarge; nothing written on failure", "[lyndon]") {
  const Str s = {1, 2, 2, 1, 3};
  const std::uint32_t n = 5;
  Guarded lens(n);
  std::uint32_t k = 0x12345678u, rot = 0x87654321u;
  const std::uint32_t k_before = k, rot_before = rot;
  // lyndon_factor
  CHECK(lyndon_factor(n, nullptr, lens.p(), &k) == LyndonResult::NullArgument);
  CHECK(lyndon_factor(n, s.data(), nullptr, &k) == LyndonResult::NullArgument);
  CHECK(lyndon_factor(n, s.data(), lens.p(), nullptr) == LyndonResult::NullArgument);
  CHECK(lyndon_factor(0, nullptr, nullptr, nullptr) == LyndonResult::NullArgument);  // out_count is always needed
  CHECK(lyndon_factor(kMaxLen + 1, nullptr, lens.p(), &k) == LyndonResult::NullArgument);  // null before TooLarge
  CHECK(lyndon_factor(kMaxLen + 1, s.data(), nullptr, &k) == LyndonResult::NullArgument);
  CHECK(lyndon_factor(kMaxLen + 1, s.data(), lens.p(), nullptr) == LyndonResult::NullArgument);
  CHECK(lyndon_factor(kMaxLen + 1, s.data(), lens.p(), &k) == LyndonResult::TooLarge);
  CHECK(lyndon_factor(0xFFFFFFFFu, s.data(), lens.p(), &k) == LyndonResult::TooLarge);
  CHECK(k == k_before);
  CHECK(lens.untouched());
  CHECK(lyndon_factor(0, nullptr, nullptr, &k) == LyndonResult::Ok);  // n = 0: no s and no lens needed, nothing written to lens
  CHECK(k == 0u);
  k = k_before;
  CHECK(lyndon_factor(0, s.data(), lens.p(), &k) == LyndonResult::Ok);
  CHECK(k == 0u);
  CHECK(lens.untouched());
  // lyndon_min_rotation
  CHECK(lyndon_min_rotation(n, nullptr, &rot) == LyndonResult::NullArgument);
  CHECK(lyndon_min_rotation(n, s.data(), nullptr) == LyndonResult::NullArgument);
  CHECK(lyndon_min_rotation(0, nullptr, nullptr) == LyndonResult::NullArgument);
  CHECK(lyndon_min_rotation(kMaxLen + 1, nullptr, &rot) == LyndonResult::NullArgument);
  CHECK(lyndon_min_rotation(kMaxLen + 1, s.data(), nullptr) == LyndonResult::NullArgument);
  CHECK(lyndon_min_rotation(kMaxLen + 1, s.data(), &rot) == LyndonResult::TooLarge);
  CHECK(lyndon_min_rotation(0xFFFFFFFFu, s.data(), &rot) == LyndonResult::TooLarge);
  CHECK(rot == rot_before);
  CHECK(lyndon_min_rotation(0, nullptr, &rot) == LyndonResult::Ok);
  CHECK(rot == 0u);
  rot = rot_before;
  CHECK(lyndon_min_rotation(0, s.data(), &rot) == LyndonResult::Ok);
  CHECK(rot == 0u);
  // a success writes the count and the lengths and leaves the rest alone
  lens = Guarded(n);
  CHECK(lyndon_factor(n, s.data(), lens.p(), &k) == LyndonResult::Ok);
  CHECK(k == 1u);  // 12213 is one Lyndon word
  CHECK(lens.p()[0] == 5u);
  CHECK(verify_factor(s, lens.p(), k));
  CHECK(lens.guards_ok(n));
}
