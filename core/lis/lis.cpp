// Longest strictly increasing subsequence: length, number of longest ones modulo 2^64 and the lexicographically smallest index list. See lis.h for the
// contract and docs/lis.md for the method, the check order, the scratch layout and the step bound.
//
// Method (no allocation, no recursion; everything lives in the caller's scratch, 10 * n + 6 words of the 16 * n + 8 that are required):
//  1. Forward pass: patience tails with a binary search give len[i], the length of the longest increasing subsequence that ends at i.
//  2. The elements with the same len form a "level". Inside one level the values never increase with the index (two elements of a level with
//     a[j] < a[k] and j < k would give k a longer subsequence). Levels are stored as contiguous segments of the index order.
//  3. Counting pass: cnt[i] = the sum of cnt[j] over j < i, a[j] < a[i], len[j] = len[i] - 1. In level len[i] - 1 those j are a suffix of the
//     segment, found by a binary search, and a running prefix sum per segment (two u32 halves, wrapping modulo 2^64) gives their sum in O(1).
//  4. Backward pass: the same tails idea from the right gives rlen[i], the length of the longest increasing subsequence that starts at i.
//  5. Greedy: the first index is the smallest index with rlen == L; each next index is the first index after the previous one whose rlen is one
//     smaller (among the indices with equal rlen the values do not increase, so the first one is large enough). A counting sort of the indices
//     by rlen gives each group in index order, and a binary search finds "the first index after".
#include "lis.h"

namespace lumora::lis {

namespace {

inline std::uint64_t join(std::uint32_t hi, std::uint32_t lo) { return (static_cast<std::uint64_t>(hi) << 32) | lo; }

}  // namespace

LisResult lis_scratch_words(std::uint32_t n, std::uint32_t* out_words) {
  if (out_words == nullptr) return LisResult::NullArgument;
  if (n > kMaxLen) return LisResult::TooLarge;
  *out_words = 16 * n + 8;
  return LisResult::Ok;
}

LisResult lis_run(std::uint32_t n, const std::uint32_t* a, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out_len,
                  std::uint64_t* out_count, std::uint32_t* out_idx) {
  if ((n > 0 && (a == nullptr || scratch == nullptr || out_idx == nullptr)) || out_len == nullptr || out_count == nullptr) return LisResult::NullArgument;
  if (n > kMaxLen) return LisResult::TooLarge;
  if (n > 0 && scratch_words < 16 * n + 8) return LisResult::ScratchTooSmall;
  if (n == 0) {
    *out_len = 0;
    *out_count = 1;
    return LisResult::Ok;
  }
  // Scratch layout (word counts): tails n, len n, off n + 2, fill n + 2, lidx n, cum_lo n, cum_hi n, rlen n, roff n + 2, rlist n.
  std::uint32_t* const tails = scratch;
  std::uint32_t* const len = tails + n;
  std::uint32_t* const off = len + n;
  std::uint32_t* const fill = off + (n + 2);
  std::uint32_t* const lidx = fill + (n + 2);
  std::uint32_t* const cum_lo = lidx + n;
  std::uint32_t* const cum_hi = cum_lo + n;
  std::uint32_t* const rlen = cum_hi + n;
  std::uint32_t* const roff = rlen + n;
  std::uint32_t* const rlist = roff + (n + 2);

  // 1. len[i] by patience tails: tails[p] = the smallest last value of an increasing subsequence of length p + 1.
  std::uint32_t top = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    std::uint32_t lo = 0, hi = top;  // first p with tails[p] >= a[i]
    while (lo < hi) {
      const std::uint32_t mid = lo + (hi - lo) / 2;
      if (tails[mid] < a[i])
        lo = mid + 1;
      else
        hi = mid;
    }
    tails[lo] = a[i];
    if (lo == top) ++top;
    len[i] = lo + 1;
  }
  const std::uint32_t total_len = top;

