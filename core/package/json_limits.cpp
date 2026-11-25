// Structural limits for untrusted JSON (see json_limits.h and docs/json-limits.md).
// One pass over the text, an explicit stack, no recursion, no document is built.
#include "json_limits.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lumora::package {

namespace {

constexpr std::uint64_t kNoLimit = std::numeric_limits<std::uint64_t>::max();

struct StrResult {
  JsonVerdict verdict = JsonVerdict::Ok;
  std::size_t offset = 0;  // problem offset when verdict is not Ok
  std::size_t end = 0;     // index just past the closing quote when Ok
};

int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Reads four hex digits at t[at..at+3]. Returns -1 when one is not a hex digit.
long read_hex4(std::string_view t, std::size_t at) {
  long v = 0;
  for (std::size_t k = 0; k < 4; ++k) {
    const int h = hex_value(t[at + k]);
    if (h < 0) return -1;
    v = v * 16 + h;
  }
  return v;
}

// Scans the string whose opening quote is t[q]. Calls emit(byte) for every decoded byte. Stops with
// StringTooLong as soon as the decoded length would pass max_bytes.
template <class Emit>
StrResult read_string(std::string_view t, std::size_t q, std::uint64_t max_bytes, Emit&& emit) {
  const std::size_t n = t.size();
  std::size_t i = q + 1;
  std::uint64_t decoded = 0;
  auto bad = [](JsonVerdict v, std::size_t off) { return StrResult{v, off, 0}; };
  while (true) {
    if (i >= n) return bad(JsonVerdict::Malformed, n);  // unterminated
    const unsigned char c = static_cast<unsigned char>(t[i]);
    if (c == '"') return StrResult{JsonVerdict::Ok, 0, i + 1};
    if (c < 0x20) return bad(JsonVerdict::Malformed, i);  // raw control character
    if (c != '\\') {
      if (decoded + 1 > max_bytes) return bad(JsonVerdict::StringTooLong, i);
      emit(c);
      ++decoded;
      ++i;
      continue;
    }
    // Escape sequence starting at i.
    if (i + 1 >= n) return bad(JsonVerdict::Malformed, n);
    const char e = t[i + 1];
    unsigned char single = 0;
    switch (e) {
      case '"': single = '"'; break;
      case '\\': single = '\\'; break;
      case '/': single = '/'; break;
      case 'b': single = '\b'; break;
      case 'f': single = '\f'; break;
      case 'n': single = '\n'; break;
      case 'r': single = '\r'; break;
      case 't': single = '\t'; break;
      default: break;
    }
    if (single != 0) {
      if (decoded + 1 > max_bytes) return bad(JsonVerdict::StringTooLong, i);
      emit(single);
      ++decoded;
      i += 2;
      continue;
    }
    if (e != 'u') return bad(JsonVerdict::Malformed, i);  // unknown escape
    if (i + 6 > n) return bad(JsonVerdict::Malformed, i);  // truncated \u
    long cp = read_hex4(t, i + 2);
    if (cp < 0) return bad(JsonVerdict::Malformed, i);
    std::size_t consumed = 6;
    if (cp >= 0xDC00 && cp <= 0xDFFF) return bad(JsonVerdict::Malformed, i);  // lone low surrogate
    if (cp >= 0xD800 && cp <= 0xDBFF) {
      if (i + 12 > n || t[i + 6] != '\\' || t[i + 7] != 'u') return bad(JsonVerdict::Malformed, i);
      const long low = read_hex4(t, i + 8);
      if (low < 0xDC00 || low > 0xDFFF) return bad(JsonVerdict::Malformed, i);  // lone high surrogate
      cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
      consumed = 12;
    }
    unsigned char out[4];
    std::size_t len = 0;
    if (cp < 0x80) {
      out[len++] = static_cast<unsigned char>(cp);
    } else if (cp < 0x800) {
      out[len++] = static_cast<unsigned char>(0xC0 | (cp >> 6));
      out[len++] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out[len++] = static_cast<unsigned char>(0xE0 | (cp >> 12));
      out[len++] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F));
      out[len++] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
    } else {
      out[len++] = static_cast<unsigned char>(0xF0 | (cp >> 18));
      out[len++] = static_cast<unsigned char>(0x80 | ((cp >> 12) & 0x3F));
      out[len++] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F));
      out[len++] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
    }
    if (decoded + len > max_bytes) return bad(JsonVerdict::StringTooLong, i);
    for (std::size_t k = 0; k < len; ++k) emit(out[k]);
    decoded += len;
    i += consumed;
  }
}

