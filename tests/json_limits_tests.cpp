// Tests for the JSON structural pre-scan (core/package/json_limits.h). Pure logic: no files, no
// GPU. One rule per test case, with the exact limit and limit + 1. See docs/json-limits.md.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "json_limits.h"

using namespace lumora::package;

namespace {

constexpr std::uint64_t kMax64 = std::numeric_limits<std::uint64_t>::max();

JsonScanResult scan(std::string_view text, const JsonLimits& l = {}) { return scan_json(text, l); }

#define CHECK_SCAN(text, limits, v, off)             \
  do {                                               \
    const JsonScanResult r_ = scan((text), (limits)); \
    CHECK(r_.verdict == (v));                        \
    CHECK(r_.offset == (off));                       \
  } while (0)

std::string bytes(std::initializer_list<int> list) {
  std::string s;
  for (int b : list) s.push_back(static_cast<char>(b));
  return s;
}

}  // namespace

// ---- accepted input ----

TEST_CASE("json scan: simple documents are accepted with offset zero") {
  for (const char* t : {"{}", "[]", "{\"a\":1}", "[1,2,3]", "{\"a\":[1,{\"b\":null}],\"c\":\"x\"}", "1", "\"x\"",
                        "null", "true", "false", "  [ 1 , 2 ]  ", "\t\r\n{ \"a\" : true }\n"}) {
    const JsonScanResult r = scan(t);
    INFO(t);
    CHECK(r.verdict == JsonVerdict::Ok);
    CHECK(r.offset == 0);
  }
}

TEST_CASE("json scan: numbers are checked loosely") {
  for (const char* t : {"-", "01", "1.", "1e+5", "[-0.5E-3, 1.2.3]", "-+1"}) {
    INFO(t);
    CHECK(scan(t).verdict == JsonVerdict::Ok);
  }
}

TEST_CASE("json scan: escapes are accepted") {
  CHECK(scan(R"(["\" \\ \/ \b \f \n \r \t \u0041 \u00e9 \uD83D\uDE00"])").verdict == JsonVerdict::Ok);
}

TEST_CASE("json scan: bytes above 0x7F inside strings are not checked for UTF-8") {
  CHECK(scan("[\"" + bytes({0xFF, 0xC0}) + "\"]").verdict == JsonVerdict::Ok);
}

// ---- total bytes ----

TEST_CASE("json scan: size at the limit is accepted and one more byte is TooLarge") {
  JsonLimits l;
  l.max_bytes = 10;
  CHECK_SCAN("\"" + std::string(8, 'a') + "\"", l, JsonVerdict::Ok, 0u);                   // 10 bytes
  CHECK_SCAN("\"" + std::string(9, 'a') + "\"", l, JsonVerdict::TooLarge, 10u);            // 11 bytes
}

TEST_CASE("json scan: a size limit of uint64 max accepts a normal text") {
  JsonLimits l;
  l.max_bytes = kMax64;
  CHECK(scan("[1]", l).verdict == JsonVerdict::Ok);
}

TEST_CASE("json scan: a size limit of zero rejects any text") {
  JsonLimits l;
  l.max_bytes = 0;
  CHECK_SCAN("1", l, JsonVerdict::TooLarge, 0u);
  CHECK_SCAN("", l, JsonVerdict::Malformed, 0u);  // empty text is not too large, it is not JSON
}

TEST_CASE("json scan: TooLarge is reported before any scanning") {
  JsonLimits l;
  l.max_bytes = 3;
  CHECK_SCAN("[[[[", l, JsonVerdict::TooLarge, 3u);  // would also be unbalanced
}

// ---- nesting depth ----

