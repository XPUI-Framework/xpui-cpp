//! The Rust half of a C++ application that hosts `xpui` screens.
//!
//! This is the layer a firmware calls `crosspoint_rs`: the screens, and the
//! implementations of everything the framework needs that the *application*
//! owns rather than the backend. It compiles to a `staticlib`, and the C++
//! under `cpp/` links it.
//!
//! ```text
//! C++ (main, ScreenStack, Display, Input)
//!   |  xpui_screen_*   lifecycle, defined in xpui-fui   ->
//!   |  xpui_app_*      this crate                       ->
//!   <- xpui_host_*     answered by cpp/host_*.cpp
//!   <- xpui_fui_*      drawing, answered by xpui-fui's cpp/xpui_fui.cpp
//! ```
//!
//! Four sets of symbols, two crossing each way, and every one of them is
//! declared in exactly one header. See `cpp/xpui_host.h` for the pair this
//! crate depends on.
//!
//! # What is deliberately not here
//!
//! No frame loop, no window, no allocator. The loop belongs to the host —
//! that is the whole distinction between this and [`xpui::App`], which is what
//! a Rust binary with no stack of its own would use instead.

#![cfg_attr(target_os = "none", no_std)]

/// This repository's prose, compiled: the tutorial's snippets need the crates
/// this one depends on.
#[cfg(doctest)]
mod guides {
    #[doc = include_str!("../../docs/tutorial.md")]
    pub mod tutorial {}
}

extern crate alloc;

mod device;
mod heap;
mod platform;
mod raw;
mod runtime;
mod shell;
mod strings;

// Public because the screens are the half of this crate a firmware would
// reuse: the C++ under `cpp/` is a desktop harness, and these are not.
pub mod screens;

use xpui_fui::{Backend, register_screen};

use crate::platform::HostPlatform;
use crate::shell::Shell;

static PLATFORM: HostPlatform = HostPlatform;
static BACKEND: Backend<HostPlatform> = Backend::new(&PLATFORM);
static SHELL: Shell = Shell;

/// Installs what paints and what navigates.
///
/// **Two installs, not one.** `xpui` keeps the two apart because they answer
/// to different owners, and a host that supplies only the first gets a screen
/// that draws perfectly and whose Back button silently does nothing.
/// Idempotent: a firmware with a separate render task installs from every
/// entry point that could run first.
///
/// # Safety
/// Call from the thread that will run the frame loop, before the first
/// screen is created. Installing while a frame is in flight is a data race,
/// not a stale pointer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xpui_app_install() {
    if !xpui::host::is_installed() {
        // Safety: the caller promises no frame is in flight; both statics live
        // for the program.
        unsafe { xpui::host::install(&BACKEND) };
    }
    if !xpui::host::is_navigator_installed() {
        // Safety: as above.
        unsafe { xpui::host::install_navigator(&SHELL) };
    }
}

// The root screen's factory. The screen type never crosses the boundary — the
// host gets an opaque handle — so adding a screen is one line here and one
// declaration in `cpp/xpui_app.h`.
register_screen!(screens::Menu, xpui_app_create_menu);
