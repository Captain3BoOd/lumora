// Frame statistics and the fps overlay text. See frame_stats.h for the contract and docs/frame_stats.md for the
// bounds and the check order. Integer only: no libm, no 128-bit types, no standard formatting functions.
#include "frame_stats.h"

#include <cstddef>
#include <cstdint>

namespace lumora::lifecycle {

namespace {

constexpr std::uint64_t kTenthsNumerator = 10000000000ull;  // 10 * 1e9: tenths of a frame per second
constexpr std::size_t kTextMax = 160;                        // the longest overlay line is well below this

bool in_range(const FrameStats& s) { return s.count <= kFrameWindow && s.next < kFrameWindow; }

// Appends the decimal digits of v. Returns the new length.
std::size_t put_u64(char* buf, std::size_t len, std::uint64_t v) {
  char digits[20];
  std::size_t n = 0;
  do {
    digits[n++] = static_cast<char>('0' + v % 10);
    v /= 10;
  } while (v != 0);
  while (n != 0) buf[len++] = digits[--n];
  return len;
}

std::size_t put_text(char* buf, std::size_t len, const char* text) {
  while (*text != '\0') buf[len++] = *text++;
  return len;
}

// "<whole>.<two digits>" for a nanosecond value: the value in hundredths of a millisecond, truncated.
std::size_t put_ms(char* buf, std::size_t len, std::uint64_t ns) {
  const std::uint64_t hundredths = ns / 10000;
  len = put_u64(buf, len, hundredths / 100);
  buf[len++] = '.';
  buf[len++] = static_cast<char>('0' + (hundredths % 100) / 10);
  buf[len++] = static_cast<char>('0' + hundredths % 10);
  return len;
}

}  // namespace

bool record_frame(FrameStats* stats, std::uint64_t frame_ns) {
  if (stats == nullptr || frame_ns == 0) return false;
  if (!in_range(*stats)) {
    stats->count = 0;
    stats->next = 0;
  }
  stats->samples[stats->next] = frame_ns;
  stats->next = stats->next + 1 == kFrameWindow ? 0 : stats->next + 1;
  if (stats->count < kFrameWindow) stats->count += 1;
  return true;
}

bool summarize(const FrameStats* stats, FrameSummary* out) {
  if (stats == nullptr || out == nullptr) return false;
  FrameSummary r;
  if (in_range(*stats) && stats->count != 0) {
    const std::size_t count = stats->count;
    std::uint64_t w[kFrameWindow];
    std::uint64_t sum = 0;
    std::uint64_t worst = 0;
    for (std::size_t i = 0; i < count; ++i) {
      const std::uint64_t v = stats->samples[(stats->next + kFrameWindow - count + i) % kFrameWindow];
      w[i] = v;
      sum = (sum > UINT64_MAX - v) ? UINT64_MAX : sum + v;
      if (v > worst) worst = v;
    }
    for (std::size_t i = 1; i < count; ++i) {  // insertion sort, ascending
      const std::uint64_t v = w[i];
      std::size_t j = i;
      while (j > 0 && w[j - 1] > v) {
        w[j] = w[j - 1];
        --j;
      }
      w[j] = v;
    }
    r.count = stats->count;
    r.average_ns = sum / count;
    r.worst_ns = worst;
    r.p99_ns = w[count - count / 100 - 1];
    if (r.average_ns != 0) {
      const std::uint64_t tenths = kTenthsNumerator / r.average_ns;
      r.fps_tenths = tenths > UINT32_MAX ? UINT32_MAX : static_cast<std::uint32_t>(tenths);
    }
  }
  *out = r;
  return true;
}

std::size_t format_overlay(const FrameSummary& summary, char* out, std::size_t capacity) {
  if (out == nullptr || capacity == 0) return 0;
  char text[kTextMax];
  std::size_t len = 0;
  if (summary.count == 0) {
    len = put_text(text, len, "no frames");
  } else {
    len = put_u64(text, len, summary.fps_tenths / 10);
    text[len++] = '.';
    len = put_u64(text, len, summary.fps_tenths % 10);
    len = put_text(text, len, " fps  avg ");
    len = put_ms(text, len, summary.average_ns);
    len = put_text(text, len, " ms  p99 ");
    len = put_ms(text, len, summary.p99_ns);
    len = put_text(text, len, " ms  worst ");
    len = put_ms(text, len, summary.worst_ns);
    len = put_text(text, len, " ms");
  }
  const std::size_t keep = len < capacity - 1 ? len : capacity - 1;
  for (std::size_t i = 0; i < keep; ++i) out[i] = text[i];
  out[keep] = '\0';
  return len;
}

}  // namespace lumora::lifecycle
