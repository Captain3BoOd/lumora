// Tests for core/bitrank. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/bitrank_ref.py, an independent Python implementation (big-integer
// counting with int.bit_count, select by position list cross-checked with bisect on prefix counts). Regenerate it with: python3 tools/bitrank_ref.py
// The tests also carry a naive oracle that shares no word or popcount logic with core/bitrank/bitrank.cpp: the vector is one char per position
// and rank and select are plain loops over positions. The directory is only used through the queries; its contents are never asserted, nothing
// is asserted past the first `need` words of the directory, and no stale or corrupt directory is tested (the header leaves those open).
// Buffers are heap buffers of exactly the stated size (AddressSanitizer sees any access past the end): bits has (nbits + 63) / 64 words, the
// directory has exactly `need` words. Bits above nbits in the last word are set to all ones or to zero and must never change a result.
// Maximum-size query coverage (2^24 bits) is SAMPLED (word boundaries and a stride), not exhaustive.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "bitrank.h"

using namespace lumora::bitrank;

namespace {

// BEGIN GENERATED REFERENCE (tools/bitrank_ref.py)
const std::uint32_t kRefExhMaxBits = 14, kRefRandSeeds = 400;
const std::uint32_t kRefShapeSizes[] = {1, 63, 64, 65, 127, 128, 129, 191, 192, 193, 256, 257};
const std::uint64_t kRefDigest_exh = 0xB153240B17F3D25Bull;
const std::size_t kRefCases_exh = 65534, kRefOps_exh = 1540094;
const std::uint64_t kRefDigest_shape = 0xD77B5F07323A8FBDull;
const std::size_t kRefCases_shape = 22142, kRefOps_shape = 3329610;
const std::uint64_t kRefDigest_rand = 0xEBA369C59AA06ECEull;
const std::size_t kRefCases_rand = 400, kRefOps_rand = 58400;
// END GENERATED REFERENCE

constexpr std::uint32_t kSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kOver = (1u << 24) + 1u;

using Bits = std::vector<char>;  // one char (0 or 1) per position

std::uint32_t code(BitResult r) { return static_cast<std::uint32_t>(r); }

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
};

std::uint32_t words_of(std::uint32_t nbits) { return (nbits + 63u) / 64u; }

// ---- oracle: plain loops over positions ----
std::uint32_t o_total(const Bits& b) {
  std::uint32_t t = 0;
  for (char c : b) t += c != 0 ? 1u : 0u;
  return t;
}
bool o_rank(const Bits& b, std::uint32_t i, std::uint32_t* out) {
  if (i > b.size()) return false;
  std::uint32_t t = 0;
  for (std::uint32_t p = 0; p < i; ++p) t += b[p] != 0 ? 1u : 0u;
  *out = t;
  return true;
}
bool o_select(const Bits& b, std::uint32_t k, std::uint32_t* out) {
  std::uint32_t seen = 0;
  for (std::uint32_t p = 0; p < b.size(); ++p) {
    if (b[p] == 0) continue;
    if (seen == k) {
      *out = p;
      return true;
    }
    ++seen;
  }
  return false;
}

// Packs the positions into words, with the padding above nbits in the last word set to ones or zeros.
std::unique_ptr<std::uint64_t[]> pack(const Bits& b, bool pad_ones) {
  const std::uint32_t n = static_cast<std::uint32_t>(b.size());
  const std::uint32_t words = words_of(n);
  std::unique_ptr<std::uint64_t[]> w(new std::uint64_t[words == 0u ? 1u : words]);
  for (std::uint32_t i = 0; i < words; ++i) w[i] = 0;
  for (std::uint32_t p = 0; p < n; ++p) {
    if (b[p] != 0) w[p / 64u] |= 1ull << (p % 64u);
  }
  if (pad_ones && n % 64u != 0u) w[words - 1u] |= ~0ull << (n % 64u);
  return w;
}

