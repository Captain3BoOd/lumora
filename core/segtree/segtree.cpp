// Range add and range sum over u64 values (wrapping); see segtree.h for the contract and docs/segtree.md for the layout, the check order and
// the argument for the storage size and the O(log n) bound. Integer only, includes <cstdint> only, no builtin or intrinsic, no allocation, no
// recursion (the helpers below are plain loops), no global state.
//
// Layout (internal, never part of the contract): size is the smallest power of two that is at least n (size >= 1 for n >= 1), log2(size)
// levels. The storage holds sum[0 .. 2 * size) followed by lazy[0 .. size), 3 * size words in all. sum[k] is the sum of the elements under
// node k with every tag ABOVE k still pending; leaf size + i is element i, node k covers (size >> floor(log2 k)) leaves, and lazy[k] is the
// delta not yet pushed to the two children of k. Because size < 2 * n for n >= 1, 3 * size < 6 * n, which fits the 8 * n words of the header.
#include "segtree.h"

#include <cstdint>

namespace lumora::segtree {

namespace {

constexpr std::uint64_t kWordsPerElement = 8;

// Sizes of the tree for n >= 1: levels = ceil(log2 n), size = 2^levels.
struct Shape {
  std::uint32_t levels;
  std::uint32_t size;
};

Shape shape_of(std::uint32_t n) {
  Shape s{0u, 1u};
  while (s.size < n) {
    s.size <<= 1;
    ++s.levels;
  }
  return s;
}

// A tree view over the caller's storage.
struct Tree {
  std::uint64_t* sum;
  std::uint64_t* lazy;
  std::uint32_t size;
  std::uint32_t levels;
};

Tree view(std::uint64_t* storage, std::uint32_t n) {
  const Shape s = shape_of(n);
  return Tree{storage, storage + 2u * static_cast<std::uint64_t>(s.size), s.size, s.levels};
}

// Adds delta to every element under node k, which covers len leaves.
void apply(const Tree& t, std::uint32_t k, std::uint64_t delta, std::uint64_t len) {
  t.sum[k] += delta * len;
  if (k < t.size) t.lazy[k] += delta;
}

// Pushes the pending delta of node k (covering len leaves) to its two children.
void push(const Tree& t, std::uint32_t k, std::uint64_t len) {
  const std::uint64_t d = t.lazy[k];
  if (d != 0u) {
    apply(t, 2u * k, d, len / 2u);
    apply(t, 2u * k + 1u, d, len / 2u);
    t.lazy[k] = 0u;
  }
}

void pull(const Tree& t, std::uint32_t k) { t.sum[k] = t.sum[2u * k] + t.sum[2u * k + 1u]; }

// Pushes down every pending delta on the paths from the root to the leaves l and r (leaf indices already offset by size; r is exclusive).
void push_paths(const Tree& t, std::uint32_t l, std::uint32_t r) {
  for (std::uint32_t i = t.levels; i >= 1u; --i) {
    if (((l >> i) << i) != l) push(t, l >> i, std::uint64_t{1} << i);
    if (((r >> i) << i) != r) push(t, (r - 1u) >> i, std::uint64_t{1} << i);
  }
}

bool null_storage(const std::uint64_t* storage, std::uint32_t n) { return storage == nullptr && n > 0u; }

// The size checks shared by every function after the null checks: TooLarge, then ScratchTooSmall.
SegResult size_checks(std::uint32_t n, std::uint32_t storage_words) {
  if (n > kMaxLen) return SegResult::TooLarge;
  if (static_cast<std::uint64_t>(storage_words) < kWordsPerElement * n) return SegResult::ScratchTooSmall;
  return SegResult::Ok;
}

}  // namespace

SegResult seg_storage_words(std::uint32_t n, std::uint32_t* out_words) {
  if (out_words == nullptr) return SegResult::NullArgument;
  if (n > kMaxLen) return SegResult::TooLarge;
  *out_words = static_cast<std::uint32_t>(kWordsPerElement * n);
  return SegResult::Ok;
}

SegResult seg_init(const std::uint64_t* values, std::uint32_t n, std::uint64_t* storage, std::uint32_t storage_words) {
  if ((values == nullptr && n > 0u) || null_storage(storage, n)) return SegResult::NullArgument;
  const SegResult sc = size_checks(n, storage_words);
  if (sc != SegResult::Ok) return sc;
  if (n == 0u) return SegResult::Ok;
  const Tree t = view(storage, n);
  t.sum[0] = 0u;
  for (std::uint32_t i = 0; i < t.size; ++i) {
    t.sum[t.size + i] = i < n ? values[i] : 0u;
    t.lazy[i] = 0u;
  }
  for (std::uint32_t k = t.size - 1u; k >= 1u; --k) pull(t, k);
  return SegResult::Ok;
}

SegResult seg_range_add(std::uint64_t* storage, std::uint32_t storage_words, std::uint32_t n, std::uint32_t l, std::uint32_t r, std::uint64_t delta) {
  if (null_storage(storage, n)) return SegResult::NullArgument;
  const SegResult sc = size_checks(n, storage_words);
  if (sc != SegResult::Ok) return sc;
  if (l > r || r > n) return SegResult::OutOfRange;
  if (l == r) return SegResult::Ok;
  const Tree t = view(storage, n);
  const std::uint32_t lo = l + t.size;
  const std::uint32_t hi = r + t.size;
  push_paths(t, lo, hi);
  {
    std::uint32_t a = lo;
    std::uint32_t b = hi;
    std::uint64_t len = 1u;
    while (a < b) {
      if ((a & 1u) != 0u) apply(t, a++, delta, len);
      if ((b & 1u) != 0u) apply(t, --b, delta, len);
      a >>= 1;
      b >>= 1;
      len <<= 1;
    }
  }
  for (std::uint32_t i = 1; i <= t.levels; ++i) {
    if (((lo >> i) << i) != lo) pull(t, lo >> i);
    if (((hi >> i) << i) != hi) pull(t, (hi - 1u) >> i);
  }
  return SegResult::Ok;
}

SegResult seg_range_sum(std::uint64_t* storage, std::uint32_t storage_words, std::uint32_t n, std::uint32_t l, std::uint32_t r, std::uint64_t* out) {
  if (out == nullptr || null_storage(storage, n)) return SegResult::NullArgument;
  const SegResult sc = size_checks(n, storage_words);
  if (sc != SegResult::Ok) return sc;
  if (l > r || r > n) return SegResult::OutOfRange;
  if (l == r) {
    *out = 0u;
    return SegResult::Ok;
  }
  const Tree t = view(storage, n);
  const std::uint32_t lo = l + t.size;
  const std::uint32_t hi = r + t.size;
  push_paths(t, lo, hi);
  std::uint64_t total = 0u;
  std::uint32_t a = lo;
  std::uint32_t b = hi;
  while (a < b) {
    if ((a & 1u) != 0u) total += t.sum[a++];
    if ((b & 1u) != 0u) total += t.sum[--b];
    a >>= 1;
    b >>= 1;
  }
  *out = total;
  return SegResult::Ok;
}

}  // namespace lumora::segtree
