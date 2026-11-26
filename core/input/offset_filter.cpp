// Wallpaper offset filter: dead zone and integer smoothing for a two-axis Q16 value. See offset_filter.h for the
// contract and docs/offset_filter.md for the check order and the bounds. Integer only: no libm, no 128-bit types,
// no allocation.
#include "offset_filter.h"

#include <cstdint>

namespace lumora::input {

namespace {

std::int32_t clamp_one(std::int32_t v) {
  if (v > kOne) return kOne;
  if (v < -kOne) return -kOne;
  return v;
}

// Dead zone on an input already clamped to [-kOne, kOne].
std::int32_t dead_zone(std::int32_t v, std::int32_t deadzone) {
  const std::int64_t m = v < 0 ? -static_cast<std::int64_t>(v) : static_cast<std::int64_t>(v);
  if (m <= deadzone) return 0;
  const std::int64_t scaled = (m - deadzone) * kOne / (kOne - deadzone);
  return static_cast<std::int32_t>(v < 0 ? -scaled : scaled);
}

std::int32_t smooth(std::int32_t state, std::int32_t value, std::uint32_t smooth_percent) {
  const std::int64_t delta = static_cast<std::int64_t>(value) - state;
  const std::int64_t step = delta * static_cast<std::int64_t>(100 - smooth_percent) / 100;
  return static_cast<std::int32_t>(state + step);
}

}  // namespace

bool valid(const OffsetFilterConfig& c) { return c.smooth_percent <= 99 && c.deadzone >= 0 && c.deadzone <= kMaxDeadzone; }

bool reset(OffsetFilter* f) {
  if (f == nullptr) return false;
  f->x = 0;
  f->y = 0;
  f->primed = false;
  return true;
}

bool step(OffsetFilter* f, std::int32_t in_x, std::int32_t in_y, const OffsetFilterConfig& config, std::int32_t* out_x,
          std::int32_t* out_y) {
  if (f == nullptr || out_x == nullptr || out_y == nullptr) return false;
  if (!valid(config)) return false;
  const std::int32_t vx = dead_zone(clamp_one(in_x), config.deadzone);
  const std::int32_t vy = dead_zone(clamp_one(in_y), config.deadzone);
  f->x = clamp_one(f->x);
  f->y = clamp_one(f->y);
  if (!f->primed) {
    f->x = vx;
    f->y = vy;
  } else {
    f->x = smooth(f->x, vx, config.smooth_percent);
    f->y = smooth(f->y, vy, config.smooth_percent);
  }
  f->primed = true;
  *out_x = f->x;
  *out_y = f->y;
  return true;
}

}  // namespace lumora::input