// Builds the directory with exact buffers and runs every rank (i = 0 .. n + 1) and select (k = 0 .. total + 1) query against the oracle,
// folding the results in the reference order. A failure must leave the output untouched.
void run_vector(Fold* f, std::size_t* bad, const Bits& b, bool pad_ones) {
  const std::uint32_t n = static_cast<std::uint32_t>(b.size());
  std::unique_ptr<std::uint64_t[]> bits = pack(b, pad_ones);
  const std::uint64_t* bp = n == 0u ? nullptr : bits.get();
  std::uint32_t need = 0;
  if (bitrank_dir_words(n, &need) != BitResult::Ok || need != words_of(n) + 1u) ++*bad;
  std::unique_ptr<std::uint32_t[]> dir(new std::uint32_t[need]);
  for (std::uint32_t i = 0; i < need; ++i) dir[i] = kSentinel;
  if (bitrank_build(bp, n, dir.get(), need) != BitResult::Ok) ++*bad;
  ++f->cases;
  const std::uint32_t total = o_total(b);
  for (std::uint32_t i = 0; i <= n + 1u; ++i) {
    std::uint32_t out = kSentinel;
    const BitResult r = bitrank_rank1(bp, n, dir.get(), need, i, &out);
    std::uint32_t want = 0;
    const bool ok = o_rank(b, i, &want);
    if (ok) {
      if (r != BitResult::Ok || out != want) ++*bad;
    } else if (r != BitResult::OutOfRange || out != kSentinel) {
      ++*bad;
    }
    ++f->ops;
    f->add(code(r));
    if (r == BitResult::Ok) f->add(out);
  }
  for (std::uint32_t k = 0; k <= total + 1u; ++k) {
    std::uint32_t out = kSentinel;
    const BitResult r = bitrank_select1(bp, n, dir.get(), need, k, &out);
    std::uint32_t want = 0;
    const bool ok = o_select(b, k, &want);
    if (ok) {
      if (r != BitResult::Ok || out != want) ++*bad;
    } else if (r != BitResult::OutOfRange || out != kSentinel) {
      ++*bad;
    }
    ++f->ops;
    f->add(code(r));
    if (r == BitResult::Ok) f->add(out);
  }
}

// The reference folds each vector once per padding choice (zero, then ones).
void run_vector_both(Fold* f, std::size_t* bad, const Bits& b) {
  run_vector(f, bad, b, false);
  run_vector(f, bad, b, true);
}

void shape_vectors(std::uint32_t n, Fold* f, std::size_t* bad) {
  Bits b(n, 0);
  auto go = [&]() { run_vector_both(f, bad, b); };
  go();                                                       // empty
  for (std::uint32_t p = 0; p < n; ++p) b[p] = 1;
  go();                                                       // full
  for (std::uint32_t p = 0; p < n; ++p) b[p] = p % 2u == 0u ? 1 : 0;
  go();                                                       // even positions
  for (std::uint32_t p = 0; p < n; ++p) b[p] = p % 2u == 0u ? 0 : 1;
  go();                                                       // odd positions
  for (std::uint32_t p = 0; p < n; ++p) {                     // every single bit
    b.assign(n, 0);
    b[p] = 1;
    go();
  }
  if (n <= 65u) {                                             // every pair of bits
    for (std::uint32_t p = 0; p < n; ++p) {
      for (std::uint32_t q = p + 1u; q < n; ++q) {
        b.assign(n, 0);
        b[p] = 1;
        b[q] = 1;
        go();
      }
    }
  }
  for (std::uint32_t p = 1; p < n; ++p) {                     // every run [0, p)
    for (std::uint32_t q = 0; q < n; ++q) b[q] = q < p ? 1 : 0;
    go();
  }
  for (std::uint32_t p = 1; p < n; ++p) {                     // every run [p, n)
    for (std::uint32_t q = 0; q < n; ++q) b[q] = q >= p ? 1 : 0;
    go();
  }
}

