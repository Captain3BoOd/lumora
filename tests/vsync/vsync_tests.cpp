// Tests for core/lifecycle/vsync. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/vsync_ref.py, an independent Python
// implementation on unbounded integers. Regenerate it with: python3 tools/vsync_ref.py
// The tests also carry a naive oracle (a ring walk and rank counting instead of sorting) that shares no code with
// core/lifecycle/vsync.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "vsync.h"

using namespace lumora::lifecycle;

namespace {

constexpr std::uint64_t kU64 = UINT64_MAX;
constexpr std::uint32_t kU32 = UINT32_MAX;

// BEGIN GENERATED REFERENCE (tools/vsync_ref.py)
const int kRefSeqN = 300;
const int kRefSeqKinds[] = {0, 1, 2, 3};
const std::uint32_t kRefEsCounts[] = {0u, 1u, 2u, 3u, 4u, 5u, 8u, 15u, 16u, 17u, 255u, 4294967295u};
const std::uint32_t kRefEsNexts[] = {0u, 1u, 3u, 4u, 8u, 15u, 16u, 17u, 4294967295u};
const int kRefEsKinds[] = {0, 1, 2, 3, 4, 5};
const int kRefHbKinds[] = {0, 1, 5};
const std::uint64_t kRefHbLast = 1000ull;
const std::uint64_t kRefHbTs[] = {0ull, 999ull, 1000ull, 1001ull, 1000001000ull, 1000001001ull, 18446744073709551615ull};
const std::uint64_t kRefSentinel = 0xDEADBEEFCAFEF00Dull;
const std::uint64_t kRefDigest_seq = 0x931D2713AA463A7Dull;
const std::size_t kRefCases_seq = 1004;
const std::uint64_t kRefDigest_estimate = 0xF1DC5E5FA3D758A7ull;
const std::size_t kRefCases_estimate = 1296;
const std::uint64_t kRefDigest_hand = 0x4459EAD5D00FEB65ull;
const std::size_t kRefCases_hand = 4536;
// END GENERATED REFERENCE

// FNV-1a 64, the same as tools/vsync_ref.py (integers are fed least significant byte first).
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

std::vector<std::uint64_t> lcg_values(std::uint64_t seed, std::size_t n, std::uint64_t mod, std::uint64_t add) {
  std::vector<std::uint64_t> out;
  std::uint64_t x = seed;
  for (std::size_t i = 0; i < n; ++i) {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    out.push_back((x >> 33) % mod + add);
  }
  return out;
}

const std::uint64_t kExt[16] = {kU64, 1, kU64 - 1, 2, 1ull << 63, 1000000000ull, 1000000001ull, 16666667ull,
                                3, 0, 1, 7, 100, kU64, 5, 9};

std::vector<std::uint64_t> pattern(int kind) {
  std::vector<std::uint64_t> v;
  if (kind == 0 || kind == 1) {
    v = lcg_values(4242, kVsyncWindow, 40000000ull, 1ull);
    if (kind == 1)
      for (std::size_t i = 0; i < kVsyncWindow; i += 3) v[i] = 0;
    return v;
  }
  for (std::size_t i = 0; i < kVsyncWindow; ++i) {
    if (kind == 2) v.push_back(0);
    else if (kind == 3) v.push_back(i + 1);
    else if (kind == 4) v.push_back(kVsyncWindow - i);
    else v.push_back(kExt[i]);
  }
  return v;
}

std::vector<std::uint64_t> seq_timestamps(int kind, std::size_t n) {
  std::vector<std::uint64_t> out;
  if (kind == 0) {
    std::uint64_t t = 5;
    for (std::uint64_t d : lcg_values(99, n, 4000ull, 16000000ull)) {
      t += d;
      out.push_back(t);
    }
  } else if (kind == 1) {
    std::uint64_t t = 1000;
    for (std::uint64_t s : lcg_values(7, n, 12ull, 0ull)) {
      if (s == 0) {
      } else if (s == 1) {
        if (t > 3) t -= 3;
      } else if (s == 2) {
        t += 1000000000ull;
      } else if (s == 3) {
        t += 1000000001ull;
      } else if (s == 4) {
        t += 999999999ull;
      } else {
        t += 16666667ull + s;
      }
      out.push_back(t);
    }
  } else if (kind == 2) {
    for (std::size_t i = 0; i < n / 3; ++i) out.push_back(kU64 - 400 + 3 * i);
    for (std::uint64_t v : {kU64 - 5, kU64, kU64 - 1, kU64}) out.push_back(v);
  } else {
    std::uint64_t t = 0;
    out.push_back(0);
    for (std::uint64_t d : lcg_values(5, n - 1, 2000000000ull, 0ull)) {
      t += d;
      out.push_back(t);
    }
  }
  return out;
}

VsyncEstimator make_est(const std::vector<std::uint64_t>& deltas, std::uint32_t count, std::uint32_t next,
                        std::uint64_t last, bool has) {
  VsyncEstimator e;
  for (std::size_t i = 0; i < kVsyncWindow; ++i) e.deltas[i] = deltas[i];
  e.count = count;
  e.next = next;
  e.last_ns = last;
  e.has_last = has;
  return e;
}

void feed_est(Fnv* f, const VsyncEstimator& e) {
  f->u64(e.last_ns);
  f->byte(e.has_last ? 1 : 0);
  f->u32(e.count);
  f->u32(e.next);
  for (std::size_t i = 0; i < kVsyncWindow; ++i) f->u64(e.deltas[i]);
}

bool same_est(const VsyncEstimator& a, const VsyncEstimator& b) {
  if (a.last_ns != b.last_ns || a.has_last != b.has_last || a.count != b.count || a.next != b.next) return false;
  for (std::size_t i = 0; i < kVsyncWindow; ++i)
    if (a.deltas[i] != b.deltas[i]) return false;
  return true;
}

// ---- naive oracle -------------------------------------------------------------------------------------------
// Returns the estimate by walking back from next - 1 and picking the rank-th smallest by counting.
bool oracle_estimate(const VsyncEstimator& e, std::uint64_t* out) {
  if (e.count > kVsyncWindow || e.next >= kVsyncWindow || e.count < 4) return false;
  std::vector<std::uint64_t> win;
  std::size_t idx = e.next;
  for (std::uint32_t k = 0; k < e.count; ++k) {
    idx = idx == 0 ? kVsyncWindow - 1 : idx - 1;
    win.push_back(e.deltas[idx]);
  }
  const std::size_t rank = (e.count - 1) / 2 + 1;  // 1-based
  std::uint64_t m = 0;
  for (std::size_t i = 0; i < win.size(); ++i) {
    std::size_t less = 0, leq = 0;
    for (std::uint64_t o : win) {
      if (o < win[i]) ++less;
      if (o <= win[i]) ++leq;
    }
    if (less < rank && rank <= leq) m = win[i];
  }
  if (m == 0) return false;
  *out = m;
  return true;
}

// The plain state machine of the header, written with early-out flags instead of returns.
bool oracle_on_vsync(VsyncEstimator* e, std::uint64_t ts) {
  if (e->count > kVsyncWindow || e->next >= kVsyncWindow) {
    e->has_last = false;
    e->count = 0;
    e->next = 0;
  }
  if (!e->has_last) {
    e->last_ns = ts;
    e->has_last = true;
    return true;
  }
  if (ts > e->last_ns) {
    const std::uint64_t delta = ts - e->last_ns;
    if (delta > 1000000000ull) {
      e->count = 0;
      e->next = 0;
    } else {
      e->deltas[e->next] = delta;
      e->next = (e->next + 1) % kVsyncWindow;
      if (e->count < kVsyncWindow) e->count = e->count + 1;
    }
    e->last_ns = ts;
    return true;
  }
  return false;
}

bool est_ok(const VsyncEstimator& e, std::uint64_t* out) { return estimate_vsync_ns(&e, out); }

std::uint64_t estimate_of(const VsyncEstimator& e) {
  std::uint64_t out = 0;
  REQUIRE(estimate_vsync_ns(&e, &out));
  return out;
}

VsyncEstimator feed(std::initializer_list<std::uint64_t> ts) {
  VsyncEstimator e;
  for (std::uint64_t t : ts) on_vsync(&e, t);
  return e;
}

}  // namespace

