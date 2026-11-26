// Fixed-timestep accumulator: integer only, no 128-bit types. See fixed_step.h and docs/fixed-step.md.
#include "fixed_step.h"

#include <cstdint>
#include <limits>

namespace lumora::timing {

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();

// a + b, stopping at UINT64_MAX.
std::uint64_t saturating_add(std::uint64_t a, std::uint64_t b) { return a > kMax - b ? kMax : a + b; }

// floor(accumulator * 65536 / step) for accumulator < step, by 16 shift-and-subtract rounds on the remainder.
// The remainder r stays below step. 2r >= step is tested as r >= step - r, which cannot overflow.
std::uint32_t interpolate_q16(std::uint64_t accumulator, std::uint64_t step) {
  std::uint64_t r = accumulator;
  std::uint32_t q = 0;
  for (int i = 0; i < 16; ++i) {
    q <<= 1;
    if (r >= step - r) {
      r -= step - r;
      q |= 1u;
    } else {
      r += r;
    }
  }
  return q;
}

}  // namespace

bool valid(const StepConfig& c) { return c.step_ns >= 1 && c.max_steps >= 1; }

bool advance(StepState* state, std::uint64_t delta_ns, const StepConfig& config, StepResult* out) {
  if (state == nullptr || out == nullptr || !valid(config)) return false;
  const std::uint64_t step = config.step_ns;

  const std::uint64_t clamped = delta_ns < config.max_delta_ns ? delta_ns : config.max_delta_ns;
  std::uint64_t dropped = saturating_add(state->dropped_ns, delta_ns - clamped);

  std::uint64_t accumulator = saturating_add(state->accumulator_ns, clamped);

  const std::uint64_t wanted = accumulator / step;
  const std::uint64_t steps = wanted < config.max_steps ? wanted : config.max_steps;
  accumulator -= steps * step;

  if (wanted > config.max_steps) {
    const std::uint64_t remainder = accumulator % step;
    dropped = saturating_add(dropped, accumulator - remainder);
    accumulator = remainder;
  }

  out->steps = static_cast<std::uint32_t>(steps);
  out->alpha_q16 = interpolate_q16(accumulator, step);
  state->accumulator_ns = accumulator;
  state->dropped_ns = dropped;
  return true;
}

void reset_accumulator(StepState* state) {
  if (state != nullptr) state->accumulator_ns = 0;
}

}  // namespace lumora::timing
