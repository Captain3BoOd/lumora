// Tests for core/lifecycle/frame_stats. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/frame_stats_ref.py, an independent Python
// implementation on unbounded integers. Regenerate it with: python3 tools/frame_stats_ref.py
// The tests also carry a naive oracle (a ring walk, rank counting instead of sorting, shift-subtract division,
// decimal long division, std::to_string text) that shares no code with core/lifecycle/frame_stats.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "frame_stats.h"

using namespace lumora::lifecycle;

namespace {

constexpr std::uint64_t kU64 = UINT64_MAX;
constexpr std::uint32_t kU32 = UINT32_MAX;

// BEGIN GENERATED REFERENCE (tools/frame_stats_ref.py)
const int kRefSeqN = 300;
const int kRefSeqKinds[] = {0, 1, 2, 3};
const std::uint32_t kRefStCounts[] = {0u, 1u, 2u, 3u, 50u, 98u, 99u, 100u, 101u, 119u, 120u, 121u, 255u, 4294967295u};
const std::uint32_t kRefStNexts[] = {0u, 1u, 2u, 59u, 60u, 99u, 100u, 119u, 120u, 121u, 4294967295u};
const int kRefStKinds[] = {0, 1, 2, 3, 4};
const std::uint32_t kRefRecCounts[] = {0u, 5u, 120u, 121u, 4294967295u};
const std::uint32_t kRefRecNexts[] = {0u, 5u, 119u, 120u, 121u, 4294967295u};
const std::uint64_t kRefRecFrames[] = {1ull, 18446744073709551615ull, 0ull};
const std::uint64_t kRefNsList[] = {0ull, 1ull, 9999ull, 10000ull, 10001ull, 16690000ull, 18200000ull, 25040000ull, 999999999ull, 1000000000ull, 1000000001ull, 1000000000000000ull, 9007199254740992ull, 18446744073709551614ull, 18446744073709551615ull};
const std::uint32_t kRefFpsList[] = {0u, 1u, 9u, 10u, 11u, 599u, 600u, 4294967295u};
const std::uint32_t kRefCntList[] = {0u, 1u, 120u, 4294967295u};
const std::uint32_t kRefFullFps[] = {599u, 4294967295u};
const std::size_t kRefCaps[] = {0u, 1u, 2u, 3u, 5u, 9u, 10u, 11u, 40u, 70u, 95u, 96u, 97u, 98u, 100u, 200u};
const std::uint64_t kRefDigest_seq = 0x5008226D77B45F6Aull;
const std::size_t kRefCases_seq = 1200;
const std::uint64_t kRefDigest_structs = 0x1BB8820713D9C450ull;
const std::size_t kRefCases_structs = 770;
const std::uint64_t kRefDigest_record = 0x1D3306F652F18428ull;
const std::size_t kRefCases_record = 90;
const std::uint64_t kRefDigest_format = 0xAA8E4C972E76EC0Dull;
const std::size_t kRefCases_format = 115680;
// END GENERATED REFERENCE

// FNV-1a 64, the same as tools/frame_stats_ref.py (integers are fed least significant byte first).
struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void u64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
};

const std::uint64_t kExt[8] = {1, kU64, 2, kU64 - 1, 1ull << 63, 1000000000ull, 16666667ull, 3};

std::vector<std::uint64_t> lcg_values(std::uint64_t seed, std::size_t n) {
  std::vector<std::uint64_t> out;
  std::uint64_t x = seed;
  for (std::size_t i = 0; i < n; ++i) {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    out.push_back((x >> 33) % 40000000ull + 1ull);
  }
  return out;
}

std::vector<std::uint64_t> pattern(int kind) {
  std::vector<std::uint64_t> v;
  if (kind == 0 || kind == 2) {
    v = lcg_values(12345, kFrameWindow);
    if (kind == 2)
      for (std::size_t i = 0; i < kFrameWindow; i += 5) v[i] = 0;
  } else {
    for (std::size_t i = 0; i < kFrameWindow; ++i) {
      if (kind == 1) v.push_back(kExt[i % 8]);
      else if (kind == 3) v.push_back(i + 1);
      else v.push_back(kFrameWindow - i);
    }
  }
  return v;
}