TEST_CASE("constants and defaults", "[vsync]") {
  CHECK(kVsyncWindow == 16u);
  CHECK(kVsyncMinSamples == 4u);
  CHECK(kVsyncGapNs == 1000000000ull);
  const VsyncEstimator e;
  CHECK(e.last_ns == 0u);
  CHECK_FALSE(e.has_last);
  CHECK(e.count == 0u);
  CHECK(e.next == 0u);
  for (std::size_t i = 0; i < kVsyncWindow; ++i) CHECK(e.deltas[i] == 0u);
}

TEST_CASE("reset clears everything", "[vsync][reset]") {
  CHECK_FALSE(reset(nullptr));
  VsyncEstimator e = make_est(pattern(5), 12, 7, 987654321ull, true);
  CHECK(reset(&e));
  CHECK(same_est(e, VsyncEstimator{}));
  // Also an invalid object, and one that is already default.
  e = make_est(pattern(0), kU32, kU32, kU64, true);
  CHECK(reset(&e));
  CHECK(same_est(e, VsyncEstimator{}));
  CHECK(reset(&e));
  CHECK(same_est(e, VsyncEstimator{}));
  for (std::uint32_t c : {17u, 255u}) {
    e = make_est(pattern(3), c, 3, 5, false);
    CHECK(reset(&e));
    CHECK(same_est(e, VsyncEstimator{}));
  }
}

