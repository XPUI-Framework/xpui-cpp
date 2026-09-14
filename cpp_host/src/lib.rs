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
//! The root screen's factory, [`xpui_app_create_menu`], is public at the crate
//! root beside [`xpui_app_install`], so both have a page here. This stops
//! compiling if it is not:
//!
//! ```
//! let _ = xpui_cpp_host::xpui_app_create_menu;
//! ```
//!
//! # What is deliberately not here
//!
//! No frame loop, no window, no allocator. The loop belongs to the host —
//! that is the whole distinction between this and [`xpui::App`], which is what
//! a Rust binary with no stack of its own would use instead.

#![cfg_attr(target_os = "none", no_std)]
#![deny(missing_docs)]

/// The tutorial's three pages, `docs/boundary.md` and the reference pages,
/// mounted: the tutorial's and `host.md`'s snippets need the crates this one depends on,
/// and `boundary.md` and `abi.md` are mounted so a fence added to either is
/// compiled from the start.
#[cfg(doctest)]
mod guides {
    #[doc = include_str!("../../docs/tutorial.md")]
    pub mod tutorial {}
    #[doc = include_str!("../../docs/tutorial-host.md")]
    pub mod tutorial_host {}
    #[doc = include_str!("../../docs/tutorial-firmware.md")]
    pub mod tutorial_firmware {}
    #[doc = include_str!("../../docs/boundary.md")]
    pub mod boundary {}
    #[doc = include_str!("../../docs/reference/host.md")]
    pub mod reference_host {}
    #[doc = include_str!("../../docs/reference/abi.md")]
    pub mod reference_abi {}
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

use xpui_fui::Backend;

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
/// Idempotent, so a firmware with a separate render task can call it from
/// every entry point that could run first, before the first screen exists.
///
/// # Safety
/// No frame in flight on any task, and no other call to this running at the
/// same time. The installs, and the checks that skip them, are plain statics
/// with no locking, so overlapping one with a frame or with another call is a
/// data race, not a stale pointer.
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
// declaration in `cpp/xpui_app.h`. A module of its own because the macro
// writes an undocumented `pub fn` and takes no doc of its own. The `allow` is
// load-bearing: the re-export below makes the function public, so
// `missing_docs` reaches its definition here, and without the `allow` the
// build fails. The re-export is what gives it a page and its contract back.
mod factory {
    #![allow(missing_docs)]

    use crate::screens;
    use xpui_fui::register_screen;

    register_screen!(screens::Menu, xpui_app_create_menu);
}

/// The root screen's factory, as `cpp/xpui_app.h` declares it.
///
/// The host calls it once, and owns the opaque handle it gets back until it
/// hands that handle to `xpui_screen_destroy`.
pub use factory::xpui_app_create_menu;
