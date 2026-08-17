#include "Input.h"

#include <SDL.h>
#include <string.h>

namespace xpui_host {

namespace {

// xpui::Button's discriminants, named here so the table below reads.
constexpr uint8_t kBack = 0;
constexpr uint8_t kConfirm = 1;
constexpr uint8_t kLeft = 2;
constexpr uint8_t kRight = 3;
constexpr uint8_t kUp = 4;
constexpr uint8_t kDown = 5;
constexpr uint8_t kPageBack = 7;
constexpr uint8_t kPageForward = 8;

struct NamedButton {
  const char* name;
  uint8_t button;
};

// The `--keys` vocabulary. Logical names, because a scripted run should read
// as what the user did rather than as which key they hit.
constexpr NamedButton kNames[] = {
    {"back", kBack},
    {"confirm", kConfirm},
    {"left", kLeft},
    {"right", kRight},
    {"up", kUp},
    {"down", kDown},
    {"pageback", kPageBack},
    {"pageforward", kPageForward},
    {"home", kHomeGesture},
};

}  // namespace

bool buttonForKey(const int32_t keycode, uint8_t& button) {
  switch (keycode) {
    case SDLK_UP:
      button = kUp;
      return true;
    case SDLK_DOWN:
      button = kDown;
      return true;
    case SDLK_LEFT:
      button = kLeft;
      return true;
    case SDLK_RIGHT:
      button = kRight;
      return true;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE:
      button = kConfirm;
      return true;
    case SDLK_BACKSPACE:
      button = kBack;
      return true;
    case SDLK_PAGEUP:
      button = kPageBack;
      return true;
    case SDLK_PAGEDOWN:
      button = kPageForward;
      return true;
    default:
      return false;
  }
}

bool buttonForName(const char* name, uint8_t& button) {
  if (!name) return false;
  for (const NamedButton& named : kNames) {
    if (strcmp(named.name, name) == 0) {
      button = named.button;
      return true;
    }
  }
  return false;
}

void Input::beginFrame() {
  memset(pressed_, 0, sizeof(pressed_));
  memset(released_, 0, sizeof(released_));
  home_ = false;
}

void Input::keyDown(const int32_t keycode) {
  // H is the home gesture in both desktop hosts. It is a gesture rather than a
  // button: the screen is offered it first and the stack acts only if the
  // screen declines.
  if (keycode == SDLK_h) {
    home_ = true;
    return;
  }

  uint8_t button = 0;
  if (!buttonForKey(keycode, button)) return;
  // SDL repeats a held key; the edge is only the first of them.
  if (!down_[button]) pressed_[button] = true;
  down_[button] = true;
}

void Input::keyUp(const int32_t keycode) {
  uint8_t button = 0;
  if (!buttonForKey(keycode, button)) return;
  if (down_[button]) released_[button] = true;
  down_[button] = false;
}

void Input::tapButton(const uint8_t button) {
  if (button >= kButtonCount) return;
  pressed_[button] = true;
  released_[button] = true;
  down_[button] = false;
}

void Input::tapHome() { home_ = true; }

bool Input::wasPressed(const uint8_t button) const { return button < kButtonCount && pressed_[button]; }

bool Input::isPressed(const uint8_t button) const { return button < kButtonCount && down_[button]; }

bool Input::wasReleased(const uint8_t button) const { return button < kButtonCount && released_[button]; }

}  // namespace xpui_host