std::vector<std::uint64_t> seq_frames(int kind, std::size_t n) {
  if (kind == 0) return lcg_values(7, n);
  std::vector<std::uint64_t> v;
  for (std::size_t i = 0; i < n; ++i) v.push_back(kind == 1 ? kExt[i % 8] : kind == 2 ? 1ull : kU64);
  return v;
}

FrameStats make_stats(const std::vector<std::uint64_t>& samples, std::uint32_t count, std::uint32_t next) {
  FrameStats s;
  for (std::size_t i = 0; i < kFrameWindow; ++i) s.samples[i] = samples[i];
  s.count = count;
  s.next = next;
  return s;
}

void feed_summary(Fnv* f, bool ok, const FrameSummary& s) {
  f->byte(ok ? 1 : 0);
  f->u32(s.count);
  f->u64(s.average_ns);
  f->u64(s.worst_ns);
  f->u64(s.p99_ns);
  f->u32(s.fps_tenths);
}

void feed_struct(Fnv* f, const FrameStats& s) {
  f->u32(s.count);
  f->u32(s.next);
  for (std::size_t i = 0; i < kFrameWindow; ++i) f->u64(s.samples[i]);
}

bool same_summary(const FrameSummary& a, const FrameSummary& b) {
  return a.count == b.count && a.average_ns == b.average_ns && a.worst_ns == b.worst_ns && a.p99_ns == b.p99_ns &&
         a.fps_tenths == b.fps_tenths;
}

bool same_stats(const FrameStats& a, const FrameStats& b) {
  if (a.count != b.count || a.next != b.next) return false;
  for (std::size_t i = 0; i < kFrameWindow; ++i)
    if (a.samples[i] != b.samples[i]) return false;
  return true;
}

// ---- naive oracle -------------------------------------------------------------------------------------------
// Shift-subtract division of a 64-bit value by a small divisor.
std::uint64_t oracle_div(std::uint64_t n, std::uint64_t d) {
  std::uint64_t q = 0, r = 0;
  for (int bit = 63; bit >= 0; --bit) {
    r = (r << 1) | ((n >> bit) & 1u);
    if (r >= d) {
      r -= d;
      q |= 1ull << bit;
    }
  }
  return q;
}

// Decimal long division of 10000000000 by avg, saturated at UINT32_MAX.
std::uint32_t oracle_fps(std::uint64_t avg) {
  if (avg == 0 || avg > 10000000000ull) return 0;
  const char* digits = "10000000000";
  std::uint64_t rem = 0, q = 0;
  for (const char* p = digits; *p != '\0'; ++p) {
    rem = rem * 10 + static_cast<std::uint64_t>(*p - '0');
    std::uint64_t d = 0;
    while (rem >= avg) {
      rem -= avg;
      ++d;
    }
    q = q * 10 + d;
  }
  return q > kU32 ? kU32 : static_cast<std::uint32_t>(q);
}

bool oracle_summarize(const FrameStats& s, FrameSummary* out) {
  FrameSummary r;
  if (s.count <= kFrameWindow && s.next < kFrameWindow && s.count > 0) {
    std::vector<std::uint64_t> win;
    std::size_t idx = s.next;
    for (std::uint32_t k = 0; k < s.count; ++k) {
      idx = idx == 0 ? kFrameWindow - 1 : idx - 1;
      win.push_back(s.samples[idx]);
    }
    std::uint64_t sum = 0, worst = 0;
    for (std::uint64_t v : win) {
      const std::uint64_t t = sum + v;
      sum = t < sum ? kU64 : t;
      if (v > worst) worst = v;
    }
    std::uint64_t rank = 1;
    while (rank * 100 < 99ull * s.count) ++rank;
    std::uint64_t p99 = 0;
    for (std::size_t i = 0; i < win.size(); ++i) {
      std::uint64_t less = 0, leq = 0;
      for (std::uint64_t o : win) {
        if (o < win[i]) ++less;
        if (o <= win[i]) ++leq;
      }
      if (less < rank && rank <= leq) p99 = win[i];
    }
    r.count = s.count;
    r.average_ns = oracle_div(sum, s.count);
    r.worst_ns = worst;
    r.p99_ns = p99;
    r.fps_tenths = oracle_fps(r.average_ns);
  }
  *out = r;
  return true;
}

