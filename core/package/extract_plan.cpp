// Extraction plan for an audited package. Pure function on names and sizes: no file system, no
// I/O. The check order and the output order are in extract_plan.h; docs/extract-plan.md lists the
// exact bounds. The folding code below is a private copy of the entry policy's folds (they are
// private to entry_policy.cpp); tests cross-check the copy against validate_entries.
#include "extract_plan.h"

#include <algorithm>
#include <limits>
#include <string_view>
#include <unordered_set>

namespace lumora::package {

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();

bool is_ascii_letter(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

char ascii_lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

// Letter + combining mark -> precomposed Latin-1 Supplement letter (same table as entry_policy.cpp).
struct Composition {
  char base;
  char32_t mark;
  char32_t composed;
};

constexpr Composition kCompositions[] = {
    {'A', 0x300, 0xC0}, {'A', 0x301, 0xC1}, {'A', 0x302, 0xC2}, {'A', 0x303, 0xC3},
    {'A', 0x308, 0xC4}, {'A', 0x30A, 0xC5}, {'C', 0x327, 0xC7}, {'E', 0x300, 0xC8},
    {'E', 0x301, 0xC9}, {'E', 0x302, 0xCA}, {'E', 0x308, 0xCB}, {'I', 0x300, 0xCC},
    {'I', 0x301, 0xCD}, {'I', 0x302, 0xCE}, {'I', 0x308, 0xCF}, {'N', 0x303, 0xD1},
    {'O', 0x300, 0xD2}, {'O', 0x301, 0xD3}, {'O', 0x302, 0xD4}, {'O', 0x303, 0xD5},
    {'O', 0x308, 0xD6}, {'U', 0x300, 0xD9}, {'U', 0x301, 0xDA}, {'U', 0x302, 0xDB},
    {'U', 0x308, 0xDC}, {'Y', 0x301, 0xDD}, {'a', 0x300, 0xE0}, {'a', 0x301, 0xE1},
    {'a', 0x302, 0xE2}, {'a', 0x303, 0xE3}, {'a', 0x308, 0xE4}, {'a', 0x30A, 0xE5},
    {'c', 0x327, 0xE7}, {'e', 0x300, 0xE8}, {'e', 0x301, 0xE9}, {'e', 0x302, 0xEA},
    {'e', 0x308, 0xEB}, {'i', 0x300, 0xEC}, {'i', 0x301, 0xED}, {'i', 0x302, 0xEE},
    {'i', 0x308, 0xEF}, {'n', 0x303, 0xF1}, {'o', 0x300, 0xF2}, {'o', 0x301, 0xF3},
    {'o', 0x302, 0xF4}, {'o', 0x303, 0xF5}, {'o', 0x308, 0xF6}, {'u', 0x300, 0xF9},
    {'u', 0x301, 0xFA}, {'u', 0x302, 0xFB}, {'u', 0x308, 0xFC}, {'y', 0x301, 0xFD},
    {'y', 0x308, 0xFF},
};

void append_utf8(std::string& out, char32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else {  // the supported range is below U+0800
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// True when s[i..] starts with one of the supported combining marks (two bytes in UTF-8).
bool supported_mark_at(std::string_view s, std::size_t i, char32_t& mark) {
  if (i + 1 >= s.size()) return false;
  const auto b0 = static_cast<unsigned char>(s[i]);
  const auto b1 = static_cast<unsigned char>(s[i + 1]);
  if ((b0 & 0xFE) != 0xCC) return false;
  const char32_t cp = (static_cast<char32_t>(b0 & 0x1F) << 6) | (b1 & 0x3F);
  switch (cp) {
    case 0x300: case 0x301: case 0x302: case 0x303: case 0x308: case 0x30A: case 0x327:
      mark = cp;
      return true;
    default:
      return false;
  }
}

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

std::uint64_t depth_of(std::string_view path) {
  std::uint64_t depth = 1;
  for (char c : path) {
    if (c == '/') ++depth;
  }
  return depth;
}

ExtractPlan fail(ExtractVerdict v, std::size_t index) {
  ExtractPlan p;
  p.verdict = v;
  p.entry_index = index;
  return p;
}

}  // namespace

ExtractPlan plan_extraction(const std::vector<EntryInfo>& entries, std::uint64_t free_bytes,
                            const ExtractLimits& limits) {
  // 1. Per entry: path length, then depth.
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const std::string& name = entries[i].name;
    if (static_cast<std::uint64_t>(name.size()) > limits.max_path_bytes) {
      return fail(ExtractVerdict::PathTooLong, i);
    }
    if (depth_of(name) > limits.max_depth) return fail(ExtractVerdict::PathTooDeep, i);
  }

  // Directories: every non-empty proper prefix that ends at a '/', each once, by exact bytes.
  std::unordered_set<std::string> dir_set;
  std::vector<std::string> dir_list;
  std::vector<std::size_t> dir_owner;  // entry that first introduced each directory
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const std::string& name = entries[i].name;
    for (std::size_t pos = name.find('/'); pos != std::string::npos; pos = name.find('/', pos + 1)) {
      if (pos == 0) continue;
      std::string dir = name.substr(0, pos);
      if (dir_set.insert(dir).second) {
        dir_list.push_back(std::move(dir));
        dir_owner.push_back(i);
      }
    }
  }

  // 2. A file name that is also a parent directory, exact or under one fold alone.
  {
    std::unordered_set<std::string> ascii_dirs;
    std::unordered_set<std::string> norm_dirs;
    for (const std::string& d : dir_list) {
      ascii_dirs.insert(ascii_fold_key(d));
      norm_dirs.insert(normalization_key(d));
    }
    for (std::size_t i = 0; i < entries.size(); ++i) {
      const std::string& name = entries[i].name;
      if (dir_set.count(name) != 0 || ascii_dirs.count(ascii_fold_key(name)) != 0 ||
          norm_dirs.count(normalization_key(name)) != 0) {
        return fail(ExtractVerdict::FileDirectoryConflict, i);
      }
    }
  }

  // 3. Directory count: the entry that introduces the first directory above the limit.
  if (static_cast<std::uint64_t>(dir_list.size()) > limits.max_directories) {
    return fail(ExtractVerdict::TooManyDirectories,
                dir_owner[static_cast<std::size_t>(limits.max_directories)]);
  }

  // 4. Sizes, overflow safe.
  std::uint64_t total = 0;
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].uncompressed_bytes > kMax - total) {
      return fail(ExtractVerdict::SizeOverflow, i);
    }
    total += entries[i].uncompressed_bytes;
  }
  if (total > kMax - limits.reserve_bytes) return fail(ExtractVerdict::SizeOverflow, 0);
  const std::uint64_t required = total + limits.reserve_bytes;

  // 5. Space.
  if (free_bytes < required) {
    ExtractPlan p = fail(ExtractVerdict::InsufficientSpace, 0);
    p.total_bytes = total;
    p.required_free_bytes = required;
    return p;
  }

  ExtractPlan plan;
  plan.total_bytes = total;
  plan.required_free_bytes = required;
  std::stable_sort(dir_list.begin(), dir_list.end(), [](const std::string& a, const std::string& b) {
    const std::uint64_t da = depth_of(a);
    const std::uint64_t db = depth_of(b);
    if (da != db) return da < db;
    return a < b;  // std::string compares bytes as unsigned char
  });
  plan.directories = std::move(dir_list);
  plan.file_order.reserve(entries.size());
  for (std::size_t i = 0; i < entries.size(); ++i) plan.file_order.push_back(i);
  return plan;
}

