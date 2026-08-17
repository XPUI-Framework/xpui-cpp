// What the host answers, and Rust asks.
//
// The application half of the boundary. `xpui_fui.h` is what the *backend*
// asks a host to draw; this is what the *application* asks a host to know —
// which button moved, what a string means in the user's language, what device
// this is, and where the screen stack is. Rust declares exactly these symbols
// in `src/raw.rs`, and `ffi_symbols_agree()` in `build-and-test.sh` fails when
// the two lists drift apart.
//
// Strings cross as NUL-terminated `const uint8_t*` rather than `const char*`,
// because `char`'s signedness is implementation-defined and Rust's `u8` is
// not. Every function here says what its pointers promise; there are only two
// promises and they are worth reading:
//
//   * IMMORTAL — the pointer is valid for the rest of the program. Rust reads
//     these as `&'static str`, which is a claim the compiler cannot check, so
//     an implementation that returns a pointer into anything it will later
//     free hands the framework a dangling reference. `intern()` exists for
//     exactly this, and the translation table is static.
//
//   * BORROWED FOR THE CALL — valid until the function returns, and not after.
//     Copy what you need.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// -- input -------------------------------------------------------------------
//
// `button` is xpui::Button's own discriminant:
//
//   0 Back        1 Confirm      2 Left        3 Right       4 Up
//   5 Down        6 Power        7 PageBack    8 PageForward 9 NavNext
//  10 NavPrevious 11 ScreenLeft 12 ScreenRight 13 ScreenUp  14 ScreenDown
//
// Anything outside that range answers 0 rather than reading off the end of a
// table: the enum can grow on the Rust side without this file changing.
//
// An edge query is true for exactly one frame and is not consumed by reading —
// the framework asks the same question more than once per frame.

uint8_t xpui_host_was_pressed(uint8_t button);
uint8_t xpui_host_is_pressed(uint8_t button);
uint8_t xpui_host_was_released(uint8_t button);

// The system-level "go home" gesture. Offered to the running screen first; a
// host applies its own meaning only when the screen declines it.
uint8_t xpui_host_was_home_gesture(void);

// Milliseconds since the host started. The only clock the framework has, and
// what key auto-repeat is timed against.
uint32_t xpui_host_millis(void);

// -- the screen stack --------------------------------------------------------

// The running screen's title. Never null; a host with no screen bound answers
// with an empty string. IMMORTAL.
const uint8_t* xpui_host_screen_title(void);

// Pops the running screen.
//
// Called from inside that screen's own frame, so an implementation must
// RECORD the request and act on it once the frame is over. Popping here would
// free the screen currently executing.
void xpui_host_screen_finish(void);

// Pushes a screen the Rust side built, with the title to show for it.
//
// `screen` is an opaque handle from `xpui_fui::lifecycle::handle_for`; drive
// it through `xpui_screen.h` and free it with `xpui_screen_destroy`.
//
// Returns non-zero when the host TOOK OWNERSHIP of the handle. Returning zero
// means it did not, and the caller reclaims the screen — so a host that
// declines must not have destroyed, entered or retained the handle. Same
// re-entrancy rule as `xpui_host_screen_finish`: record, then act after the
// frame.
//
// `title` is BORROWED FOR THE CALL.
uint8_t xpui_host_screen_present(void* screen, const uint8_t* title);

// -- translations ------------------------------------------------------------

// Looks `key` up in the host's string table. Returns the key itself when it is
// unknown, so a typo shows up on screen rather than as a blank row. IMMORTAL.
const uint8_t* xpui_host_tr(const uint8_t* key);

// -- device ------------------------------------------------------------------

const uint8_t* xpui_host_device_name(void);       // IMMORTAL
const uint8_t* xpui_host_firmware_version(void);  // IMMORTAL

// Charge, 0-100. NEGATIVE means this host has no battery to report, which is
// not the same as a flat one and must not be reported as 0.
int32_t xpui_host_battery_percent(void);

// -- heap --------------------------------------------------------------------
//
// Bytes. On a firmware Rust allocates from this same heap and these cover both
// languages; on a desktop they cover the C++ side only, which is what the
// screen showing them says.
//
// NEGATIVE means "this host cannot measure that", the same convention as the
// battery. `largest_block` is the one that usually cannot: fragmentation is
// what a device with no MMU actually runs out of, and a host that reported
// `free` in its place would turn the only figure that matters into decoration.

int32_t xpui_host_heap_total(void);
int32_t xpui_host_heap_free(void);
int32_t xpui_host_heap_largest_block(void);
int32_t xpui_host_heap_min_free(void);

#ifdef __cplusplus
}  // extern "C"
#endif
