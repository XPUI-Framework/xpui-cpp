//! Nothing but a home for the ABI tests in `tests/`.
//!
//! Two of the five boundaries between Rust and C++ in this organisation
//! cross into the *application* rather than the backend, and this crate is
//! where they are checked on types: `cpp_host/cpp/xpui_host.h` against the
//! `raw.rs` that calls it, and `cpp_host/cpp/xpui_app.h` against `src/lib.rs`
//! and what `register_screen!` generates — `xpui_app_install` is written by
//! hand, the factory by the macro. The macro's signatures exist only in its own
//! body, in `xpui-fui`, which exports its source as a string for exactly this
//! test to parse.
//!
//! A link cannot catch what these do: C has no mangling, so two parameters
//! swapped resolve fine and corrupt the call frame.

#![deny(missing_docs)]