std::string oracle_ms(std::uint64_t ns) {
  const std::uint64_t h = ns / 10000;
  const std::uint64_t frac = h % 100;
  return std::to_string(h / 100) + "." + (frac < 10 ? "0" : "") + std::to_string(frac);
}

std::string oracle_text(const FrameSummary& s) {
  if (s.count == 0) return "no frames";
  return std::to_string(s.fps_tenths / 10) + "." + std::to_string(s.fps_tenths % 10) + " fps  avg " +
         oracle_ms(s.average_ns) + " ms  p99 " + oracle_ms(s.p99_ns) + " ms  worst " + oracle_ms(s.worst_ns) + " ms";
}

FrameSummary sm(std::uint32_t count, std::uint32_t fps, std::uint64_t avg, std::uint64_t p99, std::uint64_t worst) {
  FrameSummary s;
  s.count = count;
  s.fps_tenths = fps;
  s.average_ns = avg;
  s.p99_ns = p99;
  s.worst_ns = worst;
  return s;
}

FrameSummary sum_of(const FrameStats& s) {
  FrameSummary r;
  REQUIRE(summarize(&s, &r));
  return r;
}

// Formats into a buffer with guard bytes around the capacity and checks everything exact.
std::string fmt(const FrameSummary& s, std::size_t cap, std::size_t* ret) {
  std::vector<char> buf(cap + 16, '\x7F');
  *ret = format_overlay(s, buf.data(), cap);
  for (std::size_t i = cap; i < buf.size(); ++i) REQUIRE(buf[i] == '\x7F');  // nothing written past capacity
  if (cap == 0) return std::string();
  std::size_t n = 0;
  while (n < cap && buf[n] != '\0') ++n;
  REQUIRE(n < cap);  // always terminated
  for (std::size_t i = n + 1; i < cap; ++i) REQUIRE(buf[i] == '\x7F');  // nothing written after the NUL
  return std::string(buf.data(), n);
}

}  // namespace

TEST_CASE("constants and defaults", "[frame_stats]") {
  CHECK(kFrameWindow == 120u);
  const FrameStats s;
  CHECK(s.count == 0u);
  CHECK(s.next == 0u);
  for (std::size_t i = 0; i < kFrameWindow; ++i) CHECK(s.samples[i] == 0u);
  const FrameSummary r;
  CHECK(r.count == 0u);
  CHECK(r.average_ns == 0u);
  CHECK(r.worst_ns == 0u);
  CHECK(r.p99_ns == 0u);
  CHECK(r.fps_tenths == 0u);
}

TEST_CASE("record_frame basics, wrap and rejection", "[frame_stats][record]") {
  FrameStats s;
  CHECK_FALSE(record_frame(nullptr, 5));
  CHECK_FALSE(record_frame(nullptr, 0));
  CHECK_FALSE(record_frame(&s, 0));
  CHECK(s.count == 0u);
  CHECK(s.next == 0u);
  CHECK(s.samples[0] == 0u);
  CHECK(record_frame(&s, 7));
  CHECK(s.count == 1u);
  CHECK(s.next == 1u);
  CHECK(s.samples[0] == 7u);
  CHECK(record_frame(&s, kU64));
  CHECK(s.samples[1] == kU64);
  CHECK(s.count == 2u);
  // Fill to 119: next is 119 and count 119, then the 120th makes count 120 and next wraps to 0.
  for (std::uint64_t i = 3; i <= 119; ++i) CHECK(record_frame(&s, i));
  CHECK(s.count == 119u);
  CHECK(s.next == 119u);
  CHECK(record_frame(&s, 1000));
  CHECK(s.count == 120u);
  CHECK(s.next == 0u);
  CHECK(s.samples[119] == 1000u);
  // The oldest sample is overwritten once full; count stays 120.
  CHECK(record_frame(&s, 2000));
  CHECK(s.count == 120u);
  CHECK(s.next == 1u);
  CHECK(s.samples[0] == 2000u);
  CHECK(s.samples[1] == kU64);
  for (int i = 0; i < 119; ++i) CHECK(record_frame(&s, 3000));
  CHECK(s.next == 0u);
  CHECK(s.count == 120u);
  // A rejected frame leaves a full window alone.
  const FrameStats before = s;
  CHECK_FALSE(record_frame(&s, 0));
  CHECK(same_stats(s, before));
}

