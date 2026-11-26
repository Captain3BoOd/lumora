// Tests for core/lifecycle/pacing. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/pacing_ref.py, an independent Python
// implementation on unbounded integers. Regenerate it with: python3 tools/pacing_ref.py
// The tests also carry a naive oracle (limb arithmetic for the thresholds, a search for the vsync multiple, a plain
// state machine) that shares no code with core/lifecycle/pacing.cpp.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "pacing.h"

using namespace lumora::lifecycle;

namespace {

constexpr std::uint64_t kU64 = UINT64_MAX;
constexpr std::uint32_t kU32 = UINT32_MAX;

struct RefCfg {
  std::uint32_t max_notch, overrun_frames, recover_frames, overrun_percent, fast_percent;
};

// BEGIN GENERATED REFERENCE (tools/pacing_ref.py)
const std::uint64_t kRefCapPeriods[] = {0ull, 33333333ull, 16666667ull, 11111111ull, 8333333ull};
const std::uint8_t kRefAllCaps[] = {0, 1, 2, 3, 4, 5, 6, 127, 128, 255};
const std::uint64_t kRefVsyncs[] = {0ull, 1ull, 2ull, 3ull, 4ull, 5ull, 6ull, 7ull, 8ull, 9ull, 10ull, 11ull, 12ull, 13ull, 14ull, 15ull, 16ull, 17ull, 18ull, 19ull, 20ull, 21ull, 22ull, 23ull, 24ull, 25ull, 26ull, 27ull, 28ull, 29ull, 30ull, 31ull, 32ull, 33ull, 34ull, 35ull, 36ull, 37ull, 38ull, 39ull, 40ull, 41ull, 42ull, 43ull, 44ull, 45ull, 46ull, 47ull, 48ull, 49ull, 50ull, 51ull, 52ull, 53ull, 54ull, 55ull, 56ull, 57ull, 58ull, 59ull, 60ull, 61ull, 62ull, 63ull, 64ull, 65ull, 66ull, 67ull, 68ull, 69ull, 70ull, 71ull, 72ull, 73ull, 74ull, 75ull, 76ull, 77ull, 78ull, 79ull, 80ull, 81ull, 82ull, 83ull, 84ull, 85ull, 86ull, 87ull, 88ull, 89ull, 90ull, 91ull, 92ull, 93ull, 94ull, 95ull, 96ull, 97ull, 98ull, 99ull, 100ull, 101ull, 102ull, 103ull, 104ull, 105ull, 106ull, 107ull, 108ull, 109ull, 110ull, 111ull, 112ull, 113ull, 114ull, 115ull, 116ull, 117ull, 118ull, 119ull, 120ull, 121ull, 122ull, 123ull, 124ull, 125ull, 126ull, 127ull, 128ull, 129ull, 130ull, 131ull, 132ull, 133ull, 134ull, 135ull, 136ull, 137ull, 138ull, 139ull, 140ull, 141ull, 142ull, 143ull, 144ull, 145ull, 146ull, 147ull, 148ull, 149ull, 150ull, 151ull, 152ull, 153ull, 154ull, 155ull, 156ull, 157ull, 158ull, 159ull, 160ull, 161ull, 162ull, 163ull, 164ull, 165ull, 166ull, 167ull, 168ull, 169ull, 170ull, 171ull, 172ull, 173ull, 174ull, 175ull, 176ull, 177ull, 178ull, 179ull, 180ull, 181ull, 182ull, 183ull, 184ull, 185ull, 186ull, 187ull, 188ull, 189ull, 190ull, 191ull, 192ull, 193ull, 194ull, 195ull, 196ull, 197ull, 198ull, 199ull, 200ull, 201ull, 202ull, 203ull, 204ull, 205ull, 206ull, 207ull, 208ull, 209ull, 210ull, 211ull, 212ull, 213ull, 214ull, 215ull, 216ull, 217ull, 218ull, 219ull, 220ull, 221ull, 222ull, 223ull, 224ull, 225ull, 226ull, 227ull, 228ull, 229ull, 230ull, 231ull, 232ull, 233ull, 234ull, 235ull, 236ull, 237ull, 238ull, 239ull, 240ull, 241ull, 242ull, 243ull, 244ull, 245ull, 246ull, 247ull, 248ull, 249ull, 250ull, 251ull, 252ull, 253ull, 254ull, 255ull, 256ull, 257ull, 258ull, 259ull, 260ull, 261ull, 262ull, 263ull, 264ull, 265ull, 266ull, 267ull, 268ull, 269ull, 270ull, 271ull, 272ull, 273ull, 274ull, 275ull, 276ull, 277ull, 278ull, 279ull, 280ull, 281ull, 282ull, 283ull, 284ull, 285ull, 286ull, 287ull, 288ull, 289ull, 290ull, 291ull, 292ull, 293ull, 294ull, 295ull, 296ull, 297ull, 298ull, 299ull, 300ull, 301ull, 302ull, 303ull, 304ull, 305ull, 306ull, 307ull, 308ull, 309ull, 310ull, 311ull, 312ull, 313ull, 314ull, 315ull, 316ull, 317ull, 318ull, 319ull, 320ull, 321ull, 322ull, 323ull, 324ull, 325ull, 326ull, 327ull, 328ull, 329ull, 330ull, 331ull, 332ull, 333ull, 334ull, 335ull, 336ull, 337ull, 338ull, 339ull, 340ull, 341ull, 342ull, 343ull, 344ull, 345ull, 346ull, 347ull, 348ull, 349ull, 350ull, 351ull, 352ull, 353ull, 354ull, 355ull, 356ull, 357ull, 358ull, 359ull, 360ull, 361ull, 362ull, 363ull, 364ull, 365ull, 366ull, 367ull, 368ull, 369ull, 370ull, 371ull, 372ull, 373ull, 374ull, 375ull, 376ull, 377ull, 378ull, 379ull, 380ull, 381ull, 382ull, 383ull, 384ull, 385ull, 386ull, 387ull, 388ull, 389ull, 390ull, 391ull, 392ull, 393ull, 394ull, 395ull, 396ull, 397ull, 398ull, 399ull, 400ull, 4166667ull, 6944444ull, 8333333ull, 8333334ull, 11111111ull, 13888889ull, 16666666ull, 16666667ull, 16666668ull, 20833333ull, 22222222ull, 33333332ull, 33333333ull, 33333334ull, 33333335ull, 41666667ull, 66666666ull, 66666667ull, 100000000ull, 4294967296ull, 4294967297ull, 4611686018427387904ull, 9223372036854775807ull, 9223372036854775808ull, 9223372036854775809ull, 18446744073709551614ull, 18446744073709551615ull};
const std::uint32_t kRefValidGrid0[] = {0u, 1u, 2u, 14u, 15u, 16u, 4294967295u};
const std::uint32_t kRefValidGrid1[] = {0u, 1u, 2u, 4294967295u};
const std::uint32_t kRefValidGrid2[] = {0u, 1u, 2u, 4294967295u};
const std::uint32_t kRefValidGrid3[] = {0u, 99u, 100u, 101u, 1000u, 1001u, 4294967295u};
const std::uint32_t kRefValidGrid4[] = {0u, 9u, 10u, 11u, 99u, 100u, 101u, 4294967295u};
const std::uint64_t kRefBudgets[] = {1ull, 2ull, 3ull, 9ull, 10ull, 11ull, 99ull, 100ull, 101ull, 999ull, 1000ull, 1001ull, 16666667ull, 1000000000000000000ull, 18446744073709551ull, 18446744073709552ull, 184467440737095516ull, 184467440737095517ull, 1844674407370955161ull, 1844674407370955162ull, 9223372036854775808ull, 18446744073709551614ull, 18446744073709551615ull};
const std::uint32_t kRefStartNotches[] = {0u, 1u, 2u, 3u, 15u, 16u, 4294967295u};
const std::uint32_t kRefStartRuns[] = {0u, 1u, 2u, 4294967294u, 4294967295u};
const std::uint64_t kRefFrameFixed[] = {0ull, 1ull, 18446744073709551614ull, 18446744073709551615ull};
const int kRefFrameOffsets[] = {-1, 0, 1};
const RefCfg kRefFrameConfigs[] = {{3u, 30u, 300u, 110u, 90u}, {1u, 1u, 1u, 100u, 100u}, {15u, 2u, 3u, 1000u, 10u}, {2u, 4294967295u, 4294967295u, 100u, 10u}, {2u, 2u, 2u, 150u, 50u}, {1u, 3u, 2u, 100u, 100u}};
const RefCfg kRefSeqConfigs[] = {{2u, 2u, 2u, 110u, 90u}, {1u, 1u, 1u, 100u, 100u}, {3u, 3u, 2u, 120u, 80u}};
const std::uint64_t kRefSeqBudget = 100ull;
const std::uint64_t kRefSeqFrames[] = {111ull, 100ull, 90ull};
const int kRefSeqLen = 7;
const std::uint32_t kRefSeqStartNotches[] = {0u, 1u, 2u, 3u, 5u};
const std::uint64_t kRefDigest_cap = 0x0EACD8B9A61D04F4ull;
const std::size_t kRefCases_cap = 10;
const std::uint64_t kRefDigest_interval = 0x842FC02FD98687A6ull;
const std::size_t kRefCases_interval = 8560;
const std::uint64_t kRefDigest_valid = 0x47997CC3ED8AE5D5ull;
const std::size_t kRefCases_valid = 6272;
const std::uint64_t kRefDigest_frames = 0xBCCD433BFE2C6338ull;
const std::size_t kRefCases_frames = 198975;
const std::uint64_t kRefDigest_thermal = 0x0E3218B1B6B66CAEull;
const std::size_t kRefCases_thermal = 1050;
const std::uint64_t kRefDigest_sequences = 0x5B5F8DC4CB18D6EFull;
const std::size_t kRefCases_sequences = 245760;
// END GENERATED REFERENCE

// FNV-1a 64, the same as tools/pacing_ref.py (integers are fed least significant byte first).
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

FrameCap cap_of(unsigned v) { return static_cast<FrameCap>(static_cast<std::uint8_t>(v)); }

ScaleConfig cfg_of(const RefCfg& r) {
  ScaleConfig c;
  c.max_notch = r.max_notch;
  c.overrun_frames = r.overrun_frames;
  c.recover_frames = r.recover_frames;
  c.overrun_percent = r.overrun_percent;
  c.fast_percent = r.fast_percent;
  return c;
}

ScaleConfig make_cfg(std::uint32_t mx, std::uint32_t of, std::uint32_t rf, std::uint32_t op, std::uint32_t fp) {
  return cfg_of(RefCfg{mx, of, rf, op, fp});
}

// ---- naive oracle -------------------------------------------------------------------------------------------
// floor(budget * percent / 100) saturated at UINT64_MAX, by schoolbook arithmetic on 32-bit limbs.
std::uint64_t oracle_threshold(std::uint64_t budget, std::uint32_t percent) {
  const std::uint64_t lo = budget & 0xFFFFFFFFull;
  const std::uint64_t hi = budget >> 32;
  const std::uint64_t plo = lo * percent;  // below 2^42
  const std::uint64_t phi = hi * percent + (plo >> 32);  // below 2^42
  std::uint32_t limb[3] = {static_cast<std::uint32_t>(plo & 0xFFFFFFFFull), static_cast<std::uint32_t>(phi & 0xFFFFFFFFull),
                           static_cast<std::uint32_t>(phi >> 32)};
  std::uint64_t rem = 0;
  std::uint32_t quo[3] = {0, 0, 0};
  for (int i = 2; i >= 0; --i) {
    const std::uint64_t cur = (rem << 32) | limb[i];
    quo[i] = static_cast<std::uint32_t>(cur / 100);
    rem = cur % 100;
  }
  if (quo[2] != 0) return kU64;
  return (static_cast<std::uint64_t>(quo[1]) << 32) | quo[0];
}

// The smallest multiple k * vsync (k >= 1) whose distance below it to desired is at most vsync / 20.
bool oracle_interval(unsigned cap, std::uint64_t vsync, bool saver, std::uint64_t* out, bool* skipped) {
  *skipped = false;
  if (cap > 4 || vsync == 0) return false;
  static const std::uint64_t periods[5] = {0, 33333333ull, 16666667ull, 11111111ull, 8333333ull};
  std::uint64_t desired = cap == 0 ? vsync : periods[cap];
  if (saver && desired < 33333333ull) desired = 33333333ull;
  if (desired < vsync) desired = vsync;
  const std::uint64_t tol = vsync / 20;
  if (desired / vsync > 200000) {  // too many steps to walk; the reference digest covers these
    *skipped = true;
    return true;
  }
  std::uint64_t k = 1;
  while (k * vsync < desired && desired - k * vsync > tol) ++k;
  *out = k * vsync;
  return true;
}

struct Model {
  std::uint32_t notch, over, fast;
};

std::uint32_t sat_inc(std::uint32_t v) { return v == kU32 ? v : v + 1; }

bool oracle_frame(Model* m, std::uint64_t frame, std::uint64_t budget, const ScaleConfig& c, bool* changed) {
  if (budget == 0 || c.max_notch < 1 || c.max_notch > 15 || c.overrun_frames < 1 || c.recover_frames < 1 ||
      c.overrun_percent < 100 || c.overrun_percent > 1000 || c.fast_percent < 10 || c.fast_percent > 100)
    return false;
  const std::uint32_t orig = m->notch;
  std::uint32_t notch = orig;
  if (notch > c.max_notch) notch = c.max_notch;
  std::uint32_t over = m->over, fast = m->fast;
  if (frame > oracle_threshold(budget, c.overrun_percent)) {
    over = sat_inc(over);
    fast = 0;
    if (over >= c.overrun_frames && notch < c.max_notch) {
      ++notch;
      over = 0;
    }
  } else if (frame <= oracle_threshold(budget, c.fast_percent)) {
    fast = sat_inc(fast);
    over = 0;
    if (fast >= c.recover_frames && notch > 0) {
      --notch;
      fast = 0;
    }
  } else {
    over = 0;
    fast = 0;
  }
  *m = Model{notch, over, fast};
  *changed = notch != orig;
  return true;
}

bool oracle_thermal(Model* m, const ScaleConfig& c, bool* changed) {
  if (c.max_notch < 1 || c.max_notch > 15 || c.overrun_frames < 1 || c.recover_frames < 1 || c.overrun_percent < 100 ||
      c.overrun_percent > 1000 || c.fast_percent < 10 || c.fast_percent > 100)
    return false;
  const std::uint32_t orig = m->notch;
  std::uint32_t notch = orig;
  if (notch > c.max_notch) notch = c.max_notch;
  if (notch < c.max_notch) ++notch;
  *m = Model{notch, 0, 0};
  *changed = notch != orig;
  return true;
}

void feed(Fnv* f, bool changed, const ScaleState& s) {
  f->byte(changed ? 1 : 0);
  f->u32(s.notch);
  f->u32(s.overrun_run);
  f->u32(s.fast_run);
}

bool same(const ScaleState& s, const Model& m) { return s.notch == m.notch && s.overrun_run == m.over && s.fast_run == m.fast; }

PacingInput in_of(FrameCap cap, std::uint64_t vsync, bool saver) {
  PacingInput in;
  in.cap = cap;
  in.vsync_period_ns = vsync;
  in.battery_saver = saver;
  return in;
}

std::uint64_t interval_of(FrameCap cap, std::uint64_t vsync, bool saver) {
  std::uint64_t out = 0;
  REQUIRE(frame_interval_ns(in_of(cap, vsync, saver), &out));
  return out;
}

struct Step {
  bool ok;
  bool changed;
  ScaleState st;
};

Step run_frame(ScaleState s, std::uint64_t frame, std::uint64_t budget, const ScaleConfig& c) {
  Step r{};
  r.changed = false;
  r.ok = on_frame(&s, frame, budget, c, &r.changed);
  r.st = s;
  return r;
}

}  // namespace

