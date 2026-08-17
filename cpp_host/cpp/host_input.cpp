// Buttons, the home gesture, and the clock the runtime repeats keys on.

#include <chrono>

#include "Input.h"
#include "internal.h"
#include "xpui_host.h"

namespace xpui_host {

Input* g_input = nullptr;

}  // namespace xpui_host

using xpui_host::g_input;

extern "C" {

uint8_t xpui_host_was_pressed(const uint8_t button) { return g_input && g_input->wasPressed(button) ? 1 : 0; }

uint8_t xpui_host_is_pressed(const uint8_t button) { return g_input && g_input->isPressed(button) ? 1 : 0; }

uint8_t xpui_host_was_released(const uint8_t button) { return g_input && g_input->wasReleased(button) ? 1 : 0; }

uint8_t xpui_host_was_home_gesture(void) { return g_input && g_input->wasHomeGesture() ? 1 : 0; }

// `steady_clock` rather than `SDL_GetTicks`, so a headless run needs no SDL
// subsystem initialised — which is the whole point of headless.
uint32_t xpui_host_millis(void) {
  static const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  const auto since = std::chrono::steady_clock::now() - start;
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(since).count());
}
}
