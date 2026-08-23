//! Whether this repository's C headers and the Rust that answers them agree.
//!
//! Two boundaries, and they are the two that cross into the application rather
//! than into the backend:
//!
//! | C | Rust | who defines |
//! |---|---|---|
//! | `cpp_host/cpp/xpui_host.h` | `cpp_host/src/raw.rs` | C++ |
//! | `cpp_host/cpp/xpui_app.h` | `cpp_host/src/lib.rs`, plus `register_screen!` for what it generates | Rust |
//!
//! The other three belong to `xpui-fui` and are checked there. A repository
//! checks the boundary it owns, because a checker that reaches into a sibling
//! resolves nothing the day the sibling moves.
//!
//! C has no name mangling, so two parameters in the wrong order link cleanly
//! and produce a corrupt call frame. That is what this catches, and nothing
//! else does.

use std::path::{Path, PathBuf};

use xpui_abi_check::{
    Boundary, Signatures, assert_agree, signatures_from_c, signatures_from_register_screen,
    signatures_from_rust,
};

/// A boundary with no typedef of its own and nothing the Rust side leaves out.
const PLAIN: Boundary<'_> = Boundary {
    prefix: "xpui_",
    skip: &[],
    types: &[],
    rust_aliases: &[],
};

/// A file in this repository, resolved from this crate's manifest.
///
/// Not from a workspace root and not by counting directory levels: a depth
/// that is wrong resolves somewhere else and reads nothing, and a checker that
/// reads nothing reports an agreement it never established.
fn repo(relative: &str) -> (String, PathBuf) {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("this crate sits one level below the repository root");
    let path = root.join(relative);
    let text = std::fs::read_to_string(&path)
        .unwrap_or_else(|error| panic!("{}: {error}", path.display()));
    (text, path)
}

#[test]
fn what_the_host_answers_agrees_with_what_rust_asks_for() {
    let (header, header_path) = repo("cpp_host/cpp/xpui_host.h");
    let (rust, _) = repo("cpp_host/src/raw.rs");

    assert_agree(
        "xpui_host.h and cpp_host/src/raw.rs",
        &signatures_from_c(&header, &header_path, &PLAIN),
        &signatures_from_rust(&rust, &PLAIN),
    );
}

#[test]
fn what_the_application_exports_agrees_with_its_header() {
    let (header, header_path) = repo("cpp_host/cpp/xpui_app.h");
    let (rust, _) = repo("cpp_host/src/lib.rs");

    // The factories are generated, so no Rust source spells them out. Their
    // signature comes from the macro's own body, with `$factory` standing in
    // for each name it was invoked with — and the crate that owns the macro
    // hands out its text, because a git dependency has no path to read.
    let mut declared: Signatures = signatures_from_rust(&rust, &PLAIN);
    declared.extend(signatures_from_register_screen(
        xpui_fui::LIFECYCLE_SOURCE,
        &rust,
        &PLAIN,
    ));

    assert_agree(
        "xpui_app.h and cpp_host/src/lib.rs",
        &signatures_from_c(&header, &header_path, &PLAIN),
        &declared,
    );
}

/// Every file the two boundaries name is there and is not empty.
///
/// `assert_agree` refuses an empty parse, so a boundary that read nothing
/// fails anyway — but it fails with a message about parsing. This says plainly
/// which file is missing, which is what a moved crate or a split repository
/// actually produces.
#[test]
fn every_boundary_names_a_file_that_is_there() {
    for relative in [
        "cpp_host/cpp/xpui_host.h",
        "cpp_host/cpp/xpui_app.h",
        "cpp_host/src/raw.rs",
        "cpp_host/src/lib.rs",
    ] {
        let (text, path) = repo(relative);
        assert!(!text.trim().is_empty(), "{} is empty", path.display());
    }

    assert!(
        !xpui_fui::LIFECYCLE_SOURCE.trim().is_empty(),
        "xpui-fui handed out no lifecycle source"
    );
}
