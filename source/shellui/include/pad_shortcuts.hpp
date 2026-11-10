/* Controller shortcuts for the fps-monitor overlay. Pure (standard library
 * only, host-tested by tests/host/pad_test.cpp); the GamePad.GetData hook in
 * fps_monitor/shellui/monitor_overlay.cpp feeds it.
 *   touchpad x3   overlay on / off (three presses, each within kClickGapMs)
 *   hold L2+L3    next detail level (minimal, compact, panel)
 *   hold L2+R3    next position (top left, center, right, then bottom)
 */
#pragma once

#include <cstdint>

namespace fpsmon::pad {

/* GamePadButtons bits (hooked_funcs.hpp). */
inline constexpr unsigned kL2 = 4096U, kL3 = 16384U, kR3 = 32768U;
inline constexpr unsigned kTouchPad = 262144U;
inline constexpr int64_t kHoldMs = 1000;
inline constexpr int64_t kClickGapMs = 500;

enum class Action { None, Toggle, Detail, Move };

/* One per controller. Combo: which of L2/L3/R3 are down, since when, and
 * whether this hold already fired. Touchpad: down last poll, presses so far,
 * time of the last press. */
struct State {
  unsigned held = 0;
  int64_t since_ms = 0;
  bool fired = false;
  bool pad_down = false;
  int clicks = 0;
  int64_t last_click_ms = 0;
};

/* Feed every GetData poll. */
inline Action update(State &s, unsigned buttons, int64_t now_ms) {
  /* Touchpad triple-click: count presses (not polls while it is held). */
  const bool pad = (buttons & kTouchPad) != 0;
  const bool pressed = pad && !s.pad_down;
  s.pad_down = pad;
  if (pressed) {
    s.clicks = now_ms - s.last_click_ms <= kClickGapMs ? s.clicks + 1 : 1;
    s.last_click_ms = now_ms;
    if (s.clicks == 3) {
      s.clicks = 0;
      return Action::Toggle;
    }
  }

  /* Combos: exactly these buttons among L2/L3/R3 (so L2+L3+R3 is nothing),
   * once per hold, after kHoldMs. */
  const unsigned held = buttons & (kL2 | kL3 | kR3);
  if (held != s.held) {
    s.held = held;
    s.since_ms = now_ms;
    s.fired = false;
    return Action::None;
  }
  if (s.fired || now_ms - s.since_ms < kHoldMs)
    return Action::None;
  const Action a = held == (kL2 | kL3)   ? Action::Detail
                   : held == (kL2 | kR3) ? Action::Move
                                         : Action::None;
  s.fired = a != Action::None;
  return a;
}

/* onion::kOverlayDetail* runs 0..3; full (3) draws the same as panel (2), so
 * the cycle skips it. */
inline constexpr int next_detail(int detail) {
  return detail >= 2 ? 0 : detail + 1;
}

/* overlay_pos 0/1 top, 2/3 bottom; align 0 left, 1 center, 2 right.
 * Top left → top center → top right → bottom left → ... → top left. */
inline constexpr void next_position(int &pos, int &align) {
  const int i = ((pos == 2 || pos == 3) ? 3 : 0) + align;
  const int n = (i + 1) % 6;
  pos = n >= 3 ? 2 : 0;
  align = n % 3;
}

} // namespace fpsmon::pad
