// Lumora span set. See spans.h for the contract and docs/spans.md for the bounds. Integer only, no heap, no recursion.
#include "spans.h"

namespace lumora::spans {

namespace {

using i32 = std::int32_t;
using i64 = std::int64_t;
using u32 = std::uint32_t;

bool corrupt(const SpanSet* set) { return set->count > kMaxSpans; }

}  // namespace

void clear(SpanSet* set) {
  if (set == nullptr) return;
  set->count = 0;
}

bool add(SpanSet* set, i32 begin, i32 end) {
  if (set == nullptr || corrupt(set)) return false;
  if (begin >= end) return true;
  Span out[kMaxSpans];
  u32 n = 0;
  i32 mb = begin;
  i32 me = end;
  bool placed = false;
  for (u32 i = 0; i < set->count; ++i) {
    const Span sp = set->s[i];
    if (sp.begin <= end && sp.end >= begin) {
      if (sp.begin < mb) mb = sp.begin;
      if (sp.end > me) me = sp.end;
    } else if (sp.end < begin) {
      if (n >= kMaxSpans) return false;
      out[n++] = sp;
    } else {
      if (!placed) {
        if (n >= kMaxSpans) return false;
        out[n++] = Span{mb, me};
        placed = true;
      }
      if (n >= kMaxSpans) return false;
      out[n++] = sp;
    }
  }
  if (!placed) {
    if (n >= kMaxSpans) return false;
    out[n++] = Span{mb, me};
  }
  for (u32 i = 0; i < n; ++i) set->s[i] = out[i];
  set->count = n;
  return true;
}

bool remove(SpanSet* set, i32 begin, i32 end) {
  if (set == nullptr || corrupt(set)) return false;
  if (begin >= end) return true;
  Span out[kMaxSpans];
  u32 n = 0;
  for (u32 i = 0; i < set->count; ++i) {
    const Span sp = set->s[i];
    if (sp.end <= begin || sp.begin >= end) {
      if (n >= kMaxSpans) return false;
      out[n++] = sp;
      continue;
    }
    if (sp.begin < begin) {
      if (n >= kMaxSpans) return false;
      out[n++] = Span{sp.begin, begin};
    }
    if (sp.end > end) {
      if (n >= kMaxSpans) return false;
      out[n++] = Span{end, sp.end};
    }
  }
  for (u32 i = 0; i < n; ++i) set->s[i] = out[i];
  set->count = n;
  return true;
}

bool contains(const SpanSet* set, i32 x) {
  if (set == nullptr || corrupt(set)) return false;
  for (u32 i = 0; i < set->count; ++i) {
    if (set->s[i].begin <= x && x < set->s[i].end) return true;
  }
  return false;
}

std::uint64_t total_length(const SpanSet* set) {
  if (set == nullptr || corrupt(set)) return 0;
  std::uint64_t sum = 0;
  for (u32 i = 0; i < set->count; ++i) {
    const i64 d = static_cast<i64>(set->s[i].end) - set->s[i].begin;
    if (d > 0) sum += static_cast<std::uint64_t>(d);
  }
  return sum;
}

u32 size(const SpanSet* set) {
  if (set == nullptr || corrupt(set)) return 0;
  return set->count;
}

bool get(const SpanSet* set, u32 i, Span* out) {
  if (set == nullptr || out == nullptr || corrupt(set) || i >= set->count) return false;
  *out = set->s[i];
  return true;
}

}  // namespace lumora::spans
