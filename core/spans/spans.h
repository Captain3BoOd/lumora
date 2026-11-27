// Set of up to 16 disjoint integer intervals (dirty ranges, free ranges, selection runs). Pure integer: no floating point, no libm, no
// 128-bit types (MSVC), no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::spans {

constexpr std::uint32_t kMaxSpans = 16;

struct Span {
  std::int32_t begin = 0;  // first covered integer
  std::int32_t end = 0;    // one past the last covered integer; the span is [begin, end) and has end - begin elements
};

struct SpanSet {
  Span s[kMaxSpans];          // s[0 .. count) are used
  std::uint32_t count = 0;    // 0 .. kMaxSpans
};

// Invariant of a well-formed set: count <= kMaxSpans, every used span has begin < end, the spans are in ascending order, and there is a
// gap of at least one integer between neighbours (s[i].end < s[i + 1].begin), so spans that overlap OR touch are always one span. Every
// function keeps it. A set that breaks the invariant (hand-built) gives unspecified results from add, remove, contains and total_length,
// but never an out-of-range access or a count above kMaxSpans; a count above kMaxSpans is corrupt and every function treats it as invalid.

// Sets count to 0; a null set is ignored. Repairs a corrupt set.
void clear(SpanSet* set);

// Adds [begin, end). A null or corrupt set returns false. begin >= end is an empty interval: true, the set is unchanged. Otherwise every
// used span that overlaps or touches [begin, end) (s.begin <= end and s.end >= begin) is merged with it into one span from the smallest
// begin to the largest end, which replaces them at the right place in the order. If the merged result needs a slot that does not exist
// (count would exceed kMaxSpans) the call returns false and the set is unchanged: there is no collapse. Otherwise true.
bool add(SpanSet* set, std::int32_t begin, std::int32_t end);

// Removes [begin, end) from the set. A null or corrupt set returns false. begin >= end is empty: true, unchanged. A span that contains the
// whole interval strictly inside is split in two (count + 1); a span that is covered completely disappears; a span that is partly covered
// is cut to its remaining part. If the result would need more than kMaxSpans spans the call returns false and the set is unchanged.
// Otherwise true.
bool remove(SpanSet* set, std::int32_t begin, std::int32_t end);

// True when x lies in a used span (begin <= x < end). False for a null or corrupt set.
bool contains(const SpanSet* set, std::int32_t x);

// Sum of (end - begin) over the used spans in 64 bits; 0 for a null or corrupt set.
std::uint64_t total_length(const SpanSet* set);

// Number of used spans; 0 for a null or corrupt set.
std::uint32_t size(const SpanSet* set);

// Copies span i to *out; false (out unchanged) for a null set or out, a corrupt set, or i >= count.
bool get(const SpanSet* set, std::uint32_t i, Span* out);

// Decisions: all differences end - begin and sums are computed in 64 bits, so spans reaching INT32_MIN and INT32_MAX are fine (end is
// exclusive, so INT32_MAX itself can never be covered). Touching spans merge, so [0,2) and [2,4) become [0,4). add never collapses a full
// set; it fails instead.

}  // namespace lumora::spans
