// Tests for the package entry policy (core/package/entry_policy.h). Pure logic: no ZIP library,
// no files, no GPU. One rule per test case, with boundaries at the limit, limit + 1 and the
// uint64 maximum. See docs/package-policy.md for the rules and their order.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "entry_policy.h"

using namespace lumora::package;

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();

// Builds names with explicit bytes so hex escapes cannot swallow the next character.
std::string bytes(std::initializer_list<int> list) {
  std::string s;
  for (int b : list) s.push_back(static_cast<char>(b));
  return s;
}

EntryInfo entry(std::string name, std::uint64_t compressed = 1, std::uint64_t uncompressed = 1) {
  return EntryInfo{std::move(name), compressed, uncompressed};
}

Limits all_max() {
  Limits l;
  l.max_entries = kMax;
  l.max_name_bytes = kMax;
  l.max_entry_bytes = kMax;
  l.max_total_uncompressed_bytes = kMax;
  l.max_compression_ratio = kMax;
  return l;
}

const std::string kEAcute = bytes({0xC3, 0xA9});          // U+00E9
const std::string kEDecomposed = "e" + bytes({0xCC, 0x81});  // e + U+0301

}  // namespace

// ---- validate_name: one case per rule ----

TEST_CASE("package name: a plain relative path is accepted") {
  CHECK(validate_name("scene.json") == NameVerdict::Ok);
  CHECK(validate_name("textures/sky/day_01.ktx2") == NameVerdict::Ok);
  CHECK(validate_name("a..b/c") == NameVerdict::Ok);
}

TEST_CASE("package name: empty is rejected") {
  CHECK(validate_name("") == NameVerdict::Empty);
}

TEST_CASE("package name: absolute paths are rejected") {
  CHECK(validate_name("/etc/passwd") == NameVerdict::Absolute);
  CHECK(validate_name("//server/share/x") == NameVerdict::Absolute);
  CHECK(validate_name("C:/x") == NameVerdict::Absolute);
  CHECK(validate_name("z:x") == NameVerdict::Absolute);
}

TEST_CASE("package name: a leading backslash reports Backslash because that rule comes first") {
  CHECK(validate_name("\\x") == NameVerdict::Backslash);
  CHECK(validate_name("\\\\server\\share") == NameVerdict::Backslash);
}

TEST_CASE("package name: a colon after the first letter pair is not a drive letter") {
  CHECK(validate_name("a/b:c") == NameVerdict::Ok);  // open item, see docs/package-policy.md
  CHECK(validate_name("1:x") == NameVerdict::Ok);
}

TEST_CASE("package name: parent directory components are rejected") {
  CHECK(validate_name("..") == NameVerdict::Traversal);
  CHECK(validate_name("../a") == NameVerdict::Traversal);
  CHECK(validate_name("a/../b") == NameVerdict::Traversal);
  CHECK(validate_name("a/..") == NameVerdict::Traversal);
}

TEST_CASE("package name: backslashes are rejected anywhere") {
  CHECK(validate_name("a\\b") == NameVerdict::Backslash);
  CHECK(validate_name("a/b\\") == NameVerdict::Backslash);
}

TEST_CASE("package name: control characters are rejected") {
  CHECK(validate_name(std::string("a\0b", 3)) == NameVerdict::ControlCharacter);
  CHECK(validate_name("a\tb") == NameVerdict::ControlCharacter);
  CHECK(validate_name("a\nb") == NameVerdict::ControlCharacter);
  CHECK(validate_name("a\x1F" "b") == NameVerdict::ControlCharacter);
  CHECK(validate_name("a\x7F" "b") == NameVerdict::ControlCharacter);
}

TEST_CASE("package name: the byte just above the control range is accepted") {
  CHECK(validate_name("a b") == NameVerdict::Ok);  // 0x20
  CHECK(validate_name("a~b") == NameVerdict::Ok);  // 0x7E
}

TEST_CASE("package name: empty components are rejected") {
  CHECK(validate_name("a//b") == NameVerdict::EmptyComponent);
  CHECK(validate_name("a/") == NameVerdict::EmptyComponent);  // trailing slash (directory entry)
  CHECK(validate_name("a/b/") == NameVerdict::EmptyComponent);
}

