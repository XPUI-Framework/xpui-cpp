// What the application exports, and this host calls.
//
// The fourth and smallest of the four headers on this boundary, and the only
// one whose symbols are defined in `src/`. `xpui_screen.h` covers driving a
// screen once you have one; this is how you get one, and how the framework is
// wired up before the first frame.
//
// Adding a screen the host can open directly is one `register_screen!` line in
// `src/lib.rs` and one declaration here.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Installs what paints and what navigates.
//
// Call before creating the first screen; a second call is a no-op, so a
// firmware with a separate render task may call it from each entry point that
// could run first. No call may overlap a frame on any task, or another call:
// both are plain statics with no locking, so overlapping them is a data race
// rather than a stale pointer. Order matters with `xpui_fui_attach`: attaching tells the *shim*
// where the panel is, and this tells the *framework* where the shim is.
// Neither works without the other, and nothing checks that both happened.
void xpui_app_install(void);

// Builds the root screen. The handle belongs to the caller from here on; see
// `xpui_screen.h` for what to do with it.
void* xpui_app_create_menu(void);

#ifdef __cplusplus
}  // extern "C"
#endif