std::uint64_t draw64(Lcg* g) {
  const std::uint64_t hi = g->next();
  const std::uint64_t lo = g->next();
  return (hi << 32) | lo;
}

}  // namespace

TEST_CASE("header anchors", "[bitrank]") {
  CHECK(kMaxBits == (1u << 24));
  CHECK(code(BitResult::Ok) == 0u);
  CHECK(code(BitResult::NullArgument) == 1u);
  CHECK(code(BitResult::TooLarge) == 2u);
  CHECK(code(BitResult::ScratchTooSmall) == 3u);
  CHECK(code(BitResult::OutOfRange) == 4u);
}

TEST_CASE("exhaustive: every bit vector of 0 to 14 bits, zero and all-ones padding, every rank i in 0..n+1 and select k in 0..total+1", "[sweep][bitrank]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t n = 0; n <= kRefExhMaxBits; ++n) {
    Bits b(n, 0);
    for (std::uint32_t v = 0; v < (1u << n); ++v) {
      for (std::uint32_t p = 0; p < n; ++p) b[p] = static_cast<char>((v >> p) & 1u);
      run_vector_both(&f, &bad, b);
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_exh);
  CHECK(f.ops == kRefOps_exh);
  CHECK(f.h == kRefDigest_exh);
}

TEST_CASE("word boundary shapes: 1, 63, 64, 65, 127, 128, 129, 191, 192, 193, 256, 257 bits", "[sweep][bitrank]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t n : kRefShapeSizes) shape_vectors(n, &f, &bad);
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_shape);
  CHECK(f.ops == kRefOps_shape);
  CHECK(f.h == kRefDigest_shape);
}

TEST_CASE("pseudo-random vectors of 0 to 4096 bits, sampled queries (not exhaustive)", "[sweep][bitrank]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t seed = 1; seed <= kRefRandSeeds; ++seed) {
    Lcg g(static_cast<std::uint64_t>(seed) * 7919u + 13u);
    const std::uint32_t n = g.next() % 4097u;
    const std::uint32_t words = words_of(n);
    const std::uint32_t mode = g.next() % 4u;
    Bits b(n, 0);
    for (std::uint32_t w = 0; w < words; ++w) {
      std::uint64_t raw = draw64(&g);
      if (mode == 0u) {
        const std::uint64_t x = draw64(&g);
        const std::uint64_t y = draw64(&g);
        raw &= x & y;
      } else if (mode == 2u) {
        const std::uint64_t x = draw64(&g);
        const std::uint64_t y = draw64(&g);
        raw |= x | y;
      } else if (mode == 3u) {
        raw = g.next() % 2u != 0u ? ~0ull : 0ull;
      }
      for (std::uint32_t p = 0; p < 64u && w * 64u + p < n; ++p) b[w * 64u + p] = static_cast<char>((raw >> p) & 1u);
    }
    const bool pad_ones = g.next() % 2u != 0u;
    std::unique_ptr<std::uint64_t[]> bits = pack(b, pad_ones);
    const std::uint64_t* bp = n == 0u ? nullptr : bits.get();
    const std::uint32_t need = words + 1u;
    std::unique_ptr<std::uint32_t[]> dir(new std::uint32_t[need]);
    if (bitrank_build(bp, n, dir.get(), need) != BitResult::Ok) ++bad;
    const std::uint32_t total = o_total(b);
    ++f.cases;
    std::vector<std::uint32_t> is = {0u, n, n + 1u};
    for (int j = 0; j < 70; ++j) is.push_back(g.next() % (n + 2u));
    for (std::uint32_t i : is) {
      std::uint32_t out = kSentinel;
      const BitResult r = bitrank_rank1(bp, n, dir.get(), need, i, &out);
      std::uint32_t want = 0;
      if (o_rank(b, i, &want)) {
        if (r != BitResult::Ok || out != want) ++bad;
      } else if (r != BitResult::OutOfRange || out != kSentinel) {
        ++bad;
      }
      ++f.ops;
      f.add(code(r));
      if (r == BitResult::Ok) f.add(out);
    }
    std::vector<std::uint32_t> ks = {0u, total, total + 1u};
    for (int j = 0; j < 70; ++j) ks.push_back(g.next() % (total + 2u));
    for (std::uint32_t k : ks) {
      std::uint32_t out = kSentinel;
      const BitResult r = bitrank_select1(bp, n, dir.get(), need, k, &out);
      std::uint32_t want = 0;
      if (o_select(b, k, &want)) {
        if (r != BitResult::Ok || out != want) ++bad;
      } else if (r != BitResult::OutOfRange || out != kSentinel) {
        ++bad;
      }
      ++f.ops;
      f.add(code(r));
      if (r == BitResult::Ok) f.add(out);
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_rand);
  CHECK(f.ops == kRefOps_rand);
  CHECK(f.h == kRefDigest_rand);
}