TEST_CASE("package name: components ending in a dot or space are rejected") {
  CHECK(validate_name("a.") == NameVerdict::TrailingDotOrSpace);
  CHECK(validate_name("a ") == NameVerdict::TrailingDotOrSpace);
  CHECK(validate_name("dir./file") == NameVerdict::TrailingDotOrSpace);
  CHECK(validate_name("dir /file") == NameVerdict::TrailingDotOrSpace);
  CHECK(validate_name(".") == NameVerdict::TrailingDotOrSpace);
  CHECK(validate_name("a/./b") == NameVerdict::TrailingDotOrSpace);
}

TEST_CASE("package name: a leading dot is accepted") {
  CHECK(validate_name(".hidden") == NameVerdict::Ok);
  CHECK(validate_name("a/.hidden") == NameVerdict::Ok);
}

TEST_CASE("package name: Windows reserved names are rejected with or without extension") {
  for (const char* n : {"CON", "PRN", "AUX", "NUL", "COM1", "COM9", "LPT1", "LPT9", "con", "Nul", "cOm5",
                        "CON.txt", "nul.tar.gz", "a/CON", "a/lpt3.log", "dir/AUX/b"})
    CHECK(validate_name(n) == NameVerdict::WindowsReserved);
}

TEST_CASE("package name: reserved names ignore spaces before the extension dot") {
  CHECK(validate_name("CON .txt") == NameVerdict::WindowsReserved);
}

TEST_CASE("package name: near misses of reserved names are accepted") {
  for (const char* n : {"CONS", "CO", "COM0", "COM10", "LPT0", "COM", "LPT", "NULL", "xCON", "CON_x", "AUXILIARY.txt"})
    CHECK(validate_name(n) == NameVerdict::Ok);
}

TEST_CASE("package name: invalid UTF-8 is rejected") {
  CHECK(validate_name(bytes({0xFF})) == NameVerdict::InvalidUtf8);
  CHECK(validate_name(bytes({'a', 0x80})) == NameVerdict::InvalidUtf8);        // lone continuation
  CHECK(validate_name(bytes({0xC3})) == NameVerdict::InvalidUtf8);             // truncated
  CHECK(validate_name(bytes({0xC0, 0xAF})) == NameVerdict::InvalidUtf8);       // overlong '/'
  CHECK(validate_name(bytes({0xE0, 0x80, 0xAF})) == NameVerdict::InvalidUtf8); // overlong
  CHECK(validate_name(bytes({0xED, 0xA0, 0x80})) == NameVerdict::InvalidUtf8); // surrogate
  CHECK(validate_name(bytes({0xF4, 0x90, 0x80, 0x80})) == NameVerdict::InvalidUtf8);  // > U+10FFFF
  CHECK(validate_name(bytes({0xF5, 0x80, 0x80, 0x80})) == NameVerdict::InvalidUtf8);
}

TEST_CASE("package name: valid multi-byte UTF-8 is accepted") {
  CHECK(validate_name(kEAcute) == NameVerdict::Ok);
  CHECK(validate_name(bytes({0xE2, 0x82, 0xAC})) == NameVerdict::Ok);        // U+20AC
  CHECK(validate_name(bytes({0xF0, 0x9F, 0x98, 0x80})) == NameVerdict::Ok);  // U+1F600
  CHECK(validate_name(bytes({0xF4, 0x8F, 0xBF, 0xBF})) == NameVerdict::Ok);  // U+10FFFF
}

TEST_CASE("package name: length boundary is in bytes") {
  Limits l;
  l.max_name_bytes = 8;
  CHECK(validate_name(std::string(8, 'a'), l) == NameVerdict::Ok);
  CHECK(validate_name(std::string(9, 'a'), l) == NameVerdict::TooLong);
  // Four two-byte characters are 8 bytes, five are 10.
  std::string four, five;
  for (int i = 0; i < 4; ++i) four += kEAcute;
  five = four + kEAcute;
  CHECK(validate_name(four, l) == NameVerdict::Ok);
  CHECK(validate_name(five, l) == NameVerdict::TooLong);
}

