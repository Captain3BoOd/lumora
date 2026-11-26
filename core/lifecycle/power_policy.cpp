// Power policy: render scale percent, quality preset and scaled pixel size. See power_policy.h for the contract and
// docs/power_policy.md for the check order and the bounds. Integer only: no libm, no 128-bit types, no allocation.
#include "power_policy.h"

#include <cstdint>

namespace lumora::lifecycle {

namespace {

constexpr std::uint32_t kMaxNotchLimit = 15;

bool notch_range_ok(std::uint32_t max_notch) { return max_notch >= 1 && max_notch <= kMaxNotchLimit; }

std::uint32_t clamp_notch(std::uint32_t notch, std::uint32_t max_notch) { return notch > max_notch ? max_notch : notch; }

}  // namespace

bool render_scale_percent(std::uint32_t notch, std::uint32_t max_notch, std::uint32_t* percent_out) {
  if (percent_out == nullptr) return false;
  if (!notch_range_ok(max_notch)) return false;
  const std::uint32_t n = clamp_notch(notch, max_notch);
  *percent_out = 100 - (50 * n) / max_notch;
  return true;
}

bool quality_preset(bool battery_saver, std::uint32_t notch, std::uint32_t max_notch, Quality* out) {
  if (out == nullptr) return false;
  if (!notch_range_ok(max_notch)) return false;
  const std::uint32_t effective = battery_saver ? max_notch : clamp_notch(notch, max_notch);
  if (effective == 0) {
    *out = Quality::High;
  } else if (static_cast<std::uint64_t>(effective) * 2 >= max_notch) {
    *out = Quality::Low;
  } else {
    *out = Quality::Medium;
  }
  return true;
}

bool scaled_size(std::uint32_t size, std::uint32_t percent, std::uint32_t* out) {
  if (out == nullptr) return false;
  if (percent < 1 || percent > 100) return false;
  const std::uint64_t scaled = static_cast<std::uint64_t>(size) * percent / 100;
  *out = scaled < 1 ? 1u : static_cast<std::uint32_t>(scaled);
  return true;
}

const char* to_string(Quality q) {
  switch (q) {
    case Quality::High: return "High";
    case Quality::Medium: return "Medium";
    case Quality::Low: return "Low";
  }
  return "unknown";
}

}  // namespace lumora::lifecycle
