// One Rust screen, as the host holds it.
//
// The equivalent of CrossPoint's `ActivityRs`: an opaque handle, the six
// lifecycle calls that drive it, and the globals that let the `xpui_host_*`
// entry points find whichever screen is running.

#pragma once

#include <stdint.h>

namespace xpui_host {

class ScreenHost {
 public:
  // Takes ownership of `screen`, which must be a handle from a Rust factory.
  // `title` must be interned — it is handed to the framework as a `'static`.
  ScreenHost(void* screen, const uint8_t* title);
  ~ScreenHost();

  ScreenHost(const ScreenHost&) = delete;
  ScreenHost& operator=(const ScreenHost&) = delete;

  void onEnter();
  void onExit();
  void loop();
  void render();

  // Offers the home gesture. True when the screen consumed it.
  bool homeGesture();

  const uint8_t* title() const { return title_; }

 private:
  // Points the FFI globals at this screen. Called at every entry point, so a
  // screen resumed from the stack regains them without a second onEnter().
  void bindGlobals();

  void* screen_;
  const uint8_t* title_;
};

}  // namespace xpui_host