TEST_CASE("json scan: depth at the limit is accepted and one more is TooDeep") {
  JsonLimits l;
  l.max_depth = 3;
  CHECK_SCAN("[[[]]]", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("[[[[]]]]", l, JsonVerdict::TooDeep, 3u);  // the fourth '['
}

TEST_CASE("json scan: depth counts arrays and objects together") {
  JsonLimits l;
  l.max_depth = 3;
  CHECK_SCAN("{\"a\":[{\"b\":1}]}", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("{\"a\":[{\"b\":[]}]}", l, JsonVerdict::TooDeep, 11u);
}

TEST_CASE("json scan: the top-level container is depth 1") {
  JsonLimits l;
  l.max_depth = 1;
  CHECK_SCAN("[1]", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("[[1]]", l, JsonVerdict::TooDeep, 1u);
}

TEST_CASE("json scan: a depth limit of zero rejects containers but not scalars") {
  JsonLimits l;
  l.max_depth = 0;
  CHECK_SCAN("[]", l, JsonVerdict::TooDeep, 0u);
  CHECK_SCAN("{}", l, JsonVerdict::TooDeep, 0u);
  CHECK_SCAN("1", l, JsonVerdict::Ok, 0u);
}

TEST_CASE("json scan: a depth limit of uint32 max accepts deep nesting") {
  JsonLimits l;
  l.max_depth = std::numeric_limits<std::uint32_t>::max();
  const std::size_t depth = 5000;
  CHECK(scan(std::string(depth, '[') + std::string(depth, ']'), l).verdict == JsonVerdict::Ok);
}

TEST_CASE("json scan: 100000 levels of nesting stop at the depth limit without recursion") {
  const std::string deep(100000, '[');
  CHECK_SCAN(deep, JsonLimits{}, JsonVerdict::TooDeep, 32u);
  const std::string deep_obj = [] {
    std::string s;
    for (int i = 0; i < 100000; ++i) s += "{\"a\":";
    return s;
  }();
  CHECK(scan(deep_obj).verdict == JsonVerdict::TooDeep);
}

// ---- string and key length ----

TEST_CASE("json scan: string length at the limit is accepted and one more byte is StringTooLong") {
  JsonLimits l;
  l.max_string_bytes = 4;
  CHECK_SCAN("\"abcd\"", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("\"abcde\"", l, JsonVerdict::StringTooLong, 5u);  // offset of the byte past the limit
}

TEST_CASE("json scan: escapes count by their decoded length") {
  JsonLimits l;
  l.max_string_bytes = 4;
  CHECK_SCAN(R"("\n\n\n\n")", l, JsonVerdict::Ok, 0u);            // four bytes
  CHECK_SCAN(R"("\n\n\n\n\n")", l, JsonVerdict::StringTooLong, 9u);
  CHECK_SCAN(R"("\u00e9\u00e9")", l, JsonVerdict::Ok, 0u);        // two bytes each
  CHECK_SCAN(R"("\u00e9\u00e9\u0041")", l, JsonVerdict::StringTooLong, 13u);
}

TEST_CASE("json scan: an escape that straddles the limit is reported at its backslash") {
  JsonLimits l;
  l.max_string_bytes = 3;
  CHECK_SCAN(R"("ab\u00e9")", l, JsonVerdict::StringTooLong, 3u);  // 2 + 2 > 3
  CHECK_SCAN(R"("\u20ac")", l, JsonVerdict::Ok, 0u);               // three bytes
}

TEST_CASE("json scan: a surrogate pair decodes to four bytes") {
  JsonLimits l;
  l.max_string_bytes = 4;
  CHECK_SCAN(R"("\ud83d\ude00")", l, JsonVerdict::Ok, 0u);
  l.max_string_bytes = 3;
  CHECK_SCAN(R"("\ud83d\ude00")", l, JsonVerdict::StringTooLong, 1u);
}

TEST_CASE("json scan: key length uses the same limit") {
  JsonLimits l;
  l.max_string_bytes = 4;
  CHECK_SCAN("{\"abcd\":1}", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("{\"abcde\":1}", l, JsonVerdict::StringTooLong, 6u);
}

TEST_CASE("json scan: a string limit of zero rejects any non-empty string") {
  JsonLimits l;
  l.max_string_bytes = 0;
  CHECK_SCAN("\"\"", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("\"a\"", l, JsonVerdict::StringTooLong, 1u);
}

TEST_CASE("json scan: a string limit of uint64 max accepts a long string") {
  JsonLimits l;
  l.max_string_bytes = kMax64;
  CHECK(scan("\"" + std::string(200000, 'a') + "\"", l).verdict == JsonVerdict::Ok);
}

TEST_CASE("json scan: a huge string stops at the limit offset") {
  const std::string big = "[\"" + std::string(10 * 1000 * 1000, 'a') + "\"]";
  JsonLimits l;
  l.max_bytes = kMax64;
  CHECK_SCAN(big, l, JsonVerdict::StringTooLong, 2u + 64 * 1024);
}

// ---- keys per object ----

TEST_CASE("json scan: keys per object at the limit are accepted and one more is TooManyKeys") {
  JsonLimits l;
  l.max_keys_per_object = 2;
  CHECK_SCAN("{\"a\":1,\"b\":2}", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("{\"a\":1,\"b\":2,\"c\":3}", l, JsonVerdict::TooManyKeys, 13u);  // quote of "c"
}

TEST_CASE("json scan: the key limit applies to each object separately") {
  JsonLimits l;
  l.max_keys_per_object = 1;
  CHECK_SCAN("{\"a\":{\"a\":1}}", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("[{\"a\":1},{\"a\":2}]", l, JsonVerdict::Ok, 0u);
}

TEST_CASE("json scan: a key limit of zero rejects any member") {
  JsonLimits l;
  l.max_keys_per_object = 0;
  CHECK_SCAN("{}", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("{\"a\":1}", l, JsonVerdict::TooManyKeys, 1u);
}

TEST_CASE("json scan: a key limit of uint64 max accepts many keys") {
  JsonLimits l;
  l.max_keys_per_object = kMax64;
  std::string s = "{";
  for (int i = 0; i < 5000; ++i) s += (i ? ",\"k" : "\"k") + std::to_string(i) + "\":0";
  s += "}";
  CHECK(scan(s, l).verdict == JsonVerdict::Ok);
}

// ---- array items ----

TEST_CASE("json scan: array items at the limit are accepted and one more is TooManyArrayItems") {
  JsonLimits l;
  l.max_array_items = 2;
  CHECK_SCAN("[1,2]", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("[1,2,3]", l, JsonVerdict::TooManyArrayItems, 5u);  // offset of the third item
}

TEST_CASE("json scan: the item limit applies to each array separately") {
  JsonLimits l;
  l.max_array_items = 2;
  CHECK_SCAN("[[1,2],[3,4]]", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("[[1,2,3]]", l, JsonVerdict::TooManyArrayItems, 6u);
}

TEST_CASE("json scan: an item limit of zero rejects any item") {
  JsonLimits l;
  l.max_array_items = 0;
  CHECK_SCAN("[]", l, JsonVerdict::Ok, 0u);
  CHECK_SCAN("[1]", l, JsonVerdict::TooManyArrayItems, 1u);
}

TEST_CASE("json scan: an item limit of uint64 max accepts many items") {
  JsonLimits l;
  l.max_array_items = kMax64;
  std::string s = "[";
  for (int i = 0; i < 100000; ++i) s += (i ? ",0" : "0");
  s += "]";
  l.max_total_values = kMax64;
  CHECK(scan(s, l).verdict == JsonVerdict::Ok);
}

// ---- total values ----

TEST_CASE("json scan: total values at the limit are accepted and one more is TooManyValues") {
  JsonLimits l;
  l.max_total_values = 3;
  CHECK_SCAN("[1,2]", l, JsonVerdict::Ok, 0u);  // the array and two items
  CHECK_SCAN("[1,2,3]", l, JsonVerdict::TooManyValues, 5u);
}

TEST_CASE("json scan: keys are not values, members are") {
  JsonLimits l;
  l.max_total_values = 2;
  CHECK_SCAN("{\"a\":1}", l, JsonVerdict::Ok, 0u);  // the object and one member value
  CHECK_SCAN("{\"a\":1,\"b\":2}", l, JsonVerdict::TooManyValues, 11u);
}

TEST_CASE("json scan: a values limit of zero rejects any value") {
  JsonLimits l;
  l.max_total_values = 0;
  CHECK_SCAN("1", l, JsonVerdict::TooManyValues, 0u);
}

TEST_CASE("json scan: a values limit of uint64 max accepts a normal text") {
  JsonLimits l;
  l.max_total_values = kMax64;
  CHECK(scan("[1,[2,[3]]]", l).verdict == JsonVerdict::Ok);
}

// ---- duplicate keys ----

TEST_CASE("json scan: a repeated key is DuplicateKey at the second key") {
  CHECK_SCAN("{\"a\":1,\"a\":2}", JsonLimits{}, JsonVerdict::DuplicateKey, 7u);
}

TEST_CASE("json scan: an escaped spelling of a key is a duplicate of the plain one") {
  CHECK_SCAN(R"({"a":1,"\u0061":2})", JsonLimits{}, JsonVerdict::DuplicateKey, 7u);
  CHECK_SCAN(R"({"\u0061":1,"a":2})", JsonLimits{}, JsonVerdict::DuplicateKey, 12u);
  CHECK_SCAN(R"({"a\/b":1,"a/b":2})", JsonLimits{}, JsonVerdict::DuplicateKey, 10u);
}

TEST_CASE("json scan: two escape spellings of the same key are duplicates") {
  CHECK(scan(R"({"\u00e9":1,"\u00E9":2})").verdict == JsonVerdict::DuplicateKey);
  CHECK(scan(R"({"\ud83d\ude00":1,"\uD83D\uDE00":2})").verdict == JsonVerdict::DuplicateKey);
}

TEST_CASE("json scan: a surrogate pair escape equals the raw UTF-8 character") {
  const std::string raw = bytes({0xF0, 0x9F, 0x98, 0x80});
  CHECK(scan("{\"" + raw + "\":1,\"\\ud83d\\ude00\":2}").verdict == JsonVerdict::DuplicateKey);
}

TEST_CASE("json scan: keys that differ in letter case are not duplicates") {
  CHECK(scan("{\"a\":1,\"A\":2}").verdict == JsonVerdict::Ok);
}

TEST_CASE("json scan: the same key in different objects is not a duplicate") {
  CHECK(scan("{\"a\":{\"a\":1},\"b\":{\"a\":2}}").verdict == JsonVerdict::Ok);
  CHECK(scan("[{\"a\":1},{\"a\":2}]").verdict == JsonVerdict::Ok);
}

TEST_CASE("json scan: a key that equals a string value is not a duplicate") {
  CHECK(scan("{\"a\":\"a\",\"b\":\"b\"}").verdict == JsonVerdict::Ok);
}

TEST_CASE("json scan: keys with an escaped NUL compare by decoded bytes") {
  CHECK(scan(R"({"a\u0000":1,"a\u0000":2})").verdict == JsonVerdict::DuplicateKey);
  CHECK(scan(R"({"a\u0000":1,"a":2})").verdict == JsonVerdict::Ok);
}

TEST_CASE("json scan: many distinct keys sharing a long common prefix are accepted") {
  const std::string prefix(1000, 'p');
  std::string s = "{";
  for (int i = 0; i < 2000; ++i) s += (i ? ",\"" : "\"") + prefix + std::to_string(i) + "\":0";
  s += "}";
  JsonLimits l;
  l.max_bytes = kMax64;
  CHECK(scan(s, l).verdict == JsonVerdict::Ok);
  // The same set with one repeated key is caught.
  s.pop_back();
  s += ",\"" + prefix + "1500\":0}";
  CHECK(scan(s, l).verdict == JsonVerdict::DuplicateKey);
}

TEST_CASE("json scan: 4096 keys is exactly the default limit and 4097 is TooManyKeys") {
  auto object_with = [](int count) {
    std::string s = "{";
    for (int i = 0; i < count; ++i) s += (i ? ",\"k" : "\"k") + std::to_string(i) + "\":0";
    return s + "}";
  };
  CHECK(scan(object_with(4096)).verdict == JsonVerdict::Ok);
  const std::string over = object_with(4097);
  const JsonScanResult r = scan(over);
  CHECK(r.verdict == JsonVerdict::TooManyKeys);
  CHECK(over.substr(r.offset, 7) == "\"k4096\"");
}

TEST_CASE("json scan: keys that differ only in the last byte are not duplicates")
{
  std::string s = "{";
  for (char c = 'a'; c <= 'z'; ++c) s += std::string(c == 'a' ? "" : ",") + "\"" + std::string(500, 'x') + c + "\":0";
  s += "}";
  CHECK(scan(s).verdict == JsonVerdict::Ok);
}

TEST_CASE("json scan: a duplicate is found among many keys") {
  std::string s = "{";
  for (int i = 0; i < 2000; ++i) s += "\"k" + std::to_string(i) + "\":0,";
  s += "\"k1234\":1}";
  const JsonScanResult r = scan(s);
  CHECK(r.verdict == JsonVerdict::DuplicateKey);
  CHECK(s.substr(r.offset, 7) == "\"k1234\"");
}

// ---- malformed input ----

TEST_CASE("json scan: empty and whitespace-only text is Malformed") {
  CHECK_SCAN("", JsonLimits{}, JsonVerdict::Malformed, 0u);
  CHECK_SCAN("  \n", JsonLimits{}, JsonVerdict::Malformed, 3u);
}

TEST_CASE("json scan: text after the top-level value is Malformed") {
  CHECK_SCAN("1 2", JsonLimits{}, JsonVerdict::Malformed, 2u);
  CHECK_SCAN("{} {}", JsonLimits{}, JsonVerdict::Malformed, 3u);
  CHECK_SCAN("[]]", JsonLimits{}, JsonVerdict::Malformed, 2u);
}

TEST_CASE("json scan: a UTF-8 byte order mark is Malformed") {
  CHECK_SCAN(bytes({0xEF, 0xBB, 0xBF}) + "{}", JsonLimits{}, JsonVerdict::Malformed, 0u);
}

TEST_CASE("json scan: raw control characters in strings are Malformed") {
  CHECK_SCAN(std::string("\"a\nb\""), JsonLimits{}, JsonVerdict::Malformed, 2u);
  CHECK_SCAN(std::string("\"a\0b\"", 5), JsonLimits{}, JsonVerdict::Malformed, 2u);
  CHECK_SCAN(std::string("\"a\tb\""), JsonLimits{}, JsonVerdict::Malformed, 2u);
  CHECK(scan("\"a\x7f" "b\"").verdict == JsonVerdict::Ok);  // 0x7F is not a JSON control character
}

TEST_CASE("json scan: bad escapes are Malformed at the backslash") {
  CHECK_SCAN(R"("a\x")", JsonLimits{}, JsonVerdict::Malformed, 2u);
  CHECK_SCAN(R"("\u12")", JsonLimits{}, JsonVerdict::Malformed, 1u);
  CHECK_SCAN(R"("\u12G4")", JsonLimits{}, JsonVerdict::Malformed, 1u);
  CHECK_SCAN("\"\\", JsonLimits{}, JsonVerdict::Malformed, 2u);
}

TEST_CASE("json scan: lone surrogate escapes are Malformed") {
  CHECK(scan(R"("\ud83d")").verdict == JsonVerdict::Malformed);
  CHECK(scan(R"("\ude00")").verdict == JsonVerdict::Malformed);
  CHECK(scan(R"("\ud83d\u0041")").verdict == JsonVerdict::Malformed);
  CHECK(scan(R"("\ud83dx")").verdict == JsonVerdict::Malformed);
  CHECK(scan(R"("\ude00\ud83d")").verdict == JsonVerdict::Malformed);
}

TEST_CASE("json scan: unterminated strings are Malformed at the end of the text") {
  CHECK_SCAN("\"abc", JsonLimits{}, JsonVerdict::Malformed, 4u);
  CHECK_SCAN("{\"abc", JsonLimits{}, JsonVerdict::Malformed, 5u);
}

TEST_CASE("json scan: structure errors are Malformed") {
  struct Row {
    const char* text;
    std::size_t offset;
  };
  const Row rows[] = {
      {"[1,]", 3},         {"[,1]", 1},        {"[1 2]", 3},       {"{\"a\":1,}", 7},   {"{,}", 1},
      {"{\"a\" 1}", 5},    {"{\"a\":}", 5},    {"{1:2}", 1},       {"{\"a\"}", 4},      {"{\"a\":1 \"b\":2}", 7},
      {"[}", 1},           {"{]", 1},          {"]", 0},           {"}", 0},            {"[1}", 2},
      {"{\"a\":[1}", 7},   {"[\"a\"", 4},      {"{\"a\":1", 6},    {"[", 1},            {"{", 1},
      {"tru", 0},          {"truex", 4},       {"nul", 0},         {"falsey", 5},       {"True", 0},
      {"[1,,2]", 3},       {":", 0},           {",", 0},           {"'a'", 0},          {"{'a':1}", 1},
      {"[1]]", 3},         {"[[1]", 4},        {"{\"a\":1}}", 7},  {"abc", 0},          {"[1,2", 4},
  };
  for (const Row& r : rows) {
    const JsonScanResult res = scan(r.text);
    INFO(r.text);
    CHECK(res.verdict == JsonVerdict::Malformed);
    CHECK(res.offset == r.offset);
  }
}

TEST_CASE("json scan: hostile input table is rejected without crashing") {
  JsonLimits l;
  l.max_depth = 16;
  std::vector<std::pair<std::string, JsonVerdict>> rows = {
      {std::string(100000, '['), JsonVerdict::TooDeep},
      {std::string(100000, '{'), JsonVerdict::Malformed},  // '{{' is not a valid key
      {std::string(100000, ']'), JsonVerdict::Malformed},
      {std::string(100000, '"'), JsonVerdict::Malformed},  // "" then a stray quote
      {"[" + std::string(100000, '1'), JsonVerdict::Malformed},
      {"\"" + std::string(100000, 'a'), JsonVerdict::StringTooLong},  // stops at the 64 KiB limit
      {"[\"" + std::string(100000, 'a') + "\"]", JsonVerdict::StringTooLong},  // default 64 KiB
      {R"({"a":1,"\u0061":2})", JsonVerdict::DuplicateKey},
      {R"({"\u0061\u0062":1,"ab":2})", JsonVerdict::DuplicateKey},
      {R"({"a":1,"a":2,"a":3})", JsonVerdict::DuplicateKey},
      {R"({"a":{"b":1,"b":2}})", JsonVerdict::DuplicateKey},
      {R"([{"a":1,"a":1}])", JsonVerdict::DuplicateKey},
      {"{\"a\":1", JsonVerdict::Malformed},
      {"{\"a\":", JsonVerdict::Malformed},
      {"{\"a", JsonVerdict::Malformed},
      {"[1,2,", JsonVerdict::Malformed},
      {"[1,2", JsonVerdict::Malformed},
      {"]", JsonVerdict::Malformed},
      {"[]]", JsonVerdict::Malformed},
      {"[}", JsonVerdict::Malformed},
      {"{]", JsonVerdict::Malformed},
      {"[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]", JsonVerdict::TooDeep},  // 17 levels
      {R"("\u00")", JsonVerdict::Malformed},
      {R"("\ud800")", JsonVerdict::Malformed},
      {R"("\q")", JsonVerdict::Malformed},
      {std::string("[\"\x01\"]"), JsonVerdict::Malformed},
      {std::string("{\"a\0\":1}", 8), JsonVerdict::Malformed},
      {"{\"a\":1}x", JsonVerdict::Malformed},
      {"{\"a\":1}{\"a\":1}", JsonVerdict::Malformed},
      {"[1,2,3,]", JsonVerdict::Malformed},
      {"nulll", JsonVerdict::Malformed},
      {bytes({0xEF, 0xBB, 0xBF, '[', ']'}), JsonVerdict::Malformed},
      {"\x1b[0m", JsonVerdict::Malformed},
      {"<html>", JsonVerdict::Malformed},
      {"//comment\n{}", JsonVerdict::Malformed},
      {"/* c */ {}", JsonVerdict::Malformed},
  };
  REQUIRE(rows.size() >= 30);
  for (const auto& [text, expect] : rows) {
    const JsonScanResult r = scan(text, l);
    INFO("size " << text.size() << " expected " << to_string(expect) << " got " << to_string(r.verdict));
    CHECK(r.verdict == expect);
    CHECK(r.offset <= text.size());
  }
}

// ---- randomized, fixed seed ----

namespace {

// Independent reference: do the brackets outside strings match, and is every string terminated?
bool reference_balanced(std::string_view s) {
  std::vector<char> stack;
  bool in_string = false;
  for (std::size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (in_string) {
      if (c == '\\') ++i;
      else if (c == '"') in_string = false;
      continue;
    }
    if (c == '"') in_string = true;
    else if (c == '[' || c == '{') stack.push_back(c);
    else if (c == ']' || c == '}') {
      if (stack.empty() || stack.back() != (c == ']' ? '[' : '{')) return false;
      stack.pop_back();
    }
  }
  return !in_string && stack.empty();
}

}  // namespace

TEST_CASE("json scan: random token soup never crashes and is never Ok when brackets are unbalanced") {
  std::mt19937_64 rng(0x4A534F4Eull);  // fixed seed
  static const char alphabet[] = {'[', ']', '{', '}', '"', ',', ':', '0', '1', 'a', ' ', '\\', 'n', 'u', '-', 't'};
  JsonLimits l;
  l.max_depth = 64;
  std::size_t unbalanced = 0;
  for (int iter = 0; iter < 30000; ++iter) {
    const std::size_t len = static_cast<std::size_t>(rng() % 30);
    std::string s;
    for (std::size_t k = 0; k < len; ++k) s.push_back(alphabet[rng() % sizeof(alphabet)]);
    const JsonScanResult r = scan(s, l);
    REQUIRE(r.offset <= s.size());
    if (!reference_balanced(s)) {
      ++unbalanced;
      REQUIRE(r.verdict != JsonVerdict::Ok);
    }
  }
  CHECK(unbalanced > 1000);
}

TEST_CASE("json scan: deleting one character from a valid document is never Ok when it unbalances it") {
  const std::string valid = R"({"a":[1,{"b":[true,null,"x\"y"]}],"c":{"d":"e"},"f":[[],{}]})";
  REQUIRE(scan(valid).verdict == JsonVerdict::Ok);
  std::size_t checked = 0;
  for (std::size_t i = 0; i < valid.size(); ++i) {
    std::string cut = valid;
    cut.erase(i, 1);
    const JsonScanResult r = scan(cut);
    REQUIRE(r.offset <= cut.size());
    if (!reference_balanced(cut)) {
      ++checked;
      REQUIRE(r.verdict != JsonVerdict::Ok);
    }
  }
  CHECK(checked > 10);
}

TEST_CASE("json scan: random mutation of valid documents never crashes") {
  std::mt19937_64 rng(0x4D55544Eull);
  const std::string valid = R"({"a":[1,{"b":[true,null,"x\u00e9"]}],"c":{"d":"e"}})";
  for (int iter = 0; iter < 20000; ++iter) {
    std::string s = valid;
    const int edits = 1 + static_cast<int>(rng() % 3);
    for (int e = 0; e < edits; ++e) {
      const std::size_t pos = static_cast<std::size_t>(rng() % s.size());
      switch (rng() % 3) {
        case 0: s.erase(pos, 1); break;
        case 1: s.insert(pos, 1, static_cast<char>(rng() % 256)); break;
        default: s[pos] = static_cast<char>(rng() % 256); break;
      }
      if (s.empty()) s = "x";
    }
    const JsonScanResult r = scan(s);
    REQUIRE(r.offset <= s.size());
  }
}

// ---- to_string ----

TEST_CASE("json scan: to_string gives a distinct name per verdict") {
  const JsonVerdict all[] = {JsonVerdict::Ok,           JsonVerdict::TooLarge,          JsonVerdict::TooDeep,
                             JsonVerdict::StringTooLong, JsonVerdict::TooManyKeys,       JsonVerdict::TooManyArrayItems,
                             JsonVerdict::TooManyValues, JsonVerdict::DuplicateKey,      JsonVerdict::Malformed};
  for (std::size_t i = 0; i < std::size(all); ++i) {
    CHECK(std::string_view(to_string(all[i])).size() > 0);
    for (std::size_t j = i + 1; j < std::size(all); ++j)
      CHECK(std::string_view(to_string(all[i])) != std::string_view(to_string(all[j])));
  }
  CHECK(std::string_view(to_string(JsonVerdict::DuplicateKey)) == "DuplicateKey");
}
