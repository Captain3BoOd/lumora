// Crash-loop breaker: per-backend crash logs, a time window and the Run/Disable decision. Pure functions on small
// values. See crash_breaker.h and docs/crash-breaker.md.
#include "crash_breaker.h"

#include <cstdint>

namespace lumora::lifecycle {

namespace {

// A crash at time t is inside the window when age < window_ms, with age 0 for a time in the future.
bool inside_window(std::uint64_t t, std::uint64_t now_ms, std::uint64_t window_ms) {
  const std::uint64_t age = t > now_ms ? 0 : now_ms - t;
  return age < window_ms;
}

std::uint32_t usable_count(const CrashLog& log) { return log.count > kMaxCrashes ? kMaxCrashes : log.count; }

}  // namespace

bool valid(const Policy& p) {
  return p.max_crashes >= 1 && p.max_crashes <= kMaxCrashes && p.backend_crashes >= 1 &&
         p.backend_crashes <= p.max_crashes && p.window_ms >= 1;
}

std::uint32_t recent_crashes(const CrashLog& log, std::uint64_t now_ms, std::uint64_t window_ms) {
  const std::uint32_t n = usable_count(log);
  std::uint32_t recent = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    if (inside_window(log.times_ms[i], now_ms, window_ms)) ++recent;
  }
  return recent;
}

bool record_crash(CrashLog* log, std::uint64_t now_ms, const Policy& policy) {
  if (log == nullptr || !valid(policy)) return false;

  // The corrupted-count clamp comes first, then pruning, then the append.
  const std::uint32_t n = usable_count(*log);
  std::uint64_t kept[kMaxCrashes] = {};
  std::uint32_t k = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    if (inside_window(log->times_ms[i], now_ms, policy.window_ms)) kept[k++] = log->times_ms[i];
  }

  std::uint32_t first = 0;
  if (k == kMaxCrashes) first = 1;  // still full after pruning: the oldest entry goes

  std::uint32_t out = 0;
  for (std::uint32_t i = first; i < k; ++i) log->times_ms[out++] = kept[i];
  log->times_ms[out++] = now_ms;
  log->count = out;
  for (std::uint32_t i = out; i < kMaxCrashes; ++i) log->times_ms[i] = 0;
  return true;
}

bool decide(const CrashLog* vulkan, const CrashLog* gles, std::uint64_t now_ms, const Policy& policy,
            bool gles_available, Decision* out) {
  if (out == nullptr || !valid(policy)) return false;
  const std::uint32_t recent_vulkan = vulkan != nullptr ? recent_crashes(*vulkan, now_ms, policy.window_ms) : 0;
  const std::uint32_t recent_gles = gles != nullptr ? recent_crashes(*gles, now_ms, policy.window_ms) : 0;

  Decision d;
  if (recent_vulkan + recent_gles >= policy.max_crashes) {
    d.action = Action::Disable;
    d.backend = Backend::Vulkan;
  } else if (gles_available && recent_vulkan >= policy.backend_crashes) {
    d.action = Action::Run;
    d.backend = Backend::Gles;
  } else {
    d.action = Action::Run;
    d.backend = Backend::Vulkan;
  }
  *out = d;
  return true;
}

const char* to_string(Action a) {
  switch (a) {
    case Action::Run: return "Run";
    case Action::Disable: return "Disable";
  }
  return "unknown";
}

const char* to_string(Backend b) {
  switch (b) {
    case Backend::Vulkan: return "Vulkan";
    case Backend::Gles: return "Gles";
  }
  return "unknown";
}

}  // namespace lumora::lifecycle
