// Diagnostics log ring: the 64 most recent events, a dropped counter and an FNV-1a 64 digest. See diag_log.h for the
// contract and docs/diag_log.md for the check order and the bounds. Integer only: no libm, no 128-bit types, no
// allocation, no formatting.
#include "diag_log.h"

#include <cstdint>

namespace lumora::diag {

namespace {

constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ull;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ull;

bool state_ok(const DiagLog& log) { return log.count <= kLogCapacity && log.next < kLogCapacity; }

void fnv_word(std::uint64_t* h, std::uint32_t w) {
  for (int i = 0; i < 4; ++i) {
    *h ^= static_cast<std::uint8_t>(w >> (8 * i));
    *h *= kFnvPrime;
  }
}

}  // namespace

bool log_clear(DiagLog* log) {
  if (log == nullptr) return false;
  *log = DiagLog{};
  return true;
}

bool log_push(DiagLog* log, std::uint64_t timestamp_ns, std::uint16_t code, std::int32_t a, std::int32_t b) {
  if (log == nullptr) return false;
  if (!state_ok(*log)) {
    log->count = 0;
    log->next = 0;
  }
  LogEntry& e = log->entries[log->next];
  e.timestamp_ns = timestamp_ns;
  e.code = code;
  e.a = a;
  e.b = b;
  log->next = log->next + 1 == kLogCapacity ? 0 : log->next + 1;
  if (log->count < kLogCapacity) {
    log->count += 1;
  } else if (log->dropped != UINT64_MAX) {
    log->dropped += 1;
  }
  return true;
}

bool log_read(const DiagLog* log, std::uint32_t index, LogEntry* out) {
  if (log == nullptr || out == nullptr) return false;
  if (!state_ok(*log) || index >= log->count) return false;
  *out = log->entries[(log->next + kLogCapacity - log->count + index) % kLogCapacity];
  return true;
}

std::uint32_t log_size(const DiagLog* log) {
  if (log == nullptr || !state_ok(*log)) return 0;
  return log->count;
}

std::uint64_t log_digest(const DiagLog* log) {
  std::uint64_t h = kFnvOffset;
  if (log == nullptr) return h;
  if (state_ok(*log)) {
    for (std::uint32_t i = 0; i < log->count; ++i) {
      const LogEntry& e = log->entries[(log->next + kLogCapacity - log->count + i) % kLogCapacity];
      fnv_word(&h, static_cast<std::uint32_t>(e.timestamp_ns));
      fnv_word(&h, static_cast<std::uint32_t>(e.timestamp_ns >> 32));
      fnv_word(&h, static_cast<std::uint32_t>(e.code));
      fnv_word(&h, static_cast<std::uint32_t>(e.a));
      fnv_word(&h, static_cast<std::uint32_t>(e.b));
    }
  }
  fnv_word(&h, static_cast<std::uint32_t>(log->dropped));
  fnv_word(&h, static_cast<std::uint32_t>(log->dropped >> 32));
  return h;
}

}  // namespace lumora::diag