bool is_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
bool is_number_char(char c) {
  return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E';
}

// Per-process random seed for the key hash, so a hostile text cannot be built to collide offline.
std::uint64_t hash_seed() {
  static const std::uint64_t seed = [] {
    std::random_device rd;
    return (static_cast<std::uint64_t>(rd()) << 32) ^ rd();
  }();
  return seed;
}

std::uint64_t hash_step(std::uint64_t h, unsigned char b) {
  h ^= b;
  h *= 0x9E3779B97F4A7C15ull;
  h ^= h >> 32;
  return h;
}

struct KeyRef {
  std::size_t quote;     // offset of the opening quote in the text
  std::uint64_t length;  // decoded length in bytes
};

enum class State {
  TopValue,   // waiting for the single top-level value
  TopDone,    // the value is complete, only whitespace may follow
  ObjFirst,   // after '{': key or '}'
  ObjKey,     // after ',': key
  ObjColon,
  ObjValue,
  ObjAfter,   // after a member value: ',' or '}'
  ArrFirst,   // after '[': value or ']'
  ArrValue,   // after ',': value
  ArrAfter,   // after an item: ',' or ']'
};

struct Frame {
  bool is_object = false;
  std::uint64_t count = 0;  // keys in an object, items in an array
  // Hash of each decoded key -> where it is. Only objects have one. Duplicates are confirmed by
  // decoding both keys again, so no key text is copied.
  std::unique_ptr<std::unordered_multimap<std::uint64_t, KeyRef>> keys;
};

}  // namespace