TEST_CASE("record_frame treats an invalid object as empty first", "[frame_stats][record]") {
  const std::vector<std::uint64_t> p = pattern(0);
  // count above the window.
  FrameStats s = make_stats(p, 121, 5);
  CHECK(record_frame(&s, 42));
  CHECK(s.count == 1u);
  CHECK(s.next == 1u);
  CHECK(s.samples[0] == 42u);
  CHECK(s.samples[5] == p[5]);  // old slots are not cleared
  // next at the window size, and above it.
  s = make_stats(p, 5, 120);
  CHECK(record_frame(&s, 43));
  CHECK(s.count == 1u);
  CHECK(s.next == 1u);
  CHECK(s.samples[0] == 43u);
  s = make_stats(p, 120, kU32);
  CHECK(record_frame(&s, 44));
  CHECK(s.count == 1u);
  CHECK(s.next == 1u);
  s = make_stats(p, kU32, 3);
  CHECK(record_frame(&s, 45));
  CHECK(s.count == 1u);
  CHECK(s.next == 1u);
  // A rejected frame leaves an invalid object untouched (no repair on failure).
  s = make_stats(p, 121, 120);
  const FrameStats before = s;
  CHECK_FALSE(record_frame(&s, 0));
  CHECK(same_stats(s, before));
  // Valid but inconsistent: count 3 with next 50 writes at 50, then the window is 4 slots ending at 50.
  s = make_stats(p, 3, 50);
  CHECK(record_frame(&s, 9));
  CHECK(s.count == 4u);
  CHECK(s.next == 51u);
  CHECK(s.samples[50] == 9u);
  // Boundary values that are still valid: count 120, next 119.
  s = make_stats(p, 120, 119);
  CHECK(record_frame(&s, 9));
  CHECK(s.count == 120u);
  CHECK(s.next == 0u);
  CHECK(s.samples[119] == 9u);
}

TEST_CASE("summarize null handling and empty windows", "[frame_stats][summarize]") {
  const FrameStats empty;
  FrameSummary out = sm(7, 7, 7, 7, 7);
  CHECK_FALSE(summarize(nullptr, &out));
  CHECK(out.count == 7u);
  CHECK(out.average_ns == 7u);
  CHECK_FALSE(summarize(&empty, nullptr));
  CHECK_FALSE(summarize(nullptr, nullptr));
  CHECK(summarize(&empty, &out));
  CHECK(same_summary(out, FrameSummary{}));
  // Invalid objects give a zeroed summary and true.
  const std::vector<std::uint64_t> p = pattern(0);
  const FrameStats bad[] = {make_stats(p, 121, 0), make_stats(p, 5, 120), make_stats(p, kU32, kU32),
                            make_stats(p, 120, 121), make_stats(p, 0, 7)};
  for (const FrameStats& b : bad) {
    out = sm(7, 7, 7, 7, 7);
    CHECK(summarize(&b, &out));
    CHECK(same_summary(out, FrameSummary{}));
  }
}

TEST_CASE("summarize written examples", "[frame_stats][summarize]") {
  FrameStats s;
  for (std::uint64_t v : {16000000ull, 16700000ull, 18200000ull, 25040000ull}) REQUIRE(record_frame(&s, v));
  FrameSummary r = sum_of(s);
  CHECK(r.count == 4u);
  CHECK(r.average_ns == 18985000u);  // 75940000 / 4
  CHECK(r.worst_ns == 25040000u);
  CHECK(r.p99_ns == 25040000u);
  CHECK(r.fps_tenths == 526u);  // floor(1e10 / 18985000)
  // One sample.
  FrameStats one;
  REQUIRE(record_frame(&one, 16666667));
  r = sum_of(one);
  CHECK(r.count == 1u);
  CHECK(r.average_ns == 16666667u);
  CHECK(r.worst_ns == 16666667u);
  CHECK(r.p99_ns == 16666667u);
  CHECK(r.fps_tenths == 599u);
  // The average is floored: 1 + 2 = 3 over two samples.
  FrameStats two;
  REQUIRE(record_frame(&two, 1));
  REQUIRE(record_frame(&two, 2));
  CHECK(sum_of(two).average_ns == 1u);
}