TEST_CASE("package name: default limit is 255 bytes") {
  CHECK(validate_name(std::string(255, 'a')) == NameVerdict::Ok);
  CHECK(validate_name(std::string(256, 'a')) == NameVerdict::TooLong);
}

TEST_CASE("package name: a limit of uint64 max accepts a long name") {
  CHECK(validate_name(std::string(100000, 'a'), all_max()) == NameVerdict::Ok);
}

TEST_CASE("package name: a limit of zero rejects every non-empty name") {
  Limits l;
  l.max_name_bytes = 0;
  CHECK(validate_name("a", l) == NameVerdict::TooLong);
  CHECK(validate_name("", l) == NameVerdict::Empty);
}

TEST_CASE("package name: precedence when several rules apply") {
  CHECK(validate_name("") == NameVerdict::Empty);
  Limits l;
  l.max_name_bytes = 3;
  CHECK(validate_name(std::string("..\\\\x\0", 6), l) == NameVerdict::TooLong);         // length first
  CHECK(validate_name(std::string("a\0\\", 3), l) == NameVerdict::ControlCharacter);     // control before backslash
  CHECK(validate_name(bytes({'.', '.', 0xFF})) == NameVerdict::InvalidUtf8);             // UTF-8 before the rest
  CHECK(validate_name("../a\\b") == NameVerdict::Backslash);                             // backslash before traversal
  CHECK(validate_name("/../x") == NameVerdict::Absolute);                                // absolute before traversal
  CHECK(validate_name("a//../b") == NameVerdict::EmptyComponent);                        // empty before traversal
  CHECK(validate_name("../CON.") == NameVerdict::Traversal);                             // traversal before dot
  CHECK(validate_name("CON.") == NameVerdict::TrailingDotOrSpace);                       // dot before reserved
}

// ---- validate_name: hostile names, table driven ----

TEST_CASE("package name: hostile name table") {
  struct Row {
    std::string name;
    NameVerdict expect;
  };
  const std::vector<Row> rows = {
      {"../../etc/passwd", NameVerdict::Traversal},
      {"a/../../b", NameVerdict::Traversal},
      {"..", NameVerdict::Traversal},
      {"a/b/../../..", NameVerdict::Traversal},
      {"/absolute", NameVerdict::Absolute},
      {"/", NameVerdict::Absolute},
      {"//unc/share", NameVerdict::Absolute},
      {"C:/Windows/system32", NameVerdict::Absolute},
      {"c:evil", NameVerdict::Absolute},
      {"Z:", NameVerdict::Absolute},
      {"..\\..\\x", NameVerdict::Backslash},
      {"dir\\file", NameVerdict::Backslash},
      {"\\\\server\\share", NameVerdict::Backslash},
      {"C:\\x", NameVerdict::Backslash},
      {std::string("a\0b", 3), NameVerdict::ControlCharacter},
      {"line\nbreak", NameVerdict::ControlCharacter},
      {"tab\there", NameVerdict::ControlCharacter},
      {"bell\a", NameVerdict::ControlCharacter},
      {"esc\x1b[0m", NameVerdict::ControlCharacter},
      {"del\x7f", NameVerdict::ControlCharacter},
      {"a//b", NameVerdict::EmptyComponent},
      {"a///b", NameVerdict::EmptyComponent},
      {"dir/", NameVerdict::EmptyComponent},
      {"trailing.", NameVerdict::TrailingDotOrSpace},
      {"trailing ", NameVerdict::TrailingDotOrSpace},
      {"dir./file", NameVerdict::TrailingDotOrSpace},
      {"dir /file", NameVerdict::TrailingDotOrSpace},
      {"...", NameVerdict::TrailingDotOrSpace},
      {"a/./b", NameVerdict::TrailingDotOrSpace},
      {"CON", NameVerdict::WindowsReserved},
      {"con.txt", NameVerdict::WindowsReserved},
      {"PRN.png", NameVerdict::WindowsReserved},
      {"dir/AUX.lumora", NameVerdict::WindowsReserved},
      {"NUL.tar.gz", NameVerdict::WindowsReserved},
      {"COM1", NameVerdict::WindowsReserved},
      {"lpt9.x", NameVerdict::WindowsReserved},
      {"CON .txt", NameVerdict::WindowsReserved},
      {bytes({'a', 0xC0, 0xAF}), NameVerdict::InvalidUtf8},
      {bytes({0xED, 0xA0, 0x80}), NameVerdict::InvalidUtf8},
      {bytes({0xFF, 0xFE}), NameVerdict::InvalidUtf8},
      {"", NameVerdict::Empty},
      {std::string(300, 'x'), NameVerdict::TooLong},
  };
  REQUIRE(rows.size() >= 30);
  for (const Row& r : rows) {
    INFO("name bytes: " << r.name.size() << ", expected " << to_string(r.expect));
    CHECK(validate_name(r.name) == r.expect);
  }
}