TEST_CASE("on_vsync first timestamp and rejection", "[vsync][on_vsync]") {
  CHECK_FALSE(on_vsync(nullptr, 5));
  VsyncEstimator e;
  CHECK(on_vsync(&e, 1000));
  CHECK(e.has_last);
  CHECK(e.last_ns == 1000u);
  CHECK(e.count == 0u);
  CHECK(e.next == 0u);
  // Equal and backwards timestamps are rejected with no change at all.
  const VsyncEstimator before = e;
  CHECK_FALSE(on_vsync(&e, 1000));
  CHECK(same_est(e, before));
  CHECK_FALSE(on_vsync(&e, 999));
  CHECK_FALSE(on_vsync(&e, 0));
  CHECK(same_est(e, before));
  // The next good timestamp measures against the kept last_ns.
  CHECK(on_vsync(&e, 1016));
  CHECK(e.count == 1u);
  CHECK(e.deltas[0] == 16u);
  CHECK(e.next == 1u);
  CHECK(e.last_ns == 1016u);
  // A first timestamp of 0 is accepted; a later 0 is not greater than it.
  VsyncEstimator z;
  CHECK(on_vsync(&z, 0));
  CHECK(z.has_last);
  CHECK(z.last_ns == 0u);
  CHECK_FALSE(on_vsync(&z, 0));
  CHECK(on_vsync(&z, 1));
  CHECK(z.deltas[0] == 1u);
  // A first timestamp of UINT64_MAX; then nothing is greater.
  VsyncEstimator m;
  CHECK(on_vsync(&m, kU64));
  CHECK_FALSE(on_vsync(&m, kU64));
  CHECK_FALSE(on_vsync(&m, kU64 - 1));
  CHECK(m.count == 0u);
  // The largest recordable step near the top of the range.
  VsyncEstimator t;
  CHECK(on_vsync(&t, kU64 - 5));
  CHECK(on_vsync(&t, kU64));
  CHECK(t.deltas[0] == 5u);
  CHECK(t.last_ns == kU64);
}

TEST_CASE("on_vsync gap rule", "[vsync][on_vsync]") {
  VsyncEstimator e = feed({100, 116, 132, 148, 164});
  REQUIRE(e.count == 4u);
  // Exactly 1 s is a normal delta.
  CHECK(on_vsync(&e, 164 + 1000000000ull));
  CHECK(e.count == 5u);
  CHECK(e.deltas[4] == 1000000000ull);
  CHECK(e.next == 5u);
  // Above 1 s clears the window and moves last_ns; has_last stays true; the slots keep their stale values.
  const std::uint64_t last = e.last_ns;
  CHECK(on_vsync(&e, last + 1000000001ull));
  CHECK(e.count == 0u);
  CHECK(e.next == 0u);
  CHECK(e.has_last);
  CHECK(e.last_ns == last + 1000000001ull);
  CHECK(e.deltas[4] == 1000000000ull);
  CHECK(e.deltas[0] == 16u);
  // And the next delta is recorded again from slot 0.
  CHECK(on_vsync(&e, e.last_ns + 17));
  CHECK(e.count == 1u);
  CHECK(e.next == 1u);
  CHECK(e.deltas[0] == 17u);
  // A huge jump also clears.
  CHECK(on_vsync(&e, kU64));
  CHECK(e.count == 0u);
  CHECK(e.last_ns == kU64);
  // A gap on a full window.
  VsyncEstimator f;
  for (std::uint64_t i = 0; i < 40; ++i) on_vsync(&f, 100 + 16 * i);
  CHECK(f.count == 16u);
  CHECK(on_vsync(&f, f.last_ns + 5000000000ull));
  CHECK(f.count == 0u);
  CHECK(f.next == 0u);
}