std::vector<std::string> cleanup_paths(const ExtractPlan& plan, const std::vector<EntryInfo>& entries,
                                       std::size_t completed_files, bool partial_file_exists) {
  std::vector<std::string> out;
  if (plan.verdict != ExtractVerdict::Ok) return out;
  const std::size_t count = plan.file_order.size();
  const std::size_t done = std::min(completed_files, count);
  if (partial_file_exists && done < count) {
    const std::size_t idx = plan.file_order[done];
    if (idx < entries.size()) out.push_back(entries[idx].name);
  }
  for (std::size_t k = done; k > 0; --k) {
    const std::size_t idx = plan.file_order[k - 1];
    if (idx < entries.size()) out.push_back(entries[idx].name);
  }
  for (std::size_t k = plan.directories.size(); k > 0; --k) out.push_back(plan.directories[k - 1]);
  return out;
}

const char* to_string(ExtractVerdict v) {
  switch (v) {
    case ExtractVerdict::Ok: return "Ok";
    case ExtractVerdict::TooManyDirectories: return "TooManyDirectories";
    case ExtractVerdict::PathTooDeep: return "PathTooDeep";
    case ExtractVerdict::PathTooLong: return "PathTooLong";
    case ExtractVerdict::FileDirectoryConflict: return "FileDirectoryConflict";
    case ExtractVerdict::SizeOverflow: return "SizeOverflow";
    case ExtractVerdict::InsufficientSpace: return "InsufficientSpace";
  }
  return "Unknown";
}

}  // namespace lumora::package
