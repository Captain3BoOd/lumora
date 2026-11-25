// Archive entry policy for .lumora packages (docs/security-model.md, "Archive defenses").
// Pure functions on names and sizes. See docs/package-policy.md for the exact rules, the order in
// which they apply, and the cases the policy does not cover.
#include "entry_policy.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace lumora::package {

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();

bool is_ascii_letter(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

char ascii_lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

// Strict UTF-8: no overlong forms, no surrogates, nothing above U+10FFFF.
bool is_valid_utf8(std::string_view s) {
  std::size_t i = 0;
  const std::size_t n = s.size();
  auto byte = [&](std::size_t k) { return static_cast<unsigned char>(s[k]); };
  auto cont = [&](std::size_t k) { return k < n && (byte(k) & 0xC0) == 0x80; };
  while (i < n) {
    const unsigned char c = byte(i);
    if (c < 0x80) {
      ++i;
    } else if (c >= 0xC2 && c <= 0xDF) {
      if (!cont(i + 1)) return false;
      i += 2;
    } else if (c >= 0xE0 && c <= 0xEF) {
      if (!cont(i + 1) || !cont(i + 2)) return false;
      const unsigned char c1 = byte(i + 1);
      if (c == 0xE0 && c1 < 0xA0) return false;  // overlong
      if (c == 0xED && c1 > 0x9F) return false;  // surrogates
      i += 3;
    } else if (c >= 0xF0 && c <= 0xF4) {
      if (!cont(i + 1) || !cont(i + 2) || !cont(i + 3)) return false;
      const unsigned char c1 = byte(i + 1);
      if (c == 0xF0 && c1 < 0x90) return false;  // overlong
      if (c == 0xF4 && c1 > 0x8F) return false;  // above U+10FFFF
      i += 4;
    } else {
      return false;  // continuation byte alone, C0, C1, F5..FF
    }
  }
  return true;
}

bool is_reserved_stem(std::string_view stem) {
  // Windows ignores trailing spaces in the base name, so "CON .txt" is still CON.
  while (!stem.empty() && stem.back() == ' ') stem.remove_suffix(1);
  if (stem.size() == 3) {
    char u[3];
    for (int k = 0; k < 3; ++k) {
      char c = stem[static_cast<std::size_t>(k)];
      u[k] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
    const std::string_view v(u, 3);
    return v == "CON" || v == "PRN" || v == "AUX" || v == "NUL";
  }
  if (stem.size() == 4) {
    char u[3];
    for (int k = 0; k < 3; ++k) {
      char c = stem[static_cast<std::size_t>(k)];
      u[k] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
    const std::string_view v(u, 3);
    const char d = stem[3];
    return (v == "COM" || v == "LPT") && d >= '1' && d <= '9';
  }
  return false;
}

// Letter + combining mark -> precomposed Latin-1 Supplement letter (canonical NFC for these).
struct Composition {
  char base;
  char32_t mark;
  char32_t composed;
};

constexpr Composition kCompositions[] = {
    {'A', 0x300, 0xC0}, {'A', 0x301, 0xC1}, {'A', 0x302, 0xC2}, {'A', 0x303, 0xC3}, {'A', 0x308, 0xC4},
    {'A', 0x30A, 0xC5}, {'C', 0x327, 0xC7}, {'E', 0x300, 0xC8}, {'E', 0x301, 0xC9}, {'E', 0x302, 0xCA},
    {'E', 0x308, 0xCB}, {'I', 0x300, 0xCC}, {'I', 0x301, 0xCD}, {'I', 0x302, 0xCE}, {'I', 0x308, 0xCF},
    {'N', 0x303, 0xD1}, {'O', 0x300, 0xD2}, {'O', 0x301, 0xD3}, {'O', 0x302, 0xD4}, {'O', 0x303, 0xD5},
    {'O', 0x308, 0xD6}, {'U', 0x300, 0xD9}, {'U', 0x301, 0xDA}, {'U', 0x302, 0xDB}, {'U', 0x308, 0xDC},
    {'Y', 0x301, 0xDD}, {'a', 0x300, 0xE0}, {'a', 0x301, 0xE1}, {'a', 0x302, 0xE2}, {'a', 0x303, 0xE3},
    {'a', 0x308, 0xE4}, {'a', 0x30A, 0xE5}, {'c', 0x327, 0xE7}, {'e', 0x300, 0xE8}, {'e', 0x301, 0xE9},
    {'e', 0x302, 0xEA}, {'e', 0x308, 0xEB}, {'i', 0x300, 0xEC}, {'i', 0x301, 0xED}, {'i', 0x302, 0xEE},
    {'i', 0x308, 0xEF}, {'n', 0x303, 0xF1}, {'o', 0x300, 0xF2}, {'o', 0x301, 0xF3}, {'o', 0x302, 0xF4},
    {'o', 0x303, 0xF5}, {'o', 0x308, 0xF6}, {'u', 0x300, 0xF9}, {'u', 0x301, 0xFA}, {'u', 0x302, 0xFB},
    {'u', 0x308, 0xFC}, {'y', 0x301, 0xFD}, {'y', 0x308, 0xFF},
};

// Appends cp as UTF-8. Only used for the precomposed letters above, all below U+0800.
void append_utf8(std::string& out, char32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else {  // the supported range is below U+0800
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// True when s[i..] starts with one of the supported combining marks (all are two bytes in UTF-8).
bool supported_mark_at(std::string_view s, std::size_t i, char32_t& mark) {
  if (i + 1 >= s.size()) return false;
  const auto b0 = static_cast<unsigned char>(s[i]);
  const auto b1 = static_cast<unsigned char>(s[i + 1]);
  if ((b0 & 0xFE) != 0xCC) return false;  // U+0300..U+037F are CC 80..CD BF
  const char32_t cp = (static_cast<char32_t>(b0 & 0x1F) << 6) | (b1 & 0x3F);
  switch (cp) {
    case 0x300: case 0x301: case 0x302: case 0x303: case 0x308: case 0x30A: case 0x327:
      mark = cp;
      return true;
    default:
      return false;
  }
}

// Folds the supported decomposed pairs to their precomposed form. Input is valid UTF-8.
std::string normalization_key(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  std::size_t i = 0;
  while (i < s.size()) {
    const auto c = static_cast<unsigned char>(s[i]);
    char32_t mark = 0;
    if (is_ascii_letter(c) && supported_mark_at(s, i + 1, mark)) {
      bool found = false;
      for (const Composition& k : kCompositions) {
        if (k.base == s[i] && k.mark == mark) {
          append_utf8(out, k.composed);
          i += 3;
          found = true;
          break;
        }
      }
      if (found) continue;
    }
    out.push_back(s[i]);
    ++i;
  }
  return out;
}

std::string ascii_fold_key(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = ascii_lower(c);
  return out;
}

}  // namespace

NameVerdict validate_name(std::string_view name, const Limits& limits) {
  if (name.empty()) return NameVerdict::Empty;
  if (static_cast<std::uint64_t>(name.size()) > limits.max_name_bytes) return NameVerdict::TooLong;
  if (!is_valid_utf8(name)) return NameVerdict::InvalidUtf8;
  for (char ch : name) {
    const auto c = static_cast<unsigned char>(ch);
    if (c < 0x20 || c == 0x7F) return NameVerdict::ControlCharacter;
  }
  if (name.find('\\') != std::string_view::npos) return NameVerdict::Backslash;
  if (name.front() == '/') return NameVerdict::Absolute;
  if (name.size() >= 2 && is_ascii_letter(static_cast<unsigned char>(name[0])) && name[1] == ':')
    return NameVerdict::Absolute;

  // Split on '/'. Each pass below walks the components in order.
  auto for_each_component = [&](auto&& fn) {
    std::size_t start = 0;
    while (true) {
      const std::size_t slash = name.find('/', start);
      const std::string_view comp =
          name.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
      if (!fn(comp)) return false;
      if (slash == std::string_view::npos) return true;
      start = slash + 1;
    }
  };
  if (!for_each_component([](std::string_view c) { return !c.empty(); })) return NameVerdict::EmptyComponent;
  if (!for_each_component([](std::string_view c) { return c != ".."; })) return NameVerdict::Traversal;
  if (!for_each_component([](std::string_view c) { return c.back() != '.' && c.back() != ' '; }))
    return NameVerdict::TrailingDotOrSpace;
  if (!for_each_component([](std::string_view c) {
        const std::size_t dot = c.find('.');
        return !is_reserved_stem(dot == std::string_view::npos ? c : c.substr(0, dot));
      }))
    return NameVerdict::WindowsReserved;
  return NameVerdict::Ok;
}

const char* to_string(NameVerdict v) {
  switch (v) {
    case NameVerdict::Ok: return "Ok";
    case NameVerdict::Empty: return "Empty";
    case NameVerdict::Absolute: return "Absolute";
    case NameVerdict::Traversal: return "Traversal";
    case NameVerdict::Backslash: return "Backslash";
    case NameVerdict::ControlCharacter: return "ControlCharacter";
    case NameVerdict::TrailingDotOrSpace: return "TrailingDotOrSpace";
    case NameVerdict::WindowsReserved: return "WindowsReserved";
    case NameVerdict::EmptyComponent: return "EmptyComponent";
    case NameVerdict::InvalidUtf8: return "InvalidUtf8";
    case NameVerdict::TooLong: return "TooLong";
  }
  return "Unknown";
}

const char* to_string(SetVerdict v) {
  switch (v) {
    case SetVerdict::Ok: return "Ok";
    case SetVerdict::BadName: return "BadName";
    case SetVerdict::TooManyEntries: return "TooManyEntries";
    case SetVerdict::EntryTooLarge: return "EntryTooLarge";
    case SetVerdict::TotalTooLarge: return "TotalTooLarge";
    case SetVerdict::RatioTooHigh: return "RatioTooHigh";
    case SetVerdict::SizeOverflow: return "SizeOverflow";
    case SetVerdict::DuplicateName: return "DuplicateName";
    case SetVerdict::CaseFoldCollision: return "CaseFoldCollision";
    case SetVerdict::NormalizationCollision: return "NormalizationCollision";
  }
  return "Unknown";
}

SetResult validate_entries(const std::vector<EntryInfo>& entries, const Limits& limits) {
  SetResult r;
  auto fail = [&](SetVerdict v, std::size_t index) {
    r.verdict = v;
    r.first_index = index;
    return r;
  };

  if (static_cast<std::uint64_t>(entries.size()) > limits.max_entries)
    return fail(SetVerdict::TooManyEntries, static_cast<std::size_t>(limits.max_entries));

  std::uint64_t total = 0;
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const EntryInfo& e = entries[i];
    if (const NameVerdict nv = validate_name(e.name, limits); nv != NameVerdict::Ok) {
      r.name_verdict = nv;
      return fail(SetVerdict::BadName, i);
    }
    if (e.uncompressed_bytes > limits.max_entry_bytes) return fail(SetVerdict::EntryTooLarge, i);

    // Ratio: uncompressed / compressed must not exceed max_compression_ratio. Compare as
    // uncompressed <= ratio * compressed without overflowing the product.
    if (e.compressed_bytes == 0) {
      if (e.uncompressed_bytes > 0) return fail(SetVerdict::RatioTooHigh, i);
    } else {
      const bool product_fits =
          limits.max_compression_ratio == 0 || e.compressed_bytes <= kMax / limits.max_compression_ratio;
      if (product_fits && e.uncompressed_bytes > limits.max_compression_ratio * e.compressed_bytes)
        return fail(SetVerdict::RatioTooHigh, i);
    }

    if (e.uncompressed_bytes > kMax - total) return fail(SetVerdict::SizeOverflow, i);
    total += e.uncompressed_bytes;
    if (total > limits.max_total_uncompressed_bytes) return fail(SetVerdict::TotalTooLarge, i);
  }

  // Collisions. Each name is checked against the earlier ones: exact duplicate first, then ASCII
  // case folding, then the Latin-1 normalization fold. other_index is the earliest match.
  std::unordered_map<std::string, std::size_t> exact, folded, normalized;
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const std::string& name = entries[i].name;
    const std::string fold = ascii_fold_key(name);
    const std::string norm = normalization_key(name);
    if (auto it = exact.find(name); it != exact.end()) {
      r.other_index = it->second;
      return fail(SetVerdict::DuplicateName, i);
    }
    if (auto it = folded.find(fold); it != folded.end()) {
      r.other_index = it->second;
      return fail(SetVerdict::CaseFoldCollision, i);
    }
    if (auto it = normalized.find(norm); it != normalized.end()) {
      r.other_index = it->second;
      return fail(SetVerdict::NormalizationCollision, i);
    }
    exact.emplace(name, i);
    folded.emplace(fold, i);
    normalized.emplace(norm, i);
  }
  return r;
}

}  // namespace lumora::package