TEST_CASE("hand-checked vectors, padding never counted", "[bitrank]") {
  std::uint32_t out = kSentinel;
  std::uint32_t dir[4];
  {
    // 65 bits: word 0 all ones, word 1 holds position 64 and the padding position 65.
    const std::uint64_t bits[2] = {~0ull, 3ull};
    REQUIRE(bitrank_build(bits, 65, dir, 3) == BitResult::Ok);
    CHECK(bitrank_rank1(bits, 65, dir, 3, 64, &out) == BitResult::Ok);
    CHECK(out == 64u);
    CHECK(bitrank_rank1(bits, 65, dir, 3, 65, &out) == BitResult::Ok);
    CHECK(out == 65u);
    CHECK(bitrank_select1(bits, 65, dir, 3, 64, &out) == BitResult::Ok);
    CHECK(out == 64u);
    out = kSentinel;
    CHECK(bitrank_select1(bits, 65, dir, 3, 65, &out) == BitResult::OutOfRange);
    CHECK(out == kSentinel);
  }
  {
    // 64 bits: positions 0 and 63.
    const std::uint64_t bits[1] = {0x8000000000000001ull};
    REQUIRE(bitrank_build(bits, 64, dir, 2) == BitResult::Ok);
    CHECK(bitrank_rank1(bits, 64, dir, 2, 0, &out) == BitResult::Ok);
    CHECK(out == 0u);
    CHECK(bitrank_rank1(bits, 64, dir, 2, 1, &out) == BitResult::Ok);
    CHECK(out == 1u);
    CHECK(bitrank_rank1(bits, 64, dir, 2, 63, &out) == BitResult::Ok);
    CHECK(out == 1u);
    CHECK(bitrank_rank1(bits, 64, dir, 2, 64, &out) == BitResult::Ok);
    CHECK(out == 2u);
    CHECK(bitrank_select1(bits, 64, dir, 2, 0, &out) == BitResult::Ok);
    CHECK(out == 0u);
    CHECK(bitrank_select1(bits, 64, dir, 2, 1, &out) == BitResult::Ok);
    CHECK(out == 63u);
    CHECK(bitrank_select1(bits, 64, dir, 2, 2, &out) == BitResult::OutOfRange);
  }
  {
    // 3 bits, all 64 bits of the word set: only positions 0 to 2 count.
    const std::uint64_t bits[1] = {~0ull};
    REQUIRE(bitrank_build(bits, 3, dir, 2) == BitResult::Ok);
    CHECK(bitrank_rank1(bits, 3, dir, 2, 3, &out) == BitResult::Ok);
    CHECK(out == 3u);
    CHECK(bitrank_select1(bits, 3, dir, 2, 2, &out) == BitResult::Ok);
    CHECK(out == 2u);
    CHECK(bitrank_select1(bits, 3, dir, 2, 3, &out) == BitResult::OutOfRange);
  }
  {
    // the empty vector: no word is read, bits may be null
    REQUIRE(bitrank_build(nullptr, 0, dir, 1) == BitResult::Ok);
    CHECK(bitrank_rank1(nullptr, 0, dir, 1, 0, &out) == BitResult::Ok);
    CHECK(out == 0u);
    out = kSentinel;
    CHECK(bitrank_rank1(nullptr, 0, dir, 1, 1, &out) == BitResult::OutOfRange);
    CHECK(bitrank_select1(nullptr, 0, dir, 1, 0, &out) == BitResult::OutOfRange);
    CHECK(bitrank_select1(nullptr, 0, dir, 1, 0xFFFFFFFFu, &out) == BitResult::OutOfRange);
    CHECK(out == kSentinel);
  }
}

