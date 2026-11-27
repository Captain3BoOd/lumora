// Binary min-heap of (key, value) pairs in caller arrays; see minheap.h for the contract and docs/minheap.md for the check order and bounds.
// Pairs compare lexicographically (key, then value). The heap is the usual array layout (children of i are 2i+1 and 2i+2); push sifts up and
// pop sifts down with a hole, so each moved pair is written once. Writes happen only at indexes below the larger of the old and the new size.
// No floating point, no libm, no allocation, no recursion, no global state.
#include "minheap.h"

#include <cstdint>

namespace lumora::minheap {

namespace {

bool less(std::uint32_t ka, std::uint32_t va, std::uint32_t kb, std::uint32_t vb) { return ka < kb || (ka == kb && va < vb); }

// Argument checks shared by the three functions, in the order of the header. null_arg is true when any pointer argument is null.
HeapResult check_state(bool null_arg, std::uint32_t size, std::uint32_t capacity) {
  if (null_arg) return HeapResult::NullArgument;
  if (capacity > kMaxCapacity) return HeapResult::TooLarge;
  if (size > capacity) return HeapResult::BadSize;
  return HeapResult::Ok;
}

}  // namespace

HeapResult heap_push(std::uint32_t* keys, std::uint32_t* values, std::uint32_t* size, std::uint32_t capacity, std::uint32_t key,
                     std::uint32_t value) {
  const HeapResult st = check_state(keys == nullptr || values == nullptr || size == nullptr, size == nullptr ? 0u : *size, capacity);
  if (st != HeapResult::Ok) return st;
  std::uint32_t i = *size;
  if (i == capacity) return HeapResult::Full;
  while (i > 0u) {
    const std::uint32_t parent = (i - 1u) / 2u;
    if (!less(key, value, keys[parent], values[parent])) break;
    keys[i] = keys[parent];
    values[i] = values[parent];
    i = parent;
  }
  keys[i] = key;
  values[i] = value;
  *size += 1u;
  return HeapResult::Ok;
}

HeapResult heap_pop_min(std::uint32_t* keys, std::uint32_t* values, std::uint32_t* size, std::uint32_t capacity, std::uint32_t* out_key,
                        std::uint32_t* out_value) {
  const HeapResult st = check_state(keys == nullptr || values == nullptr || size == nullptr || out_key == nullptr || out_value == nullptr,
                                    size == nullptr ? 0u : *size, capacity);
  if (st != HeapResult::Ok) return st;
  if (*size == 0u) return HeapResult::Empty;
  const std::uint32_t top_key = keys[0];
  const std::uint32_t top_value = values[0];
  const std::uint32_t n = *size - 1u;  // the new size; the last pair is moved into the hole
  if (n > 0u) {
    const std::uint32_t last_key = keys[n];
    const std::uint32_t last_value = values[n];
    std::uint32_t i = 0;
    for (;;) {
      std::uint32_t child = 2u * i + 1u;
      if (child >= n) break;
      if (child + 1u < n && less(keys[child + 1u], values[child + 1u], keys[child], values[child])) child += 1u;
      if (!less(keys[child], values[child], last_key, last_value)) break;
      keys[i] = keys[child];
      values[i] = values[child];
      i = child;
    }
    keys[i] = last_key;
    values[i] = last_value;
  }
  *size = n;
  *out_key = top_key;
  *out_value = top_value;
  return HeapResult::Ok;
}

HeapResult heap_peek_min(const std::uint32_t* keys, const std::uint32_t* values, const std::uint32_t* size, std::uint32_t capacity,
                         std::uint32_t* out_key, std::uint32_t* out_value) {
  const HeapResult st = check_state(keys == nullptr || values == nullptr || size == nullptr || out_key == nullptr || out_value == nullptr,
                                    size == nullptr ? 0u : *size, capacity);
  if (st != HeapResult::Ok) return st;
  if (*size == 0u) return HeapResult::Empty;
  *out_key = keys[0];
  *out_value = values[0];
  return HeapResult::Ok;
}

}  // namespace lumora::minheap