TEST_CASE("p99 is nearest rank, count - floor(count / 100)", "[frame_stats][summarize]") {
  // Ascending samples 1..count in slots 0..count-1: the rank-r sample is r.
  struct Row {
    std::uint32_t count;
    std::uint64_t p99;
  };
  const Row rows[] = {{1, 1}, {2, 2}, {50, 50}, {99, 99}, {100, 99}, {101, 100}, {119, 118}, {120, 119}};
  for (const Row& row : rows) {
    INFO("count " << row.count);
    const FrameStats asc = make_stats(pattern(3), row.count, row.count % 120);
    CHECK(sum_of(asc).p99_ns == row.p99);
    CHECK(sum_of(asc).worst_ns == row.count);
    // The same window in descending order of slots (slot i holds 120 - i): the window is slots 0..count-1 again,
    // holding 120 down to 121 - count, so the p99 is the (rank)th smallest of that set.
    const FrameStats desc = make_stats(pattern(4), row.count, row.count % 120);
    CHECK(sum_of(desc).p99_ns == 120u - row.count + row.p99);
    CHECK(sum_of(desc).worst_ns == 120u);
  }
  // Duplicates: 100 samples, 98 of value 5, one 9, one 100 gives rank 99 = 9.
  std::vector<std::uint64_t> v(kFrameWindow, 5);
  v[3] = 9;
  v[77] = 100;
  CHECK(sum_of(make_stats(v, 100, 100)).p99_ns == 9u);
  // 120 samples: rank 119 is the second largest.
  std::vector<std::uint64_t> w(kFrameWindow, 5);
  w[10] = 50;
  w[20] = 60;
  CHECK(sum_of(make_stats(w, 120, 0)).p99_ns == 50u);
  CHECK(sum_of(make_stats(w, 120, 0)).worst_ns == 60u);
}

TEST_CASE("window selection follows next, not the slot order", "[frame_stats][summarize]") {
  std::vector<std::uint64_t> v(kFrameWindow, 1000000000ull);
  v[47] = 7;
  v[48] = 8;
  v[49] = 9;
  FrameSummary r = sum_of(make_stats(v, 3, 50));
  CHECK(r.count == 3u);
  CHECK(r.average_ns == 8u);
  CHECK(r.worst_ns == 9u);
  CHECK(r.p99_ns == 9u);
  // Wrapping window: count 5 with next 2 reads slots 117, 118, 119, 0, 1.
  std::vector<std::uint64_t> u(kFrameWindow, 1000000000ull);
  u[117] = 10;
  u[118] = 20;
  u[119] = 30;
  u[0] = 40;
  u[1] = 50;
  r = sum_of(make_stats(u, 5, 2));
  CHECK(r.average_ns == 30u);
  CHECK(r.worst_ns == 50u);
  CHECK(r.p99_ns == 50u);
  // next 0 with count 2 reads slots 118 and 119.
  r = sum_of(make_stats(u, 2, 0));
  CHECK(r.average_ns == 25u);
  CHECK(r.worst_ns == 30u);
  // A full window reads everything whatever next is.
  const FrameSummary a = sum_of(make_stats(pattern(3), 120, 0));
  const FrameSummary b = sum_of(make_stats(pattern(3), 120, 77));
  CHECK(same_summary(a, b));
  CHECK(a.average_ns == 60u);  // 7260 / 120 = 60.5, floored
  // Slots outside the window are ignored, even extreme ones.
  std::vector<std::uint64_t> x(kFrameWindow, kU64);
  x[10] = 4;
  x[11] = 6;
  r = sum_of(make_stats(x, 2, 12));
  CHECK(r.average_ns == 5u);
  CHECK(r.worst_ns == 6u);
}

