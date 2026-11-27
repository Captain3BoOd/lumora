// Wildcard matching of byte strings ('?', '*' and backslash escapes); see globmatch.h for the contract and docs/globmatch.md for the check
// order, the time bound and why it holds. Integer only, includes <cstdint> only, no allocation, no recursion, no global state.
//
// Greedy matching with ONE saved star: the pattern is read as tokens (a '*', a '?', a literal byte, or a backslash pair that is one literal
// byte). On a mismatch the matcher returns to the most recent '*' and lets it take one more text byte. Only the latest star is ever saved,
// because any match of the rest of the pattern that starts at an earlier star can be moved to the latest one (see the docs).
#include "globmatch.h"

#include <cstdint>

namespace lumora::globmatch {

GlobResult glob_match(const char* pattern, std::uint32_t pattern_len, const char* text, std::uint32_t text_len, bool* out_match) {
  if ((pattern == nullptr && pattern_len > 0u) || (text == nullptr && text_len > 0u) || out_match == nullptr) return GlobResult::NullArgument;
  if (pattern_len > kMaxLen || text_len > kMaxLen) return GlobResult::TooLarge;

  // A trailing lone backslash is an odd run of backslashes at the very end (the first byte of such a run is never itself escaped).
  std::uint32_t run = 0;
  while (run < pattern_len && pattern[pattern_len - 1u - run] == '\\') ++run;
  if ((run & 1u) != 0u) return GlobResult::BadPattern;

  constexpr std::uint32_t kNone = 0xFFFFFFFFu;
  std::uint32_t pi = 0;         // pattern position, always at a token boundary
  std::uint32_t ti = 0;         // text position
  std::uint32_t star_next = kNone;  // pattern position just after the latest '*'
  std::uint32_t star_text = 0;      // text position the latest '*' has consumed up to
  while (ti < text_len) {
    if (pi < pattern_len) {
      const unsigned char c = static_cast<unsigned char>(pattern[pi]);
      if (c == static_cast<unsigned char>('*')) {
        ++pi;
        star_next = pi;
        star_text = ti;
        continue;
      }
      std::uint32_t next = pi + 1u;
      bool hit = false;
      if (c == static_cast<unsigned char>('?')) {
        hit = true;
      } else if (c == static_cast<unsigned char>('\\')) {
        next = pi + 2u;  // the pattern was checked, so a byte follows
        hit = static_cast<unsigned char>(pattern[pi + 1u]) == static_cast<unsigned char>(text[ti]);
      } else {
        hit = c == static_cast<unsigned char>(text[ti]);
      }
      if (hit) {
        pi = next;
        ++ti;
        continue;
      }
    }
    if (star_next == kNone) {
      *out_match = false;
      return GlobResult::Ok;
    }
    ++star_text;  // the star takes one more byte
    ti = star_text;
    pi = star_next;
  }
  while (pi < pattern_len && pattern[pi] == '*') ++pi;
  *out_match = pi == pattern_len;
  return GlobResult::Ok;
}

}  // namespace lumora::globmatch