TEST_CASE("defaults and enum values", "[pacing]") {
  CHECK(static_cast<unsigned>(FrameCap::Auto) == 0u);
  CHECK(static_cast<unsigned>(FrameCap::Fps30) == 1u);
  CHECK(static_cast<unsigned>(FrameCap::Fps60) == 2u);
  CHECK(static_cast<unsigned>(FrameCap::Fps90) == 3u);
  CHECK(static_cast<unsigned>(FrameCap::Fps120) == 4u);
  const PacingInput in;
  CHECK(in.cap == FrameCap::Auto);
  CHECK(in.vsync_period_ns == 0u);
  CHECK_FALSE(in.battery_saver);
  const ScaleConfig c;
  CHECK(c.max_notch == 3u);
  CHECK(c.overrun_frames == 30u);
  CHECK(c.recover_frames == 300u);
  CHECK(c.overrun_percent == 110u);
  CHECK(c.fast_percent == 90u);
  CHECK(valid(c));
  const ScaleState s;
  CHECK(s.notch == 0u);
  CHECK(s.overrun_run == 0u);
  CHECK(s.fast_run == 0u);
}

TEST_CASE("cap_period_ns and to_string", "[pacing][cap]") {
  CHECK(cap_period_ns(FrameCap::Auto) == 0u);
  CHECK(cap_period_ns(FrameCap::Fps30) == 33333333u);
  CHECK(cap_period_ns(FrameCap::Fps60) == 16666667u);
  CHECK(cap_period_ns(FrameCap::Fps90) == 11111111u);
  CHECK(cap_period_ns(FrameCap::Fps120) == 8333333u);
  for (unsigned v : {5u, 6u, 127u, 128u, 255u}) {
    CHECK(cap_period_ns(cap_of(v)) == 0u);
    CHECK(std::string(to_string(cap_of(v))) == "unknown");
  }
  CHECK(std::string(to_string(FrameCap::Auto)) == "Auto");
  CHECK(std::string(to_string(FrameCap::Fps30)) == "Fps30");
  CHECK(std::string(to_string(FrameCap::Fps60)) == "Fps60");
  CHECK(std::string(to_string(FrameCap::Fps90)) == "Fps90");
  CHECK(std::string(to_string(FrameCap::Fps120)) == "Fps120");
  Fnv f;
  std::size_t n = 0;
  for (std::uint8_t c : kRefAllCaps) {
    f.u64(cap_period_ns(cap_of(c)));
    ++n;
  }
  CHECK(n == kRefCases_cap);
  CHECK(f.h == kRefDigest_cap);
  for (std::size_t i = 0; i < 5; ++i) CHECK(cap_period_ns(cap_of(static_cast<unsigned>(i))) == kRefCapPeriods[i]);
}

