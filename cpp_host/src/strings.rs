//! Text across the boundary, in both directions.
//!
//! Every string the host hands back — a title, a translation, a device name —
//! comes from storage the host promises never to free, so it is read as
//! `&'static str`. That promise is the whole reason this module exists rather
//! than each caller doing its own `from_ptr`: it is stated once, here, and
//! `cpp/xpui_host.h` states the other half of it.

use alloc::ffi::CString;
use core::ffi::CStr;

use crate::raw;

/// Looks up a translation.
///
/// Takes a [`CStr`] so a caller can pass a `c"…"` literal and nothing is
/// allocated: this is called from `body()`, which runs on every paint.
///
/// Fenced as `text`, and it is the only reason this crate's examples ever are:
/// a doctest links this crate without the C++ half, so a snippet that actually
/// called this would fail at the link. Doctests are on — the tutorial is nine
/// of them — and they run because none of them crosses the boundary.
///
/// ```text
/// Text::new(strings::tr(c"STR_ABOUT_TITLE"))
/// ```
///
/// **The key must be `'static`, and that is a soundness requirement rather
/// than a convenience.** An unknown key comes back as *the caller's own
/// pointer* — that is how a typo shows up on screen as the key itself — so a
/// borrowed key would be handed back as a `&'static str` that dangles the
/// moment it is dropped. The bound makes that unwriteable.
pub fn tr(key: &'static CStr) -> &'static str {
    // Safety: `key` is NUL-terminated by construction and lives for the
    // program, so both answers `xpui_host_tr` can give — a pointer into the
    // host's static table, or `key` itself — outlive the return.
    unsafe { borrow_static(raw::xpui_host_tr(key.as_ptr().cast())) }
}

/// A pointer the host promised is immortal, as a string.
///
/// Empty for null, and empty for anything that is not UTF-8 — the host's
/// tables are ASCII, and a screen showing nothing is a better failure than a
/// panic in the middle of a frame.
///
/// # Safety
/// `ptr` must be null, or NUL-terminated and valid for the rest of the
/// program. Every `xpui_host_*` function returning a pointer documents that
/// its answer is — with one exception that the caller has to close:
/// `xpui_host_tr` returns *the key it was given* when the key is unknown, so
/// it is only immortal if the key was, which is why [`tr`] takes a
/// `&'static CStr`.
pub(crate) unsafe fn borrow_static(ptr: *const u8) -> &'static str {
    if ptr.is_null() {
        return "";
    }
    // Safety: the caller guarantees a NUL-terminated string that outlives the
    // program, which is exactly what `&'static str` claims.
    unsafe { CStr::from_ptr(ptr.cast()) }.to_str().unwrap_or("")
}

/// Copies `text` into a NUL-terminated buffer for the host to read.
///
/// The host copies what it needs during the call, so the buffer is freed as
/// soon as the call returns. A string with an interior NUL cannot be one, and
/// comes back empty rather than silently truncated.
pub(crate) fn as_c(text: &str) -> CString {
    CString::new(text).unwrap_or_default()
}