TEST_CASE("directory size", "[args][bitrank]") {
  std::uint32_t w = kSentinel;
  CHECK(bitrank_dir_words(0, &w) == BitResult::Ok);
  CHECK(w == 1u);
  CHECK(bitrank_dir_words(1, &w) == BitResult::Ok);
  CHECK(w == 2u);
  CHECK(bitrank_dir_words(64, &w) == BitResult::Ok);
  CHECK(w == 2u);
  CHECK(bitrank_dir_words(65, &w) == BitResult::Ok);
  CHECK(w == 3u);
  CHECK(bitrank_dir_words(kMaxBits, &w) == BitResult::Ok);
  CHECK(w == 262145u);
  w = kSentinel;
  CHECK(bitrank_dir_words(kOver, &w) == BitResult::TooLarge);
  CHECK(bitrank_dir_words(0xFFFFFFFFu, &w) == BitResult::TooLarge);
  CHECK(w == kSentinel);
  CHECK(bitrank_dir_words(5, nullptr) == BitResult::NullArgument);
  CHECK(bitrank_dir_words(kOver, nullptr) == BitResult::NullArgument);  // Null beats TooLarge
  CHECK(bitrank_dir_words(0xFFFFFFFFu, nullptr) == BitResult::NullArgument);
}

TEST_CASE("null rules, check order and untouched outputs", "[args][bitrank]") {
  const std::uint64_t bits[2] = {0xF0F0F0F0F0F0F0F0ull, 0x5ull};
  std::uint32_t dir[3] = {kSentinel, kSentinel, kSentinel};
  std::uint32_t out = kSentinel;
  // build
  CHECK(bitrank_build(nullptr, 100, dir, 3) == BitResult::NullArgument);
  CHECK(bitrank_build(bits, 100, nullptr, 3) == BitResult::NullArgument);
  CHECK(bitrank_build(nullptr, 0, nullptr, 1) == BitResult::NullArgument);
  CHECK(bitrank_build(nullptr, kOver, dir, 3) == BitResult::NullArgument);  // Null beats TooLarge
  CHECK(bitrank_build(bits, kOver, nullptr, 3) == BitResult::NullArgument);
  CHECK(bitrank_build(bits, kOver, dir, 3) == BitResult::TooLarge);
  CHECK(bitrank_build(bits, 0xFFFFFFFFu, dir, 0) == BitResult::TooLarge);  // TooLarge beats ScratchTooSmall
  CHECK(bitrank_build(bits, 100, dir, 2) == BitResult::ScratchTooSmall);
  CHECK(bitrank_build(bits, 100, dir, 0) == BitResult::ScratchTooSmall);
  CHECK(bitrank_build(bits, 65, dir, 2) == BitResult::ScratchTooSmall);
  for (std::uint32_t d : dir) CHECK(d == kSentinel);  // a failed build leaves the directory untouched
  CHECK(bitrank_build(bits, 100, dir, 3) == BitResult::Ok);
  // queries, on the valid directory
  CHECK(bitrank_rank1(nullptr, 100, dir, 3, 5, &out) == BitResult::NullArgument);
  CHECK(bitrank_rank1(bits, 100, nullptr, 3, 5, &out) == BitResult::NullArgument);
  CHECK(bitrank_rank1(bits, 100, dir, 3, 5, nullptr) == BitResult::NullArgument);
  CHECK(bitrank_rank1(nullptr, 0, dir, 1, 0, nullptr) == BitResult::NullArgument);
  CHECK(bitrank_rank1(nullptr, kOver, dir, 3, 5, &out) == BitResult::NullArgument);
  CHECK(bitrank_rank1(bits, kOver, dir, 3, 5, nullptr) == BitResult::NullArgument);
  CHECK(bitrank_rank1(bits, kOver, dir, 3, 5, &out) == BitResult::TooLarge);
  CHECK(bitrank_rank1(bits, 0xFFFFFFFFu, dir, 0, 0xFFFFFFFFu, &out) == BitResult::TooLarge);
  CHECK(bitrank_rank1(bits, 100, dir, 2, 5, &out) == BitResult::ScratchTooSmall);
  CHECK(bitrank_rank1(bits, 100, dir, 2, 500, &out) == BitResult::ScratchTooSmall);  // beats OutOfRange
  CHECK(bitrank_rank1(bits, 100, dir, 3, 101, &out) == BitResult::OutOfRange);
  CHECK(bitrank_rank1(bits, 100, dir, 3, 0xFFFFFFFFu, &out) == BitResult::OutOfRange);
  CHECK(bitrank_select1(nullptr, 100, dir, 3, 5, &out) == BitResult::NullArgument);
  CHECK(bitrank_select1(bits, 100, nullptr, 3, 5, &out) == BitResult::NullArgument);
  CHECK(bitrank_select1(bits, 100, dir, 3, 5, nullptr) == BitResult::NullArgument);
  CHECK(bitrank_select1(nullptr, 0, dir, 1, 0, nullptr) == BitResult::NullArgument);
  CHECK(bitrank_select1(nullptr, kOver, dir, 3, 5, &out) == BitResult::NullArgument);
  CHECK(bitrank_select1(bits, kOver, dir, 3, 5, nullptr) == BitResult::NullArgument);
  CHECK(bitrank_select1(bits, kOver, dir, 3, 5, &out) == BitResult::TooLarge);
  CHECK(bitrank_select1(bits, 0xFFFFFFFFu, dir, 0, 0xFFFFFFFFu, &out) == BitResult::TooLarge);
  CHECK(bitrank_select1(bits, 100, dir, 2, 5, &out) == BitResult::ScratchTooSmall);
  CHECK(bitrank_select1(bits, 100, dir, 2, 5000, &out) == BitResult::ScratchTooSmall);  // beats OutOfRange
  CHECK(bitrank_select1(bits, 100, dir, 3, 5000, &out) == BitResult::OutOfRange);
  CHECK(bitrank_select1(bits, 100, dir, 3, 0xFFFFFFFFu, &out) == BitResult::OutOfRange);
  CHECK(out == kSentinel);  // no failure wrote the output
  // a directory with more words than needed is accepted by every function (nothing is asserted about words past the need)
  std::uint32_t big[8];
  for (std::uint32_t& d : big) d = kSentinel;
  CHECK(bitrank_build(bits, 100, big, 8) == BitResult::Ok);
  CHECK(bitrank_rank1(bits, 100, big, 8, 100, &out) == BitResult::Ok);
  CHECK(out == 34u);  // 32 bits in word 0, two in word 1 (0x5)
  CHECK(bitrank_select1(bits, 100, big, 8, 0, &out) == BitResult::Ok);
  CHECK(out == 4u);
}