TEST_CASE("frame_interval_ns written examples", "[pacing][interval]") {
  // Auto follows the display.
  CHECK(interval_of(FrameCap::Auto, 16666667, false) == 16666667u);
  CHECK(interval_of(FrameCap::Auto, 6944444, false) == 6944444u);
  CHECK(interval_of(FrameCap::Auto, 1, false) == 1u);
  // The header's example: 60 fps does not divide a 90 Hz panel.
  CHECK(interval_of(FrameCap::Fps60, 11111111, false) == 22222222u);
  CHECK(interval_of(FrameCap::Fps90, 11111111, false) == 11111111u);
  // A cap equal to the panel rate.
  CHECK(interval_of(FrameCap::Fps60, 16666667, false) == 16666667u);
  CHECK(interval_of(FrameCap::Fps120, 8333333, false) == 8333333u);
  // 144 Hz panel with a 60 fps cap: 2.4 periods rounds up to 3.
  CHECK(interval_of(FrameCap::Fps60, 6944444, false) == 20833332u);
  // Never faster than the display: a 30 Hz panel with a 120 fps cap.
  CHECK(interval_of(FrameCap::Fps120, 33333333, false) == 33333333u);
  CHECK(interval_of(FrameCap::Fps120, 100000000, false) == 100000000u);
  // Battery saver lifts a faster request to 30 fps and does nothing to a slower one.
  CHECK(interval_of(FrameCap::Auto, 6944444, true) == 34722220u);
  CHECK(interval_of(FrameCap::Fps120, 8333333, true) == 33333332u);
  CHECK(interval_of(FrameCap::Fps30, 16666667, true) == 33333334u);
  CHECK(interval_of(FrameCap::Auto, 41666667, true) == 41666667u);
  CHECK(interval_of(FrameCap::Fps60, 1, true) == 33333333u);
  // vsync 1 is a valid period: every desired value is already a whole number of periods.
  CHECK(interval_of(FrameCap::Auto, 1, false) == 1u);
  CHECK(interval_of(FrameCap::Fps30, 1, false) == 33333333u);
  CHECK(interval_of(FrameCap::Fps60, 1, false) == 16666667u);
  CHECK(interval_of(FrameCap::Fps90, 1, false) == 11111111u);
  CHECK(interval_of(FrameCap::Fps120, 1, false) == 8333333u);
  CHECK(interval_of(FrameCap::Fps120, 1, true) == 33333333u);
  // vsync 7 with Fps30: 33333333 = 4761904 * 7 + 5, tolerance 0, so it rounds up.
  CHECK(interval_of(FrameCap::Fps30, 7, false) == 33333335u);
  // Huge periods.
  CHECK(interval_of(FrameCap::Auto, kU64, false) == kU64);
  CHECK(interval_of(FrameCap::Fps30, kU64, true) == kU64);
  CHECK(interval_of(FrameCap::Fps120, kU64 - 1, false) == kU64 - 1);
  CHECK(interval_of(FrameCap::Fps60, 1ull << 63, true) == (1ull << 63));
}

