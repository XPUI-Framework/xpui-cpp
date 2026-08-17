//! The screens this host shows.
//!
//! Three, and no more: the example exists to prove the C ABI, not to be a
//! gallery. Between them they reach every kind of call that crosses it — a
//! list with its per-cell callback, a header, button hints, a slider, a dialog
//! with a scrim over content, and text measured through the host's own font.
//!
//! Screen work belongs in `cargo run -p xpui-gallery`, where iterating costs a
//! Rust rebuild rather than a CMake one.

mod about;
mod controls;
mod menu;

pub use about::About;
pub use controls::Controls;
pub use menu::{Example, Menu};
