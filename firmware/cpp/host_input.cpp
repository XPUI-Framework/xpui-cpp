// Buttons and the clock.
//
// A real firmware wires these to its input manager — CrossPoint's answers them
// from `MappedInputManager`, which has already applied the user's button
// remapping and the screen orientation. This example has no input hardware
// configured, so every button reads as up and the clock is real.
//
// Saying "nothing is pressed" is the honest answer for a board whose buttons
// are not wired here. It is also enough to run: the screens lay out and paint
// without input, and what they do with input is tested on the host.

#include <esp_timer.h>

#include "internal.h"
#include "xpui_host.h"

// Deliberately no default. A board that forgets to say gets a build error
// rather than a guess: half the boards this example targets would inherit the
// wrong answer, and the symptom is a mode that silently never opens.
#if !defined(XPUI_HAS_LEFT_RIGHT_KEYS)
#error "platformio.ini must define XPUI_HAS_LEFT_RIGHT_KEYS for this board"
#endif

namespace xpui_host {

// Defined here because this is the file that owns it — the desktop host's
// `host_input.cpp` does the same. `internal.h` only declares it, so whichever
// of the two hosts is linked has to supply one, and neither can supply both.
//
// Null, and it stays null: no input manager is wired on this board. Every
// query below reads as "nothing pressed" without going near it.
Input* g_input = nullptr;

}  // namespace xpui_host

extern "C" {

uint8_t xpui_host_was_pressed(uint8_t) { return 0; }
uint8_t xpui_host_is_pressed(uint8_t) { return 0; }
uint8_t xpui_host_was_released(uint8_t) { return 0; }
uint8_t xpui_host_was_home_gesture(void) { return 0; }

// Whether this board has a Left/Right pair, from its own build flag beside the
// two that say how big its panel is — the keys and the panel being two facts
// about the same device.
//
// It is not cosmetic. A board **without** the pair opens a value control on
// Confirm and gives four keys a second meaning while it is open; a board with
// the pair nudges the value in place and never opens anything. Answering wrong
// is either a mode that never opens or one that opens where its keys do
// something else.
uint8_t xpui_host_has_left_right_keys(void) { return XPUI_HAS_LEFT_RIGHT_KEYS; }

// Microseconds since boot, as milliseconds. The framework needs it for key
// auto-repeat, and its contract is that this wraps at 49 days — what reads it
// measures short intervals, not absolute time.
uint32_t xpui_host_millis(void) { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
}