TEST_CASE("frame_interval_ns tolerance boundary", "[pacing][interval]") {
  // Fps30 is 33333333. vsync 112: remainder 5, tolerance 112 / 20 = 5, so the remainder is within tolerance and rounds down.
  CHECK(33333333ull % 112u == 5u);
  CHECK(interval_of(FrameCap::Fps30, 112, false) == 297619ull * 112u);
  // vsync 82: remainder 5, tolerance 4, one over, so it rounds up.
  CHECK(33333333ull % 82u == 5u);
  CHECK(interval_of(FrameCap::Fps30, 82, false) == 406505ull * 82u);
  // vsync 19 has tolerance 0: any remainder rounds up, an exact multiple does not.
  CHECK(interval_of(FrameCap::Fps30, 19, false) == 1754386ull * 19u);  // 33333333 = 1754385 * 19 + 18
  CHECK(interval_of(FrameCap::Auto, 19, false) == 19u);
  // vsync 20 has tolerance 1.
  CHECK(33333333ull % 20u == 13u);
  CHECK(interval_of(FrameCap::Fps30, 20, false) == 1666667ull * 20u);
}

TEST_CASE("frame_interval_ns failures leave out untouched", "[pacing][interval]") {
  std::uint64_t out = 0x1234567890ABCDEFull;
  CHECK_FALSE(frame_interval_ns(in_of(FrameCap::Auto, 0, false), &out));
  CHECK(out == 0x1234567890ABCDEFull);
  CHECK_FALSE(frame_interval_ns(in_of(FrameCap::Fps60, 0, true), &out));
  CHECK(out == 0x1234567890ABCDEFull);
  for (unsigned v : {5u, 6u, 127u, 255u}) {
    CHECK_FALSE(frame_interval_ns(in_of(cap_of(v), 16666667, false), &out));
    CHECK_FALSE(frame_interval_ns(in_of(cap_of(v), 16666667, true), &out));
    CHECK(out == 0x1234567890ABCDEFull);
  }
  CHECK_FALSE(frame_interval_ns(in_of(FrameCap::Fps60, 16666667, false), nullptr));
  CHECK_FALSE(frame_interval_ns(in_of(FrameCap::Auto, 0, false), nullptr));
  CHECK(frame_interval_ns(in_of(FrameCap::Auto, 5, false), &out));
  CHECK(out == 5u);
}

TEST_CASE("frame_interval_ns digest, oracle and invariants", "[pacing][interval][sweep]") {
  Fnv f;
  std::size_t n = 0, compared = 0, bad = 0;
  for (std::uint64_t v : kRefVsyncs) {
    for (std::uint8_t c : kRefAllCaps) {
      for (int s = 0; s < 2; ++s) {
        std::uint64_t out = 0xFEEDFACEull;
        const bool ok = frame_interval_ns(in_of(cap_of(c), v, s != 0), &out);
        f.byte(ok ? 1 : 0);
        if (ok) f.u64(out);
        ++n;
        std::uint64_t want = 0;
        bool skipped = false;
        const bool want_ok = oracle_interval(c, v, s != 0, &want, &skipped);
        if (ok != want_ok) ++bad;
        if (!ok && out != 0xFEEDFACEull) ++bad;
        if (ok && !skipped) {
          ++compared;
          if (out != want) ++bad;
        }
        if (ok) {
          if (out == 0 || out % v != 0) ++bad;  // a whole number of vsync periods, at least one
        }
      }
    }
  }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_interval);
  CHECK(compared > 1500u);
  CHECK(f.h == kRefDigest_interval);
}

TEST_CASE("valid(ScaleConfig) bounds", "[pacing][valid]") {
  CHECK(valid(make_cfg(1, 1, 1, 100, 10)));
  CHECK(valid(make_cfg(15, kU32, kU32, 1000, 100)));
  CHECK_FALSE(valid(make_cfg(0, 30, 300, 110, 90)));
  CHECK_FALSE(valid(make_cfg(16, 30, 300, 110, 90)));
  CHECK_FALSE(valid(make_cfg(kU32, 30, 300, 110, 90)));
  CHECK(valid(make_cfg(15, 30, 300, 110, 90)));
  CHECK_FALSE(valid(make_cfg(3, 0, 300, 110, 90)));
  CHECK_FALSE(valid(make_cfg(3, 30, 0, 110, 90)));
  CHECK_FALSE(valid(make_cfg(3, 30, 300, 99, 90)));
  CHECK(valid(make_cfg(3, 30, 300, 100, 90)));
  CHECK(valid(make_cfg(3, 30, 300, 1000, 90)));
  CHECK_FALSE(valid(make_cfg(3, 30, 300, 1001, 90)));
  CHECK_FALSE(valid(make_cfg(3, 30, 300, 0, 90)));
  CHECK_FALSE(valid(make_cfg(3, 30, 300, 110, 9)));
  CHECK(valid(make_cfg(3, 30, 300, 110, 10)));
  CHECK(valid(make_cfg(3, 30, 300, 110, 100)));
  CHECK_FALSE(valid(make_cfg(3, 30, 300, 110, 101)));
  CHECK_FALSE(valid(make_cfg(3, 30, 300, 110, 0)));
  CHECK_FALSE(valid(make_cfg(3, 30, 300, 110, kU32)));
  CHECK_FALSE(valid(make_cfg(3, 30, 300, kU32, 90)));
  // The two classes can meet at 100 / 100 and still never overlap: overrun is strict, fast is inclusive.
  CHECK(valid(make_cfg(3, 30, 300, 100, 100)));
}