TEST_CASE("on_vsync ring wrap and count growth", "[vsync][on_vsync]") {
  VsyncEstimator e;
  std::uint64_t t = 100;
  CHECK(on_vsync(&e, t));
  for (std::uint32_t i = 1; i <= 16; ++i) {
    t += 10 + i;
    CHECK(on_vsync(&e, t));
    CHECK(e.count == i);
    CHECK(e.next == i % 16);
    CHECK(e.deltas[i - 1] == 10u + i);
  }
  // The oldest delta is overwritten once full.
  t += 99;
  CHECK(on_vsync(&e, t));
  CHECK(e.count == 16u);
  CHECK(e.next == 1u);
  CHECK(e.deltas[0] == 99u);
  CHECK(e.deltas[1] == 12u);
  for (int i = 0; i < 15; ++i) CHECK(on_vsync(&e, t += 7));
  CHECK(e.next == 0u);
  CHECK(e.count == 16u);
}

TEST_CASE("on_vsync repairs invalid objects by clearing only the named fields", "[vsync][on_vsync]") {
  const std::vector<std::uint64_t> p = pattern(0);
  const std::pair<std::uint32_t, std::uint32_t> bad[] = {{17, 0}, {0, 16}, {255, 3}, {5, 17}, {kU32, kU32}, {16, kU32}};
  for (const auto& cn : bad) {
    INFO("count " << cn.first << " next " << cn.second);
    VsyncEstimator e = make_est(p, cn.first, cn.second, 777, true);
    // The repair makes this timestamp the first one: stored, no delta, no rejection even though it is below last_ns.
    CHECK(on_vsync(&e, 5));
    CHECK(e.has_last);
    CHECK(e.last_ns == 5u);
    CHECK(e.count == 0u);
    CHECK(e.next == 0u);
    for (std::size_t i = 0; i < kVsyncWindow; ++i) CHECK(e.deltas[i] == p[i]);  // slots are left as they were
    // Even with has_last false, the repair path is the same.
    e = make_est(p, cn.first, cn.second, 777, false);
    CHECK(on_vsync(&e, 5));
    CHECK(e.last_ns == 5u);
    CHECK(e.count == 0u);
  }
  // A valid object with has_last false and a positive count: stored, count and next untouched, slots kept.
  VsyncEstimator e = make_est(p, 5, 9, 777, false);
  CHECK(on_vsync(&e, 3));
  CHECK(e.has_last);
  CHECK(e.last_ns == 3u);
  CHECK(e.count == 5u);
  CHECK(e.next == 9u);
  for (std::size_t i = 0; i < kVsyncWindow; ++i) CHECK(e.deltas[i] == p[i]);
  // Valid boundary state count 16, next 15 is not repaired.
  e = make_est(p, 16, 15, 1000, true);
  CHECK(on_vsync(&e, 1010));
  CHECK(e.count == 16u);
  CHECK(e.next == 0u);
  CHECK(e.deltas[15] == 10u);
  CHECK(e.last_ns == 1010u);
  // Rejection on a valid hand-built object changes nothing.
  e = make_est(p, 6, 2, 1000, true);
  const VsyncEstimator before = e;
  CHECK_FALSE(on_vsync(&e, 1000));
  CHECK(same_est(e, before));
}

