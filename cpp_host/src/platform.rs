//! Buttons and the clock, forwarded to the host.
//!
//! FreeInkUI draws; it has no opinion about input, so [`Platform`] is the seam
//! where a host says what a button did. Here that is a keyboard, four files
//! away in `cpp/host_input.cpp`.
//!
//! **Touch is deliberately not implemented.** Every method of it defaults to
//! "no touch", and this host has a keyboard. The touch paths are a `Platform`
//! concern rather than an ABI one, and `xpui-simulator` already drives them
//! with a mouse — repeating that here would be more C++ to prove nothing new.

use xpui::Button;
use xpui_fui::Platform;

use crate::raw;

/// The host's keyboard, as logical buttons.
pub struct HostPlatform;

impl HostPlatform {
    /// `xpui::Button` is `#[repr(u8)]`, so its discriminant is the value the
    /// host maps. Going through `as u8` in one place rather than at each call
    /// site is what keeps `cpp/host_input.cpp`'s table the only other copy.
    fn tag(button: Button) -> u8 {
        button as u8
    }
}

impl Platform for HostPlatform {
    fn millis(&self) -> u32 {
        raw::xpui_host_millis()
    }

    fn was_pressed(&self, button: Button) -> bool {
        raw::xpui_host_was_pressed(Self::tag(button)) != 0
    }

    fn is_pressed(&self, button: Button) -> bool {
        raw::xpui_host_is_pressed(Self::tag(button)) != 0
    }

    fn was_released(&self, button: Button) -> bool {
        raw::xpui_host_was_released(Self::tag(button)) != 0
    }

    fn was_home_gesture(&self) -> bool {
        raw::xpui_host_was_home_gesture() != 0
    }
}