// ---- validate_name: randomized, fixed seed ----

TEST_CASE("package name: random names never crash and never pass with control characters or dot-dot") {
  std::mt19937_64 rng(0x4C756D6F7261ull);  // fixed seed
  static const unsigned char alphabet[] = {'a', 'b', 'Z', '0', '.', '.', '/', '/', '\\', ' ', ':', 'C', 'O', 'N',
                                           0x00, 0x01, 0x1F, 0x7F, 0x80, 0xC3, 0xA9, 0xCC, 0x81, 0xE2, 0xFF};
  const std::size_t n_alpha = sizeof(alphabet);
  std::size_t accepted = 0;
  for (int iter = 0; iter < 20000; ++iter) {
    const std::size_t len = static_cast<std::size_t>(rng() % 24);
    std::string s;
    for (std::size_t k = 0; k < len; ++k) s.push_back(static_cast<char>(alphabet[rng() % n_alpha]));
    const NameVerdict v = validate_name(s);
    if (v != NameVerdict::Ok) continue;
    ++accepted;
    CHECK_FALSE(s.empty());
    for (char ch : s) {
      const auto c = static_cast<unsigned char>(ch);
      REQUIRE(c >= 0x20);
      REQUIRE(c != 0x7F);
      REQUIRE(ch != '\\');
    }
    // No component may be "..".
    std::size_t start = 0;
    while (true) {
      const std::size_t slash = s.find('/', start);
      const std::string comp = s.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
      REQUIRE(comp != "..");
      REQUIRE_FALSE(comp.empty());
      if (slash == std::string::npos) break;
      start = slash + 1;
    }
  }
  CHECK(accepted > 0);  // the generator does produce valid names, so the checks above ran
}

// ---- to_string ----

TEST_CASE("package to_string gives a distinct non-empty name for every verdict") {
  const NameVerdict names[] = {NameVerdict::Ok, NameVerdict::Empty, NameVerdict::Absolute, NameVerdict::Traversal,
                               NameVerdict::Backslash, NameVerdict::ControlCharacter, NameVerdict::TrailingDotOrSpace,
                               NameVerdict::WindowsReserved, NameVerdict::EmptyComponent, NameVerdict::InvalidUtf8,
                               NameVerdict::TooLong};
  for (std::size_t i = 0; i < std::size(names); ++i) {
    CHECK(std::string_view(to_string(names[i])).size() > 0);
    for (std::size_t j = i + 1; j < std::size(names); ++j)
      CHECK(std::string_view(to_string(names[i])) != std::string_view(to_string(names[j])));
  }
  const SetVerdict sets[] = {SetVerdict::Ok, SetVerdict::BadName, SetVerdict::TooManyEntries, SetVerdict::EntryTooLarge,
                             SetVerdict::TotalTooLarge, SetVerdict::RatioTooHigh, SetVerdict::SizeOverflow,
                             SetVerdict::DuplicateName, SetVerdict::CaseFoldCollision,
                             SetVerdict::NormalizationCollision};
  for (std::size_t i = 0; i < std::size(sets); ++i) {
    CHECK(std::string_view(to_string(sets[i])).size() > 0);
    for (std::size_t j = i + 1; j < std::size(sets); ++j)
      CHECK(std::string_view(to_string(sets[i])) != std::string_view(to_string(sets[j])));
  }
}