TEST_CASE("estimate_vsync_ns lower median and thresholds", "[vsync][estimate]") {
  std::uint64_t out = 0x55;
  // Fewer than 4 deltas: false, out untouched.
  VsyncEstimator e;
  CHECK_FALSE(estimate_vsync_ns(&e, &out));
  for (std::uint64_t t : {100ull, 116ull, 132ull, 148ull}) on_vsync(&e, t);  // 3 deltas of 16
  REQUIRE(e.count == 3u);
  CHECK_FALSE(estimate_vsync_ns(&e, &out));
  CHECK(out == 0x55u);
  on_vsync(&e, 164);
  REQUIRE(e.count == 4u);
  CHECK(estimate_vsync_ns(&e, &out));
  CHECK(out == 16u);
  // Null pointers.
  out = 0x55;
  CHECK_FALSE(estimate_vsync_ns(nullptr, &out));
  CHECK_FALSE(estimate_vsync_ns(&e, nullptr));
  CHECK_FALSE(estimate_vsync_ns(nullptr, nullptr));
  CHECK(out == 0x55u);
  // Lower median: index (count - 1) / 2 of the sorted deltas.
  std::vector<std::uint64_t> v(kVsyncWindow, 1000000);
  // count 4: sorted 10 20 30 40 -> index 1 -> 20
  v[0] = 40; v[1] = 10; v[2] = 30; v[3] = 20;
  CHECK(estimate_of(make_est(v, 4, 4, 0, true)) == 20u);
  // count 5: sorted 10 20 30 40 50 -> index 2 -> 30
  v[4] = 50;
  CHECK(estimate_of(make_est(v, 5, 5, 0, true)) == 30u);
  // count 6: add 5 -> sorted 5 10 20 30 40 50 -> index 2 -> 20
  v[5] = 5;
  CHECK(estimate_of(make_est(v, 6, 6, 0, true)) == 20u);
  // count 7: add 60 -> sorted 5 10 20 30 40 50 60 -> index 3 -> 30
  v[6] = 60;
  CHECK(estimate_of(make_est(v, 7, 7, 0, true)) == 30u);
  // count 16 with deltas 1..16 in a shuffled order: index 7 -> 8
  std::vector<std::uint64_t> s = {9, 1, 16, 4, 12, 7, 15, 2, 8, 14, 3, 11, 6, 13, 5, 10};
  CHECK(estimate_of(make_est(s, 16, 0, 0, true)) == 8u);
  // count 15 with 1..15: index 7 -> 8
  std::vector<std::uint64_t> a(kVsyncWindow, 999);
  for (std::size_t i = 0; i < 15; ++i) a[i] = i + 1;
  CHECK(estimate_of(make_est(a, 15, 15, 0, true)) == 8u);
  // Equal values and extremes.
  const std::vector<std::uint64_t> big(kVsyncWindow, kU64);
  CHECK(estimate_of(make_est(big, 16, 0, 0, true)) == kU64);
  const std::vector<std::uint64_t> ones(kVsyncWindow, 1);
  CHECK(estimate_of(make_est(ones, 4, 4, 0, true)) == 1u);
}

TEST_CASE("estimate_vsync_ns window, has_last, invalid state and zero median", "[vsync][estimate]") {
  std::uint64_t out = 0x77;
  // Window selection: count 5, next 2 reads slots 13, 14, 15, 0, 1; the other slots hold huge values.
  std::vector<std::uint64_t> v(kVsyncWindow, kU64);
  v[13] = 10; v[14] = 20; v[15] = 30; v[0] = 40; v[1] = 50;
  CHECK(estimate_of(make_est(v, 5, 2, 0, true)) == 30u);
  // count 4, next 10 reads slots 6..9 and ignores the rest.
  std::vector<std::uint64_t> w(kVsyncWindow, 1);
  w[6] = 100; w[7] = 200; w[8] = 300; w[9] = 400;
  CHECK(estimate_of(make_est(w, 4, 10, 0, true)) == 200u);
  // Stale zeros outside the window do not matter.
  std::vector<std::uint64_t> z(kVsyncWindow, 0);
  z[2] = 7; z[3] = 8; z[4] = 9; z[5] = 10;
  CHECK(estimate_of(make_est(z, 4, 6, 0, true)) == 8u);
  // has_last is not checked.
  CHECK(estimate_of(make_est(z, 4, 6, 0, false)) == 8u);
  // Invalid state: count above 16 or next at 16 or above.
  const std::vector<std::uint64_t> ok(kVsyncWindow, 5);
  out = 0x77;
  CHECK_FALSE(est_ok(make_est(ok, 17, 0, 0, true), &out));
  CHECK_FALSE(est_ok(make_est(ok, 8, 16, 0, true), &out));
  CHECK_FALSE(est_ok(make_est(ok, kU32, 3, 0, true), &out));
  CHECK_FALSE(est_ok(make_est(ok, 8, kU32, 0, true), &out));
  CHECK(out == 0x77u);
  // Valid boundary state count 16, next 15.
  CHECK(estimate_of(make_est(ok, 16, 15, 0, true)) == 5u);
  // A zero median is false with out untouched: the median of 0 0 0 0 and of 0 0 0 5.
  const std::vector<std::uint64_t> zeros(kVsyncWindow, 0);
  CHECK_FALSE(est_ok(make_est(zeros, 4, 4, 0, true), &out));
  CHECK(out == 0x77u);
  std::vector<std::uint64_t> y(kVsyncWindow, 0);
  y[3] = 5;
  CHECK_FALSE(est_ok(make_est(y, 4, 4, 0, true), &out));  // sorted 0 0 0 5, index 1 -> 0
  CHECK(out == 0x77u);
  y[2] = 5;
  CHECK_FALSE(est_ok(make_est(y, 4, 4, 0, true), &out));  // sorted 0 0 5 5, index 1 -> 0
  CHECK(out == 0x77u);
  y[1] = 5;
  CHECK(estimate_of(make_est(y, 4, 4, 0, true)) == 5u);  // sorted 0 5 5 5, index 1 -> 5
}

