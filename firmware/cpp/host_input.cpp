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

// Null, and it stays null: no input manager is wired on this board. Every
// query below reads as "nothing pressed" without going near it.
Input* g_input = nullptr;

}  // namespace xpui_host

extern "C" {

uint8_t xpui_host_was_pressed(uint8_t) { return 0; }
uint8_t xpui_host_is_pressed(uint8_t) { return 0; }
uint8_t xpui_host_was_released(uint8_t) { return 0; }
uint8_t xpui_host_was_home_gesture(void) { return 0; }

// From the board's own build flag, beside the two that say how big its panel
// is; `xpui_host.h` says what hangs on the answer.
uint8_t xpui_host_has_left_right_keys(void) { return XPUI_HAS_LEFT_RIGHT_KEYS; }

// Microseconds since boot, as milliseconds. The framework needs it for key
// auto-repeat, and its contract is that this wraps at 49 days — what reads it
// measures short intervals, not absolute time.
uint32_t xpui_host_millis(void) { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
}