TEST_CASE("sizes: one over the limit and UINT32_MAX are TooLarge on every function, nothing is read", "[max][bitrank]") {
  const std::uint64_t one_word[1] = {~0ull};
  std::uint32_t dir1[1] = {kSentinel};
  std::uint32_t out = kSentinel;
  std::uint32_t w = kSentinel;
  for (std::uint32_t n : {kOver, 0xFFFFFFFFu}) {
    CHECK(bitrank_dir_words(n, &w) == BitResult::TooLarge);
    CHECK(bitrank_build(one_word, n, dir1, 1) == BitResult::TooLarge);
    CHECK(bitrank_build(one_word, n, dir1, 0xFFFFFFFFu) == BitResult::TooLarge);
    CHECK(bitrank_rank1(one_word, n, dir1, 1, 0, &out) == BitResult::TooLarge);
    CHECK(bitrank_rank1(one_word, n, dir1, 0xFFFFFFFFu, n, &out) == BitResult::TooLarge);
    CHECK(bitrank_select1(one_word, n, dir1, 1, 0, &out) == BitResult::TooLarge);
    CHECK(bitrank_select1(one_word, n, dir1, 0xFFFFFFFFu, 0, &out) == BitResult::TooLarge);
  }
  CHECK(w == kSentinel);
  CHECK(out == kSentinel);
  CHECK(dir1[0] == kSentinel);
}

