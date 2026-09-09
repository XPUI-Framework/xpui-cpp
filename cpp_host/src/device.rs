//! Who the host says it is.
//!
//! A firmware answers these from its board configuration and its power
//! manager. This host answers from a build-time define and a fake battery —
//! but the *shape* is the point: a screen asks the host, never the framework,
//! because the framework has no idea what it is running on.

use crate::raw;
use crate::strings::borrow_static;

/// The device's name, as a person would recognise it.
pub fn name() -> &'static str {
    // Safety: the host returns a pointer into static storage; see `raw`.
    unsafe { borrow_static(raw::xpui_host_device_name()) }
}

pub fn firmware_version() -> &'static str {
    // Safety: as above.
    unsafe { borrow_static(raw::xpui_host_firmware_version()) }
}

/// Battery charge, 0–100, or a negative number when the host has no battery
/// to report. A desktop says so rather than claiming to be full.
pub fn battery_percent() -> i32 {
    raw::xpui_host_battery_percent()
}
