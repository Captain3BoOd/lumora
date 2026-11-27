// Rank and select over a bit vector of 64-bit words; see bitrank.h for the contract and docs/bitrank.md for the directory layout, the check
// order and the algorithms. Integer only, includes <cstdint> only, no builtin or intrinsic (the popcount is the SWAR sequence below), no
// allocation, no recursion, no global state.
//
// Directory layout (internal, never part of the contract): dir[w] is the number of set bits in the words 0 .. w-1, with the bits of the last
// word at positions nbits and above left out, for w = 0 .. words, so dir[words] is the total and the directory has words + 1 entries. Every
// entry is at most kMaxBits, so it fits a u32.
#include "bitrank.h"

#include <cstdint>

namespace lumora::bitrank {

namespace {

// Number of set bits of x. Pairs, then nibbles, then bytes are summed in place, and the multiplication adds the 8 byte counts into the top
// byte (an unsigned multiplication wraps modulo 2^64, which is what this needs).
std::uint32_t popcount64(std::uint64_t x) {
  x = x - ((x >> 1) & 0x5555555555555555ull);
  x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
  x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full;
  return static_cast<std::uint32_t>((x * 0x0101010101010101ull) >> 56);
}

std::uint32_t word_count(std::uint32_t nbits) { return (nbits + 63u) / 64u; }

// Word w of the vector with the positions at nbits and above cleared (only the last word can have any).
std::uint64_t vector_word(const std::uint64_t* bits, std::uint32_t nbits, std::uint32_t w) {
  std::uint64_t x = bits[w];
  const std::uint32_t tail = nbits % 64u;
  if (tail != 0u && w == word_count(nbits) - 1u) x &= (1ull << tail) - 1u;
  return x;
}

bool bad_pointers(const std::uint64_t* bits, std::uint32_t nbits, const void* dir) { return (bits == nullptr && nbits > 0u) || dir == nullptr; }

}  // namespace

BitResult bitrank_dir_words(std::uint32_t nbits, std::uint32_t* out_words) {
  if (out_words == nullptr) return BitResult::NullArgument;
  if (nbits > kMaxBits) return BitResult::TooLarge;
  *out_words = word_count(nbits) + 1u;
  return BitResult::Ok;
}

BitResult bitrank_build(const std::uint64_t* bits, std::uint32_t nbits, std::uint32_t* dir, std::uint32_t dir_words) {
  if (bad_pointers(bits, nbits, dir)) return BitResult::NullArgument;
  if (nbits > kMaxBits) return BitResult::TooLarge;
  const std::uint32_t words = word_count(nbits);
  if (dir_words < words + 1u) return BitResult::ScratchTooSmall;
  std::uint32_t total = 0;
  for (std::uint32_t w = 0; w < words; ++w) {
    dir[w] = total;
    total += popcount64(vector_word(bits, nbits, w));
  }
  dir[words] = total;
  return BitResult::Ok;
}

BitResult bitrank_rank1(const std::uint64_t* bits, std::uint32_t nbits, const std::uint32_t* dir, std::uint32_t dir_words, std::uint32_t i,
                        std::uint32_t* out) {
  if (bad_pointers(bits, nbits, dir) || out == nullptr) return BitResult::NullArgument;
  if (nbits > kMaxBits) return BitResult::TooLarge;
  const std::uint32_t words = word_count(nbits);
  if (dir_words < words + 1u) return BitResult::ScratchTooSmall;
  if (i > nbits) return BitResult::OutOfRange;
  const std::uint32_t w = i / 64u;
  const std::uint32_t r = i % 64u;
  std::uint32_t result = dir[w];
  // r > 0 means i < words * 64, so w is a real word; its bits below r are all inside the vector because r <= nbits - 64 * w.
  if (r != 0u) result += popcount64(bits[w] & ((1ull << r) - 1u));
  *out = result;
  return BitResult::Ok;
}

BitResult bitrank_select1(const std::uint64_t* bits, std::uint32_t nbits, const std::uint32_t* dir, std::uint32_t dir_words, std::uint32_t k,
                          std::uint32_t* out_pos) {
  if (bad_pointers(bits, nbits, dir) || out_pos == nullptr) return BitResult::NullArgument;
  if (nbits > kMaxBits) return BitResult::TooLarge;
  const std::uint32_t words = word_count(nbits);
  if (dir_words < words + 1u) return BitResult::ScratchTooSmall;
  if (k >= dir[words]) return BitResult::OutOfRange;
  // Binary search for the word: the largest lo below words with dir[lo] <= k. Invariant: dir[lo] <= k < dir[hi].
  std::uint32_t lo = 0;
  std::uint32_t hi = words;
  while (hi - lo > 1u) {
    const std::uint32_t mid = lo + (hi - lo) / 2u;
    if (dir[mid] <= k) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  // The (k - dir[lo])-th set bit of that word (counting from 0): drop that many lowest set bits, then find the lowest one left.
  std::uint64_t x = vector_word(bits, nbits, lo);
  for (std::uint32_t skip = k - dir[lo]; skip > 0u; --skip) x &= x - 1u;
  std::uint32_t pos = 0;
  while ((x & 1u) == 0u) {
    x >>= 1;
    ++pos;
  }
  *out_pos = lo * 64u + pos;
  return BitResult::Ok;
}

}  // namespace lumora::bitrank
