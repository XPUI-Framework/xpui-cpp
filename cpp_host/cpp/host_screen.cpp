// The screen a Rust screen is running inside.
//
// The three calls that make `xpui::host::Navigator` mean something on a host
// whose stack lives in C++. All three answer for whichever screen is bound,
// which is why `ScreenHost` rebinds at the top of every frame.

#include "ScreenHost.h"
#include "ScreenStack.h"
#include "internal.h"
#include "xpui_host.h"

using xpui_host::g_screen;
using xpui_host::g_stack;

namespace {

// Never null: the framework reads this as a string, and a host with nothing
// bound has an empty title rather than no title.
const uint8_t* kNoTitle = reinterpret_cast<const uint8_t*>("");

const char* asText(const uint8_t* text) { return reinterpret_cast<const char*>(text); }

}  // namespace

extern "C" {

const uint8_t* xpui_host_screen_title(void) { return g_screen ? g_screen->title() : kNoTitle; }

void xpui_host_screen_finish(void) {
  if (g_stack) g_stack->requestFinish();
}

uint8_t xpui_host_screen_present(void* screen, const uint8_t* title) {
  if (!g_stack) return 0;
  return g_stack->requestPresent(screen, title ? asText(title) : "") ? 1 : 0;
}
}
