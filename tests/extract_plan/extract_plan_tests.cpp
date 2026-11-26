// Tests for the extraction plan (core/package/extract_plan.h). Pure logic on names and sizes: no
// file system, no I/O. One rule per test case, with every limit at 0, equal, plus one and the
// integer maximum. Three independent checks sit beside the hand-written cases: constants made by
// tools/extract_plan_ref.py (a second implementation in Python, folds taken from unicodedata), a
// differential test against a naive oracle in this file that shares no code with the plan, and the
// source mutation run described in docs/extract-plan.md. See that page for the rules and order.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "entry_policy.h"
#include "extract_plan.h"

using namespace lumora::package;

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint32_t kMax32 = std::numeric_limits<std::uint32_t>::max();

// BEGIN GENERATED REFERENCE (tools/extract_plan_ref.py)
struct RefEntry {
  const char* name;
  std::uint64_t size;
};
struct RefCleanup {
  std::size_t completed;
  bool partial;
  std::vector<const char*> paths;
};
struct RefCase {
  const char* label;
  std::vector<RefEntry> entries;
  std::uint64_t free_bytes;
  std::uint32_t max_depth;
  std::uint64_t max_path_bytes;
  std::uint64_t max_directories;
  std::uint64_t reserve_bytes;
  const char* verdict;
  std::size_t entry_index;
  std::vector<const char*> directories;
  std::uint64_t total_bytes;
  std::uint64_t required_free_bytes;
  std::vector<RefCleanup> cleanups;
};
const RefCase kRefCases[] = {
    {"empty",
     {}, 100u,
     16u, 1024u, 4096u, 67108864u,
     "InsufficientSpace", 0u, {}, 0u, 67108864u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"empty_exact_space",
     {}, 67108864u,
     16u, 1024u, 4096u, 67108864u,
     "Ok", 0u, {}, 0u, 67108864u,
     {{0u, false, {}}, {0u, true, {}}, {1u, false, {}}, {1u, true, {}}, {2u, true, {}}, {3u, true, {}}, {3u, false, {}}, {9u, true, {}}}},
    {"empty_short_space",
     {}, 67108863u,
     16u, 1024u, 4096u, 67108864u,
     "InsufficientSpace", 0u, {}, 0u, 67108864u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"one_top_level_file",
     {{"a.txt", 10u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "Ok", 0u, {}, 10u, 67108874u,
     {{0u, false, {}}, {0u, true, {"a.txt"}}, {1u, false, {"a.txt"}}, {1u, true, {"a.txt"}}, {2u, true, {"a.txt"}}, {3u, true, {"a.txt"}}, {3u, false, {"a.txt"}}, {9u, true, {"a.txt"}}}},
    {"nested_dirs_depth_order",
     {{"b/x", 1u}, {"a/c/d.txt", 2u}, {"a/y", 3u}, {"z", 4u}, {"a/c/e", 5u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "Ok", 0u, {"a", "b", "a/c"}, 15u, 67108879u,
     {{0u, false, {"a/c", "b", "a"}}, {0u, true, {"b/x", "a/c", "b", "a"}}, {1u, false, {"b/x", "a/c", "b", "a"}}, {1u, true, {"a/c/d.txt", "b/x", "a/c", "b", "a"}}, {2u, true, {"a/y", "a/c/d.txt", "b/x", "a/c", "b", "a"}}, {3u, true, {"z", "a/y", "a/c/d.txt", "b/x", "a/c", "b", "a"}}, {3u, false, {"a/y", "a/c/d.txt", "b/x", "a/c", "b", "a"}}, {9u, true, {"a/c/e", "z", "a/y", "a/c/d.txt", "b/x", "a/c", "b", "a"}}}},
    {"byte_order_within_depth",
     {{"B/x", 1u}, {"a/x", 1u}, {"_/x", 1u}, {"\303\251/x", 1u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "Ok", 0u, {"B", "_", "a", "\303\251"}, 4u, 67108868u,
     {{0u, false, {"\303\251", "a", "_", "B"}}, {0u, true, {"B/x", "\303\251", "a", "_", "B"}}, {1u, false, {"B/x", "\303\251", "a", "_", "B"}}, {1u, true, {"a/x", "B/x", "\303\251", "a", "_", "B"}}, {2u, true, {"_/x", "a/x", "B/x", "\303\251", "a", "_", "B"}}, {3u, true, {"\303\251/x", "_/x", "a/x", "B/x", "\303\251", "a", "_", "B"}}, {3u, false, {"_/x", "a/x", "B/x", "\303\251", "a", "_", "B"}}, {9u, true, {"\303\251/x", "_/x", "a/x", "B/x", "\303\251", "a", "_", "B"}}}},
    {"case_distinct_directories",
     {{"A/x", 1u}, {"a/y", 1u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "Ok", 0u, {"A", "a"}, 2u, 67108866u,
     {{0u, false, {"a", "A"}}, {0u, true, {"A/x", "a", "A"}}, {1u, false, {"A/x", "a", "A"}}, {1u, true, {"a/y", "A/x", "a", "A"}}, {2u, true, {"a/y", "A/x", "a", "A"}}, {3u, true, {"a/y", "A/x", "a", "A"}}, {3u, false, {"a/y", "A/x", "a", "A"}}, {9u, true, {"a/y", "A/x", "a", "A"}}}},
    {"file_is_dir_exact",
     {{"a", 1u}, {"a/b", 1u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "FileDirectoryConflict", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"file_is_dir_exact_dir_first",
     {{"a/b", 1u}, {"a", 1u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "FileDirectoryConflict", 1u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"file_is_dir_nested",
     {{"x/y/z", 1u}, {"q", 1u}, {"x/y", 1u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "FileDirectoryConflict", 2u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"file_is_dir_ascii_fold",
     {{"ok", 1u}, {"README", 1u}, {"readme/doc", 1u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "FileDirectoryConflict", 1u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"file_is_dir_latin1_fold",
     {{"cafe\314\201", 1u}, {"caf\303\251/x", 1u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "FileDirectoryConflict", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"file_is_dir_latin1_fold_reverse",
     {{"caf\303\251", 1u}, {"cafe\314\201/x", 1u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "FileDirectoryConflict", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"folds_not_composed",
     {{"E\314\201", 1u}, {"\303\251/x", 1u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "Ok", 0u, {"\303\251"}, 2u, 67108866u,
     {{0u, false, {"\303\251"}}, {0u, true, {"E\314\201", "\303\251"}}, {1u, false, {"E\314\201", "\303\251"}}, {1u, true, {"\303\251/x", "E\314\201", "\303\251"}}, {2u, true, {"\303\251/x", "E\314\201", "\303\251"}}, {3u, true, {"\303\251/x", "E\314\201", "\303\251"}}, {3u, false, {"\303\251/x", "E\314\201", "\303\251"}}, {9u, true, {"\303\251/x", "E\314\201", "\303\251"}}}},
    {"depth_equal_limit",
     {{"a/b/c", 1u}}, 1073741824u,
     3u, 1024u, 4096u, 67108864u,
     "Ok", 0u, {"a", "a/b"}, 1u, 67108865u,
     {{0u, false, {"a/b", "a"}}, {0u, true, {"a/b/c", "a/b", "a"}}, {1u, false, {"a/b/c", "a/b", "a"}}, {1u, true, {"a/b/c", "a/b", "a"}}, {2u, true, {"a/b/c", "a/b", "a"}}, {3u, true, {"a/b/c", "a/b", "a"}}, {3u, false, {"a/b/c", "a/b", "a"}}, {9u, true, {"a/b/c", "a/b", "a"}}}},
    {"depth_over_limit",
     {{"a/b/c", 1u}}, 1073741824u,
     2u, 1024u, 4096u, 67108864u,
     "PathTooDeep", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"depth_zero",
     {{"a", 1u}}, 1073741824u,
     0u, 1024u, 4096u, 67108864u,
     "PathTooDeep", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"path_equal_limit",
     {{"a/bcd", 1u}}, 1073741824u,
     16u, 5u, 4096u, 67108864u,
     "Ok", 0u, {"a"}, 1u, 67108865u,
     {{0u, false, {"a"}}, {0u, true, {"a/bcd", "a"}}, {1u, false, {"a/bcd", "a"}}, {1u, true, {"a/bcd", "a"}}, {2u, true, {"a/bcd", "a"}}, {3u, true, {"a/bcd", "a"}}, {3u, false, {"a/bcd", "a"}}, {9u, true, {"a/bcd", "a"}}}},
    {"path_over_limit",
     {{"ok", 1u}, {"a/bcde", 1u}}, 1073741824u,
     16u, 5u, 4096u, 67108864u,
     "PathTooLong", 1u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"path_counts_bytes",
     {{"\303\251", 1u}}, 1073741824u,
     16u, 1u, 4096u, 67108864u,
     "PathTooLong", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"long_wins_over_deep_same_entry",
     {{"a/b/c/d", 1u}}, 1073741824u,
     2u, 3u, 4096u, 67108864u,
     "PathTooLong", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"first_failing_entry_wins",
     {{"a/b/c", 1u}, {"abcdefgh", 1u}}, 1073741824u,
     2u, 5u, 4096u, 67108864u,
     "PathTooDeep", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"dirs_equal_limit",
     {{"a/x", 1u}, {"b/y", 1u}}, 1073741824u,
     16u, 1024u, 2u, 67108864u,
     "Ok", 0u, {"a", "b"}, 2u, 67108866u,
     {{0u, false, {"b", "a"}}, {0u, true, {"a/x", "b", "a"}}, {1u, false, {"a/x", "b", "a"}}, {1u, true, {"b/y", "a/x", "b", "a"}}, {2u, true, {"b/y", "a/x", "b", "a"}}, {3u, true, {"b/y", "a/x", "b", "a"}}, {3u, false, {"b/y", "a/x", "b", "a"}}, {9u, true, {"b/y", "a/x", "b", "a"}}}},
    {"dirs_over_limit",
     {{"a/x", 1u}, {"a/b/y", 1u}, {"c/z", 1u}}, 1073741824u,
     16u, 1024u, 2u, 67108864u,
     "TooManyDirectories", 2u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"dirs_zero_top_level_only",
     {{"a", 1u}, {"b", 1u}}, 1073741824u,
     16u, 1024u, 0u, 67108864u,
     "Ok", 0u, {}, 2u, 67108866u,
     {{0u, false, {}}, {0u, true, {"a"}}, {1u, false, {"a"}}, {1u, true, {"b", "a"}}, {2u, true, {"b", "a"}}, {3u, true, {"b", "a"}}, {3u, false, {"b", "a"}}, {9u, true, {"b", "a"}}}},
    {"dirs_zero_nested",
     {{"a", 1u}, {"b/c", 1u}}, 1073741824u,
     16u, 1024u, 0u, 67108864u,
     "TooManyDirectories", 1u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"dirs_prefixes_count",
     {{"a/b/c/d", 1u}}, 1073741824u,
     16u, 1024u, 2u, 67108864u,
     "TooManyDirectories", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"sum_exactly_fits_space",
     {{"a", 40u}, {"b", 60u}}, 100u,
     16u, 1024u, 4096u, 0u,
     "Ok", 0u, {}, 100u, 100u,
     {{0u, false, {}}, {0u, true, {"a"}}, {1u, false, {"a"}}, {1u, true, {"b", "a"}}, {2u, true, {"b", "a"}}, {3u, true, {"b", "a"}}, {3u, false, {"b", "a"}}, {9u, true, {"b", "a"}}}},
    {"space_short_by_one",
     {{"a", 40u}, {"b", 60u}}, 99u,
     16u, 1024u, 4096u, 0u,
     "InsufficientSpace", 0u, {}, 100u, 100u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"reserve_counts",
     {{"a", 40u}}, 140u,
     16u, 1024u, 4096u, 100u,
     "Ok", 0u, {}, 40u, 140u,
     {{0u, false, {}}, {0u, true, {"a"}}, {1u, false, {"a"}}, {1u, true, {"a"}}, {2u, true, {"a"}}, {3u, true, {"a"}}, {3u, false, {"a"}}, {9u, true, {"a"}}}},
    {"reserve_short_by_one",
     {{"a", 40u}}, 139u,
     16u, 1024u, 4096u, 100u,
     "InsufficientSpace", 0u, {}, 40u, 140u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"size_max_alone",
     {{"a", 18446744073709551615ull}}, 18446744073709551615ull,
     16u, 1024u, 4096u, 0u,
     "Ok", 0u, {}, 18446744073709551615ull, 18446744073709551615ull,
     {{0u, false, {}}, {0u, true, {"a"}}, {1u, false, {"a"}}, {1u, true, {"a"}}, {2u, true, {"a"}}, {3u, true, {"a"}}, {3u, false, {"a"}}, {9u, true, {"a"}}}},
    {"size_max_alone_with_reserve",
     {{"a", 18446744073709551615ull}}, 18446744073709551615ull,
     16u, 1024u, 4096u, 1u,
     "SizeOverflow", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"size_sum_overflow_second",
     {{"a", 18446744073709551615ull}, {"b", 1u}}, 18446744073709551615ull,
     16u, 1024u, 4096u, 0u,
     "SizeOverflow", 1u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"size_sum_overflow_index",
     {{"a", 1u}, {"b", 18446744073709551614ull}, {"c", 1u}, {"d", 5u}}, 18446744073709551615ull,
     16u, 1024u, 4096u, 0u,
     "SizeOverflow", 2u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"reserve_max_empty",
     {}, 18446744073709551615ull,
     16u, 1024u, 4096u, 18446744073709551615ull,
     "Ok", 0u, {}, 0u, 18446744073709551615ull,
     {{0u, false, {}}, {0u, true, {}}, {1u, false, {}}, {1u, true, {}}, {2u, true, {}}, {3u, true, {}}, {3u, false, {}}, {9u, true, {}}}},
    {"reserve_max_one_byte",
     {{"a", 1u}}, 18446744073709551615ull,
     16u, 1024u, 4096u, 18446744073709551615ull,
     "SizeOverflow", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"free_zero_zero_reserve",
     {{"a", 0u}}, 0u,
     16u, 1024u, 4096u, 0u,
     "Ok", 0u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {"a"}}, {1u, false, {"a"}}, {1u, true, {"a"}}, {2u, true, {"a"}}, {3u, true, {"a"}}, {3u, false, {"a"}}, {9u, true, {"a"}}}},
    {"many_names_same_dir",
     {{"d/f0", 0u}, {"d/f1", 1u}, {"d/f2", 2u}, {"d/f3", 3u}, {"d/f4", 4u}}, 1073741824u,
     16u, 1024u, 4096u, 67108864u,
     "Ok", 0u, {"d"}, 10u, 67108874u,
     {{0u, false, {"d"}}, {0u, true, {"d/f0", "d"}}, {1u, false, {"d/f0", "d"}}, {1u, true, {"d/f1", "d/f0", "d"}}, {2u, true, {"d/f2", "d/f1", "d/f0", "d"}}, {3u, true, {"d/f3", "d/f2", "d/f1", "d/f0", "d"}}, {3u, false, {"d/f2", "d/f1", "d/f0", "d"}}, {9u, true, {"d/f4", "d/f3", "d/f2", "d/f1", "d/f0", "d"}}}},
    {"conflict_before_dir_count",
     {{"a/x", 1u}, {"b/y", 1u}, {"a", 1u}}, 1073741824u,
     16u, 1024u, 1u, 67108864u,
     "FileDirectoryConflict", 2u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"dir_count_before_size",
     {{"a/x", 18446744073709551615ull}, {"b/y", 18446744073709551615ull}}, 1u,
     16u, 1024u, 1u, 67108864u,
     "TooManyDirectories", 1u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
    {"size_before_space",
     {{"a", 18446744073709551615ull}, {"b", 2u}}, 0u,
     16u, 1024u, 4096u, 67108864u,
     "SizeOverflow", 1u, {}, 0u, 0u,
     {{0u, false, {}}, {0u, true, {}}}},
};
struct RefPair {
  const char* decomposed;
  const char* composed;
};
const RefPair kRefPairs[] = {
    {"A\314\200", "\303\200"},
    {"A\314\201", "\303\201"},
    {"A\314\202", "\303\202"},
    {"A\314\203", "\303\203"},
    {"A\314\210", "\303\204"},
    {"A\314\212", "\303\205"},
    {"C\314\247", "\303\207"},
    {"E\314\200", "\303\210"},
    {"E\314\201", "\303\211"},
    {"E\314\202", "\303\212"},
    {"E\314\210", "\303\213"},
    {"I\314\200", "\303\214"},
    {"I\314\201", "\303\215"},
    {"I\314\202", "\303\216"},
    {"I\314\210", "\303\217"},
    {"N\314\203", "\303\221"},
    {"O\314\200", "\303\222"},
    {"O\314\201", "\303\223"},
    {"O\314\202", "\303\224"},
    {"O\314\203", "\303\225"},
    {"O\314\210", "\303\226"},
    {"U\314\200", "\303\231"},
    {"U\314\201", "\303\232"},
    {"U\314\202", "\303\233"},
    {"U\314\210", "\303\234"},
    {"Y\314\201", "\303\235"},
    {"a\314\200", "\303\240"},
    {"a\314\201", "\303\241"},
    {"a\314\202", "\303\242"},
    {"a\314\203", "\303\243"},
    {"a\314\210", "\303\244"},
    {"a\314\212", "\303\245"},
    {"c\314\247", "\303\247"},
    {"e\314\200", "\303\250"},
    {"e\314\201", "\303\251"},
    {"e\314\202", "\303\252"},
    {"e\314\210", "\303\253"},
    {"i\314\200", "\303\254"},
    {"i\314\201", "\303\255"},
    {"i\314\202", "\303\256"},
    {"i\314\210", "\303\257"},
    {"n\314\203", "\303\261"},
    {"o\314\200", "\303\262"},
    {"o\314\201", "\303\263"},
    {"o\314\202", "\303\264"},
    {"o\314\203", "\303\265"},
    {"o\314\210", "\303\266"},
    {"u\314\200", "\303\271"},
    {"u\314\201", "\303\272"},
    {"u\314\202", "\303\273"},
    {"u\314\210", "\303\274"},
    {"y\314\201", "\303\275"},
    {"y\314\210", "\303\277"},
};
// END GENERATED REFERENCE

std::vector<EntryInfo> entries_of(std::initializer_list<std::pair<const char*, std::uint64_t>> l) {
  std::vector<EntryInfo> v;
  for (const auto& p : l) {
    EntryInfo e;
    e.name = p.first;
    e.uncompressed_bytes = p.second;
    e.compressed_bytes = p.second / 2;
    v.push_back(std::move(e));
  }
  return v;
}

std::vector<EntryInfo> names_of(const std::vector<std::string>& names, std::uint64_t size = 1) {
  std::vector<EntryInfo> v;
  for (const std::string& n : names) {
    EntryInfo e;
    e.name = n;
    e.uncompressed_bytes = size;
    v.push_back(std::move(e));
  }
  return v;
}

// A path with exactly `depth` distinct components: "c0/c1/.../c<depth-1>".
std::string path_of_depth(std::size_t depth) {
  std::string p;
  for (std::size_t i = 0; i < depth; ++i) {
    if (i != 0) p += '/';
    p += "c" + std::to_string(i);
  }
  return p;
}

ExtractLimits limits_of(std::uint32_t depth, std::uint64_t path, std::uint64_t dirs,
                        std::uint64_t reserve) {
  ExtractLimits l;
  l.max_depth = depth;
  l.max_path_bytes = path;
  l.max_directories = dirs;
  l.reserve_bytes = reserve;
  return l;
}

constexpr std::uint64_t kFree = 1ull << 40;

void expect_failed(const ExtractPlan& p, ExtractVerdict v, std::size_t index) {
  INFO("got " << to_string(p.verdict) << " index " << p.entry_index);
  REQUIRE(p.verdict == v);
  REQUIRE(p.entry_index == index);
  REQUIRE(p.directories.empty());
  REQUIRE(p.file_order.empty());
  if (v != ExtractVerdict::InsufficientSpace) {
    REQUIRE(p.total_bytes == 0);
    REQUIRE(p.required_free_bytes == 0);
  }
}

}  // namespace

TEST_CASE("extract plan derives directories and file order", "[extract_plan]") {
  const auto e = entries_of({{"a/b.txt", 5}, {"a/c/d.bin", 7}, {"e.txt", 0}, {"z/y/x/w", 1}});
  const ExtractPlan p = plan_extraction(e, kFree);
  REQUIRE(p.verdict == ExtractVerdict::Ok);
  REQUIRE(p.entry_index == 0);
  REQUIRE(p.directories == std::vector<std::string>{"a", "z", "a/c", "z/y", "z/y/x"});
  REQUIRE(p.file_order == std::vector<std::size_t>{0, 1, 2, 3});
  REQUIRE(p.total_bytes == 13);
  REQUIRE(p.required_free_bytes == 13 + (64ull << 20));
  REQUIRE(std::string(to_string(p.verdict)) == "Ok");
}

TEST_CASE("extract plan accepts an empty package", "[extract_plan]") {
  const ExtractPlan p = plan_extraction({}, 64ull << 20);
  REQUIRE(p.verdict == ExtractVerdict::Ok);
  REQUIRE(p.directories.empty());
  REQUIRE(p.file_order.empty());
  REQUIRE(p.total_bytes == 0);
  REQUIRE(p.required_free_bytes == 64ull << 20);
  REQUIRE(plan_extraction({}, (64ull << 20) - 1).verdict == ExtractVerdict::InsufficientSpace);
}

TEST_CASE("extract plan names every verdict", "[extract_plan]") {
  const ExtractVerdict all[] = {ExtractVerdict::Ok,
                                ExtractVerdict::TooManyDirectories,
                                ExtractVerdict::PathTooDeep,
                                ExtractVerdict::PathTooLong,
                                ExtractVerdict::FileDirectoryConflict,
                                ExtractVerdict::SizeOverflow,
                                ExtractVerdict::InsufficientSpace};
  for (std::size_t i = 0; i < std::size(all); ++i) {
    REQUIRE(std::string(to_string(all[i])) != "Unknown");
    for (std::size_t j = i + 1; j < std::size(all); ++j) {
      REQUIRE(std::string(to_string(all[i])) != std::string(to_string(all[j])));
    }
  }
  REQUIRE(std::string(to_string(static_cast<ExtractVerdict>(250))) == "Unknown");
}

TEST_CASE("extract plan default limits", "[extract_plan]") {
  const ExtractLimits l;
  REQUIRE(l.max_depth == 16);
  REQUIRE(l.max_path_bytes == 1024);
  REQUIRE(l.max_directories == 4096);
  REQUIRE(l.reserve_bytes == (64ull << 20));
}

TEST_CASE("extract plan depth limit", "[extract_plan]") {
  SECTION("default limit of 16") {
    REQUIRE(plan_extraction(names_of({path_of_depth(16)}), kFree).verdict == ExtractVerdict::Ok);
    expect_failed(plan_extraction(names_of({path_of_depth(17)}), kFree),
                  ExtractVerdict::PathTooDeep, 0);
    REQUIRE(plan_extraction(names_of({path_of_depth(1)}), kFree).verdict == ExtractVerdict::Ok);
  }
  SECTION("limit 0 rejects every entry") {
    expect_failed(plan_extraction(names_of({"a"}), kFree, limits_of(0, 1024, 4096, 0)),
                  ExtractVerdict::PathTooDeep, 0);
    expect_failed(plan_extraction(names_of({"x", "a/b"}), kFree, limits_of(0, 1024, 4096, 0)),
                  ExtractVerdict::PathTooDeep, 0);
    REQUIRE(plan_extraction({}, kFree, limits_of(0, 1024, 4096, 0)).verdict == ExtractVerdict::Ok);
  }
  SECTION("equal passes, plus one fails, at several limits") {
    for (std::uint32_t limit : {1u, 2u, 3u, 16u, 17u, 100u}) {
      CAPTURE(limit);
      const ExtractLimits l = limits_of(limit, 1ull << 20, 4096, 0);
      REQUIRE(plan_extraction(names_of({path_of_depth(limit)}), kFree, l).verdict ==
              ExtractVerdict::Ok);
      expect_failed(plan_extraction(names_of({path_of_depth(limit + 1)}), kFree, l),
                    ExtractVerdict::PathTooDeep, 0);
    }
  }
  SECTION("integer maximum") {
    const ExtractLimits l = limits_of(kMax32, kMax, 4096, 0);
    REQUIRE(plan_extraction(names_of({path_of_depth(1000)}), kFree, l).verdict ==
            ExtractVerdict::Ok);
    const ExtractLimits tight = limits_of(kMax32 - 1, kMax, kMax, 0);
    REQUIRE(plan_extraction(names_of({path_of_depth(1000)}), kFree, tight).verdict ==
            ExtractVerdict::Ok);
  }
  SECTION("the first failing entry in input order is reported") {
    expect_failed(plan_extraction(names_of({"a", "b/c/d", "e/f/g/h"}), kFree,
                                  limits_of(2, 1024, 4096, 0)),
                  ExtractVerdict::PathTooDeep, 1);
  }
}

TEST_CASE("extract plan path length limit", "[extract_plan]") {
  auto name_of = [](std::size_t n) {
    std::string s(n, 'n');
    return s;
  };
  SECTION("default limit of 1024 bytes") {
    REQUIRE(plan_extraction(names_of({name_of(1024)}), kFree).verdict == ExtractVerdict::Ok);
    expect_failed(plan_extraction(names_of({name_of(1025)}), kFree),
                  ExtractVerdict::PathTooLong, 0);
  }
  SECTION("separators count") {
    // 4 + 1 + 4 = 9 bytes
    REQUIRE(plan_extraction(names_of({"dddd/ffff"}), kFree, limits_of(16, 9, 4096, 0)).verdict ==
            ExtractVerdict::Ok);
    expect_failed(plan_extraction(names_of({"dddd/ffff"}), kFree, limits_of(16, 8, 4096, 0)),
                  ExtractVerdict::PathTooLong, 0);
  }
  SECTION("limit 0 rejects every non-empty name") {
    expect_failed(plan_extraction(names_of({"a"}), kFree, limits_of(16, 0, 4096, 0)),
                  ExtractVerdict::PathTooLong, 0);
    REQUIRE(plan_extraction({}, kFree, limits_of(16, 0, 4096, 0)).verdict == ExtractVerdict::Ok);
  }
  SECTION("bytes, not characters") {
    // U+00E9 is two bytes.
    const std::string e_acute = "\xC3\xA9";
    REQUIRE(plan_extraction(names_of({e_acute}), kFree, limits_of(16, 2, 4096, 0)).verdict ==
            ExtractVerdict::Ok);
    expect_failed(plan_extraction(names_of({e_acute}), kFree, limits_of(16, 1, 4096, 0)),
                  ExtractVerdict::PathTooLong, 0);
  }
  SECTION("integer maximum") {
    REQUIRE(plan_extraction(names_of({name_of(5000)}), kFree, limits_of(16, kMax, 4096, 0)).verdict ==
            ExtractVerdict::Ok);
    REQUIRE(plan_extraction(names_of({name_of(5000)}), kFree, limits_of(16, kMax - 1, 4096, 0))
                .verdict == ExtractVerdict::Ok);
  }
  SECTION("length is checked before depth for one entry, entries in order") {
    const ExtractLimits l = limits_of(2, 5, 4096, 0);
    expect_failed(plan_extraction(names_of({"a/b/c/d"}), kFree, l), ExtractVerdict::PathTooLong, 0);
    expect_failed(plan_extraction(names_of({"a/b/c", "abcdefg"}), kFree, l),
                  ExtractVerdict::PathTooDeep, 0);
    expect_failed(plan_extraction(names_of({"abcdefg", "a/b/c"}), kFree, l),
                  ExtractVerdict::PathTooLong, 0);
    expect_failed(plan_extraction(names_of({"ab", "a/b/c"}), kFree, l),
                  ExtractVerdict::PathTooDeep, 1);
  }
}

TEST_CASE("extract plan directory count limit", "[extract_plan]") {
  auto dirs_of = [](std::size_t n) {
    std::vector<std::string> v;
    for (std::size_t i = 0; i < n; ++i) v.push_back("d" + std::to_string(i) + "/f");
    return v;
  };
  SECTION("default limit of 4096") {
    const ExtractPlan ok = plan_extraction(names_of(dirs_of(4096)), kFree);
    REQUIRE(ok.verdict == ExtractVerdict::Ok);
    REQUIRE(ok.directories.size() == 4096);
    expect_failed(plan_extraction(names_of(dirs_of(4097)), kFree),
                  ExtractVerdict::TooManyDirectories, 4096);
  }
  SECTION("limit 0 allows top-level files only") {
    const ExtractLimits l = limits_of(16, 1024, 0, 0);
    REQUIRE(plan_extraction(names_of({"a", "b", "c.txt"}), kFree, l).verdict == ExtractVerdict::Ok);
    expect_failed(plan_extraction(names_of({"a", "b/c"}), kFree, l),
                  ExtractVerdict::TooManyDirectories, 1);
    REQUIRE(plan_extraction({}, kFree, l).verdict == ExtractVerdict::Ok);
  }
  SECTION("parents count, each once") {
    // a, a/b, a/b/c: three directories from one entry.
    const auto e = names_of({"a/b/c/f"});
    REQUIRE(plan_extraction(e, kFree, limits_of(16, 1024, 3, 0)).verdict == ExtractVerdict::Ok);
    expect_failed(plan_extraction(e, kFree, limits_of(16, 1024, 2, 0)),
                  ExtractVerdict::TooManyDirectories, 0);
    // A shared parent is one directory.
    REQUIRE(plan_extraction(names_of({"a/x", "a/y", "a/z"}), kFree, limits_of(16, 1024, 1, 0))
                .verdict == ExtractVerdict::Ok);
  }
  SECTION("exact strings: a/x and A/y are two directories") {
    REQUIRE(plan_extraction(names_of({"a/x", "A/y"}), kFree, limits_of(16, 1024, 2, 0)).verdict ==
            ExtractVerdict::Ok);
    expect_failed(plan_extraction(names_of({"a/x", "A/y"}), kFree, limits_of(16, 1024, 1, 0)),
                  ExtractVerdict::TooManyDirectories, 1);
  }
  SECTION("the entry that introduces the first directory over the limit") {
    expect_failed(plan_extraction(names_of({"a/x", "a/y", "b/c/d/z", "e/f"}), kFree,
                                  limits_of(16, 1024, 3, 0)),
                  ExtractVerdict::TooManyDirectories, 2);
    expect_failed(plan_extraction(names_of({"a/b/x", "c/d/y"}), kFree, limits_of(16, 1024, 3, 0)),
                  ExtractVerdict::TooManyDirectories, 1);
  }
  SECTION("integer maximum") {
    REQUIRE(plan_extraction(names_of(dirs_of(5000)), kFree, limits_of(16, 1024, kMax, 0)).verdict ==
            ExtractVerdict::Ok);
    REQUIRE(plan_extraction(names_of(dirs_of(5000)), kFree, limits_of(16, 1024, kMax - 1, 0))
                .verdict == ExtractVerdict::Ok);
    REQUIRE(plan_extraction(names_of(dirs_of(5000)), kFree, limits_of(16, 1024, 5000, 0)).verdict ==
            ExtractVerdict::Ok);
    expect_failed(plan_extraction(names_of(dirs_of(5000)), kFree, limits_of(16, 1024, 4999, 0)),
                  ExtractVerdict::TooManyDirectories, 4999);
  }
}

TEST_CASE("extract plan file and directory conflicts", "[extract_plan]") {
  SECTION("exact, in either order") {
    expect_failed(plan_extraction(names_of({"a", "a/b"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 0);
    expect_failed(plan_extraction(names_of({"a/b", "a"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 1);
    expect_failed(plan_extraction(names_of({"x/y", "q", "x/y/z"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 0);
    expect_failed(plan_extraction(names_of({"p/q/r", "p/q", "x"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 1);
  }
  SECTION("ASCII case folding alone") {
    expect_failed(plan_extraction(names_of({"README", "readme/doc"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 0);
    expect_failed(plan_extraction(names_of({"readme/doc", "README"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 1);
    expect_failed(plan_extraction(names_of({"a/B", "A/b/c"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 0);
  }
  SECTION("Latin-1 normalization alone") {
    const std::string decomposed = "cafe\xCC\x81";
    const std::string composed = "caf\xC3\xA9";
    expect_failed(plan_extraction(names_of({decomposed, composed + "/x"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 0);
    expect_failed(plan_extraction(names_of({composed, decomposed + "/x"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 0);
    expect_failed(plan_extraction(names_of({"d/" + composed + "/x", "d/" + decomposed}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 1);
  }
  SECTION("the folds are never composed") {
    // Uppercase E + combining acute against lowercase precomposed e-acute: not a conflict.
    const ExtractPlan p =
        plan_extraction(names_of({"E\xCC\x81", "\xC3\xA9/x"}), kFree);
    REQUIRE(p.verdict == ExtractVerdict::Ok);
    REQUIRE(plan_extraction(names_of({"\xC3\x89", "\xC3\xA9/x"}), kFree).verdict ==
            ExtractVerdict::Ok);  // U+00C9 and U+00E9: the policy folds ASCII case only
  }
  SECTION("names that share a prefix without being a parent are fine") {
    REQUIRE(plan_extraction(names_of({"ab", "a/b", "abc/d", "a.txt", "a/b.txt"}), kFree).verdict ==
            ExtractVerdict::Ok);
    REQUIRE(plan_extraction(names_of({"a/b", "a/b2", "a/bc/d"}), kFree).verdict ==
            ExtractVerdict::Ok);
  }
  SECTION("the earliest conflicting file is reported") {
    expect_failed(plan_extraction(names_of({"k", "m/n", "m", "k/j"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 0);
    expect_failed(plan_extraction(names_of({"x", "m/n", "m", "x/j", "m/n/o"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 0);
    expect_failed(plan_extraction(names_of({"p", "m", "m/n", "m/n/o"}), kFree),
                  ExtractVerdict::FileDirectoryConflict, 1);
  }
  SECTION("conflicts win over the directory count and the sizes") {
    expect_failed(plan_extraction(names_of({"a/x", "b/y", "a"}), kFree, limits_of(16, 1024, 1, 0)),
                  ExtractVerdict::FileDirectoryConflict, 2);
    auto e = names_of({"a/x", "a"}, kMax);
    expect_failed(plan_extraction(e, 0), ExtractVerdict::FileDirectoryConflict, 1);
  }
}

TEST_CASE("extract plan orders directories", "[extract_plan]") {
  SECTION("shallow first, byte order inside one depth, parents before children") {
    const ExtractPlan p = plan_extraction(
        names_of({"b/x", "a/c/d", "a/y", "z", "a/c/e", "B/q", "_/w", "\xC3\xA9/f", "a/b/c/d/e"}),
        kFree);
    REQUIRE(p.verdict == ExtractVerdict::Ok);
    REQUIRE(p.directories == std::vector<std::string>{"B", "_", "a", "b", "\xC3\xA9", "a/b", "a/c",
                                                      "a/b/c", "a/b/c/d"});
  }
  SECTION("bytes compare as unsigned") {
    const ExtractPlan p = plan_extraction(names_of({"\xC3\xA9/x", "\x7F/x", "z/x"}), kFree);
    REQUIRE(p.verdict == ExtractVerdict::Ok);
    REQUIRE(p.directories == std::vector<std::string>{"z", "\x7F", "\xC3\xA9"});
  }
  SECTION("each directory once, folded names stay separate") {
    const ExtractPlan p = plan_extraction(names_of({"A/x", "a/y", "A/z"}), kFree);
    REQUIRE(p.verdict == ExtractVerdict::Ok);
    REQUIRE(p.directories == std::vector<std::string>{"A", "a"});
  }
  SECTION("file order is input order, whatever the names") {
    const ExtractPlan p = plan_extraction(names_of({"z/z", "a", "m/m", "b/b/b"}), kFree);
    REQUIRE(p.file_order == std::vector<std::size_t>{0, 1, 2, 3});
  }
}

TEST_CASE("extract plan sizes and free space", "[extract_plan]") {
  const ExtractLimits none = limits_of(16, 1024, 4096, 0);
  SECTION("space: equal passes, one short fails") {
    const auto e = entries_of({{"a", 40}, {"b", 60}});
    const ExtractPlan ok = plan_extraction(e, 100, none);
    REQUIRE(ok.verdict == ExtractVerdict::Ok);
    REQUIRE(ok.total_bytes == 100);
    REQUIRE(ok.required_free_bytes == 100);
    const ExtractPlan short_by_one = plan_extraction(e, 99, none);
    expect_failed(short_by_one, ExtractVerdict::InsufficientSpace, 0);
    REQUIRE(short_by_one.total_bytes == 100);
    REQUIRE(short_by_one.required_free_bytes == 100);
    REQUIRE(plan_extraction(e, 0, none).verdict == ExtractVerdict::InsufficientSpace);
    REQUIRE(plan_extraction(e, kMax, none).verdict == ExtractVerdict::Ok);
  }
  SECTION("the reserve counts") {
    const auto e = entries_of({{"a", 40}});
    REQUIRE(plan_extraction(e, 140, limits_of(16, 1024, 4096, 100)).verdict == ExtractVerdict::Ok);
    const ExtractPlan p = plan_extraction(e, 139, limits_of(16, 1024, 4096, 100));
    expect_failed(p, ExtractVerdict::InsufficientSpace, 0);
    REQUIRE(p.required_free_bytes == 140);
    REQUIRE(plan_extraction(e, 0, limits_of(16, 1024, 4096, 0)).verdict ==
            ExtractVerdict::InsufficientSpace);
    REQUIRE(plan_extraction(entries_of({{"a", 0}}), 0, none).verdict == ExtractVerdict::Ok);
  }
  SECTION("compressed sizes are not counted") {
    EntryInfo e;
    e.name = "a";
    e.compressed_bytes = 1000;
    e.uncompressed_bytes = 10;
    REQUIRE(plan_extraction({e}, 10, none).verdict == ExtractVerdict::Ok);
    REQUIRE(plan_extraction({e}, 9, none).verdict == ExtractVerdict::InsufficientSpace);
  }
  SECTION("one entry of the integer maximum") {
    const auto e = entries_of({{"a", kMax}});
    const ExtractPlan p = plan_extraction(e, kMax, none);
    REQUIRE(p.verdict == ExtractVerdict::Ok);
    REQUIRE(p.total_bytes == kMax);
    REQUIRE(p.required_free_bytes == kMax);
    expect_failed(plan_extraction(e, kMax, limits_of(16, 1024, 4096, 1)),
                  ExtractVerdict::SizeOverflow, 0);
    REQUIRE(plan_extraction(e, kMax - 1, none).verdict == ExtractVerdict::InsufficientSpace);
  }
  SECTION("sum overflow reports the entry that overflows") {
    expect_failed(plan_extraction(entries_of({{"a", kMax}, {"b", 1}}), kMax, none),
                  ExtractVerdict::SizeOverflow, 1);
    expect_failed(plan_extraction(entries_of({{"a", 1}, {"b", kMax - 1}, {"c", 1}, {"d", 5}}),
                                  kMax, none),
                  ExtractVerdict::SizeOverflow, 2);
    const ExtractPlan edge = plan_extraction(entries_of({{"a", kMax - 1}, {"b", 1}, {"c", 0}}), kMax, none);
    REQUIRE(edge.verdict == ExtractVerdict::Ok);
    REQUIRE(edge.total_bytes == kMax);
  }
}

TEST_CASE("extract plan reserve and sum at the integer maximum", "[extract_plan]") {
  REQUIRE(plan_extraction({}, kMax, limits_of(16, 1024, 4096, kMax)).verdict == ExtractVerdict::Ok);
  const ExtractPlan p = plan_extraction({}, kMax, limits_of(16, 1024, 4096, kMax));
  REQUIRE(p.required_free_bytes == kMax);
  expect_failed(plan_extraction(entries_of({{"a", 1}}), kMax, limits_of(16, 1024, 4096, kMax)),
                ExtractVerdict::SizeOverflow, 0);
  expect_failed(plan_extraction(entries_of({{"a", 0}, {"b", 1}}), kMax,
                                limits_of(16, 1024, 4096, kMax)),
                ExtractVerdict::SizeOverflow, 0);
  const ExtractPlan q = plan_extraction(entries_of({{"a", 0}}), kMax - 1,
                                        limits_of(16, 1024, 4096, kMax));
  expect_failed(q, ExtractVerdict::InsufficientSpace, 0);
  REQUIRE(q.required_free_bytes == kMax);
  // One byte below the overflow edge.
  const ExtractPlan edge = plan_extraction(entries_of({{"a", 1}}), kMax,
                                           limits_of(16, 1024, 4096, kMax - 1));
  REQUIRE(edge.verdict == ExtractVerdict::Ok);
  REQUIRE(edge.required_free_bytes == kMax);
}

TEST_CASE("extract plan check order", "[extract_plan]") {
  // Each pair: the earlier check wins when both fail.
  SECTION("length and depth before conflicts") {
    expect_failed(plan_extraction(names_of({"a", "a/b"}), kFree, limits_of(16, 2, 4096, 0)),
                  ExtractVerdict::PathTooLong, 1);
    expect_failed(plan_extraction(names_of({"a", "a/b"}), kFree, limits_of(1, 1024, 4096, 0)),
                  ExtractVerdict::PathTooDeep, 1);
  }
  SECTION("conflicts before the directory count") {
    expect_failed(plan_extraction(names_of({"a/x", "b/y", "a"}), kFree, limits_of(16, 1024, 1, 0)),
                  ExtractVerdict::FileDirectoryConflict, 2);
  }
  SECTION("directory count before sizes") {
    expect_failed(plan_extraction(entries_of({{"a/x", kMax}, {"b/y", kMax}}), 1,
                                  limits_of(16, 1024, 1, 0)),
                  ExtractVerdict::TooManyDirectories, 1);
  }
  SECTION("overflow before space") {
    expect_failed(plan_extraction(entries_of({{"a", kMax}, {"b", 2}}), 0, limits_of(16, 1024, 4096, 0)),
                  ExtractVerdict::SizeOverflow, 1);
  }
}

TEST_CASE("extract plan contract violations do not crash", "[extract_plan]") {
  // Names are validated by the entry policy before they reach the plan. If one slips through, it
  // is split on '/' as given and no new verdict exists.
  SECTION("trailing slash") {
    const ExtractPlan p = plan_extraction(names_of({"a/"}), kFree);
    REQUIRE(p.verdict == ExtractVerdict::Ok);
    REQUIRE(p.directories == std::vector<std::string>{"a"});
  }
  SECTION("leading slash gives no empty directory") {
    const ExtractPlan p = plan_extraction(names_of({"/a"}), kFree);
    REQUIRE(p.verdict == ExtractVerdict::Ok);
    REQUIRE(p.directories.empty());
  }
  SECTION("empty component") {
    const ExtractPlan p = plan_extraction(names_of({"a//b"}), kFree);
    REQUIRE(p.verdict == ExtractVerdict::Ok);
    REQUIRE(p.directories == std::vector<std::string>{"a", "a/"});
    REQUIRE(plan_extraction(names_of({"a//b"}), kFree, limits_of(2, 1024, 4096, 0)).verdict ==
            ExtractVerdict::PathTooDeep);
  }
  SECTION("empty name, dot dot, drive and control bytes") {
    for (const char* n : {"", "..", "../x", "a/../b", "C:/x", "a\\b", "a\x01"
                          "b"}) {
      const ExtractPlan p = plan_extraction(names_of({n}), kFree);
      REQUIRE(p.verdict == ExtractVerdict::Ok);
    }
  }
}

TEST_CASE("extract plan cleanup order", "[extract_plan]") {
  const auto e = entries_of({{"a/f1", 1}, {"a/b/f2", 1}, {"g", 1}, {"a/b/c/f3", 1}});
  const ExtractPlan p = plan_extraction(e, kFree);
  REQUIRE(p.verdict == ExtractVerdict::Ok);
  REQUIRE(p.directories == std::vector<std::string>{"a", "a/b", "a/b/c"});
  using V = std::vector<std::string>;
  SECTION("nothing written") {
    REQUIRE(cleanup_paths(p, e, 0, false) == V{"a/b/c", "a/b", "a"});
    REQUIRE(cleanup_paths(p, e, 0, true) == V{"a/f1", "a/b/c", "a/b", "a"});
  }
  SECTION("some files written") {
    REQUIRE(cleanup_paths(p, e, 2, false) == V{"a/b/f2", "a/f1", "a/b/c", "a/b", "a"});
    REQUIRE(cleanup_paths(p, e, 2, true) == V{"g", "a/b/f2", "a/f1", "a/b/c", "a/b", "a"});
    REQUIRE(cleanup_paths(p, e, 3, true) ==
            V{"a/b/c/f3", "g", "a/b/f2", "a/f1", "a/b/c", "a/b", "a"});
  }
  SECTION("all written: no partial file even when the flag is set") {
    const V all = {"a/b/c/f3", "g", "a/b/f2", "a/f1", "a/b/c", "a/b", "a"};
    REQUIRE(cleanup_paths(p, e, 4, false) == all);
    REQUIRE(cleanup_paths(p, e, 4, true) == all);
  }
  SECTION("completed is clamped") {
    const V all = {"a/b/c/f3", "g", "a/b/f2", "a/f1", "a/b/c", "a/b", "a"};
    REQUIRE(cleanup_paths(p, e, 5, true) == all);
    REQUIRE(cleanup_paths(p, e, kMax, false) == all);
    REQUIRE(cleanup_paths(p, e, kMax, true) == all);
  }
  SECTION("children come before parents, files before directories") {
    const V out = cleanup_paths(p, e, 4, false);
    for (std::size_t i = 0; i < out.size(); ++i) {
      for (std::size_t j = i + 1; j < out.size(); ++j) {
        // A later path is never inside an earlier path.
        REQUIRE(out[j].compare(0, out[i].size() + 1, out[i] + "/") != 0);
      }
    }
  }
  SECTION("a failed plan has nothing to clean") {
    const ExtractPlan bad = plan_extraction(names_of({"a", "a/b"}), kFree);
    REQUIRE(cleanup_paths(bad, names_of({"a", "a/b"}), 1, true).empty());
    REQUIRE(cleanup_paths(ExtractPlan{ExtractVerdict::InsufficientSpace, 0, {"x"}, {0}, 0, 0}, e, 0,
                          true)
                .empty());
  }
  SECTION("an index outside the entries is skipped") {
    const V out = cleanup_paths(p, names_of({"only"}), 3, true);
    // File 0 is "only"; files 1 to 3 do not exist in the entry list and are skipped.
    REQUIRE(out == V{"only", "a/b/c", "a/b", "a"});
    const V partial = cleanup_paths(p, names_of({"only"}), 1, true);
    REQUIRE(partial == V{"only", "a/b/c", "a/b", "a"});
    const V none = cleanup_paths(p, {}, 4, true);
    REQUIRE(none == V{"a/b/c", "a/b", "a"});
  }
  SECTION("an empty plan") {
    const ExtractPlan empty = plan_extraction({}, kFree);
    REQUIRE(cleanup_paths(empty, {}, 0, true).empty());
    REQUIRE(cleanup_paths(empty, {}, 10, false).empty());
  }
  SECTION("nothing outside the plan") {
    for (std::size_t completed = 0; completed <= 5; ++completed) {
      for (bool partial : {false, true}) {
        for (const std::string& path : cleanup_paths(p, e, completed, partial)) {
          const bool is_file = std::any_of(e.begin(), e.end(),
                                           [&](const EntryInfo& x) { return x.name == path; });
          const bool is_dir =
              std::find(p.directories.begin(), p.directories.end(), path) != p.directories.end();
          REQUIRE((is_file || is_dir));
        }
      }
    }
  }
}

TEST_CASE("extract plan reference constants from tools/extract_plan_ref.py", "[extract_plan]") {
  for (const RefCase& c : kRefCases) {
    CAPTURE(c.label);
    std::vector<EntryInfo> entries;
    for (const RefEntry& r : c.entries) {
      EntryInfo e;
      e.name = r.name;
      e.uncompressed_bytes = r.size;
      entries.push_back(std::move(e));
    }
    const ExtractLimits l = limits_of(c.max_depth, c.max_path_bytes, c.max_directories,
                                      c.reserve_bytes);
    const ExtractPlan p = plan_extraction(entries, c.free_bytes, l);
    INFO("got " << to_string(p.verdict) << " index " << p.entry_index);
    REQUIRE(std::string(to_string(p.verdict)) == c.verdict);
    REQUIRE(p.entry_index == c.entry_index);
    REQUIRE(p.directories.size() == c.directories.size());
    for (std::size_t i = 0; i < c.directories.size(); ++i) {
      REQUIRE(p.directories[i] == c.directories[i]);
    }
    REQUIRE(p.total_bytes == c.total_bytes);
    REQUIRE(p.required_free_bytes == c.required_free_bytes);
    if (p.verdict == ExtractVerdict::Ok) {
      REQUIRE(p.file_order.size() == entries.size());
      for (std::size_t i = 0; i < entries.size(); ++i) REQUIRE(p.file_order[i] == i);
    } else {
      REQUIRE(p.file_order.empty());
    }
    for (const RefCleanup& k : c.cleanups) {
      const std::vector<std::string> got = cleanup_paths(p, entries, k.completed, k.partial);
      REQUIRE(got.size() == k.paths.size());
      for (std::size_t i = 0; i < got.size(); ++i) REQUIRE(got[i] == k.paths[i]);
    }
  }
}

TEST_CASE("extract plan folds agree with the entry policy", "[extract_plan]") {
  // The plan keeps a private copy of the policy's folds. Every letter + mark combination is run
  // through both: validate_entries must call it a collision exactly when the plan finds a conflict.
  auto policy_collides = [](const std::string& a, const std::string& b) {
    const SetResult r = validate_entries(names_of({a, b}, 0));
    return r.verdict == SetVerdict::DuplicateName || r.verdict == SetVerdict::CaseFoldCollision ||
           r.verdict == SetVerdict::NormalizationCollision;
  };
  auto plan_conflicts = [](const std::string& file, const std::string& dir) {
    return plan_extraction(names_of({file, dir + "/x"}), kFree).verdict ==
           ExtractVerdict::FileDirectoryConflict;
  };
  const char* letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  const char* marks[] = {"\xCC\x80", "\xCC\x81", "\xCC\x82", "\xCC\x83",
                         "\xCC\x88", "\xCC\x8A", "\xCC\xA7", "\xCC\x84", "\xCC\x8C"};
  std::size_t table_hits = 0;
  for (const char* p = letters; *p != 0; ++p) {
    for (const char* mark : marks) {
      const std::string decomposed = std::string(1, *p) + mark;
      bool in_table = false;
      for (const RefPair& pair : kRefPairs) {
        if (decomposed == pair.decomposed) {
          in_table = true;
          INFO("pair " << decomposed << " " << pair.composed); REQUIRE(policy_collides(decomposed, pair.composed));
          REQUIRE(plan_conflicts(decomposed, pair.composed));
          REQUIRE(plan_conflicts(pair.composed, decomposed));
          ++table_hits;
        }
      }
      if (!in_table) {
        // Not a Latin-1 composition: neither side folds it to anything.
        REQUIRE(!policy_collides(decomposed, std::string(1, *p)));
        REQUIRE(!plan_conflicts(decomposed, std::string(1, *p)));
      }
    }
  }
  REQUIRE(table_hits == std::size(kRefPairs));
  // Mixed pairs and the ASCII fold.
  const std::vector<std::pair<std::string, std::string>> pairs = {
      {"A", "a"}, {"README", "readme"}, {"a/B", "A/b"}, {"x", "x"}, {"x", "y"},
      {"e\xCC\x81", "\xC3\xA9"}, {"E\xCC\x81", "\xC3\xA9"}, {"\xC3\x89", "\xC3\xA9"},
      {"e\xCC\x81\xCC\x81", "\xC3\xA9"}, {"e\xCC\x81\xCC\x81", "\xC3\xA9\xCC\x81"},
      {"ae\xCC\x81", "a\xC3\xA9"}, {"\xC3\xA9", "\xC3\xA9"}, {"e", "\xC3\xA9"}, {"a.txt", "A.TXT"},
  };
  for (const auto& pr : pairs) {
    CAPTURE(pr.first);
    CAPTURE(pr.second);
    REQUIRE(policy_collides(pr.first, pr.second) == plan_conflicts(pr.first, pr.second));
  }
}

namespace {

// ---- Naive oracle: a second implementation that shares no code with extract_plan.cpp. ----

struct OracleResult {
  ExtractVerdict verdict = ExtractVerdict::Ok;
  std::size_t index = 0;
  std::vector<std::string> dirs;
  std::uint64_t total = 0;
  std::uint64_t required = 0;
};

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out(1);
  for (char c : s) {
    if (c == '/') out.emplace_back();
    else out.back().push_back(c);
  }
  return out;
}

std::string join(const std::vector<std::string>& parts, std::size_t count) {
  std::string out;
  for (std::size_t i = 0; i < count; ++i) {
    if (i != 0) out += '/';
    out += parts[i];
  }
  return out;
}

std::string o_lower(std::string s) {
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] >= 'A' && s[i] <= 'Z') s[i] = static_cast<char>(s[i] + 32);
  }
  return s;
}

// Normalization from the pairs that Python's unicodedata produced.
std::string o_norm(const std::string& s) {
  std::string out;
  std::size_t i = 0;
  while (i < s.size()) {
    bool done = false;
    for (const RefPair& p : kRefPairs) {
      const std::string d = p.decomposed;
      if (s.compare(i, d.size(), d) == 0) {
        out += p.composed;
        i += d.size();
        done = true;
        break;
      }
    }
    if (!done) out += s[i++];
  }
  return out;
}

bool o_same(const std::string& file, const std::string& dir) {
  return file == dir || o_lower(file) == o_lower(dir) || o_norm(file) == o_norm(dir);
}

OracleResult oracle(const std::vector<EntryInfo>& e, std::uint64_t free_bytes,
                    const ExtractLimits& lim) {
  OracleResult r;
  auto bad = [&](ExtractVerdict v, std::size_t i) {
    OracleResult x;
    x.verdict = v;
    x.index = i;
    return x;
  };
  for (std::size_t i = 0; i < e.size(); ++i) {
    if (e[i].name.size() > lim.max_path_bytes) return bad(ExtractVerdict::PathTooLong, i);
    if (split(e[i].name).size() > lim.max_depth) return bad(ExtractVerdict::PathTooDeep, i);
  }
  std::vector<std::string> dirs;
  std::vector<std::size_t> owner;
  for (std::size_t i = 0; i < e.size(); ++i) {
    const std::vector<std::string> parts = split(e[i].name);
    for (std::size_t k = 1; k < parts.size(); ++k) {
      const std::string d = join(parts, k);
      if (d.empty()) continue;
      if (std::find(dirs.begin(), dirs.end(), d) == dirs.end()) {
        dirs.push_back(d);
        owner.push_back(i);
      }
    }
  }
  for (std::size_t i = 0; i < e.size(); ++i) {
    for (const std::string& d : dirs) {
      if (o_same(e[i].name, d)) return bad(ExtractVerdict::FileDirectoryConflict, i);
    }
  }
  if (dirs.size() > lim.max_directories) {
    return bad(ExtractVerdict::TooManyDirectories, owner[lim.max_directories]);
  }
  // Sums as a (high, low) pair so that a carry out of 64 bits is visible.
  std::uint64_t sum_hi = 0, sum_lo = 0;
  for (std::size_t i = 0; i < e.size(); ++i) {
    sum_lo += e[i].uncompressed_bytes;
    if (sum_lo < e[i].uncompressed_bytes) ++sum_hi;
    if (sum_hi != 0) return bad(ExtractVerdict::SizeOverflow, i);
  }
  std::uint64_t need_hi = sum_hi, need_lo = sum_lo + lim.reserve_bytes;
  if (need_lo < sum_lo) ++need_hi;
  if (need_hi != 0) return bad(ExtractVerdict::SizeOverflow, 0);
  if (free_bytes < need_lo) {
    OracleResult x = bad(ExtractVerdict::InsufficientSpace, 0);
    x.total = sum_lo;
    x.required = need_lo;
    return x;
  }
  std::sort(dirs.begin(), dirs.end(), [](const std::string& a, const std::string& b) {
    const std::size_t da = split(a).size();
    const std::size_t db = split(b).size();
    return da != db ? da < db : std::lexicographical_compare(
                                    a.begin(), a.end(), b.begin(), b.end(),
                                    [](char x, char y) {
                                      return static_cast<unsigned char>(x) <
                                             static_cast<unsigned char>(y);
                                    });
  });
  r.dirs = dirs;
  r.total = sum_lo;
  r.required = need_lo;
  return r;
}

std::vector<std::string> oracle_cleanup(const OracleResult& r, const std::vector<EntryInfo>& e,
                                        std::size_t completed, bool partial) {
  std::vector<std::string> out;
  if (r.verdict != ExtractVerdict::Ok) return out;
  const std::size_t n = e.size();
  const std::size_t done = completed > n ? n : completed;
  if (partial && done < n) out.push_back(e[done].name);
  for (std::size_t k = 0; k < done; ++k) out.push_back(e[done - 1 - k].name);
  for (std::size_t k = 0; k < r.dirs.size(); ++k) out.push_back(r.dirs[r.dirs.size() - 1 - k]);
  return out;
}

}  // namespace

TEST_CASE("extract plan against a naive oracle", "[extract_plan]") {
  const char* tokens[] = {"a", "A", "e", "E", "\xC3\xA9", "\xC3\x89", "e\xCC\x81", "E\xCC\x81",
                          "x", "ab", "n\xCC\x83", "\xC3\xB1"};
  std::mt19937 rng(20260326);
  auto pick = [&](std::size_t n) { return static_cast<std::size_t>(rng() % n); };
  const std::uint64_t sizes[] = {0, 1, 2, 5, 100, kMax - 1, kMax};
  const std::uint32_t depths[] = {0, 1, 2, 3, 16};
  const std::uint64_t paths[] = {0, 1, 3, 5, 8, 1024};
  const std::uint64_t dir_limits[] = {0, 1, 2, 3, 4096};
  const std::uint64_t reserves[] = {0, 1, 10, kMax};
  const std::uint64_t frees[] = {0, 5, 100, 1000, kMax};
  std::size_t verdict_counts[7] = {};
  for (int iter = 0; iter < 60000; ++iter) {
    std::vector<EntryInfo> entries;
    const std::size_t count = pick(7);
    for (std::size_t i = 0; i < count; ++i) {
      EntryInfo e;
      const std::size_t depth = 1 + pick(4);
      for (std::size_t d = 0; d < depth; ++d) {
        if (d != 0) e.name += '/';
        e.name += tokens[pick(std::size(tokens))];
        if (pick(4) == 0) e.name += tokens[pick(std::size(tokens))];
      }
      e.uncompressed_bytes = pick(3) == 0 ? sizes[pick(std::size(sizes))] : pick(4);
      entries.push_back(std::move(e));
    }
    const ExtractLimits lim = limits_of(depths[pick(5)], paths[pick(6)], dir_limits[pick(5)],
                                        reserves[pick(4)]);
    const std::uint64_t free_bytes = frees[pick(5)];
    const OracleResult want = oracle(entries, free_bytes, lim);
    const ExtractPlan got = plan_extraction(entries, free_bytes, lim);
    ++verdict_counts[static_cast<int>(want.verdict)];
    if (got.verdict != want.verdict || got.entry_index != want.index ||
        got.directories != want.dirs || got.total_bytes != want.total ||
        got.required_free_bytes != want.required) {
      std::string all;
      for (const EntryInfo& e : entries) all += e.name + " (" + std::to_string(e.uncompressed_bytes) + ") ";
      FAIL("mismatch at iteration " << iter << ": " << all << " want " << to_string(want.verdict)
                                    << "/" << want.index << " got " << to_string(got.verdict) << "/"
                                    << got.entry_index);
    }
    if (got.verdict == ExtractVerdict::Ok) {
      for (std::size_t completed : {std::size_t{0}, std::size_t{1}, std::size_t{3}, std::size_t{9}}) {
        for (bool partial : {false, true}) {
          REQUIRE(cleanup_paths(got, entries, completed, partial) ==
                  oracle_cleanup(want, entries, completed, partial));
        }
      }
    }
  }
  // The run must have exercised every verdict.
  for (std::size_t v = 0; v < 7; ++v) {
    INFO("verdict " << v << " seen " << verdict_counts[v] << " times");
    REQUIRE(verdict_counts[v] > 100);
  }
}