TEST_CASE("valid digest over the grid", "[pacing][valid][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint32_t a : kRefValidGrid0)
    for (std::uint32_t b : kRefValidGrid1)
      for (std::uint32_t c : kRefValidGrid2)
        for (std::uint32_t d : kRefValidGrid3)
          for (std::uint32_t e : kRefValidGrid4) {
            const bool v = valid(make_cfg(a, b, c, d, e));
            f.byte(v ? 1 : 0);
            ++n;
            const bool want = a >= 1 && a <= 15 && b >= 1 && c >= 1 && d >= 100 && d <= 1000 && e >= 10 && e <= 100;
            if (v != want) ++bad;
          }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_valid);
  CHECK(f.h == kRefDigest_valid);
}

TEST_CASE("on_frame failures change nothing", "[pacing][frame]") {
  const ScaleConfig good = make_cfg(3, 1, 1, 110, 90);
  const ScaleState start{2, 5, 7};
  for (bool sentinel : {false, true}) {
    bool changed = sentinel;
    CHECK_FALSE(on_frame(nullptr, 200, 100, good, &changed));
    CHECK(changed == sentinel);
    CHECK_FALSE(on_frame(nullptr, 200, 100, good, nullptr));

    ScaleState s = start;
    changed = sentinel;
    CHECK_FALSE(on_frame(&s, 200, 0, good, &changed));  // budget 0
    CHECK(changed == sentinel);
    CHECK(s.notch == 2u);
    CHECK(s.overrun_run == 5u);
    CHECK(s.fast_run == 7u);

    s = start;
    changed = sentinel;
    CHECK_FALSE(on_frame(&s, 0, 0, good, &changed));
    CHECK(changed == sentinel);

    const ScaleConfig bads[] = {make_cfg(0, 1, 1, 110, 90),  make_cfg(16, 1, 1, 110, 90), make_cfg(3, 0, 1, 110, 90),
                                make_cfg(3, 1, 0, 110, 90),  make_cfg(3, 1, 1, 99, 90),   make_cfg(3, 1, 1, 1001, 90),
                                make_cfg(3, 1, 1, 110, 9),   make_cfg(3, 1, 1, 110, 101)};
    for (const ScaleConfig& bad : bads) {
      s = start;
      changed = sentinel;
      CHECK_FALSE(on_frame(&s, 200, 100, bad, &changed));
      CHECK(changed == sentinel);
      CHECK(s.notch == 2u);
      CHECK(s.overrun_run == 5u);
      CHECK(s.fast_run == 7u);
    }
    // A state with an oversized notch is also untouched by a failure (no clamp).
    s = ScaleState{9, 1, 1};
    changed = sentinel;
    CHECK_FALSE(on_frame(&s, 200, 0, good, &changed));
    CHECK(s.notch == 9u);
    CHECK(changed == sentinel);
  }
}

TEST_CASE("on_frame success writes changed and accepts null changed", "[pacing][frame]") {
  const ScaleConfig c = make_cfg(3, 1, 1, 110, 90);
  for (bool sentinel : {false, true}) {
    ScaleState s;
    bool changed = sentinel;
    CHECK(on_frame(&s, 100, 100, c, &changed));  // in between: nothing moves
    CHECK_FALSE(changed);                        // overwritten even when it was true
    s = ScaleState{};
    changed = sentinel;
    CHECK(on_frame(&s, 200, 100, c, &changed));
    CHECK(changed);
    CHECK(s.notch == 1u);
  }
  ScaleState s;
  CHECK(on_frame(&s, 200, 100, c, nullptr));
  CHECK(s.notch == 1u);
  CHECK(on_frame(&s, 200, 100, c, nullptr));
  CHECK(s.notch == 2u);
}

TEST_CASE("on_frame threshold boundaries", "[pacing][frame]") {
  const ScaleConfig c = make_cfg(3, 1, 1, 110, 90);
  // budget 100: overrun above 110, fast at or below 90.
  Step r = run_frame(ScaleState{1, 0, 0}, 110, 100, c);
  CHECK(r.ok);
  CHECK_FALSE(r.changed);
  CHECK(r.st.notch == 1u);
  CHECK(r.st.overrun_run == 0u);
  CHECK(r.st.fast_run == 0u);
  r = run_frame(ScaleState{1, 0, 0}, 111, 100, c);
  CHECK(r.changed);
  CHECK(r.st.notch == 2u);
  CHECK(r.st.overrun_run == 0u);
  r = run_frame(ScaleState{1, 0, 0}, 91, 100, c);
  CHECK_FALSE(r.changed);
  CHECK(r.st.notch == 1u);
  CHECK(r.st.fast_run == 0u);
  r = run_frame(ScaleState{1, 0, 0}, 90, 100, c);
  CHECK(r.changed);
  CHECK(r.st.notch == 0u);
  CHECK(r.st.fast_run == 0u);
  // Rounding is floor: budget 101 gives 111 and 90 (111.1 and 90.9).
  CHECK(run_frame(ScaleState{}, 111, 101, c).st.notch == 0u);
  CHECK(run_frame(ScaleState{}, 112, 101, c).st.notch == 1u);
  CHECK(run_frame(ScaleState{1, 0, 0}, 90, 101, c).st.notch == 0u);
  CHECK(run_frame(ScaleState{1, 0, 0}, 91, 101, c).st.notch == 1u);
  // Budget 1: thresholds floor(1.1) = 1 and floor(0.9) = 0.
  CHECK(run_frame(ScaleState{}, 1, 1, c).st.notch == 0u);
  CHECK(run_frame(ScaleState{}, 2, 1, c).st.notch == 1u);
  CHECK(run_frame(ScaleState{1, 0, 0}, 0, 1, c).st.notch == 0u);
  CHECK(run_frame(ScaleState{1, 0, 0}, 1, 1, c).st.notch == 1u);
  // Frame 0 is always fast.
  CHECK(run_frame(ScaleState{1, 0, 0}, 0, kU64, c).st.notch == 0u);
  // Percent bounds: 100 / 100 makes "frame > budget" overrun and "frame <= budget" fast.
  const ScaleConfig tight = make_cfg(3, 1, 1, 100, 100);
  CHECK(run_frame(ScaleState{1, 0, 0}, 100, 100, tight).st.notch == 0u);
  CHECK(run_frame(ScaleState{1, 0, 0}, 101, 100, tight).st.notch == 2u);
  // 1000 / 10: overrun above ten budgets, fast below one tenth.
  const ScaleConfig wide = make_cfg(3, 1, 1, 1000, 10);
  CHECK(run_frame(ScaleState{}, 1000, 100, wide).st.notch == 0u);
  CHECK(run_frame(ScaleState{}, 1001, 100, wide).st.notch == 1u);
  CHECK(run_frame(ScaleState{1, 0, 0}, 10, 100, wide).st.notch == 0u);
  CHECK(run_frame(ScaleState{1, 0, 0}, 11, 100, wide).st.notch == 1u);
}

