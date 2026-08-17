// Shared by the `xpui_host_*` translation units.
//
// Each `host_*.cpp` file answers one concern's entry points, and the Rust side
// declares them in `src/raw.rs`. Only what more than one file needs lives here
// — which is the globals, and the one function that makes a string immortal.

#pragma once

#include <stdint.h>

namespace xpui_host {

class Input;
class ScreenHost;
class ScreenStack;

// The screen currently running.
//
// Bound by `ScreenHost` at the top of every loop() and render() rather than
// once on entry, and that is load-bearing: screens form a stack, so a screen
// pushed over another clears this on its way out and the stack resumes the one
// beneath WITHOUT calling onEnter again. Bound once, the resumed screen would
// run with a null here and its Back button would do nothing.
extern ScreenHost* g_screen;

// Bound once by main, because neither depends on which screen is running.
extern Input* g_input;
extern ScreenStack* g_stack;

// Copies `text` somewhere that is never freed, and returns it.
//
// Rust reads a title as `&'static str`, so the pointer it is given has to
// outlive every screen — including the one it came from. A handful of strings
// per run, deliberately never reclaimed: the alternative is handing the
// framework a reference that dangles the moment a screen is popped.
const uint8_t* intern(const char* text);

}  // namespace xpui_host
