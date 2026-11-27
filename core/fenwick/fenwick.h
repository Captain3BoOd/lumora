// Fenwick tree (binary indexed tree) over a caller array of int64 values: point add, prefix and range sums, and a search for the first prefix
// that reaches a target. Pure integer: no floating point, no libm, no 128-bit types or builtins (MSVC), no allocation, no global state.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::fenwick {

constexpr std::uint32_t kMaxSize = 1u << 24;  // largest element count

// The caller supplies tree[0 .. n). n is the element count and is valid when it is at most kMaxSize and tree is non-null whenever n is
// above 0; n 0 is a valid empty tree and tree may be null. The contents of tree are an implementation layout: after init, only these
// functions may write it. Elements are indexed 0 .. n - 1. An invalid argument (invalid n, null out, an index or range outside the tree)
// makes the function return false and change nothing, including *out.
//
// Arithmetic wraps: every element and every sum is taken modulo 2^64 and read back as a signed value (two's complement), so add never
// fails because of overflow and a sum that leaves the int64 range wraps. This is a decision, not an error path.

// Sets every element to 0. False when n is invalid.
bool init(std::int64_t* tree, std::uint32_t n);

// Adds delta (wrapping) to element i. False when n is invalid or i >= n.
bool add(std::int64_t* tree, std::uint32_t n, std::uint32_t i, std::int64_t delta);

// Sum (wrapping) of the first count elements, elements 0 .. count - 1, into *out. count 0 gives 0 and count n gives the whole sum. False
// when n is invalid, out is null, or count > n.
bool prefix_sum(const std::int64_t* tree, std::uint32_t n, std::uint32_t count, std::int64_t* out);

// Sum (wrapping) of elements begin .. end - 1 into *out. begin == end gives 0. False when n is invalid, out is null, begin > end or end > n.
bool range_sum(const std::int64_t* tree, std::uint32_t n, std::uint32_t begin, std::uint32_t end, std::int64_t* out);

// Precondition for this function only: every element is at least 0 and the total does not exceed INT64_MAX; otherwise the result is
// unspecified (but never an access outside the tree). Smallest count k in 0 .. n with prefix_sum(k) >= target into *out; k 0 is the empty
// prefix, so a target of 0 or less gives 0. When no prefix reaches the target, *out = n + 1. False when n is invalid or out is null.
bool first_prefix_at_least(const std::int64_t* tree, std::uint32_t n, std::int64_t target, std::uint32_t* out);

// Decisions: tree and out must not overlap; not checked and not tested. A header ambiguity found later is a question to the maintainer,
// not an interpretation.

}  // namespace lumora::fenwick