TEST_CASE("on_frame saturating thresholds", "[pacing][frame]") {
  const ScaleConfig c = make_cfg(3, 1, 1, 110, 90);
  // budget UINT64_MAX: the exact overrun threshold does not fit, so it saturates and nothing is ever an overrun.
  CHECK(run_frame(ScaleState{}, kU64, kU64, c).st.notch == 0u);
  CHECK(run_frame(ScaleState{}, kU64 - 1, kU64, c).st.notch == 0u);
  // The fast threshold is exact: floor(UINT64_MAX * 90 / 100) = 16602069666338596453.
  const std::uint64_t fast = 16602069666338596453ull;
  CHECK(run_frame(ScaleState{1, 0, 0}, fast, kU64, c).st.notch == 0u);
  CHECK(run_frame(ScaleState{1, 0, 0}, fast + 1, kU64, c).st.notch == 1u);
  // Exactly at the edge of saturation with percent 1000.
  const ScaleConfig wide = make_cfg(3, 1, 1, 1000, 10);
  CHECK(run_frame(ScaleState{}, kU64, kU64 / 10, wide).st.notch == 1u);       // threshold 18446744073709551610
  CHECK(run_frame(ScaleState{}, kU64, kU64 / 10 + 1, wide).st.notch == 0u);  // exact value 18446744073709551620 saturates
  CHECK(run_frame(ScaleState{}, kU64 - 4, kU64 / 10, wide).st.notch == 1u);  // one above ...610
  CHECK(run_frame(ScaleState{}, kU64 - 5, kU64 / 10, wide).st.notch == 0u);  // equal to the threshold
  // A big budget with a small frame.
  CHECK(run_frame(ScaleState{2, 0, 0}, 5, kU64, c).st.notch == 1u);
  // Budgets whose product with the percent is above 2^64 but whose quotient fits.
  // 2^62 * 110 is above 2^64, the quotient 5072854620270126694 fits.
  CHECK(run_frame(ScaleState{}, 5072854620270126694ull, 1ull << 62, c).st.notch == 0u);  // equal to the threshold
  CHECK(run_frame(ScaleState{}, 5072854620270126695ull, 1ull << 62, c).st.notch == 1u);
}

TEST_CASE("on_frame run counters at their limits", "[pacing][frame]") {
  // A frame-count limit of UINT32_MAX fires on reaching it.
  const ScaleConfig big = make_cfg(3, kU32, kU32, 110, 90);
  Step r = run_frame(ScaleState{0, kU32 - 1, 0}, 200, 100, big);
  CHECK(r.changed);
  CHECK(r.st.notch == 1u);
  CHECK(r.st.overrun_run == 0u);
  r = run_frame(ScaleState{0, kU32 - 2, 0}, 200, 100, big);
  CHECK_FALSE(r.changed);
  CHECK(r.st.overrun_run == kU32 - 1);
  r = run_frame(ScaleState{0, kU32, 0}, 200, 100, big);  // already saturated: stays saturated, still fires
  CHECK(r.changed);
  CHECK(r.st.notch == 1u);
  CHECK(r.st.overrun_run == 0u);
  r = run_frame(ScaleState{2, 0, kU32 - 1}, 50, 100, big);
  CHECK(r.changed);
  CHECK(r.st.notch == 1u);
  CHECK(r.st.fast_run == 0u);
  r = run_frame(ScaleState{2, 0, kU32}, 50, 100, big);
  CHECK(r.changed);
  CHECK(r.st.notch == 1u);
  // The overrun run keeps counting at max_notch and saturates there.
  const ScaleConfig two = make_cfg(3, 2, 2, 110, 90);
  r = run_frame(ScaleState{3, 5, 0}, 200, 100, two);
  CHECK_FALSE(r.changed);
  CHECK(r.st.notch == 3u);
  CHECK(r.st.overrun_run == 6u);
  r = run_frame(ScaleState{3, kU32 - 1, 0}, 200, 100, two);
  CHECK(r.st.overrun_run == kU32);
  r = run_frame(ScaleState{3, kU32, 0}, 200, 100, two);
  CHECK_FALSE(r.changed);
  CHECK(r.st.overrun_run == kU32);
  // The fast run keeps counting at notch 0 and saturates there.
  r = run_frame(ScaleState{0, 0, 5}, 50, 100, two);
  CHECK_FALSE(r.changed);
  CHECK(r.st.fast_run == 6u);
  r = run_frame(ScaleState{0, 0, kU32}, 50, 100, two);
  CHECK(r.st.fast_run == kU32);
  CHECK(r.st.notch == 0u);
  // An overrun frame clears the fast run and a fast frame clears the overrun run, even at the edge notches.
  r = run_frame(ScaleState{3, 0, 9}, 200, 100, two);
  CHECK(r.st.fast_run == 0u);
  CHECK(r.st.overrun_run == 1u);
  r = run_frame(ScaleState{0, 9, 0}, 50, 100, two);
  CHECK(r.st.overrun_run == 0u);
  CHECK(r.st.fast_run == 1u);
  // The runs need to be consecutive: an in-between frame clears both.
  r = run_frame(ScaleState{1, 7, 8}, 100, 100, two);
  CHECK(r.ok);
  CHECK_FALSE(r.changed);
  CHECK(r.st.overrun_run == 0u);
  CHECK(r.st.fast_run == 0u);
}

