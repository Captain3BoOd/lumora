// Frame pacing and render-scale control. See pacing.h for the contract and docs/pacing.md for the check order.
// Integer only: no libm, no 128-bit types, no floating point.
#include "pacing.h"

#include <algorithm>
#include <cstdint>

namespace lumora::lifecycle {

namespace {

constexpr std::uint64_t kMaxU64 = UINT64_MAX;
constexpr std::uint32_t kMaxU32 = UINT32_MAX;
constexpr std::uint64_t kBatterySaverNs = 33333333;  // 30 fps, rounded half up
constexpr std::uint32_t kMaxNotchLimit = 15;

bool cap_in_range(FrameCap cap) { return static_cast<std::uint8_t>(cap) <= static_cast<std::uint8_t>(FrameCap::Fps120); }

// floor(budget * percent / 100) without a wide type: budget = 100 * q + r, so the value is q * percent + r * percent / 100
// (q * percent is an integer, and r * percent / 100 is below 1000). Saturates at UINT64_MAX only when the exact value
// does not fit.
std::uint64_t threshold(std::uint64_t budget, std::uint32_t percent) {
  const std::uint64_t q = budget / 100;
  const std::uint64_t r = budget % 100;
  const std::uint64_t p = percent;
  if (q != 0 && p > kMaxU64 / q) return kMaxU64;
  const std::uint64_t whole = q * p;
  const std::uint64_t part = r * p / 100;
  if (part > kMaxU64 - whole) return kMaxU64;
  return whole + part;
}

std::uint32_t bump(std::uint32_t v) { return v == kMaxU32 ? v : v + 1; }

}  // namespace

std::uint64_t cap_period_ns(FrameCap cap) {
  switch (cap) {
    case FrameCap::Fps30: return 33333333;
    case FrameCap::Fps60: return 16666667;
    case FrameCap::Fps90: return 11111111;
    case FrameCap::Fps120: return 8333333;
    case FrameCap::Auto: return 0;
  }
  return 0;
}

bool frame_interval_ns(const PacingInput& in, std::uint64_t* out) {
  if (out == nullptr || !cap_in_range(in.cap) || in.vsync_period_ns == 0) return false;
  const std::uint64_t vsync = in.vsync_period_ns;
  std::uint64_t desired = in.cap == FrameCap::Auto ? vsync : cap_period_ns(in.cap);
  if (in.battery_saver) desired = std::max(desired, kBatterySaverNs);
  desired = std::max(desired, vsync);
  const std::uint64_t q = desired / vsync;
  const std::uint64_t r = desired % vsync;
  const std::uint64_t tolerance = vsync / 20;
  const std::uint64_t k = q + (r > tolerance ? 1 : 0);
  *out = k * vsync;
  return true;
}

bool valid(const ScaleConfig& c) {
  return c.max_notch >= 1 && c.max_notch <= kMaxNotchLimit && c.overrun_frames >= 1 && c.recover_frames >= 1 &&
         c.overrun_percent >= 100 && c.overrun_percent <= 1000 && c.fast_percent >= 10 && c.fast_percent <= 100;
}

bool on_frame(ScaleState* state, std::uint64_t frame_ns, std::uint64_t budget_ns, const ScaleConfig& config,
              bool* changed) {
  if (state == nullptr || !valid(config) || budget_ns == 0) return false;
  const std::uint32_t original = state->notch;
  ScaleState s = *state;
  if (s.notch > config.max_notch) s.notch = config.max_notch;
  const std::uint64_t over = threshold(budget_ns, config.overrun_percent);
  const std::uint64_t fast = threshold(budget_ns, config.fast_percent);
  if (frame_ns > over) {
    s.overrun_run = bump(s.overrun_run);
    s.fast_run = 0;
    if (s.overrun_run >= config.overrun_frames && s.notch < config.max_notch) {
      s.notch += 1;
      s.overrun_run = 0;
    }
  } else if (frame_ns <= fast) {
    s.fast_run = bump(s.fast_run);
    s.overrun_run = 0;
    if (s.fast_run >= config.recover_frames && s.notch > 0) {
      s.notch -= 1;
      s.fast_run = 0;
    }
  } else {
    s.overrun_run = 0;
    s.fast_run = 0;
  }
  *state = s;
  if (changed != nullptr) *changed = s.notch != original;
  return true;
}

bool on_thermal_throttle(ScaleState* state, const ScaleConfig& config, bool* changed) {
  if (state == nullptr || !valid(config)) return false;
  const std::uint32_t original = state->notch;
  std::uint32_t notch = std::min(original, config.max_notch);
  if (notch < config.max_notch) notch += 1;
  state->notch = notch;
  state->overrun_run = 0;
  state->fast_run = 0;
  if (changed != nullptr) *changed = notch != original;
  return true;
}

const char* to_string(FrameCap cap) {
  switch (cap) {
    case FrameCap::Auto: return "Auto";
    case FrameCap::Fps30: return "Fps30";
    case FrameCap::Fps60: return "Fps60";
    case FrameCap::Fps90: return "Fps90";
    case FrameCap::Fps120: return "Fps120";
  }
  return "unknown";
}

}  // namespace lumora::lifecycle
