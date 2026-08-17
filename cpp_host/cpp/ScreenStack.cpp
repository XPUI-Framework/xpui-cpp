#include "ScreenStack.h"

#include <xpui_fui.h>
#include <xpui_screen.h>

#include <utility>

#include "ScreenHost.h"
#include "internal.h"

namespace xpui_host {

ScreenStack* g_stack = nullptr;

ScreenStack::ScreenStack() = default;

ScreenStack::~ScreenStack() {
  // Top down, so a screen is never destroyed while one above it is still live.
  while (!stack_.empty()) {
    pop();
  }
  if (pendingScreen_) {
    xpui_screen_destroy(pendingScreen_);
    pendingScreen_ = nullptr;
  }
}

void ScreenStack::push(void* screen, const uint8_t* title) {
  if (!screen) return;

  stack_.push_back(std::make_unique<ScreenHost>(screen, title));
  if (stack_.size() > maxDepth_) maxDepth_ = stack_.size();
  stack_.back()->onEnter();
}

void ScreenStack::pop() {
  if (stack_.empty()) return;

  // Moved out before the vector shrinks: the screen's destructor runs Rust
  // code, and that must not happen while the stack is mid-edit.
  std::unique_ptr<ScreenHost> screen = std::move(stack_.back());
  stack_.pop_back();
  screen->onExit();

  // Whatever is underneath is on screen now, and nothing else will ask.
  xpui_fui_request_update();
}

bool ScreenStack::requestPresent(void* screen, const char* title) {
  if (!screen || pendingScreen_) return false;
  pendingScreen_ = screen;
  pendingTitle_ = intern(title);
  return true;
}

void ScreenStack::loop() {
  if (!stack_.empty()) {
    stack_.back()->loop();
  }
  settle();
}

void ScreenStack::settle() {
  // Pop before push, so a screen that finishes itself and opens a replacement
  // in one frame ends up with the replacement on top of the screen it came
  // from rather than on top of itself.
  if (finishRequested_) {
    finishRequested_ = false;
    pop();
  }

  if (pendingScreen_) {
    void* screen = pendingScreen_;
    const uint8_t* title = pendingTitle_;
    // Cleared before entering: the new screen's on_enter may present another,
    // and finding the slot still occupied would lose it.
    pendingScreen_ = nullptr;
    pendingTitle_ = nullptr;
    push(screen, title);
  }
}

bool ScreenStack::render() {
  if (stack_.empty()) return false;
  stack_.back()->render();
  return true;
}

void ScreenStack::homeGesture() {
  if (stack_.empty()) return;
  if (stack_.back()->homeGesture()) return;

  // Nothing claimed it, so it means what it does everywhere else: back to the
  // root, one screen at a time so every on_exit runs.
  while (stack_.size() > 1) {
    pop();
  }
}

}  // namespace xpui_host
