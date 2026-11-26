// Render gate: a pure state machine over surface, visible and screen_on. See render_gate.h for the contract and
// docs/render_gate.md for the check order and the bounds.
#include "render_gate.h"

#include <cstdint>

namespace lumora::lifecycle {

bool should_render(const RenderGate& g) { return g.surface && g.visible && g.screen_on; }

bool apply_event(RenderGate* g, GateEvent event, GateAction* action) {
  if (g == nullptr) return false;
  switch (event) {
    case GateEvent::SurfaceCreated: g->surface = true; break;
    case GateEvent::SurfaceDestroyed: g->surface = false; break;
    case GateEvent::VisibleTrue: g->visible = true; break;
    case GateEvent::VisibleFalse: g->visible = false; break;
    case GateEvent::ScreenOn: g->screen_on = true; break;
    case GateEvent::ScreenOff: g->screen_on = false; break;
    default: return false;
  }
  const bool was = g->running;
  g->running = should_render(*g);
  if (action != nullptr) {
    *action = (!was && g->running) ? GateAction::Start : (was && !g->running) ? GateAction::Stop : GateAction::None;
  }
  return true;
}

const char* to_string(GateEvent e) {
  switch (e) {
    case GateEvent::SurfaceCreated: return "SurfaceCreated";
    case GateEvent::SurfaceDestroyed: return "SurfaceDestroyed";
    case GateEvent::VisibleTrue: return "VisibleTrue";
    case GateEvent::VisibleFalse: return "VisibleFalse";
    case GateEvent::ScreenOn: return "ScreenOn";
    case GateEvent::ScreenOff: return "ScreenOff";
  }
  return "unknown";
}

const char* to_string(GateAction a) {
  switch (a) {
    case GateAction::None: return "None";
    case GateAction::Start: return "Start";
    case GateAction::Stop: return "Stop";
  }
  return "unknown";
}

}  // namespace lumora::lifecycle