namespace {

// One maximum-size case: `bits` holds exactly nbits / 64 words (nbits is 2^24 or 2^24 - 1), the directory exactly the need. `rank_of` and `select_of`
// give the expected answers; select_of returns false when k is at or above the total. Correctness only.
template <class RankOf, class SelectOf>
std::size_t check_max(const std::uint64_t* bits, std::uint32_t nbits, std::uint32_t* dir, std::uint32_t need, std::uint32_t total, RankOf rank_of,
                      SelectOf select_of, bool stride_select) {
  std::size_t bad = 0;
  if (bitrank_build(bits, nbits, dir, need) != BitResult::Ok) return 1;
  const std::uint32_t words = words_of(nbits);
  auto rank_check = [&](std::uint32_t i) {
    std::uint32_t out = kSentinel;
    if (i > nbits) {
      if (bitrank_rank1(bits, nbits, dir, need, i, &out) != BitResult::OutOfRange || out != kSentinel) ++bad;
      return;
    }
    if (bitrank_rank1(bits, nbits, dir, need, i, &out) != BitResult::Ok || out != rank_of(i)) ++bad;
  };
  for (std::uint32_t j = 0; j <= words; ++j) {  // every word boundary and its neighbours
    const std::uint32_t at = j * 64u;
    if (at >= 1u) rank_check(at - 1u);
    rank_check(at);
    if (at < nbits) rank_check(at + 1u);
  }
  rank_check(nbits);
  rank_check(nbits + 1u);
  for (std::uint32_t i = 0; i <= nbits; i += 4099u) rank_check(i);  // a sampled stride
  auto select_check = [&](std::uint32_t k) {
    std::uint32_t out = kSentinel;
    std::uint32_t want = 0;
    const BitResult r = bitrank_select1(bits, nbits, dir, need, k, &out);
    if (select_of(k, &want)) {
      if (r != BitResult::Ok || out != want) ++bad;
    } else if (r != BitResult::OutOfRange || out != kSentinel) {
      ++bad;
    }
  };
  select_check(0);
  select_check(total / 2u);
  if (total > 0u) select_check(total - 1u);
  select_check(total);
  select_check(0xFFFFFFFFu);
  if (stride_select) {
    for (std::uint32_t k = 0; k < total; k += 4099u) select_check(k);
  }
  return bad;
}

}  // namespace

