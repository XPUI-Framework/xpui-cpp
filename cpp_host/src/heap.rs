//! What the host can say about its own memory.
//!
//! On a firmware these four figures are the only visibility into what Rust
//! costs at run time: it allocates through the same heap the C++ does, and the
//! build-time size report measures static sections, where Rust contributes
//! almost nothing.
//!
//! **On this host they cover the C++ side only.** A desktop Rust build has its
//! own system allocator and does not go through the host's, so the numbers
//! here are honest about C++ and silent about Rust. Joining the two is a
//! `#[global_allocator]` routing to the host's `malloc`, which is a firmware
//! concern and belongs with the PlatformIO example rather than here.

use crate::raw;

/// One reading, in bytes.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Reading {
    pub total: i32,
    pub free: i32,
    /// The biggest single allocation that would still fit.
    ///
    /// Below `free` once a heap has been used, because a used heap is a
    /// fragmented one. That gap is the interesting figure on a device with no
    /// MMU: plenty free and nowhere to put anything is a real failure.
    pub largest_block: i32,
    /// The lowest `free` has ever been — the only figure that says whether the
    /// host ever came close to running out.
    pub min_free: i32,
}

impl Reading {
    /// What has been handed out.
    pub fn used(&self) -> i32 {
        self.total.saturating_sub(self.free)
    }
}

/// Asks the host for the current figures.
pub fn reading() -> Reading {
    Reading {
        total: raw::xpui_host_heap_total(),
        free: raw::xpui_host_heap_free(),
        largest_block: raw::xpui_host_heap_largest_block(),
        min_free: raw::xpui_host_heap_min_free(),
    }
}
