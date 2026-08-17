#include "ScreenHost.h"

#include <xpui_fui.h>
#include <xpui_screen.h>

#include "internal.h"

namespace xpui_host {

ScreenHost* g_screen = nullptr;

ScreenHost::ScreenHost(void* screen, const uint8_t* title) : screen_(screen), title_(title) {}

ScreenHost::~ScreenHost() {
  if (screen_) {
    xpui_screen_destroy(screen_);
    screen_ = nullptr;
  }
}

void ScreenHost::bindGlobals() { g_screen = this; }

void ScreenHost::onEnter() {
  // Before any Rust runs, so a screen that asks for its title or navigates
  // from inside on_enter finds this bound.
  bindGlobals();
  xpui_screen_on_enter(screen_);

  // Paint on entry. Nothing else asks for the first frame: the framework
  // requests a repaint when a message changes something, and a screen that has
  // just appeared has had no messages. Doing it here covers every screen ever
  // pushed rather than making each one remember.
  xpui_fui_request_update();
}

void ScreenHost::onExit() {
  // Bound here too, and it is not redundant: the home gesture pops several
  // screens in a row, and each `onExit` below clears the binding on its way
  // out. Without this, every pop after the first would run with a NULL there,
  // and a screen asking its own title on the way out would get nothing.
  bindGlobals();
  xpui_screen_on_exit(screen_);

  // Only unbind what this screen bound: a pushed screen must not clear its
  // parent's binding on the way out.
  if (g_screen == this) g_screen = nullptr;
}

void ScreenHost::loop() {
  bindGlobals();
  xpui_screen_loop(screen_);
}

void ScreenHost::render() {
  bindGlobals();
  xpui_screen_render(screen_);
}

bool ScreenHost::homeGesture() {
  bindGlobals();
  return xpui_screen_home_gesture(screen_) != 0;
}

}  // namespace xpui_host