TEST_CASE("sizes: 2^24 bits (262,144 words, 262,145 directory words), sampled queries", "[max][bitrank]") {
  const std::uint32_t nbits = kMaxBits;
  const std::uint32_t words = nbits / 64u;
  std::uint32_t need = 0;
  REQUIRE(bitrank_dir_words(nbits, &need) == BitResult::Ok);
  REQUIRE(need == words + 1u);
  std::unique_ptr<std::uint64_t[]> bits(new std::uint64_t[words]);  // reused for every shape below
  std::unique_ptr<std::uint32_t[]> dir(new std::uint32_t[need]);
  auto fill = [&](std::uint64_t v) {
    for (std::uint32_t i = 0; i < words; ++i) bits[i] = v;
  };
  std::size_t bad = 0;
  // full
  fill(~0ull);
  bad += check_max(bits.get(), nbits, dir.get(), need, nbits, [](std::uint32_t i) { return i; },
                   [&](std::uint32_t k, std::uint32_t* p) {
                     if (k >= nbits) return false;
                     *p = k;
                     return true;
                   },
                   true);
  // empty
  fill(0);
  bad += check_max(bits.get(), nbits, dir.get(), need, 0u, [](std::uint32_t) { return 0u; }, [](std::uint32_t, std::uint32_t*) { return false; }, true);
  // alternating, the even positions set
  fill(0x5555555555555555ull);
  bad += check_max(bits.get(), nbits, dir.get(), need, nbits / 2u, [](std::uint32_t i) { return (i + 1u) / 2u; },
                   [&](std::uint32_t k, std::uint32_t* p) {
                     if (k >= nbits / 2u) return false;
                     *p = 2u * k;
                     return true;
                   },
                   true);
  // the last bit only
  fill(0);
  bits[words - 1u] = 1ull << 63;
  bad += check_max(bits.get(), nbits, dir.get(), need, 1u, [&](std::uint32_t i) { return i == nbits ? 1u : 0u; },
                   [&](std::uint32_t k, std::uint32_t* p) {
                     if (k >= 1u) return false;
                     *p = nbits - 1u;
                     return true;
                   },
                   false);
  // the first bit only
  fill(0);
  bits[0] = 1ull;
  bad += check_max(bits.get(), nbits, dir.get(), need, 1u, [](std::uint32_t i) { return i == 0u ? 0u : 1u; },
                   [](std::uint32_t k, std::uint32_t* p) {
                     if (k >= 1u) return false;
                     *p = 0u;
                     return true;
                   },
                   false);
  CHECK(bad == 0u);
}

TEST_CASE("sizes: 2^24 - 1 bits with all-ones padding in the last word, sampled queries", "[max][bitrank]") {
  const std::uint32_t nbits = kMaxBits - 1u;
  const std::uint32_t words = words_of(nbits);
  REQUIRE(words == 262144u);
  std::uint32_t need = 0;
  REQUIRE(bitrank_dir_words(nbits, &need) == BitResult::Ok);
  REQUIRE(need == words + 1u);
  std::unique_ptr<std::uint64_t[]> bits(new std::uint64_t[words]);
  std::unique_ptr<std::uint32_t[]> dir(new std::uint32_t[need]);
  for (std::uint32_t i = 0; i < words; ++i) bits[i] = ~0ull;  // the vector is full and so is the padding (position 2^24 - 1 is padding)
  std::size_t bad = check_max(bits.get(), nbits, dir.get(), need, nbits, [](std::uint32_t i) { return i; },
                              [&](std::uint32_t k, std::uint32_t* p) {
                                if (k >= nbits) return false;
                                *p = k;
                                return true;
                              },
                              true);
  // the same size with an empty vector and all-ones padding: nothing counts
  for (std::uint32_t i = 0; i < words; ++i) bits[i] = 0;
  bits[words - 1u] = ~0ull << 63;
  bad += check_max(bits.get(), nbits, dir.get(), need, 0u, [](std::uint32_t) { return 0u; }, [](std::uint32_t, std::uint32_t*) { return false; }, false);
  CHECK(bad == 0u);
}

TEST_CASE("null bits is an error for any nbits above zero, including 1", "[args][bitrank]") {
  std::uint32_t dir[2] = {kSentinel, kSentinel};
  std::uint32_t out = kSentinel;
  for (std::uint32_t n : {1u, 2u, 64u, 65u}) {
    CHECK(bitrank_build(nullptr, n, dir, 2) == BitResult::NullArgument);
    CHECK(bitrank_rank1(nullptr, n, dir, 2, 0, &out) == BitResult::NullArgument);
    CHECK(bitrank_select1(nullptr, n, dir, 2, 0, &out) == BitResult::NullArgument);
  }
  CHECK(dir[0] == kSentinel);
  CHECK(dir[1] == kSentinel);
  CHECK(out == kSentinel);
}
