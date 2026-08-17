// The stack of screens, and the frame that drives the top one.
//
// This is the piece that makes `Navigator` mean something. Without it the
// globals in `internal.h` would be cargo cult — what makes them load-bearing
// is a pushed screen clearing them on its way out and the screen beneath being
// resumed without a second onEnter.
//
// The order of operations mirrors `xpui::App`, deliberately: pop before push,
// and both after the frame rather than during it. A screen asks to be finished
// from inside its own loop(), so acting on the request there would free the
// object currently executing.
//
// **Every screen here is opaque, never an overlay.** `xpui`'s own stack paints
// the screens beneath an overlay before painting it, using `Driver::is_overlay`
// — which this ABI does not carry, because `xpui_screen.h` is six functions and
// a seventh only this host would use is not worth the drift. A dialog is an
// overlay *inside* a screen's own body, which is where these examples put
// theirs; a host that wants overlay screens needs that call added.

#pragma once

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace xpui_host {

class ScreenHost;

class ScreenStack {
 public:
  ScreenStack();
  ~ScreenStack();

  ScreenStack(const ScreenStack&) = delete;
  ScreenStack& operator=(const ScreenStack&) = delete;

  // Pushes a screen and enters it. Takes ownership of the handle.
  void push(void* screen, const uint8_t* title);

  // One frame of input, then whatever navigation it asked for.
  void loop();

  // Paints the top screen. False when there was none, so a caller does not
  // count a blit for a frame nothing drew.
  bool render();

  // Offers the home gesture to the top screen, and pops back to the root if it
  // declines.
  void homeGesture();

  bool empty() const { return stack_.empty(); }
  size_t depth() const { return stack_.size(); }
  // The deepest the stack has ever been, so a scripted run can prove a push
  // happened even though the screen was popped again afterwards.
  size_t maxDepth() const { return maxDepth_; }

  // -- what the FFI records ----------------------------------------------
  //
  // Both are called from inside the running screen's frame and both only
  // record; `settle()` acts once the frame is over.

  void requestFinish() { finishRequested_ = true; }

  // Returns true when the stack took the handle. One push per frame: handing
  // the second back rather than overwriting means a screen that pushed twice
  // finds out, and the Rust side reclaims the screen instead of leaking it.
  //
  // `title` is borrowed for the call and interned only if the push is taken,
  // so a declined one costs nothing that is never freed.
  bool requestPresent(void* screen, const char* title);

 private:
  void settle();
  void pop();

  std::vector<std::unique_ptr<ScreenHost>> stack_;
  bool finishRequested_ = false;
  void* pendingScreen_ = nullptr;
  const uint8_t* pendingTitle_ = nullptr;
  size_t maxDepth_ = 0;
};

}  // namespace xpui_host
