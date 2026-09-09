// The keyboard, as logical buttons.
//
// A firmware's input manager debounces GPIO and applies the user's button
// remapping; this reads SDL key events. Both end in the same place: an edge
// that is true for exactly one frame, asked for by meaning rather than by
// position.

#pragma once

#include <stdint.h>

namespace xpui_host {

// xpui::Button has fifteen variants. Kept as a count rather than an enum: this
// side indexes by the tag Rust sent and names only the keys the keyboard maps.
constexpr uint8_t kButtonCount = 15;

class Input {
 public:
  // Clears the edges. Call once at the top of every frame, before events are
  // pumped — an edge that survived into a second frame would repeat whatever
  // it triggered.
  void beginFrame();

  // An SDL key event. Unknown keys are ignored.
  void keyDown(int32_t keycode);
  void keyUp(int32_t keycode);

  // A press and release in the same frame, for a scripted run. `button` is an
  // xpui::Button tag; out-of-range values are ignored.
  void tapButton(uint8_t button);
  void tapHome();

  bool wasPressed(uint8_t button) const;
  bool isPressed(uint8_t button) const;
  bool wasReleased(uint8_t button) const;
  bool wasHomeGesture() const { return home_; }

 private:
  bool down_[kButtonCount] = {};
  bool pressed_[kButtonCount] = {};
  bool released_[kButtonCount] = {};
  bool home_ = false;
};

// What a key means.
//
// **This table is the same one `xpui-simulator`'s `button_for` holds**, and
// the two must not drift: a person moving between the Rust simulator and this
// host would otherwise find Enter doing different things. Escape is quit in
// both, which is why it is not Back in either.
//
// Returns false for a key that means nothing here.
bool buttonForKey(int32_t keycode, uint8_t& button);

// What a `--keys` name means, for a scripted run. `home` answers with
// `kHomeGesture` rather than a button, because it is not one.
constexpr uint8_t kHomeGesture = 0xFF;
bool buttonForName(const char* name, uint8_t& button);

}  // namespace xpui_host