  // 2. Level segments: off[l] = start of level l (l = 1 .. total_len) in the index-ordered arrays lidx, cum_lo and cum_hi.
  for (std::uint32_t l = 0; l <= total_len + 1; ++l) {
    off[l] = 0;
    fill[l] = 0;
  }
  for (std::uint32_t i = 0; i < n; ++i) ++off[len[i] + 1];
  for (std::uint32_t l = 1; l <= total_len + 1; ++l) off[l] += off[l - 1];
  // now off[l] = number of elements in levels below l, that is the start of level l (off[1] = 0)

  // 3. Counting pass.
  std::uint64_t answer = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint32_t l = len[i];
    std::uint64_t cnt = 1;
    if (l > 1) {
      const std::uint32_t base = off[l - 1];
      const std::uint32_t used = fill[l - 1];
      std::uint32_t lo = base, hi = base + used;  // first position whose value is below a[i] (values do not increase along the segment)
      while (lo < hi) {
        const std::uint32_t mid = lo + (hi - lo) / 2;
        if (a[lidx[mid]] < a[i])
          hi = mid;
        else
          lo = mid + 1;
      }
      const std::uint64_t all = join(cum_hi[base + used - 1], cum_lo[base + used - 1]);
      const std::uint64_t before = lo > base ? join(cum_hi[lo - 1], cum_lo[lo - 1]) : 0;
      cnt = all - before;
    }
    const std::uint32_t pos = off[l] + fill[l];
    const std::uint64_t prev = fill[l] > 0 ? join(cum_hi[pos - 1], cum_lo[pos - 1]) : 0;
    const std::uint64_t cum = prev + cnt;
    lidx[pos] = i;
    cum_lo[pos] = static_cast<std::uint32_t>(cum);
    cum_hi[pos] = static_cast<std::uint32_t>(cum >> 32);
    ++fill[l];
    if (l == total_len) answer = cum;
  }

  // 4. rlen[i] by the same idea from the right: tails[p] = the largest first value of an increasing subsequence of length p + 1 that starts to the
  // right of the current index; the values decrease with p.
  top = 0;
  for (std::uint32_t i = n; i > 0; --i) {
    const std::uint32_t x = a[i - 1];
    std::uint32_t lo = 0, hi = top;  // first p with tails[p] <= x
    while (lo < hi) {
      const std::uint32_t mid = lo + (hi - lo) / 2;
      if (tails[mid] > x)
        lo = mid + 1;
      else
        hi = mid;
    }
    tails[lo] = x;
    if (lo == top) ++top;
    rlen[i - 1] = lo + 1;
  }

  // 5. Counting sort of the indices by rlen: roff[r] = start of the group with rlen == r (r = 1 .. total_len).
  for (std::uint32_t r = 0; r <= total_len + 1; ++r) roff[r] = 0;
  for (std::uint32_t i = 0; i < n; ++i) ++roff[rlen[i] + 1];
  for (std::uint32_t r = 1; r <= total_len + 1; ++r) roff[r] += roff[r - 1];
  for (std::uint32_t r = 0; r <= total_len + 1; ++r) fill[r] = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint32_t r = rlen[i];
    rlist[roff[r] + fill[r]] = i;
    ++fill[r];
  }

  // 6. Greedy canonical list.
  std::uint32_t prev = rlist[roff[total_len]];  // the smallest index with rlen == total_len
  out_idx[0] = prev;
  for (std::uint32_t t = 1; t < total_len; ++t) {
    const std::uint32_t r = total_len - t;
    std::uint32_t lo = roff[r], hi = roff[r] + fill[r];  // first entry of the group whose index is above prev
    while (lo < hi) {
      const std::uint32_t mid = lo + (hi - lo) / 2;
      if (rlist[mid] > prev)
        hi = mid;
      else
        lo = mid + 1;
    }
    prev = rlist[lo];
    out_idx[t] = prev;
  }
  *out_len = total_len;
  *out_count = answer;
  return LisResult::Ok;
}

}  // namespace lumora::lis
