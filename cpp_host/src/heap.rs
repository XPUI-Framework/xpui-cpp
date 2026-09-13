//! What the host can say about its own memory.
//!
//! On a firmware these four figures cover both languages — `runtime.rs`
//! routes Rust's allocator through the host's `malloc` — and are the only
//! visibility into what Rust costs at run time, since a build-time size
//! report measures static sections where Rust contributes almost nothing.
//! **On the desktop host they cover the C++ side only**: a desktop Rust build
//! has its own system allocator.

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
    /// `total` less `free`, or `-1` when the host could not say either: a
    /// difference taken against an unknown is a made-up figure.
    pub fn used(&self) -> i32 {
        if self.total < 0 || self.free < 0 {
            return -1;
        }
        self.total.saturating_sub(self.free)
    }
}

pub fn reading() -> Reading {
    Reading {
        total: raw::xpui_host_heap_total(),
        free: raw::xpui_host_heap_free(),
        largest_block: raw::xpui_host_heap_largest_block(),
        min_free: raw::xpui_host_heap_min_free(),
    }
}
