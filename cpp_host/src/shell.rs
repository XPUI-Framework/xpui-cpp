//! The navigation, when the screen stack lives in C++.
//!
//! [`Navigator`] is the one trait the *application* supplies rather than the
//! backend, because who owns the stack is a different question from what
//! paints the pixels. Here the answer is `ScreenStack.cpp`, so all three
//! methods are one call each across the boundary.
//!
//! [`App`](xpui::App) is the other answer to the same question, and this is
//! the reason it is not used here: a host with its own stack already has one,
//! and running two would give a screen two places to be popped from.

use alloc::boxed::Box;

use xpui::host::Navigator;
use xpui::screen::Driver;
use xpui_fui::lifecycle::{handle_for, reclaim};

use crate::raw;
use crate::strings;

/// The C++ screen stack, as the framework sees it.
pub struct Shell;

impl Navigator for Shell {
    fn screen_title(&self) -> &'static str {
        // Safety: the host interns every title and frees none, which is the
        // promise `xpui_host_screen_title` is documented to keep.
        unsafe { strings::borrow_static(raw::xpui_host_screen_title()) }
    }

    fn finish(&self) {
        raw::xpui_host_screen_finish();
    }

    /// Pushes a Rust screen onto a stack that lives in C++.
    ///
    /// The handle is thin and opaque, so there is nothing about a Rust screen
    /// the host has to understand in order to hold one — and that is the whole
    /// reason forward navigation works at all across this boundary.
    ///
    /// A host that will not take it has **not** taken ownership, so the screen
    /// is reclaimed and handed back rather than leaked. Returning it is what
    /// lets the caller tell the difference between "pushed" and "silently
    /// dropped", which otherwise look identical from a screen's point of view.
    fn present(&self, screen: Box<dyn Driver>) -> Option<Box<dyn Driver>> {
        // Taken before the handle is made: afterwards there is no `&dyn Driver`
        // left to ask, only a `void*`.
        let title = strings::as_c(screen.title().unwrap_or(""));
        let handle = handle_for(screen);

        // Safety: `handle` is live and unowned by anything else, and `title`
        // outlives the call, which is all the host is promised.
        let taken = unsafe { raw::xpui_host_screen_present(handle, title.as_ptr().cast()) };
        if taken != 0 {
            return None;
        }

        // Safety: a zero answer means the host did not take the handle, so it
        // is still ours and has not been destroyed.
        unsafe { reclaim(handle) }
    }
}