TEST_CASE("on_frame notch above max_notch", "[pacing][frame]") {
  const ScaleConfig c = make_cfg(3, 30, 300, 110, 90);
  // In-between frame: the clamp alone moves the notch, and the in-between rule clears the runs.
  Step r = run_frame(ScaleState{9, 5, 7}, 100, 100, c);
  CHECK(r.ok);
  CHECK(r.changed);
  CHECK(r.st.notch == 3u);
  CHECK(r.st.overrun_run == 0u);
  CHECK(r.st.fast_run == 0u);
  // The clamp does not reset the runs: an overrun frame continues the run (5 -> 6), a fast frame continues 7 -> 8.
  r = run_frame(ScaleState{9, 5, 7}, 200, 100, c);
  CHECK(r.changed);
  CHECK(r.st.notch == 3u);
  CHECK(r.st.overrun_run == 6u);
  CHECK(r.st.fast_run == 0u);
  r = run_frame(ScaleState{9, 5, 7}, 50, 100, c);
  CHECK(r.changed);
  CHECK(r.st.notch == 3u);
  CHECK(r.st.overrun_run == 0u);
  CHECK(r.st.fast_run == 8u);
  r = run_frame(ScaleState{kU32, 0, 0}, 200, 100, c);
  CHECK(r.changed);
  CHECK(r.st.notch == 3u);
  // Clamp, then the rule: the clamped notch is what steps up or down.
  const ScaleConfig one = make_cfg(3, 1, 1, 110, 90);
  r = run_frame(ScaleState{4, 0, 0}, 50, 100, one);
  CHECK(r.changed);
  CHECK(r.st.notch == 2u);  // 4 -> 3 -> 2
  r = run_frame(ScaleState{4, 0, 0}, 200, 100, one);
  CHECK(r.changed);
  CHECK(r.st.notch == 3u);  // 4 -> 3, already at max, so no further step
  CHECK(r.st.overrun_run == 1u);
  // A notch equal to max_notch is not a clamp.
  r = run_frame(ScaleState{3, 0, 0}, 100, 100, one);
  CHECK_FALSE(r.changed);
  CHECK(r.st.notch == 3u);
  // A notch one above the max with an overrun stops at the max: changed compares with the original notch.
  r = run_frame(ScaleState{15, 0, 0}, 100, 100, make_cfg(15, 1, 1, 110, 90));
  CHECK_FALSE(r.changed);
  r = run_frame(ScaleState{16, 0, 0}, 100, 100, make_cfg(15, 1, 1, 110, 90));
  CHECK(r.changed);
  CHECK(r.st.notch == 15u);
}

TEST_CASE("on_frame step down and recovery", "[pacing][frame]") {
  const ScaleConfig c = make_cfg(3, 3, 2, 110, 90);
  ScaleState s;
  bool changed = false;
  CHECK(on_frame(&s, 200, 100, c, &changed));
  CHECK(on_frame(&s, 200, 100, c, &changed));
  CHECK_FALSE(changed);
  CHECK(s.overrun_run == 2u);
  CHECK(on_frame(&s, 200, 100, c, &changed));
  CHECK(changed);
  CHECK(s.notch == 1u);
  CHECK(s.overrun_run == 0u);
  for (int i = 0; i < 6; ++i) CHECK(on_frame(&s, 200, 100, c, nullptr));
  CHECK(s.notch == 3u);
  CHECK(on_frame(&s, 200, 100, c, &changed));  // at max: counts but does not move
  CHECK_FALSE(changed);
  CHECK(s.notch == 3u);
  CHECK(s.overrun_run == 1u);
  CHECK(on_frame(&s, 50, 100, c, &changed));
  CHECK_FALSE(changed);
  CHECK(s.fast_run == 1u);
  CHECK(s.overrun_run == 0u);
  CHECK(on_frame(&s, 50, 100, c, &changed));
  CHECK(changed);
  CHECK(s.notch == 2u);
  CHECK(s.fast_run == 0u);
}

TEST_CASE("on_thermal_throttle", "[pacing][thermal]") {
  const ScaleConfig c = make_cfg(3, 30, 300, 110, 90);
  ScaleState s{0, 4, 6};
  bool changed = false;
  CHECK(on_thermal_throttle(&s, c, &changed));
  CHECK(changed);
  CHECK(s.notch == 1u);
  CHECK(s.overrun_run == 0u);
  CHECK(s.fast_run == 0u);
  s = ScaleState{2, 4, 6};
  CHECK(on_thermal_throttle(&s, c, &changed));
  CHECK(changed);
  CHECK(s.notch == 3u);
  // At max_notch: no move, but both runs reset on every call.
  s = ScaleState{3, 4, 6};
  changed = true;
  CHECK(on_thermal_throttle(&s, c, &changed));
  CHECK_FALSE(changed);
  CHECK(s.notch == 3u);
  CHECK(s.overrun_run == 0u);
  CHECK(s.fast_run == 0u);
  // Above max_notch: clamp, no extra step, and changed because the final notch differs from the original.
  s = ScaleState{9, 4, 6};
  changed = false;
  CHECK(on_thermal_throttle(&s, c, &changed));
  CHECK(changed);
  CHECK(s.notch == 3u);
  CHECK(s.overrun_run == 0u);
  CHECK(s.fast_run == 0u);
  s = ScaleState{kU32, kU32, kU32};
  CHECK(on_thermal_throttle(&s, c, &changed));
  CHECK(changed);
  CHECK(s.notch == 3u);
  CHECK(s.overrun_run == 0u);
  CHECK(s.fast_run == 0u);
  s = ScaleState{4, 1, 1};
  CHECK(on_thermal_throttle(&s, c, nullptr));
  CHECK(s.notch == 3u);
  // Max notch 15 (the largest valid) and 1 (the smallest).
  s = ScaleState{14, 0, 0};
  CHECK(on_thermal_throttle(&s, make_cfg(15, 1, 1, 100, 100), &changed));
  CHECK(s.notch == 15u);
  CHECK(changed);
  CHECK(on_thermal_throttle(&s, make_cfg(15, 1, 1, 100, 100), &changed));
  CHECK(s.notch == 15u);
  CHECK_FALSE(changed);
  s = ScaleState{0, 0, 0};
  CHECK(on_thermal_throttle(&s, make_cfg(1, 1, 1, 100, 100), &changed));
  CHECK(s.notch == 1u);
  CHECK(changed);
  // Failures change nothing, including *changed.
  for (bool sentinel : {false, true}) {
    changed = sentinel;
    CHECK_FALSE(on_thermal_throttle(nullptr, c, &changed));
    CHECK(changed == sentinel);
    CHECK_FALSE(on_thermal_throttle(nullptr, c, nullptr));
    s = ScaleState{9, 4, 6};
    changed = sentinel;
    CHECK_FALSE(on_thermal_throttle(&s, make_cfg(0, 30, 300, 110, 90), &changed));
    CHECK(changed == sentinel);
    CHECK(s.notch == 9u);
    CHECK(s.overrun_run == 4u);
    CHECK(s.fast_run == 6u);
    CHECK_FALSE(on_thermal_throttle(&s, make_cfg(16, 30, 300, 110, 90), &changed));
    CHECK_FALSE(on_thermal_throttle(&s, make_cfg(3, 0, 300, 110, 90), &changed));
    CHECK_FALSE(on_thermal_throttle(&s, make_cfg(3, 30, 0, 110, 90), &changed));
    CHECK_FALSE(on_thermal_throttle(&s, make_cfg(3, 30, 300, 99, 90), &changed));
    CHECK_FALSE(on_thermal_throttle(&s, make_cfg(3, 30, 300, 1001, 90), &changed));
    CHECK_FALSE(on_thermal_throttle(&s, make_cfg(3, 30, 300, 110, 9), &changed));
    CHECK_FALSE(on_thermal_throttle(&s, make_cfg(3, 30, 300, 110, 101), &changed));
    CHECK(changed == sentinel);
    CHECK(s.notch == 9u);
    CHECK(s.overrun_run == 4u);
    CHECK(s.fast_run == 6u);
  }
}

