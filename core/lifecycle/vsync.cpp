// Vsync period estimator. See vsync.h for the contract and docs/vsync.md for the bounds and the check order.
// Integer only: no libm, no 128-bit types, no allocation.
#include "vsync.h"

#include <cstddef>
#include <cstdint>

namespace lumora::lifecycle {

namespace {

bool in_range(const VsyncEstimator& e) { return e.count <= kVsyncWindow && e.next < kVsyncWindow; }

}  // namespace

bool reset(VsyncEstimator* e) {
  if (e == nullptr) return false;
  *e = VsyncEstimator{};
  return true;
}

bool on_vsync(VsyncEstimator* e, std::uint64_t timestamp_ns) {
  if (e == nullptr) return false;
  if (!in_range(*e)) {
    e->has_last = false;
    e->count = 0;
    e->next = 0;
  }
  if (!e->has_last) {
    e->last_ns = timestamp_ns;
    e->has_last = true;
    return true;
  }
  if (timestamp_ns <= e->last_ns) return false;
  const std::uint64_t delta = timestamp_ns - e->last_ns;
  if (delta > kVsyncGapNs) {
    e->count = 0;
    e->next = 0;
    e->last_ns = timestamp_ns;
    return true;
  }
  e->deltas[e->next] = delta;
  e->next = e->next + 1 == kVsyncWindow ? 0 : e->next + 1;
  if (e->count < kVsyncWindow) e->count += 1;
  e->last_ns = timestamp_ns;
  return true;
}

bool estimate_vsync_ns(const VsyncEstimator* e, std::uint64_t* out) {
  if (e == nullptr || out == nullptr) return false;
  if (!in_range(*e) || e->count < kVsyncMinSamples) return false;
  const std::size_t count = e->count;
  std::uint64_t w[kVsyncWindow];
  for (std::size_t i = 0; i < count; ++i) {
    const std::uint64_t v = e->deltas[(e->next + kVsyncWindow - count + i) % kVsyncWindow];
    std::size_t j = i;  // insertion into the sorted prefix, ascending
    while (j > 0 && w[j - 1] > v) {
      w[j] = w[j - 1];
      --j;
    }
    w[j] = v;
  }
  const std::uint64_t median = w[(count - 1) / 2];
  if (median == 0) return false;
  *out = median;
  return true;
}

}  // namespace lumora::lifecycle
