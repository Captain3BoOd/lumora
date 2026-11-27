// Tests for core/globmatch. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/globmatch_ref.py, an independent Python implementation (a dynamic
// programme over parsed tokens, cross-checked there against a regular-expression translation). Regenerate it with: python3 tools/globmatch_ref.py
// The tests also carry a naive oracle that shares no code with core/globmatch/globmatch.cpp: the pattern is decoded into tokens first and then
// matched with a row-by-row dynamic programme over the text (a table, no saved star, no backtracking).
// Buffers are heap buffers of exactly the stated length (AddressSanitizer sees any read past the end); *out_match is preset to a value that a
// wrong result would keep, and a failure must leave it untouched. The full maximum-length quadratic worst case (65536 x 65536) is NOT run: the
// adversarial shapes run at 4096 and 8192 bytes, and the shapes at 65536 are only those whose cost is linear. Only correctness is asserted,
// never time.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "globmatch.h"

using namespace lumora::globmatch;

namespace {

// BEGIN GENERATED REFERENCE (tools/globmatch_ref.py)
const std::uint32_t kRefRandSeeds = 400;
const std::uint64_t kRefDigest_exh = 0xB6CEAA74EBA19726ull;
const std::size_t kRefCases_exh = 609961, kRefOps_exh = 609961;
const std::uint64_t kRefDigest_bytes = 0x76E0A9697C582FD1ull;
const std::size_t kRefCases_bytes = 160000, kRefOps_bytes = 160000;
const std::uint64_t kRefDigest_rand = 0xEB2F509BF41C6452ull;
const std::size_t kRefCases_rand = 400, kRefOps_rand = 400;
// END GENERATED REFERENCE

using Bytes = std::vector<unsigned char>;

std::uint32_t code(GlobResult r) { return static_cast<std::uint32_t>(r); }

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

Bytes bytes_of(const std::string& s) { return Bytes(s.begin(), s.end()); }

// ---- oracle ----
// Result code and match for the given pattern and text. Decodes the pattern into tokens, then fills a row of "the first i tokens match the
// first j text bytes" for every token.
GlobResult o_expect(const Bytes& p, const Bytes& t, bool* match) {
  struct Tok {
    int kind;  // 0 star, 1 any, 2 literal
    unsigned char b;
  };
  std::vector<Tok> toks;
  for (std::size_t i = 0; i < p.size();) {
    if (p[i] == 0x5Cu) {
      if (i + 1 >= p.size()) return GlobResult::BadPattern;
      toks.push_back({2, p[i + 1]});
      i += 2;
    } else if (p[i] == 0x2Au) {
      toks.push_back({0, 0});
      ++i;
    } else if (p[i] == 0x3Fu) {
      toks.push_back({1, 0});
      ++i;
    } else {
      toks.push_back({2, p[i]});
      ++i;
    }
  }
  const std::size_t m = t.size();
  std::vector<char> row(m + 1, 0);
  row[0] = 1;
  for (const Tok& k : toks) {
    std::vector<char> nxt(m + 1, 0);
    if (k.kind == 0) {
      char seen = 0;
      for (std::size_t j = 0; j <= m; ++j) {
        if (row[j] != 0) seen = 1;
        nxt[j] = seen;
      }
    } else {
      for (std::size_t j = 1; j <= m; ++j) {
        if (row[j - 1] != 0 && (k.kind == 1 || t[j - 1] == k.b)) nxt[j] = 1;
      }
    }
    row.swap(nxt);
  }
  *match = row[m] != 0;
  return GlobResult::Ok;
}

// ---- harness ----
std::unique_ptr<char[]> exact(const Bytes& v) {
  std::unique_ptr<char[]> b(new char[v.empty() ? 1 : v.size()]);
  for (std::size_t i = 0; i < v.size(); ++i) b[i] = static_cast<char>(v[i]);
  return b;
}

// Calls glob_match with exact buffers; returns the result code and the match (the preset is the opposite of `preset_to`).
GlobResult call(const Bytes& p, const Bytes& t, bool preset, bool* out_value) {
  std::unique_ptr<char[]> pb = exact(p);
  std::unique_ptr<char[]> tb = exact(t);
  bool out = preset;
  const GlobResult r = glob_match(pb.get(), static_cast<std::uint32_t>(p.size()), tb.get(), static_cast<std::uint32_t>(t.size()), &out);
  *out_value = out;
  return r;
}

// Runs one case against the oracle; appends to the fold when f is not null.
void do_match(Fold* f, std::size_t* bad, const Bytes& p, const Bytes& t) {
  bool want = false;
  const GlobResult wr = o_expect(p, t, &want);
  const bool preset = wr == GlobResult::Ok ? !want : (p.size() % 2u == 0u);
  bool got = false;
  const GlobResult r = call(p, t, preset, &got);
  if (r != wr) ++*bad;
  if (wr == GlobResult::Ok) {
    if (got != want) ++*bad;
  } else {
    if (got != preset) ++*bad;  // a failure leaves *out_match untouched
  }
  if (f != nullptr) {
    ++f->ops;
    ++f->cases;
    f->add((static_cast<std::uint64_t>(code(r)) << 32) | ((r == GlobResult::Ok && got) ? 1u : 0u));
  }
}

std::vector<Bytes> all_strings(const std::vector<unsigned char>& alpha, std::size_t maxlen) {
  std::vector<Bytes> out;
  out.push_back(Bytes());
  std::size_t begin = 0;
  for (std::size_t len = 1; len <= maxlen; ++len) {
    const std::size_t end = out.size();
    for (std::size_t i = begin; i < end; ++i) {
      for (unsigned char c : alpha) {
        Bytes s = out[i];
        s.push_back(c);
        out.push_back(s);
      }
    }
    begin = end;
  }
  return out;
}

void rand_case(std::uint32_t seed, Bytes* pat, Bytes* text) {
  static const unsigned char kSym[10] = {'a', 'b', 'c', '?', '*', '\\', 0x00, 0xFF, '?', '*'};
  static const unsigned char kTs[8] = {'a', 'b', 'c', 0x00, 0xFF, '\\', '*', '?'};
  Lcg g(static_cast<std::uint64_t>(seed) * 7919u + 13u);
  pat->clear();
  text->clear();
  const std::uint32_t plen = g.next() % 31u;
  for (std::uint32_t i = 0; i < plen; ++i) pat->push_back(kSym[g.next() % 10u]);
  const std::uint32_t mode = g.next() % 3u;
  if (mode == 0u) {
    const std::uint32_t tlen = g.next() % 41u;
    for (std::uint32_t i = 0; i < tlen; ++i) text->push_back(kTs[g.next() % 8u]);
  } else {
    std::size_t i = 0;
    while (i < pat->size()) {
      const unsigned char c = (*pat)[i];
      if (c == '\\') {
        if (i + 1 < pat->size()) {
          text->push_back((*pat)[i + 1]);
          i += 2;
        } else {
          i += 1;
        }
      } else if (c == '*') {
        const std::uint32_t k = g.next() % 4u;
        for (std::uint32_t j = 0; j < k; ++j) text->push_back(kTs[g.next() % 8u]);
        i += 1;
      } else if (c == '?') {
        text->push_back(kTs[g.next() % 8u]);
        i += 1;
      } else {
        text->push_back(c);
        i += 1;
      }
    }
    if (mode == 2u && !text->empty()) {
      const std::size_t pos = g.next() % text->size();
      (*text)[pos] = kTs[g.next() % 8u];
    }
  }
}

// Asserts a hand-written expectation: expect 1 = match, 0 = no match, -1 = BadPattern. Both the implementation and (when use_oracle) the
// oracle must agree with it.
std::size_t check_expect(const Bytes& p, const Bytes& t, int expect, bool use_oracle) {
  std::size_t bad = 0;
  for (int pre = 0; pre < 2; ++pre) {
    bool got = false;
    const GlobResult r = call(p, t, pre != 0, &got);
    if (expect < 0) {
      if (r != GlobResult::BadPattern || got != (pre != 0)) ++bad;
    } else {
      if (r != GlobResult::Ok || got != (expect == 1)) ++bad;
    }
  }
  if (use_oracle) {
    bool want = false;
    const GlobResult wr = o_expect(p, t, &want);
    if (expect < 0) {
      if (wr != GlobResult::BadPattern) ++bad;
    } else if (wr != GlobResult::Ok || want != (expect == 1)) {
      ++bad;
    }
  }
  return bad;
}

Bytes rep(const std::string& unit, std::size_t times) {
  Bytes out;
  out.reserve(unit.size() * times);
  for (std::size_t i = 0; i < times; ++i) out.insert(out.end(), unit.begin(), unit.end());
  return out;
}

Bytes cat(Bytes a, const Bytes& b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

}  // namespace

TEST_CASE("header anchors", "[globmatch]") {
  CHECK(kMaxLen == (1u << 16));
  CHECK(code(GlobResult::Ok) == 0u);
  CHECK(code(GlobResult::NullArgument) == 1u);
  CHECK(code(GlobResult::TooLarge) == 2u);
  CHECK(code(GlobResult::BadPattern) == 3u);
}

TEST_CASE("exhaustive: every pattern and text of length 0 to 4 over a b ? * backslash", "[sweep][globmatch]") {
  const std::vector<unsigned char> alpha = {'a', 'b', '?', '*', '\\'};
  const std::vector<Bytes> all = all_strings(alpha, 4);
  Fold f;
  std::size_t bad = 0;
  for (const Bytes& p : all) {
    for (const Bytes& t : all) do_match(&f, &bad, p, t);
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_exh);
  CHECK(f.ops == kRefOps_exh);
  CHECK(f.h == kRefDigest_exh);
}

TEST_CASE("byte sweep: every pattern and text of length 0 to 3 over 0x00 0x7F 0x80 0xFF ? * backslash", "[sweep][globmatch]") {
  const std::vector<unsigned char> alpha = {0x00, 0x7F, 0x80, 0xFF, '?', '*', '\\'};
  const std::vector<Bytes> all = all_strings(alpha, 3);
  Fold f;
  std::size_t bad = 0;
  for (const Bytes& p : all) {
    for (const Bytes& t : all) do_match(&f, &bad, p, t);
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_bytes);
  CHECK(f.ops == kRefOps_bytes);
  CHECK(f.h == kRefDigest_bytes);
}

TEST_CASE("pseudo-random pattern and text pairs (not exhaustive)", "[sweep][globmatch]") {
  Fold f;
  std::size_t bad = 0;
  std::size_t matched = 0;
  for (std::uint32_t seed = 1; seed <= kRefRandSeeds; ++seed) {
    Bytes p, t;
    rand_case(seed, &p, &t);
    do_match(&f, &bad, p, t);
    bool m = false;
    if (o_expect(p, t, &m) == GlobResult::Ok && m) ++matched;
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_rand);
  CHECK(f.ops == kRefOps_rand);
  CHECK(f.h == kRefDigest_rand);
  CHECK(matched > 40u);  // the scenario really exercises matches, not only mismatches
}

TEST_CASE("hand-checked patterns", "[globmatch]") {
  struct Row {
    std::string p, t;
    int expect;
  };
  const std::vector<Row> rows = {
      {"", "", 1}, {"", "a", 0}, {"a", "", 0}, {"*", "", 1}, {"*", "abc", 1}, {"**", "", 1}, {"***x", "x", 1}, {"?", "", 0}, {"?", "a", 1},
      {"?", "ab", 0}, {"a?c", "abc", 1}, {"a?c", "ac", 0}, {"a*c", "abc", 1}, {"a*c", "ac", 1}, {"a*c", "acb", 0}, {"*a", "ba", 1},
      {"*a", "ab", 0}, {"a*", "a", 1}, {"a*", "ab", 1}, {"a*b*c", "aXbYc", 1}, {"a*b*c", "aXcYb", 0}, {"*abc*", "xxabcxx", 1},
      {"*ab*ab", "ababab", 1}, {"*ab*ab", "abab", 1}, {"*ab*ab", "aab", 0}, {"a*a*b", "aaab", 1}, {"*aab", "aaaab", 1}, {"*abab", "abaabab", 1},
      {"*abab", "ababa", 0}, {"a*b", "ab", 1}, {"a*b", "a", 0}, {"*?", "", 0}, {"*?", "z", 1}, {"?*?", "ab", 1}, {"?*?", "a", 0},
      {"A", "a", 0}, {"a", "A", 0},
      // escapes
      {"\\*", "*", 1}, {"\\*", "a", 0}, {"\\*", "", 0}, {"\\*", "**", 0}, {"\\?", "?", 1}, {"\\?", "a", 0}, {"\\\\", "\\", 1}, {"\\\\", "\\\\", 0},
      {"\\\\", "", 0}, {"\\a", "a", 1}, {"\\a", "\\a", 0}, {"a\\*b", "a*b", 1}, {"a\\*b", "axb", 0}, {"a\\*b", "a*bb", 0}, {"*\\*", "a*", 1},
      {"*\\*", "a*b", 0}, {"\\**", "*abc", 1}, {"\\**", "abc", 0}, {"\\\\\\\\", "\\\\", 1}, {"a\\\\", "a\\", 1}, {"\\\\*", "\\xyz", 1},
      {"\\\\?", "\\", 0}, {"\\\\?", "\\\\", 1},
      // wildcards in the text are ordinary bytes
      {"a", "?", 0}, {"?", "?", 1}, {"*", "*", 1}, {"?b", "?b", 1}, {"ab", "?b", 0}, {"a*", "a*", 1}, {"a?", "a*", 1}, {"a?", "a\\", 1},
      // a trailing lone backslash is BadPattern wherever the text is
      {"\\", "", -1}, {"\\", "a", -1}, {"a\\", "a", -1}, {"a\\", "b", -1}, {"a\\", "", -1}, {"\\\\\\", "\\", -1}, {"*\\", "x", -1},
      {"\\*\\", "*", -1}, {"?\\", "ab", -1}, {"\\\\\\\\\\", "abc", -1}, {"*\\\\\\", "", -1},
  };
  std::size_t bad = 0;
  for (const Row& r : rows) bad += check_expect(bytes_of(r.p), bytes_of(r.t), r.expect, true);
  CHECK(bad == 0u);
  CHECK(rows.size() == 79u);
}

TEST_CASE("hand-checked bytes: NUL, 0x7F, 0x80, 0xFF are ordinary bytes and '?' matches them", "[globmatch]") {
  struct Row {
    Bytes p, t;
    int expect;
  };
  const std::vector<Row> rows = {
      {{'a', 0x00, 'b'}, {'a', 0x00, 'b'}, 1},
      {{'a', 0x00, 'b'}, {'a', 'b'}, 0},
      {{'a', 0x00, 'b'}, {'a', 0x01, 'b'}, 0},
      {{'?'}, {0x00}, 1},
      {{'?'}, {0xFF}, 1},
      {{'?'}, {0x80}, 1},
      {{'?', '?'}, {0x00, 0xFF}, 1},
      {{0xFF}, {0xFF}, 1},
      {{0x80}, {0x7F}, 0},
      {{0x80}, {0x80}, 1},
      {{0xFF}, {0x7F}, 0},
      {{0xFF}, {0x00}, 0},
      {{0x00}, {0x00}, 1},
      {{0x00}, {}, 0},
      {{'*'}, {0x00, 0x80, 0xFF, 0x7F}, 1},
      {{'*', 0xFF}, {0x00, 0x80, 0xFF}, 1},
      {{'*', 0xFF}, {0xFF, 0x80}, 0},
      {{'*', 0x80, '*'}, {0x7F, 0x80, 0x7F}, 1},
      {{'\\', 0x00}, {0x00}, 1},
      {{'\\', 0xFF}, {0xFF}, 1},
      {{'\\', 0xFF}, {0x7F}, 0},
      {{'\\', '?'}, {0xFF}, 0},
      {{'\\', 'a'}, {'a'}, 1},
      {{'a', '\\', 0x00}, {'a', 0x00}, 1},
      {{'a', 0x00, '\\'}, {'a', 0x00}, -1},
  };
  std::size_t bad = 0;
  for (const Row& r : rows) bad += check_expect(r.p, r.t, r.expect, true);
  CHECK(bad == 0u);
}

TEST_CASE("null rules and check order: Null, TooLarge, BadPattern", "[args][globmatch]") {
  const unsigned char one[1] = {'a'};
  const char* const pc = reinterpret_cast<const char*>(one);
  bool out = true;
  // an empty pattern or text may be null
  CHECK(glob_match(nullptr, 0, nullptr, 0, &out) == GlobResult::Ok);
  CHECK(out == true);
  out = true;
  CHECK(glob_match(nullptr, 0, pc, 1, &out) == GlobResult::Ok);
  CHECK(out == false);
  out = false;
  CHECK(glob_match(nullptr, 0, nullptr, 0, &out) == GlobResult::Ok);
  CHECK(out == true);
  out = false;
  CHECK(glob_match(pc, 1, nullptr, 0, &out) == GlobResult::Ok);
  CHECK(out == false);
  out = false;
  const char star = '*';
  CHECK(glob_match(&star, 1, nullptr, 0, &out) == GlobResult::Ok);
  CHECK(out == true);
  // null pointers with a length above 0, and a null out_match, are NullArgument and write nothing
  for (int pre = 0; pre < 2; ++pre) {
    out = pre != 0;
    CHECK(glob_match(nullptr, 1, pc, 1, &out) == GlobResult::NullArgument);
    CHECK(glob_match(pc, 1, nullptr, 1, &out) == GlobResult::NullArgument);
    CHECK(glob_match(nullptr, 1, nullptr, 1, &out) == GlobResult::NullArgument);
    CHECK(out == (pre != 0));
  }
  CHECK(glob_match(pc, 1, pc, 1, nullptr) == GlobResult::NullArgument);
  CHECK(glob_match(nullptr, 0, nullptr, 0, nullptr) == GlobResult::NullArgument);
  // Null beats TooLarge
  CHECK(glob_match(nullptr, kMaxLen + 1u, pc, 1, &out) == GlobResult::NullArgument);
  CHECK(glob_match(pc, 1, nullptr, kMaxLen + 1u, &out) == GlobResult::NullArgument);
  CHECK(glob_match(pc, kMaxLen + 1u, pc, kMaxLen + 1u, nullptr) == GlobResult::NullArgument);
  // TooLarge on either length, with a 1-byte buffer (nothing may be read), beats BadPattern and writes nothing
  const char bs = '\\';
  for (int pre = 0; pre < 2; ++pre) {
    out = pre != 0;
    CHECK(glob_match(pc, kMaxLen + 1u, pc, 1, &out) == GlobResult::TooLarge);
    CHECK(glob_match(pc, 1, pc, kMaxLen + 1u, &out) == GlobResult::TooLarge);
    CHECK(glob_match(pc, kMaxLen + 1u, pc, kMaxLen + 1u, &out) == GlobResult::TooLarge);
    CHECK(glob_match(&bs, kMaxLen + 1u, pc, 1, &out) == GlobResult::TooLarge);
    CHECK(glob_match(&bs, 1, pc, kMaxLen + 1u, &out) == GlobResult::TooLarge);
    CHECK(glob_match(pc, 0xFFFFFFFFu, pc, 1, &out) == GlobResult::TooLarge);
    CHECK(glob_match(pc, 1, pc, 0xFFFFFFFFu, &out) == GlobResult::TooLarge);
    CHECK(out == (pre != 0));
  }
  // BadPattern with an empty or null text, and writes nothing
  for (int pre = 0; pre < 2; ++pre) {
    out = pre != 0;
    CHECK(glob_match(&bs, 1, nullptr, 0, &out) == GlobResult::BadPattern);
    CHECK(glob_match(&bs, 1, pc, 1, &out) == GlobResult::BadPattern);
    CHECK(out == (pre != 0));
  }
}

TEST_CASE("the whole pattern is checked for a trailing lone backslash before any matching", "[args][globmatch]") {
  // A pattern that would fail at its first byte, or that matches the text before the backslash, is still BadPattern.
  std::size_t bad = 0;
  bad += check_expect(bytes_of("b\\"), bytes_of("a"), -1, true);
  bad += check_expect(bytes_of("a\\"), bytes_of("a"), -1, true);
  bad += check_expect(bytes_of("*\\"), bytes_of("anything"), -1, true);
  bad += check_expect(bytes_of("a*\\"), bytes_of(""), -1, true);
  bad += check_expect(bytes_of("\\*\\"), bytes_of("*"), -1, true);
  bad += check_expect(bytes_of("\\\\\\"), bytes_of("\\"), -1, true);        // 3 backslashes: a literal and a lone one
  bad += check_expect(bytes_of("\\\\\\\\"), bytes_of("\\\\"), 1, true);     // 4 backslashes: two literals
  bad += check_expect(bytes_of("x\\\\\\\\\\"), bytes_of("x\\\\"), -1, true);  // 5
  bad += check_expect(bytes_of("\\a\\"), bytes_of("a"), -1, true);
  CHECK(bad == 0u);
}

TEST_CASE("sizes: exact 65536-byte buffers, linear-cost shapes only", "[max][globmatch]") {
  const std::size_t n = kMaxLen;
  std::size_t bad = 0;
  const Bytes as = rep("a", n);
  Bytes as_b = as;  // a...ab
  as_b[n - 1] = 'b';
  Bytes b_as = as;  // ba...a
  b_as[0] = 'b';
  Bytes mixed(n);
  for (std::size_t i = 0; i < n; ++i) mixed[i] = static_cast<unsigned char>(0x80u + (i * 7u + 3u) % 128u);  // no wildcard or backslash bytes

  bad += check_expect(rep("*", n), mixed, 1, false);
  bad += check_expect(rep("*", n), Bytes(), 1, false);
  bad += check_expect(rep("?", n), mixed, 1, false);
  bad += check_expect(rep("?", n), Bytes(mixed.begin(), mixed.end() - 1), 0, false);
  bad += check_expect(as, as, 1, false);
  bad += check_expect(as, as_b, 0, false);
  bad += check_expect(as, b_as, 0, false);
  bad += check_expect(as, Bytes(as.begin(), as.end() - 1), 0, false);
  bad += check_expect(Bytes(), as, 0, false);
  bad += check_expect(mixed, mixed, 1, false);
  {
    Bytes m2 = mixed;
    m2[n - 1] = static_cast<unsigned char>(m2[n - 1] + 1u);
    bad += check_expect(mixed, m2, 0, false);
  }
  bad += check_expect(rep("*a", n / 2), as, 1, false);                           // every token matches at once
  bad += check_expect(rep("*a", n / 2), as_b, 0, false);                         // ends with a literal a, the text ends with b
  bad += check_expect(cat(Bytes{'*'}, bytes_of("b")), as, 0, false);              // "*b": every backtrack fails at once
  bad += check_expect(cat(Bytes{'*'}, bytes_of("b")), as_b, 1, false);
  bad += check_expect(cat(rep("a", n - 1), bytes_of("*")), as, 1, false);
  bad += check_expect(cat(rep("a", n - 1), bytes_of("*")), b_as, 0, false);
  bad += check_expect(cat(Bytes{'*'}, rep("a", n - 1)), as, 1, false);
  // backslash runs at the very end
  bad += check_expect(cat(rep("a", n - 1), bytes_of("\\")), as, -1, false);
  bad += check_expect(cat(rep("a", n - 2), bytes_of("\\\\")), cat(rep("a", n - 2), bytes_of("\\")), 1, false);
  bad += check_expect(cat(rep("a", n - 3), bytes_of("\\\\\\")), cat(rep("a", n - 3), bytes_of("\\")), -1, false);
  bad += check_expect(rep("\\", n), rep("\\", n / 2), 1, false);
  bad += check_expect(rep("\\", n), rep("\\", n / 2 - 1), 0, false);
  bad += check_expect(rep("\\", n - 1), rep("\\", n / 2), -1, false);
  bad += check_expect(rep("\\*", n / 2), rep("*", n / 2), 1, false);
  bad += check_expect(rep("\\*", n / 2), rep("*", n / 2 - 1), 0, false);
  CHECK(bad == 0u);
}

TEST_CASE("sizes: one byte over the limit is TooLarge on either side, nothing is read", "[max][globmatch]") {
  const unsigned char one = 'a';
  const char* const pc = reinterpret_cast<const char*>(&one);
  bool out = true;
  CHECK(glob_match(pc, kMaxLen + 1u, pc, 1, &out) == GlobResult::TooLarge);
  CHECK(glob_match(pc, 1, pc, kMaxLen + 1u, &out) == GlobResult::TooLarge);
  CHECK(out == true);
}

TEST_CASE("adversarial shapes: quadratic work on real buffers of 4096 (oracle too) and 8192 bytes", "[adversarial][globmatch]") {
  // Shapes that make a backtracking matcher do the most work. The full 65536 x 65536 worst case (about 4e9 steps) is NOT run here.
  std::size_t bad = 0;
  for (std::size_t n : {static_cast<std::size_t>(4096), static_cast<std::size_t>(8192)}) {
    const bool oracle = n <= 4096u;
    const std::size_t h = n / 2;
    const Bytes as = rep("a", n);
    Bytes as_b = rep("a", n - 1);
    as_b.push_back('b');
    // "*" a^(n/2) "b" against a run of a: every start is tried and each scans n/2 bytes
    const Bytes c = cat(cat(Bytes{'*'}, rep("a", h)), bytes_of("b"));
    bad += check_expect(c, as, 0, oracle);
    bad += check_expect(c, as_b, 1, oracle);
    // repeated "*aa" then "b"
    const std::size_t k = (n - 1u) / 3u;
    const Bytes d = cat(rep("*aa", k), bytes_of("b"));
    bad += check_expect(d, as, 0, oracle);
    bad += check_expect(d, as_b, 1, oracle);
    // "*a" repeated, then "b"
    const Bytes e = cat(rep("*a", (n - 1u) / 2u), bytes_of("b"));
    bad += check_expect(e, as, 0, oracle);
    bad += check_expect(e, as_b, 1, oracle);
    // a^(n/2) "*" a^(n/2 - 1) "b" : the prefix matches at once, then the star retries
    const Bytes g = cat(cat(rep("a", h), bytes_of("*")), cat(rep("a", h - 2), bytes_of("b")));
    bad += check_expect(g, as, 0, oracle);
    bad += check_expect(g, as_b, 1, oracle);
    // "a*" repeated against a run of a then b: ends with a star
    bad += check_expect(rep("a*", n / 2), as_b, 1, oracle);
    // star-question mixes: "*?" repeated needs at least n/2 bytes
    bad += check_expect(rep("*?", n / 2), as, 1, oracle);
    bad += check_expect(cat(rep("*?", n / 2), rep("?", 1)), rep("a", n / 2), 0, oracle);
  }
  CHECK(bad == 0u);
}
