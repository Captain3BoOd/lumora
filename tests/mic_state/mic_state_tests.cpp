// Tests for core/lifecycle/mic_state. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/mic_state_ref.py, an independent Python
// implementation whose table is written from the plan. Regenerate it with: python3 tools/mic_state_ref.py
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "mic_state.h"

using namespace lumora::lifecycle;

namespace {

// BEGIN GENERATED REFERENCE (tools/mic_state_ref.py)
// Expected next state, rows are states (Inactive, Active, Paused, Stopped), columns are events (UserActivate, Hidden, Visible, ServiceKilled, CaptureRefused, PermissionRevoked, UserStop).
const std::uint8_t kRefNext[4][7] = {
    {1, 0, 0, 0, 0, 3, 0},
    {1, 2, 1, 3, 3, 3, 3},
    {2, 2, 1, 3, 3, 3, 3},
    {1, 3, 3, 3, 3, 3, 3},
};
const bool kRefService[4] = {false, true, true, false};
const bool kRefCapturing[4] = {false, true, false, false};
const char* const kRefStateNames[4] = {"Inactive", "Active", "Paused", "Stopped"};
const char* const kRefEventNames[7] = {"UserActivate", "Hidden", "Visible", "ServiceKilled", "CaptureRefused", "PermissionRevoked", "UserStop"};
const std::uint8_t kRefBadStates[] = {4, 5, 127, 128, 255};
const std::uint8_t kRefBadEvents[] = {7, 8, 127, 128, 255};
const std::uint64_t kRefDigest_pairs = 0xACDD917731E7F48Bull;
const std::uint64_t kRefDigest_sequences = 0xBEEEE96891075AF5ull;
const std::size_t kRefPairCases = 108;
const std::size_t kRefSequenceCases = 9604;
// END GENERATED REFERENCE

// FNV-1a 64 over single bytes, the same as tools/mic_state_ref.py.
struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
};

MicState st(unsigned v) { return static_cast<MicState>(static_cast<std::uint8_t>(v)); }
MicEvent ev(unsigned v) { return static_cast<MicEvent>(static_cast<std::uint8_t>(v)); }
unsigned num(MicState s) { return static_cast<unsigned>(static_cast<std::uint8_t>(s)); }

constexpr MicState kI = MicState::Inactive, kA = MicState::Active, kP = MicState::Paused, kS = MicState::Stopped;
constexpr MicEvent kAct = MicEvent::UserActivate, kHid = MicEvent::Hidden, kVis = MicEvent::Visible,
                   kKill = MicEvent::ServiceKilled, kRef = MicEvent::CaptureRefused,
                   kRev = MicEvent::PermissionRevoked, kStop = MicEvent::UserStop;

}  // namespace

TEST_CASE("enum values", "[mic]") {
  CHECK(num(kI) == 0u);
  CHECK(num(kA) == 1u);
  CHECK(num(kP) == 2u);
  CHECK(num(kS) == 3u);
  CHECK(static_cast<unsigned>(kAct) == 0u);
  CHECK(static_cast<unsigned>(kStop) == 6u);
}

TEST_CASE("every transition, written out", "[mic][table]") {
  // Inactive
  CHECK(next_state(kI, kAct) == kA);
  CHECK(next_state(kI, kHid) == kI);
  CHECK(next_state(kI, kVis) == kI);
  CHECK(next_state(kI, kKill) == kI);
  CHECK(next_state(kI, kRef) == kI);
  CHECK(next_state(kI, kRev) == kS);
  CHECK(next_state(kI, kStop) == kI);
  // Active
  CHECK(next_state(kA, kAct) == kA);
  CHECK(next_state(kA, kHid) == kP);
  CHECK(next_state(kA, kVis) == kA);
  CHECK(next_state(kA, kKill) == kS);
  CHECK(next_state(kA, kRef) == kS);
  CHECK(next_state(kA, kRev) == kS);
  CHECK(next_state(kA, kStop) == kS);
  // Paused
  CHECK(next_state(kP, kAct) == kP);
  CHECK(next_state(kP, kHid) == kP);
  CHECK(next_state(kP, kVis) == kA);
  CHECK(next_state(kP, kKill) == kS);
  CHECK(next_state(kP, kRef) == kS);
  CHECK(next_state(kP, kRev) == kS);
  CHECK(next_state(kP, kStop) == kS);
  // Stopped: only a user activation leaves it
  CHECK(next_state(kS, kAct) == kA);
  CHECK(next_state(kS, kHid) == kS);
  CHECK(next_state(kS, kVis) == kS);
  CHECK(next_state(kS, kKill) == kS);
  CHECK(next_state(kS, kRef) == kS);
  CHECK(next_state(kS, kRev) == kS);
  CHECK(next_state(kS, kStop) == kS);
}

TEST_CASE("no automatic restart out of Stopped", "[mic]") {
  for (unsigned e = 0; e < 7; ++e) {
    if (ev(e) == kAct) continue;
    CHECK(next_state(kS, ev(e)) == kS);
  }
  // Hidden then Visible does not restart a stopped capture, nor does a refused-then-visible cycle.
  MicState s = kS;
  s = next_state(s, kHid);
  s = next_state(s, kVis);
  s = next_state(s, kVis);
  CHECK(s == kS);
  // Stopped from every path, and Visible alone never restarts.
  CHECK(next_state(next_state(kA, kKill), kVis) == kS);
  CHECK(next_state(next_state(kP, kRef), kVis) == kS);
  CHECK(next_state(next_state(kI, kRev), kVis) == kS);
  // The table can return Active after a revoked permission when the user activates again; the permission and
  // capability checks that refuse capture live outside this table.
  CHECK(next_state(next_state(kA, kRev), kAct) == kA);
}

