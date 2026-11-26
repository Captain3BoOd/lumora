// Microphone capture state machine: a pure transition table. See mic_state.h and docs/mic-state.md.
#include "mic_state.h"

#include <cstdint>

namespace lumora::lifecycle {

namespace {

bool valid_state(MicState s) { return static_cast<std::uint8_t>(s) <= static_cast<std::uint8_t>(MicState::Stopped); }

}  // namespace

MicState next_state(MicState state, MicEvent event) {
  // A state outside the enum is treated as Stopped (fail safe), then the event is applied to it.
  const MicState s = valid_state(state) ? state : MicState::Stopped;
  switch (s) {
    case MicState::Inactive:
      if (event == MicEvent::UserActivate) return MicState::Active;
      if (event == MicEvent::PermissionRevoked) return MicState::Stopped;
      return s;
    case MicState::Active:
      if (event == MicEvent::Hidden) return MicState::Paused;
      if (event == MicEvent::ServiceKilled || event == MicEvent::CaptureRefused ||
          event == MicEvent::PermissionRevoked || event == MicEvent::UserStop) {
        return MicState::Stopped;
      }
      return s;
    case MicState::Paused:
      if (event == MicEvent::Visible) return MicState::Active;
      if (event == MicEvent::ServiceKilled || event == MicEvent::CaptureRefused ||
          event == MicEvent::PermissionRevoked || event == MicEvent::UserStop) {
        return MicState::Stopped;
      }
      return s;
    case MicState::Stopped:
      // Only a user activation leaves Stopped: there is no automatic restart.
      if (event == MicEvent::UserActivate) return MicState::Active;
      return s;
  }
  return MicState::Stopped;
}

bool service_running(MicState state) { return state == MicState::Active || state == MicState::Paused; }

bool capturing(MicState state) { return state == MicState::Active; }

const char* to_string(MicState state) {
  switch (state) {
    case MicState::Inactive: return "Inactive";
    case MicState::Active: return "Active";
    case MicState::Paused: return "Paused";
    case MicState::Stopped: return "Stopped";
  }
  return "unknown";
}

const char* to_string(MicEvent event) {
  switch (event) {
    case MicEvent::UserActivate: return "UserActivate";
    case MicEvent::Hidden: return "Hidden";
    case MicEvent::Visible: return "Visible";
    case MicEvent::ServiceKilled: return "ServiceKilled";
    case MicEvent::CaptureRefused: return "CaptureRefused";
    case MicEvent::PermissionRevoked: return "PermissionRevoked";
    case MicEvent::UserStop: return "UserStop";
  }
  return "unknown";
}

}  // namespace lumora::lifecycle