TEST_CASE("package to_string names match the enumerators") {
  CHECK(std::string_view(to_string(NameVerdict::Traversal)) == "Traversal");
  CHECK(std::string_view(to_string(SetVerdict::RatioTooHigh)) == "RatioTooHigh");
}

// ---- validate_entries ----

TEST_CASE("package entries: an empty set is accepted") {
  CHECK(validate_entries({}).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: a normal set is accepted") {
  SetResult r = validate_entries({entry("scene.json", 100, 400), entry("tex/a.ktx2", 1000, 1500)});
  CHECK(r.verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: entry count boundary") {
  Limits l;
  l.max_entries = 2;
  CHECK(validate_entries({entry("a"), entry("b")}, l).verdict == SetVerdict::Ok);
  SetResult r = validate_entries({entry("a"), entry("b"), entry("c")}, l);
  CHECK(r.verdict == SetVerdict::TooManyEntries);
  CHECK(r.first_index == 2);  // equals max_entries
}

TEST_CASE("package entries: a count limit of zero rejects the first entry") {
  Limits l;
  l.max_entries = 0;
  SetResult r = validate_entries({entry("a")}, l);
  CHECK(r.verdict == SetVerdict::TooManyEntries);
  CHECK(r.first_index == 0);
  CHECK(validate_entries({}, l).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: a count limit of uint64 max accepts a normal set") {
  CHECK(validate_entries({entry("a"), entry("b")}, all_max()).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: a bad name reports its index and the name verdict") {
  SetResult r = validate_entries({entry("ok"), entry("../x")});
  CHECK(r.verdict == SetVerdict::BadName);
  CHECK(r.first_index == 1);
  CHECK(r.name_verdict == NameVerdict::Traversal);
}

TEST_CASE("package entries: name limits come from the same Limits") {
  Limits l;
  l.max_name_bytes = 3;
  SetResult r = validate_entries({entry("abcd")}, l);
  CHECK(r.verdict == SetVerdict::BadName);
  CHECK(r.name_verdict == NameVerdict::TooLong);
}

TEST_CASE("package entries: per-entry size boundary") {
  Limits l;
  l.max_entry_bytes = 1000;
  l.max_compression_ratio = kMax;
  CHECK(validate_entries({entry("a", 1000, 1000)}, l).verdict == SetVerdict::Ok);
  SetResult r = validate_entries({entry("a", 1, 1), entry("b", 1001, 1001)}, l);
  CHECK(r.verdict == SetVerdict::EntryTooLarge);
  CHECK(r.first_index == 1);
}

TEST_CASE("package entries: per-entry size at uint64 max") {
  Limits l;  // default max_entry_bytes
  CHECK(validate_entries({entry("a", kMax, kMax)}, l).verdict == SetVerdict::EntryTooLarge);
  CHECK(validate_entries({entry("a", kMax, kMax)}, all_max()).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: compression ratio boundary") {
  Limits l;  // ratio 200
  CHECK(validate_entries({entry("a", 10, 2000)}, l).verdict == SetVerdict::Ok);  // equality passes
  SetResult r = validate_entries({entry("a", 10, 2001)}, l);
  CHECK(r.verdict == SetVerdict::RatioTooHigh);
  CHECK(r.first_index == 0);
}

TEST_CASE("package entries: zero compressed bytes") {
  CHECK(validate_entries({entry("a", 0, 0)}).verdict == SetVerdict::Ok);
  CHECK(validate_entries({entry("a", 0, 1)}).verdict == SetVerdict::RatioTooHigh);
}

TEST_CASE("package entries: ratio product does not overflow") {
  Limits l;
  l.max_entry_bytes = kMax;
  l.max_total_uncompressed_bytes = kMax;
  // 200 * kMax overflows 64 bits; the entry is still within the ratio.
  CHECK(validate_entries({entry("a", kMax, kMax)}, l).verdict == SetVerdict::Ok);
  CHECK(validate_entries({entry("a", kMax / 2 + 1, kMax)}, l).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: a ratio limit of zero rejects any data from a non-empty stream") {
  Limits l;
  l.max_compression_ratio = 0;
  CHECK(validate_entries({entry("a", 5, 1)}, l).verdict == SetVerdict::RatioTooHigh);
  CHECK(validate_entries({entry("a", 5, 0)}, l).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: a ratio limit of uint64 max accepts a large ratio") {
  Limits l = all_max();
  CHECK(validate_entries({entry("a", 1, 1000000)}, l).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: running total boundary") {
  Limits l;
  l.max_total_uncompressed_bytes = 100;
  l.max_compression_ratio = kMax;
  CHECK(validate_entries({entry("a", 1, 60), entry("b", 1, 40)}, l).verdict == SetVerdict::Ok);
  SetResult r = validate_entries({entry("a", 1, 60), entry("b", 1, 41)}, l);
  CHECK(r.verdict == SetVerdict::TotalTooLarge);
  CHECK(r.first_index == 1);
}

TEST_CASE("package entries: two entries at uint64 max cap pass and fail on overflow") {
  Limits l = all_max();
  CHECK(validate_entries({entry("a", 1, kMax)}, l).verdict == SetVerdict::Ok);  // one max entry passes
  SetResult r = validate_entries({entry("a", 1, kMax), entry("b", 1, 1)}, l);
  CHECK(r.verdict == SetVerdict::SizeOverflow);
  CHECK(r.first_index == 1);
  CHECK(validate_entries({entry("a", 1, kMax), entry("b", 1, 0)}, l).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: a sum that would overflow is reported before the total comparison") {
  Limits l = all_max();
  l.max_total_uncompressed_bytes = 100;
  CHECK(validate_entries({entry("a", 1, 50), entry("b", 1, kMax)}, l).verdict == SetVerdict::SizeOverflow);
  // Without an overflow the same cap gives TotalTooLarge.
  CHECK(validate_entries({entry("a", 1, 50), entry("b", 1, 51)}, l).verdict == SetVerdict::TotalTooLarge);
}

TEST_CASE("package entries: a first entry above the total cap is TotalTooLarge, not overflow") {
  Limits l = all_max();
  l.max_total_uncompressed_bytes = 10;
  SetResult r = validate_entries({entry("a", 1, kMax)}, l);
  CHECK(r.verdict == SetVerdict::TotalTooLarge);
  CHECK(r.first_index == 0);
}

TEST_CASE("package entries: check order is name, size, ratio, then total") {
  Limits l;
  l.max_entry_bytes = 100;
  l.max_total_uncompressed_bytes = 100;
  l.max_compression_ratio = 2;
  // Fails name, size, ratio at once: name wins.
  CHECK(validate_entries({entry("../x", 1, 1000)}, l).verdict == SetVerdict::BadName);
  // Fails size and ratio: size wins.
  CHECK(validate_entries({entry("a", 1, 1000)}, l).verdict == SetVerdict::EntryTooLarge);
  // Fails ratio only.
  CHECK(validate_entries({entry("a", 1, 50)}, l).verdict == SetVerdict::RatioTooHigh);
}

TEST_CASE("package entries: the first failing entry is reported, not the worst") {
  Limits l;
  l.max_entry_bytes = 100;
  SetResult r = validate_entries({entry("a"), entry("b", 1, 101), entry("../c")}, l);
  CHECK(r.verdict == SetVerdict::EntryTooLarge);
  CHECK(r.first_index == 1);
}

TEST_CASE("package entries: exact duplicate names are rejected") {
  SetResult r = validate_entries({entry("a"), entry("b"), entry("a")});
  CHECK(r.verdict == SetVerdict::DuplicateName);
  CHECK(r.first_index == 2);
  CHECK(r.other_index == 0);
}

TEST_CASE("package entries: ASCII case-fold collisions are rejected") {
  SetResult r = validate_entries({entry("dir/File.TXT"), entry("x"), entry("DIR/file.txt")});
  CHECK(r.verdict == SetVerdict::CaseFoldCollision);
  CHECK(r.first_index == 2);
  CHECK(r.other_index == 0);
}

TEST_CASE("package entries: names that differ in more than ASCII case are accepted") {
  CHECK(validate_entries({entry("a"), entry("b"), entry("ab")}).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: decomposed and precomposed e-acute collide") {
  SetResult r = validate_entries({entry("caf" + kEAcute), entry("x"), entry("caf" + kEDecomposed)});
  CHECK(r.verdict == SetVerdict::NormalizationCollision);
  CHECK(r.first_index == 2);
  CHECK(r.other_index == 0);
  // Order of the two forms does not matter.
  SetResult r2 = validate_entries({entry("caf" + kEDecomposed), entry("caf" + kEAcute)});
  CHECK(r2.verdict == SetVerdict::NormalizationCollision);
  CHECK(r2.other_index == 0);
}

TEST_CASE("package entries: each supported mark folds with its letter") {
  struct Pair {
    std::string base;
    std::string mark;
    std::string composed;
  };
  const std::vector<Pair> pairs = {
      {"A", bytes({0xCC, 0x80}), bytes({0xC3, 0x80})},  // A grave
      {"a", bytes({0xCC, 0x81}), bytes({0xC3, 0xA1})},  // a acute
      {"O", bytes({0xCC, 0x82}), bytes({0xC3, 0x94})},  // O circumflex
      {"n", bytes({0xCC, 0x83}), bytes({0xC3, 0xB1})},  // n tilde
      {"u", bytes({0xCC, 0x88}), bytes({0xC3, 0xBC})},  // u diaeresis
      {"a", bytes({0xCC, 0x8A}), bytes({0xC3, 0xA5})},  // a ring
      {"c", bytes({0xCC, 0xA7}), bytes({0xC3, 0xA7})},  // c cedilla
  };
  for (const Pair& p : pairs) {
    SetResult r = validate_entries({entry(p.composed), entry(p.base + p.mark)});
    INFO("base " << p.base);
    CHECK(r.verdict == SetVerdict::NormalizationCollision);
  }
}

TEST_CASE("package entries: a mark that is not supported does not fold") {
  // e + U+0304 (macron) has a precomposed form outside the supported table, so no collision here.
  const std::string macron = bytes({0xCC, 0x84});
  CHECK(validate_entries({entry("e" + macron), entry(bytes({0xC4, 0x93}))}).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: a letter and mark pair outside the table does not fold") {
  // k + U+0301 has no precomposed form in Latin-1, so it is distinct from plain k.
  CHECK(validate_entries({entry("k"), entry("k" + bytes({0xCC, 0x81}))}).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: accented letters are not case folded (open item)") {
  const std::string upper = bytes({0xC3, 0x89});  // U+00C9
  CHECK(validate_entries({entry(upper), entry(kEAcute)}).verdict == SetVerdict::Ok);
}

TEST_CASE("package entries: duplicate and case-fold collisions are told apart") {
  CHECK(validate_entries({entry("a"), entry("a")}).verdict == SetVerdict::DuplicateName);
  CHECK(validate_entries({entry("a"), entry("A")}).verdict == SetVerdict::CaseFoldCollision);
}

TEST_CASE("package entries: collisions are reported at the first offending entry") {
  SetResult r = validate_entries({entry("x"), entry("X"), entry("x")});
  CHECK(r.verdict == SetVerdict::CaseFoldCollision);  // entry 1 already collides with entry 0
  CHECK(r.first_index == 1);
  CHECK(r.other_index == 0);
}

TEST_CASE("package entries: other_index is the earliest match") {
  SetResult r = validate_entries({entry("a"), entry("b"), entry("c"), entry("b")});
  CHECK(r.verdict == SetVerdict::DuplicateName);
  CHECK(r.other_index == 1);
}

TEST_CASE("package entries: size checks run before collision checks") {
  Limits l;
  l.max_entry_bytes = 10;
  SetResult r = validate_entries({entry("a"), entry("a", 1, 11)}, l);
  CHECK(r.verdict == SetVerdict::EntryTooLarge);
  CHECK(r.first_index == 1);
}