TEST_CASE("average saturates its sum before dividing", "[frame_stats][summarize]") {
  std::vector<std::uint64_t> v(kFrameWindow, kU64);
  FrameSummary r = sum_of(make_stats(v, 2, 2));
  CHECK(r.average_ns == 9223372036854775807ull);  // UINT64_MAX / 2
  CHECK(r.worst_ns == kU64);
  CHECK(r.p99_ns == kU64);
  CHECK(r.fps_tenths == 0u);
  r = sum_of(make_stats(v, 120, 0));
  CHECK(r.average_ns == 153722867280912930ull);  // UINT64_MAX / 120
  // The exact sum 2^64 does not fit: the average is UINT64_MAX / 2, not 2^63.
  std::vector<std::uint64_t> w(kFrameWindow, 1);
  w[0] = kU64 - 1;
  w[1] = 2;
  CHECK(sum_of(make_stats(w, 2, 2)).average_ns == 9223372036854775807ull);
  // A sum of exactly UINT64_MAX does not saturate.
  w[1] = 1;
  CHECK(sum_of(make_stats(w, 2, 2)).average_ns == 9223372036854775807ull);
  w[0] = kU64 - 3;
  w[1] = 1;
  CHECK(sum_of(make_stats(w, 2, 2)).average_ns == 9223372036854775806ull);
  // Saturation then larger count: three samples.
  std::vector<std::uint64_t> y(kFrameWindow, kU64);
  CHECK(sum_of(make_stats(y, 3, 3)).average_ns == 6148914691236517205ull);  // UINT64_MAX / 3
}

TEST_CASE("fps_tenths bounds and saturation", "[frame_stats][summarize]") {
  struct Row {
    std::uint64_t ns;
    std::uint32_t fps;
  };
  const Row rows[] = {{1, kU32},          {2, kU32},         {3, 3333333333u}, {4, 2500000000u},
                      {16666667, 599},     {16666666, 600},   {1000000000, 10}, {10000000000ull, 1},
                      {5000000001ull, 1},  {10000000001ull, 0}, {kU64, 0}};
  for (const Row& row : rows) {
    INFO("ns " << row.ns);
    std::vector<std::uint64_t> v(kFrameWindow, row.ns);
    CHECK(sum_of(make_stats(v, 1, 1)).fps_tenths == row.fps);
  }
  // Average 0 (zero samples in a hand-built window) gives 0 fps and zeros elsewhere.
  const std::vector<std::uint64_t> zeros(kFrameWindow, 0);
  const FrameSummary r = sum_of(make_stats(zeros, 5, 5));
  CHECK(r.count == 5u);
  CHECK(r.average_ns == 0u);
  CHECK(r.worst_ns == 0u);
  CHECK(r.p99_ns == 0u);
  CHECK(r.fps_tenths == 0u);
  // Zeros mixed in: they count as samples.
  std::vector<std::uint64_t> m(kFrameWindow, 0);
  m[0] = 10;
  CHECK(sum_of(make_stats(m, 2, 1)).average_ns == 5u);
}

TEST_CASE("format_overlay text", "[frame_stats][format]") {
  std::size_t ret = 0;
  std::string t = fmt(sm(120, 599, 16690000, 18200000, 25040000), 100, &ret);
  CHECK(t == "59.9 fps  avg 16.69 ms  p99 18.20 ms  worst 25.04 ms");
  CHECK(ret == 52u);
  CHECK(t.size() == 52u);
  // Digits without padding, truncation (not rounding) of hundredths.
  CHECK(fmt(sm(1, 5, 0, 0, 0), 100, &ret) == "0.5 fps  avg 0.00 ms  p99 0.00 ms  worst 0.00 ms");
  CHECK(fmt(sm(1, 0, 9999, 10000, 10001), 100, &ret) == "0.0 fps  avg 0.00 ms  p99 0.01 ms  worst 0.01 ms");
  CHECK(fmt(sm(1, 10, 1000000, 1999999, 2000000), 100, &ret) == "1.0 fps  avg 1.00 ms  p99 1.99 ms  worst 2.00 ms");
  CHECK(fmt(sm(1, 1, 1, 1, 1), 100, &ret) == "0.1 fps  avg 0.00 ms  p99 0.00 ms  worst 0.00 ms");
  // The longest line: UINT32_MAX fps and UINT64_MAX times.
  t = fmt(sm(kU32, kU32, kU64, kU64, kU64), 200, &ret);
  CHECK(t == "429496729.5 fps  avg 18446744073709.55 ms  p99 18446744073709.55 ms  worst 18446744073709.55 ms");
  CHECK(ret == 95u);
  // The count only selects between the two forms; its value and the other fields are not checked.
  CHECK(fmt(sm(0, 599, 16690000, 18200000, 25040000), 100, &ret) == "no frames");
  CHECK(ret == 9u);
  CHECK(fmt(sm(0, 0, 0, 0, 0), 100, &ret) == "no frames");
  CHECK(fmt(FrameSummary{}, 10, &ret) == "no frames");
  CHECK(ret == 9u);
  CHECK(fmt(sm(kU32, 599, 16690000, 18200000, 25040000), 100, &ret) == "59.9 fps  avg 16.69 ms  p99 18.20 ms  worst 25.04 ms");
  CHECK(fmt(sm(1, 599, 1, 2, 3), 100, &ret) == "59.9 fps  avg 0.00 ms  p99 0.00 ms  worst 0.00 ms");  // no recomputation
}