TEST_CASE("hide and show cycle", "[mic]") {
  MicState s = next_state(kI, kAct);
  CHECK(s == kA);
  s = next_state(s, kHid);
  CHECK(s == kP);
  s = next_state(s, kVis);
  CHECK(s == kA);
  s = next_state(s, kKill);
  CHECK(s == kS);
  s = next_state(s, kAct);
  CHECK(s == kA);
}

TEST_CASE("flags", "[mic]") {
  CHECK_FALSE(service_running(kI));
  CHECK(service_running(kA));
  CHECK(service_running(kP));
  CHECK_FALSE(service_running(kS));
  CHECK_FALSE(capturing(kI));
  CHECK(capturing(kA));
  CHECK_FALSE(capturing(kP));
  CHECK_FALSE(capturing(kS));
  for (unsigned v : {4u, 5u, 127u, 128u, 254u, 255u}) {
    CHECK_FALSE(service_running(st(v)));
    CHECK_FALSE(capturing(st(v)));
  }
  // Capturing implies the service is running, for every valid state.
  for (unsigned s = 0; s < 4; ++s) {
    if (capturing(st(s))) CHECK(service_running(st(s)));
  }
}

TEST_CASE("names", "[mic]") {
  CHECK(std::strcmp(to_string(kI), "Inactive") == 0);
  CHECK(std::strcmp(to_string(kA), "Active") == 0);
  CHECK(std::strcmp(to_string(kP), "Paused") == 0);
  CHECK(std::strcmp(to_string(kS), "Stopped") == 0);
  CHECK(std::strcmp(to_string(kAct), "UserActivate") == 0);
  CHECK(std::strcmp(to_string(kHid), "Hidden") == 0);
  CHECK(std::strcmp(to_string(kVis), "Visible") == 0);
  CHECK(std::strcmp(to_string(kKill), "ServiceKilled") == 0);
  CHECK(std::strcmp(to_string(kRef), "CaptureRefused") == 0);
  CHECK(std::strcmp(to_string(kRev), "PermissionRevoked") == 0);
  CHECK(std::strcmp(to_string(kStop), "UserStop") == 0);
  for (unsigned v : {4u, 5u, 127u, 128u, 255u}) {
    CHECK(to_string(st(v)) != nullptr);
    CHECK(std::strcmp(to_string(st(v)), "unknown") == 0);
  }
  for (unsigned v : {7u, 8u, 127u, 128u, 255u}) {
    CHECK(to_string(ev(v)) != nullptr);
    CHECK(std::strcmp(to_string(ev(v)), "unknown") == 0);
  }
}

TEST_CASE("out-of-range enums", "[mic]") {
  for (unsigned s = 0; s < 4; ++s)
    for (unsigned e : {7u, 8u, 127u, 128u, 255u}) CHECK(next_state(st(s), ev(e)) == st(s));
  for (unsigned s : {4u, 5u, 127u, 128u, 255u}) {
    CHECK(next_state(st(s), kAct) == kA);  // treated as Stopped, then activated
    for (unsigned e : {1u, 2u, 3u, 4u, 5u, 6u}) CHECK(next_state(st(s), ev(e)) == kS);
    for (unsigned e : {7u, 8u, 127u, 128u, 255u}) CHECK(next_state(st(s), ev(e)) == kS);
  }
}

TEST_CASE("generated table, every state and event pair including invalid values", "[mic][table]") {
  for (unsigned s = 0; s < 4; ++s)
    for (unsigned e = 0; e < 7; ++e) {
      if (num(next_state(st(s), ev(e))) != kRefNext[s][e]) FAIL("table mismatch: state " << s << " event " << e);
    }
  for (unsigned s = 0; s < 4; ++s) {
    CHECK(service_running(st(s)) == kRefService[s]);
    CHECK(capturing(st(s)) == kRefCapturing[s]);
    CHECK(std::strcmp(to_string(st(s)), kRefStateNames[s]) == 0);
  }
  for (unsigned e = 0; e < 7; ++e) CHECK(std::strcmp(to_string(ev(e)), kRefEventNames[e]) == 0);

  Fnv f;
  std::size_t n = 0;
  std::uint8_t states[9], events[12];
  std::size_t ns = 0, ne = 0;
  for (std::uint8_t s = 0; s < 4; ++s) states[ns++] = s;
  for (std::uint8_t s : kRefBadStates) states[ns++] = s;
  for (std::uint8_t e = 0; e < 7; ++e) events[ne++] = e;
  for (std::uint8_t e : kRefBadEvents) events[ne++] = e;
  for (std::size_t i = 0; i < ns; ++i)
    for (std::size_t j = 0; j < ne; ++j) {
      f.byte(static_cast<std::uint8_t>(next_state(st(states[i]), ev(events[j]))));
      ++n;
    }
  CHECK(n == kRefPairCases);
  CHECK(f.h == kRefDigest_pairs);
}

TEST_CASE("every event sequence of length 4 from every state matches the digest", "[mic][table]") {
  Fnv f;
  std::size_t n = 0;
  for (unsigned s = 0; s < 4; ++s)
    for (unsigned a = 0; a < 7; ++a)
      for (unsigned b = 0; b < 7; ++b)
        for (unsigned c = 0; c < 7; ++c)
          for (unsigned d = 0; d < 7; ++d) {
            MicState cur = st(s);
            for (unsigned e : {a, b, c, d}) {
              cur = next_state(cur, ev(e));
              f.byte(static_cast<std::uint8_t>(cur));
            }
            ++n;
          }
  CHECK(n == kRefSequenceCases);
  CHECK(f.h == kRefDigest_sequences);
}
