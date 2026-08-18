//! Everything the host defines and this crate calls, in one place.
//!
//! The mirror of [`xpui_fui::raw`]: that declares what the *backend* asks a
//! host to draw, this declares what the *application* asks a host to answer.
//! Each symbol here is defined in `cpp/xpui_host.h` and one of the
//! `cpp/host_*.cpp` files, and `ffi_symbols_agree()` in `build-and-test.sh`
//! fails the gate when the two lists drift apart.
//!
//! Nothing outside this crate should call these directly — the sibling modules
//! wrap them in safe APIs, which is where the pointer contracts are discharged.
//!
//! `safe fn` marks a call that needs no contract from us: it passes no
//! pointer, the C++ side guards its own state, and no argument can reach
//! undefined behaviour. Everything taking or returning a raw pointer stays
//! `unsafe`, so the keyword keeps meaning "there is a rule here you must keep".

use core::ffi::c_void;

unsafe extern "C" {
    // -- input ---------------------------------------------------------------
    //
    // `button` is `xpui::Button`'s own discriminant. The host maps it to a key,
    // and the mapping is the one `xpui-simulator` uses, so the two desktop
    // hosts agree about what Enter does.
    pub safe fn xpui_host_was_pressed(button: u8) -> u8;
    pub safe fn xpui_host_is_pressed(button: u8) -> u8;
    pub safe fn xpui_host_was_released(button: u8) -> u8;
    /// The system-level "go home" gesture, offered to the screen before the
    /// host applies its own meaning.
    pub safe fn xpui_host_was_home_gesture() -> u8;
    /// Milliseconds since the host started. The only clock this side has, and
    /// the framework needs it for key auto-repeat.
    pub safe fn xpui_host_millis() -> u32;

    // -- the screen stack ----------------------------------------------------
    /// The running screen's title.
    ///
    /// Never null: a host with no screen bound answers with an empty string.
    /// The pointer is **immortal** — the host interns every title it is given
    /// and never frees one — which is what lets this be read as a `'static`
    /// string on the Rust side.
    pub fn xpui_host_screen_title() -> *const u8;

    /// Pops the running screen. Recorded and acted on after the frame, because
    /// the call comes from inside that screen's own frame.
    pub safe fn xpui_host_screen_finish();

    /// Hands a screen to the host's stack, with the title to show for it.
    ///
    /// `screen` is a handle from `xpui_fui::lifecycle::handle_for`. Returns
    /// non-zero when the host **took ownership**; on zero it has not, and the
    /// caller still owns the handle and must reclaim it. `title` is borrowed
    /// for the duration of the call and interned by the host.
    pub fn xpui_host_screen_present(screen: *mut c_void, title: *const u8) -> u8;

    // -- translations --------------------------------------------------------
    /// Looks a key up in the host's string table.
    ///
    /// Returns the key itself when it is unknown, so a typo shows up on screen
    /// rather than crashing or drawing a blank. The table is static, so the
    /// answer is immortal in the same sense as a title.
    pub fn xpui_host_tr(key: *const u8) -> *const u8;

    // -- device --------------------------------------------------------------
    pub fn xpui_host_device_name() -> *const u8;
    pub fn xpui_host_firmware_version() -> *const u8;
    pub safe fn xpui_host_battery_percent() -> i32;

    // -- panics --------------------------------------------------------------
    /// Where a Rust panic goes.
    ///
    /// Only ever called from a device build — on a desktop the standard
    /// library has its own handler. Every implementation aborts; the caller
    /// spins afterwards rather than relying on that.
    ///
    /// Declared only where it is used, or it is dead code on the host and
    /// warnings are failures. The gate's symbol check and `tests/abi.rs` both
    /// read this file as text, so the `cfg` is invisible to them and the
    /// header still has to declare it.
    #[cfg(target_os = "none")]
    pub fn xpui_host_panic(message: *const u8);

    // -- heap ----------------------------------------------------------------
    //
    // What the host can measure of its own allocations. On a firmware Rust
    // allocates from this same heap and these cover both languages; on a
    // desktop they do not, which `heap.rs` says where a reader can see it.
    pub safe fn xpui_host_heap_total() -> i32;
    pub safe fn xpui_host_heap_free() -> i32;
    pub safe fn xpui_host_heap_largest_block() -> i32;
    pub safe fn xpui_host_heap_min_free() -> i32;
}