JsonScanResult scan_json(std::string_view text, const JsonLimits& limits) {
  auto fail = [](JsonVerdict v, std::size_t off) { return JsonScanResult{v, off}; };
  const std::size_t n = text.size();
  if (static_cast<std::uint64_t>(n) > limits.max_bytes) return fail(JsonVerdict::TooLarge, static_cast<std::size_t>(limits.max_bytes));

  std::vector<Frame> stack;
  State st = State::TopValue;
  std::uint64_t values = 0;
  std::size_t i = 0;

  auto after_value = [&] {
    if (stack.empty()) st = State::TopDone;
    else st = stack.back().is_object ? State::ObjAfter : State::ArrAfter;
  };

  while (true) {
    while (i < n && is_ws(text[i])) ++i;
    if (i >= n) break;
    const char c = text[i];

    switch (st) {
      case State::TopDone:
        return fail(JsonVerdict::Malformed, i);  // trailing text

      case State::ObjFirst:
      case State::ObjKey: {
        if (c == '}' && st == State::ObjFirst) {
          stack.pop_back();
          ++i;
          after_value();
          continue;
        }
        if (c != '"') return fail(JsonVerdict::Malformed, i);
        Frame& f = stack.back();
        if (++f.count > limits.max_keys_per_object) return fail(JsonVerdict::TooManyKeys, i);
        std::uint64_t hash = hash_seed();
        std::uint64_t length = 0;
        const StrResult r = read_string(text, i, limits.max_string_bytes, [&](unsigned char b) {
          hash = hash_step(hash, b);
          ++length;
        });
        if (r.verdict != JsonVerdict::Ok) return fail(r.verdict, r.offset);
        auto range = f.keys->equal_range(hash);
        for (auto it = range.first; it != range.second; ++it) {
          if (it->second.length != length) continue;
          std::string a, b;
          read_string(text, it->second.quote, kNoLimit, [&](unsigned char x) { a.push_back(static_cast<char>(x)); });
          read_string(text, i, kNoLimit, [&](unsigned char x) { b.push_back(static_cast<char>(x)); });
          if (a == b) return fail(JsonVerdict::DuplicateKey, i);
        }
        f.keys->emplace(hash, KeyRef{i, length});
        i = r.end;
        st = State::ObjColon;
        continue;
      }

      case State::ObjColon:
        if (c != ':') return fail(JsonVerdict::Malformed, i);
        ++i;
        st = State::ObjValue;
        continue;

      case State::ObjAfter:
        if (c == ',') {
          ++i;
          st = State::ObjKey;
        } else if (c == '}') {
          stack.pop_back();
          ++i;
          after_value();
        } else {
          return fail(JsonVerdict::Malformed, i);
        }
        continue;

      case State::ArrAfter:
        if (c == ',') {
          ++i;
          st = State::ArrValue;
        } else if (c == ']') {
          stack.pop_back();
          ++i;
          after_value();
        } else {
          return fail(JsonVerdict::Malformed, i);
        }
        continue;

      case State::ArrFirst:
        if (c == ']') {
          stack.pop_back();
          ++i;
          after_value();
          continue;
        }
        [[fallthrough]];
      case State::TopValue:
      case State::ObjValue:
      case State::ArrValue:
        break;  // a value starts here
    }

    // A value starts at i.
    const bool in_array = !stack.empty() && !stack.back().is_object;
    if (in_array && ++stack.back().count > limits.max_array_items) return fail(JsonVerdict::TooManyArrayItems, i);
    if (++values > limits.max_total_values) return fail(JsonVerdict::TooManyValues, i);

    if (c == '{' || c == '[') {
      if (static_cast<std::uint64_t>(stack.size()) + 1 > limits.max_depth) return fail(JsonVerdict::TooDeep, i);
      Frame f;
      f.is_object = (c == '{');
      if (f.is_object) f.keys = std::make_unique<std::unordered_multimap<std::uint64_t, KeyRef>>();
      stack.push_back(std::move(f));
      st = (c == '{') ? State::ObjFirst : State::ArrFirst;
      ++i;
    } else if (c == '"') {
      const StrResult r = read_string(text, i, limits.max_string_bytes, [](unsigned char) {});
      if (r.verdict != JsonVerdict::Ok) return fail(r.verdict, r.offset);
      i = r.end;
      after_value();
    } else if (c == 't' || c == 'f' || c == 'n') {
      const std::string_view lit = (c == 't') ? "true" : (c == 'f') ? "false" : "null";
      if (text.substr(i, lit.size()) != lit) return fail(JsonVerdict::Malformed, i);
      i += lit.size();
      after_value();
    } else if (c == '-' || (c >= '0' && c <= '9')) {
      while (i < n && is_number_char(text[i])) ++i;  // numbers are checked loosely
      after_value();
    } else {
      return fail(JsonVerdict::Malformed, i);
    }
  }

  // End of text. Only a complete top-level value is acceptable.
  if (st != State::TopDone) return fail(JsonVerdict::Malformed, n);
  return {};
}

const char* to_string(JsonVerdict v) {
  switch (v) {
    case JsonVerdict::Ok: return "Ok";
    case JsonVerdict::TooLarge: return "TooLarge";
    case JsonVerdict::TooDeep: return "TooDeep";
    case JsonVerdict::StringTooLong: return "StringTooLong";
    case JsonVerdict::TooManyKeys: return "TooManyKeys";
    case JsonVerdict::TooManyArrayItems: return "TooManyArrayItems";
    case JsonVerdict::TooManyValues: return "TooManyValues";
    case JsonVerdict::DuplicateKey: return "DuplicateKey";
    case JsonVerdict::Malformed: return "Malformed";
  }
  return "Unknown";
}

}  // namespace lumora::package