TEST_CASE("on_frame digest and oracle over the boundary grid", "[pacing][frame][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0, thr_bad = 0;
  for (const RefCfg& rc : kRefFrameConfigs) {
    const ScaleConfig c = cfg_of(rc);
    for (std::uint64_t b : kRefBudgets) {
      std::vector<std::uint64_t> frames(std::begin(kRefFrameFixed), std::end(kRefFrameFixed));
      for (std::uint32_t pct : {rc.overrun_percent, rc.fast_percent}) {
        const std::uint64_t t = oracle_threshold(b, pct);
        for (int d : kRefFrameOffsets) {
          if (d < 0 && t < static_cast<std::uint64_t>(-d)) continue;
          if (d > 0 && t > kU64 - static_cast<std::uint64_t>(d)) continue;
          frames.push_back(d < 0 ? t - static_cast<std::uint64_t>(-d) : t + static_cast<std::uint64_t>(d));
        }
      }
      std::sort(frames.begin(), frames.end());
      frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
      for (std::uint64_t fr : frames) {
        for (std::uint32_t nt : kRefStartNotches)
          for (std::uint32_t orun : kRefStartRuns)
            for (std::uint32_t frun : kRefStartRuns) {
              ScaleState s{nt, orun, frun};
              bool changed = false;
              const bool ok = on_frame(&s, fr, b, c, &changed);
              Model m{nt, orun, frun};
              bool want_changed = false;
              const bool want_ok = oracle_frame(&m, fr, b, c, &want_changed);
              if (!ok || !want_ok || changed != want_changed || !same(s, m)) ++bad;
              feed(&f, changed, s);
              ++n;
            }
      }
    }
  }
  // Spot-check the oracle's thresholds against the documented examples so a wrong oracle cannot hide a wrong digest.
  if (oracle_threshold(100, 110) != 110 || oracle_threshold(101, 110) != 111 || oracle_threshold(kU64, 110) != kU64 ||
      oracle_threshold(kU64, 90) != 16602069666338596453ull || oracle_threshold(kU64 / 10, 1000) != 18446744073709551610ull ||
      oracle_threshold(kU64 / 10 + 1, 1000) != kU64 || oracle_threshold(0, 1000) != 0)
    ++thr_bad;
  CHECK(thr_bad == 0u);
  CHECK(bad == 0u);
  CHECK(n == kRefCases_frames);
  CHECK(f.h == kRefDigest_frames);
}

TEST_CASE("thermal digest and oracle over the state grid", "[pacing][thermal][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (const RefCfg& rc : kRefFrameConfigs) {
    const ScaleConfig c = cfg_of(rc);
    for (std::uint32_t nt : kRefStartNotches)
      for (std::uint32_t orun : kRefStartRuns)
        for (std::uint32_t frun : kRefStartRuns) {
          ScaleState s{nt, orun, frun};
          bool changed = false;
          const bool ok = on_thermal_throttle(&s, c, &changed);
          Model m{nt, orun, frun};
          bool want_changed = false;
          const bool want_ok = oracle_thermal(&m, c, &want_changed);
          if (!ok || !want_ok || changed != want_changed || !same(s, m)) ++bad;
          feed(&f, changed, s);
          ++n;
        }
  }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_thermal);
  CHECK(f.h == kRefDigest_thermal);
}

TEST_CASE("exhaustive frame and throttle sequences", "[pacing][sequence][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (const RefCfg& rc : kRefSeqConfigs) {
    const ScaleConfig c = cfg_of(rc);
    for (std::uint32_t nt : kRefSeqStartNotches) {
      const std::uint32_t total = 1u << (2 * kRefSeqLen);
      for (std::uint32_t code = 0; code < total; ++code) {
        ScaleState s{nt, 0, 0};
        Model m{nt, 0, 0};
        for (int i = 0; i < kRefSeqLen; ++i) {
          const unsigned sym = (code >> (2 * (kRefSeqLen - 1 - i))) & 3u;
          bool changed = false, want_changed = false;
          bool ok, want_ok;
          if (sym == 3) {
            ok = on_thermal_throttle(&s, c, &changed);
            want_ok = oracle_thermal(&m, c, &want_changed);
          } else {
            ok = on_frame(&s, kRefSeqFrames[sym], kRefSeqBudget, c, &changed);
            want_ok = oracle_frame(&m, kRefSeqFrames[sym], kRefSeqBudget, c, &want_changed);
          }
          if (!ok || !want_ok || changed != want_changed || !same(s, m)) ++bad;
          feed(&f, changed, s);
        }
        ++n;
      }
    }
  }
  CHECK(bad == 0u);
  CHECK(n == kRefCases_sequences);
  CHECK(f.h == kRefDigest_sequences);
}

TEST_CASE("the controller steps down under sustained overrun and stays inside its bounds", "[pacing][sequence]") {
  // Longer scripted run: 100 overrun frames, then 100 fast frames, with the default config.
  const ScaleConfig c;
  ScaleState s;
  std::uint32_t downs = 0, ups = 0;
  for (int i = 0; i < 100; ++i) {
    bool changed = false;
    REQUIRE(on_frame(&s, 120, 100, c, &changed));
    if (changed) ++downs;
    REQUIRE(s.notch <= c.max_notch);
  }
  CHECK(downs == 3u);  // 30 frames per step, three steps, then the run keeps counting at max_notch
  CHECK(s.notch == 3u);
  CHECK(s.overrun_run == 10u);
  for (int i = 0; i < 1000; ++i) {
    bool changed = false;
    REQUIRE(on_frame(&s, 80, 100, c, &changed));
    if (changed) ++ups;
    REQUIRE(s.notch <= c.max_notch);
  }
  CHECK(ups == 3u);  // 300 frames per step, three steps, then the run keeps counting at notch 0
  CHECK(s.notch == 0u);
  CHECK(s.fast_run == 100u);
}
