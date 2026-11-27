// Monotonic stack and deque queries: nearest smaller neighbours, the largest rectangle under a histogram and the leftmost minimum of every sliding
// window. See monostack.h for the contract and docs/monostack.md for the methods, the check orders and the exact push and pop counts.
//
// All three runners keep one array of indices in the caller's scratch (n words). Every index is pushed once and popped at most once.
#include "monostack.h"

namespace lumora::monostack {

StackResult monostack_scratch_words(std::uint32_t n, std::uint32_t* out_words) {
  if (out_words == nullptr) return StackResult::NullArgument;
  if (n > kMaxN) return StackResult::TooLarge;
  *out_words = n;
  return StackResult::Ok;
}

StackResult monostack_bounds(const std::uint32_t* a, std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* left,
                             std::uint32_t* right) {
  if (n > 0 && (a == nullptr || scratch == nullptr || left == nullptr || right == nullptr)) return StackResult::NullArgument;
  if (n > kMaxN) return StackResult::TooLarge;
  if (n > 0 && scratch_words < n) return StackResult::ScratchTooSmall;
  // The stack holds indices with strictly increasing values. An index is popped by the first later element that is smaller or equal, which is its
  // right bound; after the pops the top is the nearest earlier index with a strictly smaller value, which is the left bound of the new element.
  std::uint32_t top = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    while (top > 0 && a[scratch[top - 1]] >= a[i]) {
      right[scratch[top - 1]] = i;
      --top;
    }
    left[i] = top > 0 ? scratch[top - 1] : kNone;
    scratch[top++] = i;
  }
  while (top > 0) {
    right[scratch[top - 1]] = kNone;
    --top;
  }
  return StackResult::Ok;
}

StackResult monostack_max_rect(const std::uint32_t* a, std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint64_t* out_area,
                               std::uint32_t* out_l, std::uint32_t* out_r) {
  if (out_area == nullptr || out_l == nullptr || out_r == nullptr || (n > 0 && (a == nullptr || scratch == nullptr))) return StackResult::NullArgument;
  if (n > kMaxN) return StackResult::TooLarge;
  if (n > 0 && scratch_words < n) return StackResult::ScratchTooSmall;
  std::uint64_t best = 0;
  std::uint32_t best_l = 0, best_r = 0;
  std::uint32_t top = 0;
  // The pass runs to i = n, where every remaining bar is popped. A bar popped at i has the range (below + 1) .. (i - 1): the bar below it on the
  // stack is strictly smaller, and a[i] is smaller or equal (or i = n). Of several equal bars the last one gets the widest range.
  for (std::uint32_t i = 0; i <= n; ++i) {
    while (top > 0 && (i == n || a[scratch[top - 1]] >= a[i])) {
      const std::uint32_t bar = scratch[--top];
      const std::uint32_t l = top > 0 ? scratch[top - 1] + 1 : 0;
      const std::uint32_t r = i - 1;
      const std::uint64_t area = (static_cast<std::uint64_t>(r - l) + 1u) * static_cast<std::uint64_t>(a[bar]);
      if (area > best || (area == best && (l < best_l || (l == best_l && r < best_r)))) {
        best = area;
        best_l = l;
        best_r = r;
      }
    }
    if (i < n) scratch[top++] = i;
  }
  *out_area = best;
  *out_l = best_l;
  *out_r = best_r;
  return StackResult::Ok;
}

StackResult monostack_window_min(const std::uint32_t* a, std::uint32_t n, std::uint32_t k, std::uint32_t* scratch, std::uint32_t scratch_words,
                                 std::uint32_t* out) {
  if (n > 0 && (a == nullptr || scratch == nullptr || out == nullptr)) return StackResult::NullArgument;
  if (n > kMaxN) return StackResult::TooLarge;
  if (n > 0 && (k == 0 || k > n)) return StackResult::BadWindow;
  if (n > 0 && scratch_words < n) return StackResult::ScratchTooSmall;
  // The deque is scratch[head .. tail): indices with strictly increasing values, oldest first. A new element removes every larger value from the back
  // (equal values stay, so the leftmost minimum stays in front); the front leaves when it is k or more positions behind.
  std::uint32_t head = 0, tail = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    while (tail > head && a[scratch[tail - 1]] > a[i]) --tail;
    scratch[tail++] = i;
    if (i - scratch[head] >= k) ++head;
    if (i + 1 >= k) out[i + 1 - k] = scratch[head];
  }
  return StackResult::Ok;
}

}  // namespace lumora::monostack
