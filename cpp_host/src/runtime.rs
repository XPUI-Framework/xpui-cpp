//! A heap and somewhere for a panic to go — **on a device only**.
//!
//! This crate is the staticlib a host links, and a staticlib is a final
//! artifact: on bare metal it must carry a global allocator and a panic
//! handler, or nothing that links it will build. On a desktop the standard
//! library brings both, and defining a second is a link error — so everything
//! here is `cfg(target_os = "none")`. The runtime belongs to whichever crate
//! *is* the staticlib, and there is only one.

#![cfg(target_os = "none")]

use core::alloc::{GlobalAlloc, Layout};
use core::ffi::c_void;

unsafe extern "C" {
    /// The **firmware's** allocator, not Rust's. That is the point: one heap,
    /// so `xpui_host_heap_*` reports figures covering both languages rather
    /// than one of them.
    fn malloc(size: usize) -> *mut c_void;
    fn free(pointer: *mut c_void);
}

struct FirmwareHeap;

unsafe impl GlobalAlloc for FirmwareHeap {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        // ESP-IDF's `malloc` returns memory aligned for any scalar, which
        // covers everything the framework allocates. An over-aligned type
        // would need `aligned_alloc`; there is none, and this says so rather
        // than handing one back misaligned.
        debug_assert!(
            layout.align() <= 8,
            "an over-aligned allocation needs aligned_alloc, which this does not use"
        );
        // Safety: `malloc` returns null on failure, which is what
        // `GlobalAlloc` asks for.
        unsafe { malloc(layout.size()) as *mut u8 }
    }

    unsafe fn dealloc(&self, pointer: *mut u8, _layout: Layout) {
        // Safety: `pointer` came from `alloc` above, so it came from `malloc`.
        unsafe { free(pointer as *mut c_void) }
    }
}

#[global_allocator]
static HEAP: FirmwareHeap = FirmwareHeap;

/// Hands the panic to the firmware and stops.
///
/// No formatting: `format!` needs the allocator, and the allocator is one of
/// the things that may have just failed. The workspace builds with
/// `panic = "abort"`, so nothing unwinds past here.
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    // Safety: a NUL-terminated literal, which the host reads and does not
    // retain.
    unsafe { crate::raw::xpui_host_panic(c"rust panic".as_ptr().cast()) };
    // Every implementation aborts, so this is unreachable — but the ABI does
    // not promise it, and a panic handler has to diverge on its own terms.
    loop {
        core::hint::spin_loop();
    }
}