TEST_CASE("format_overlay capacity, truncation and nulls", "[frame_stats][format]") {
  const FrameSummary s = sm(120, 599, 16690000, 18200000, 25040000);
  const std::string full = "59.9 fps  avg 16.69 ms  p99 18.20 ms  worst 25.04 ms";
  std::size_t ret = 0;
  // Capacity 0 returns 0 and writes nothing, null out returns 0.
  char c = 'x';
  CHECK(format_overlay(s, &c, 0) == 0u);
  CHECK(c == 'x');
  CHECK(format_overlay(s, nullptr, 0) == 0u);
  CHECK(format_overlay(s, nullptr, 100) == 0u);
  CHECK(format_overlay(FrameSummary{}, nullptr, 100) == 0u);
  CHECK(format_overlay(FrameSummary{}, &c, 0) == 0u);
  CHECK(c == 'x');
  // Capacity 1 writes only the NUL and returns the full length.
  c = 'x';
  CHECK(format_overlay(s, &c, 1) == full.size());
  CHECK(c == '\0');
  // Every capacity from 1 to a few above the needed size: a plain prefix, always terminated, full length returned.
  for (std::size_t cap = 1; cap <= full.size() + 4; ++cap) {
    INFO("capacity " << cap);
    const std::string got = fmt(s, cap, &ret);
    CHECK(ret == full.size());
    CHECK(got == full.substr(0, cap - 1 < full.size() ? cap - 1 : full.size()));
  }
  // Exactly needed + 1 holds the whole text; needed holds all but the last character.
  CHECK(fmt(s, full.size() + 1, &ret) == full);
  CHECK(fmt(s, full.size(), &ret) == full.substr(0, full.size() - 1));
  // The same for "no frames".
  for (std::size_t cap = 1; cap <= 12; ++cap) {
    const std::string got = fmt(FrameSummary{}, cap, &ret);
    CHECK(ret == 9u);
    CHECK(got == std::string("no frames").substr(0, cap - 1 < 9 ? cap - 1 : 9));
  }
}

TEST_CASE("record_frame digest over scripted sequences", "[frame_stats][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (int kind : kRefSeqKinds) {
    FrameStats s;
    const std::vector<std::uint64_t> frames = seq_frames(kind, static_cast<std::size_t>(kRefSeqN));
    for (std::size_t i = 0; i < frames.size(); ++i) {
      const std::uint64_t v = (i % 7 == 6) ? 0 : frames[i];
      const bool ok = record_frame(&s, v);
      FrameSummary r;
      const bool sok = summarize(&s, &r);
      if (ok != (v != 0) || !sok) ++bad;
      FrameSummary want;
      oracle_summarize(s, &want);
      if (!same_summary(r, want)) ++bad;
      if (s.count != (i + 1 - (i + 1) / 7 > kFrameWindow ? kFrameWindow : i + 1 - (i + 1) / 7)) ++bad;  // 6 of 7 recorded
      feed_summary(&f, ok, r);
      f.u32(s.count);
      f.u32(s.next);
      ++n;
    }
  }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_seq);
  CHECK(f.h == kRefDigest_seq);
}

TEST_CASE("summarize digest over hand-built objects", "[frame_stats][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (int kind : kRefStKinds)
    for (std::uint32_t c : kRefStCounts)
      for (std::uint32_t nx : kRefStNexts) {
        const FrameStats s = make_stats(pattern(kind), c, nx);
        FrameSummary r;
        const bool ok = summarize(&s, &r);
        FrameSummary want;
        oracle_summarize(s, &want);
        if (!ok || !same_summary(r, want)) ++bad;
        feed_summary(&f, ok, r);
        ++n;
      }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_structs);
  CHECK(f.h == kRefDigest_structs);
}