TEST_CASE("on_vsync then estimate, end to end", "[vsync]") {
  // A steady 60 Hz source with one 1 s pause and two spikes.
  VsyncEstimator e;
  std::uint64_t t = 1000000;
  on_vsync(&e, t);
  for (int i = 0; i < 20; ++i) on_vsync(&e, t += 16666667);
  CHECK(estimate_of(e) == 16666667u);
  // A spike or two does not move the lower median.
  on_vsync(&e, t += 40000000);
  on_vsync(&e, t += 33333334);
  CHECK(estimate_of(e) == 16666667u);
  // A pause clears the window: no estimate until four new deltas exist.
  on_vsync(&e, t += 2000000000ull);
  std::uint64_t out = 1;
  CHECK_FALSE(estimate_vsync_ns(&e, &out));
  CHECK(out == 1u);
  for (int i = 0; i < 3; ++i) on_vsync(&e, t += 11111111);
  CHECK_FALSE(estimate_vsync_ns(&e, &out));
  on_vsync(&e, t += 11111111);
  CHECK(estimate_of(e) == 11111111u);
}

TEST_CASE("on_vsync digest over scripted timestamp sequences", "[vsync][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (int kind : kRefSeqKinds) {
    VsyncEstimator e, o;
    for (std::uint64_t ts : seq_timestamps(kind, static_cast<std::size_t>(kRefSeqN))) {
      const bool ok = on_vsync(&e, ts);
      const bool want = oracle_on_vsync(&o, ts);
      if (ok != want || !same_est(e, o)) ++bad;
      f.byte(ok ? 1 : 0);
      feed_est(&f, e);
      std::uint64_t out = kRefSentinel, want_out = kRefSentinel;
      const bool eok = estimate_vsync_ns(&e, &out);
      const bool wok = oracle_estimate(e, &want_out);
      if (eok != wok || out != want_out) ++bad;
      f.byte(eok ? 1 : 0);
      f.u64(out);
      ++n;
    }
  }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_seq);
  CHECK(f.h == kRefDigest_seq);
}

TEST_CASE("estimate digest over hand-built objects", "[vsync][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (int kind : kRefEsKinds)
    for (std::uint32_t c : kRefEsCounts)
      for (std::uint32_t nx : kRefEsNexts)
        for (int has = 0; has < 2; ++has) {
          const VsyncEstimator e = make_est(pattern(kind), c, nx, 12345, has != 0);
          std::uint64_t out = kRefSentinel, want = kRefSentinel;
          const bool ok = estimate_vsync_ns(&e, &out);
          const bool wok = oracle_estimate(e, &want);
          if (ok != wok || out != want) ++bad;
          if (!ok && out != kRefSentinel) ++bad;
          f.byte(ok ? 1 : 0);
          f.u64(out);
          ++n;
        }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_estimate);
  CHECK(f.h == kRefDigest_estimate);
}

TEST_CASE("on_vsync digest over hand-built objects", "[vsync][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (int kind : kRefHbKinds)
    for (std::uint32_t c : kRefEsCounts)
      for (std::uint32_t nx : kRefEsNexts)
        for (int has = 0; has < 2; ++has)
          for (std::uint64_t ts : kRefHbTs) {
            VsyncEstimator e = make_est(pattern(kind), c, nx, kRefHbLast, has != 0);
            VsyncEstimator o = e;
            const bool ok = on_vsync(&e, ts);
            const bool want = oracle_on_vsync(&o, ts);
            if (ok != want || !same_est(e, o)) ++bad;
            f.byte(ok ? 1 : 0);
            feed_est(&f, e);
            ++n;
          }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_hand);
  CHECK(f.h == kRefDigest_hand);
}
