/* Host tests for the controller shortcuts (pad_shortcuts.hpp). Built and run
 * by scripts/test.sh. */
#undef NDEBUG
#include "pad_shortcuts.hpp"

#include <cassert>
#include <cstdio>

using namespace fpsmon::pad;

/* Press and release the touchpad at `at` ms; the action of the press. */
static Action click(State &s, int64_t at) {
  const Action a = update(s, kTouchPad, at);
  assert(update(s, 0, at + 50) == Action::None);
  return a;
}

int main() {
  /* touchpad: three presses within the gap toggle, on the third press */
  State s;
  assert(click(s, 1000) == Action::None);
  assert(click(s, 1300) == Action::None);
  assert(click(s, 1800) == Action::Toggle); // gap exactly kClickGapMs
  /* a fourth quick press starts a new count */
  assert(click(s, 2000) == Action::None);
  assert(click(s, 2200) == Action::None);
  assert(click(s, 2400) == Action::Toggle);

  /* a slow press restarts the count */
  s = {};
  assert(click(s, 1000) == Action::None);
  assert(click(s, 1300) == Action::None);
  assert(click(s, 1801) == Action::None); // 501 ms: count restarts at 1
  assert(click(s, 2000) == Action::None);
  assert(click(s, 2200) == Action::Toggle);

  /* holding the touchpad is one press, however many polls */
  s = {};
  for (int64_t t = 0; t < 400; t += 16)
    assert(update(s, kTouchPad, t) == Action::None);

  /* L2+L3 fires once after a full second, not before, not twice */
  s = {};
  assert(update(s, kL2 | kL3, 0) == Action::None);
  assert(update(s, kL2 | kL3, 999) == Action::None);
  assert(update(s, kL2 | kL3, 1000) == Action::Detail);
  assert(update(s, kL2 | kL3, 5000) == Action::None);
  /* release and hold again fires again */
  assert(update(s, 0, 5100) == Action::None);
  assert(update(s, kL2 | kL3, 5200) == Action::None);
  assert(update(s, kL2 | kL3, 6200) == Action::Detail);

  /* other buttons do not matter; L2+R3 */
  s = {};
  assert(update(s, kL2 | kR3 | 128U, 0) == Action::None);
  assert(update(s, kL2 | kR3 | 128U, 1000) == Action::Move);

  /* L3+R3 is no longer a shortcut; all three is no combo */
  s = {};
  assert(update(s, kL3 | kR3, 0) == Action::None);
  assert(update(s, kL3 | kR3, 3000) == Action::None);
  s = {};
  assert(update(s, kL2 | kL3 | kR3, 0) == Action::None);
  assert(update(s, kL2 | kL3 | kR3, 3000) == Action::None);

  /* adding a button restarts the second */
  s = {};
  assert(update(s, kL2, 0) == Action::None);
  assert(update(s, kL2 | kR3, 800) == Action::None);
  assert(update(s, kL2 | kR3, 1000) == Action::None);
  assert(update(s, kL2 | kR3, 1800) == Action::Move);

  /* detail: minimal → compact → panel → minimal; full (same as panel) skipped */
  static_assert(next_detail(0) == 1 && next_detail(1) == 2 &&
                next_detail(2) == 0 && next_detail(3) == 0);

  /* position: six stops, then back to top left; legacy pos 1/3 kept */
  int pos = 0, align = 0;
  const int want[6][2] = {{0, 1}, {0, 2}, {2, 0}, {2, 1}, {2, 2}, {0, 0}};
  for (const auto &w : want) {
    next_position(pos, align);
    assert(pos == w[0] && align == w[1]);
  }
  pos = 3, align = 2;
  next_position(pos, align);
  assert(pos == 0 && align == 0);

  std::puts("pad_test: ok");
  return 0;
}