TEST_CASE("record_frame digest over valid and invalid objects", "[frame_stats][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint32_t c : kRefRecCounts)
    for (std::uint32_t nx : kRefRecNexts)
      for (std::uint64_t fr : kRefRecFrames) {
        FrameStats s = make_stats(pattern(0), c, nx);
        const FrameStats before = s;
        const bool ok = record_frame(&s, fr);
        if (ok != (fr != 0)) ++bad;
        if (!ok && !same_stats(s, before)) ++bad;  // failure changes nothing
        if (ok) {
          const bool valid = c <= kFrameWindow && nx < kFrameWindow;
          const std::uint32_t start = valid ? nx : 0;
          const std::uint32_t want_count = valid ? (c < kFrameWindow ? c + 1 : c) : 1;
          if (s.samples[start] != fr || s.count != want_count || s.next != (start + 1) % kFrameWindow) ++bad;
        }
        feed_struct(&f, s);
        f.byte(ok ? 1 : 0);
        ++n;
      }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_record);
  // The reference feeds ok first, then the struct; recompute in that order for the digest.
  Fnv g;
  for (std::uint32_t c : kRefRecCounts)
    for (std::uint32_t nx : kRefRecNexts)
      for (std::uint64_t fr : kRefRecFrames) {
        FrameStats s = make_stats(pattern(0), c, nx);
        const bool ok = record_frame(&s, fr);
        g.byte(ok ? 1 : 0);
        feed_struct(&g, s);
      }
  CHECK(g.h == kRefDigest_record);
}

TEST_CASE("format_overlay digest and oracle over summaries and capacities", "[frame_stats][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  auto one = [&](const FrameSummary& s) {
    const std::string want = oracle_text(s);
    for (std::size_t cap : kRefCaps) {
      if (cap == 0) {
        char c = 'x';
        if (format_overlay(s, &c, 0) != 0 || c != 'x') ++bad;
        f.u64(0);
        ++n;
        continue;
      }
      std::vector<char> buf(cap + 8, '\x7F');
      const std::size_t ret = format_overlay(s, buf.data(), cap);
      if (ret != want.size()) ++bad;
      std::vector<char> exp(cap, '\x7F');
      const std::size_t keep = want.size() < cap - 1 ? want.size() : cap - 1;
      for (std::size_t i = 0; i < keep; ++i) exp[i] = want[i];
      exp[keep] = '\0';
      for (std::size_t i = 0; i < cap; ++i)
        if (buf[i] != exp[i]) ++bad;
      for (std::size_t i = cap; i < buf.size(); ++i)
        if (buf[i] != '\x7F') ++bad;
      f.u64(ret);
      for (std::size_t i = 0; i < cap; ++i) f.byte(static_cast<std::uint8_t>(buf[i]));
      ++n;
    }
  };
  for (std::uint32_t fps : kRefFullFps)
    for (std::uint64_t a : kRefNsList)
      for (std::uint64_t p : kRefNsList)
        for (std::uint64_t w : kRefNsList) one(sm(1, fps, a, p, w));
  for (std::uint32_t cnt : kRefCntList)
    for (std::uint32_t fps : kRefFpsList)
      for (std::uint64_t v : kRefNsList) one(sm(cnt, fps, v, v, v));
  CHECK(bad == 0u);
  CHECK(n == kRefCases_format);
  CHECK(f.h == kRefDigest_format);
}

TEST_CASE("end to end: a 60 fps run feeds the overlay", "[frame_stats]") {
  FrameStats s;
  for (int i = 0; i < 300; ++i) REQUIRE(record_frame(&s, i % 50 == 49 ? 33333334ull : 16666667ull));
  const FrameSummary r = sum_of(s);
  CHECK(r.count == 120u);
  // 120 samples hold 2 slow frames at most; the exact numbers come from the oracle.
  FrameSummary want;
  oracle_summarize(s, &want);
  CHECK(same_summary(r, want));
  std::size_t ret = 0;
  const std::string got = fmt(r, 100, &ret);
  CHECK(got == oracle_text(r));
  CHECK(ret == got.size());
}
