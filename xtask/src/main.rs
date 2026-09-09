//! The gate for `xpui-cpp`.
//!
//! Everything CI checks, in one command, and **only what this repository has
//! to check**. It is the C++ half of the boundary, so it
//! carries the C++ stages — and no bare-metal lint, because the Rust here is
//! a host library and a header-agreement test.
//!
//! ```text
//! ./build-and-test.sh          check everything
//! ./build-and-test.sh fix      format in place first
//! ./build-and-test.sh all      the above, plus cmake, the link and nine ctest cases
//! ```
//!
//! Each repository in the organisation has its own copy of this shape, holding
//! its own list. **This file is the part that is meant to differ**; the modules
//! under it are byte-identical, and `shared_files_agree` in `xpui-dev` hashes
//! all ten across the nine, so a fix to the fence scanner cannot land in one
//! repository and not the rest.
//!
//! A check written and never listed below is a dead function, which clippy
//! fails the build over.

mod agents;
mod boundary;
mod cargo;
mod commands;
mod comments;
mod cpp;
mod docs;
mod faults;
mod fences;
mod paths;
mod prose;
mod readme;
mod tree;

use std::process::ExitCode;

/// Files under a `src/` may not exceed this. A ratchet, not a law of nature:
/// raising it is a decision to argue for in a commit message, never a way to
/// land a file.
const LINE_LIMIT: usize = 400;

/// Crates with no tests, and why. The reason prints on every run so it is
/// re-read rather than accumulated — and an exemption for a crate that has
/// since grown tests fails, rather than sitting there as a comment nobody
/// removes.
const UNTESTED: [(&str, &str); 1] = [(
    "cpp_host",
    "proved by nine ctest cases; a Rust harness could only link doubles",
)];

/// Fence languages this repository's prose is written in.
///
/// The list exists so that ` ```rustt ` is an error rather than a shrug: an
/// unknown language silently compiles nothing, and a typo is the likeliest
/// way for a Rust block to stop being checked.
const KNOWN_LANGUAGES: [&str; 19] = [
    "text", "bash", "sh", "shell", "console", "cpp", "c", "toml", "yaml", "yml", "json", "ini",
    "diff", "ascii", "mermaid", "markdown", "md", "python", "cmake",
];

/// Documents whose ```rust is illustrative rather than compilable.
const NOT_COMPILED: [&str; 0] = [];

/// Pages that are not a repository's front door and carry no banner.
const NOT_A_FRONT_PAGE: [&str; 0] = [];

/// The root README's headings, in order. Empty until this repository's front
/// page is brought to the standard; then the eight.
const README_ORDER: &[&str] = &[];
const README_OPTIONAL: &[&str] = &["Which crate you want", "Requirements"];
const NESTED_ORDER: &[&str] = &[
    "Using it",
    "Requirements",
    "Checking it",
    "Where next",
    "License",
];
const NESTED_OPTIONAL: &[&str] = &["Requirements", "Where next"];

/// `AGENTS.md` exists and `CLAUDE.md` is a symlink to it.
const AGENTS_FILE: bool = false;

/// Every publishable crate denies `missing_docs`. `true` here says so for
/// none: nothing in this repository is published.
const DOCUMENTED: bool = true;

/// How long a comment may be. `None` is not adopted.
const COMMENT_CAPS: Option<comments::Caps> = Some(comments::Caps {
    doc: 15,
    header: 15,
    run: 10,
});
/// No comment is about the past.
const NARRATION_CHECKED: bool = true;
/// Which files the two comment checks read. `None` is every tracked source,
/// manifest and C++ file outside `tests/`.
const COMMENT_SCOPE: Option<&str> = None;

fn main() -> ExitCode {
    // Every path in every check is relative to the repository root, so the
    // gate answers the same from anywhere it is invoked.
    let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("xtask/..");
    std::env::set_current_dir(root).expect("the repository root");

    // A typo is not a check: a gate that silently treats `fx` as `check`
    // reports a pass for a run nobody asked for.
    let (fix, everything) = match std::env::args().nth(1).as_deref() {
        None | Some("check") => (false, false),
        Some("fix") => (true, false),
        Some("all") => (false, true),
        Some(other) => {
            eprintln!("unknown argument `{other}`\nusage: ./build-and-test.sh [check|fix|all]");
            return ExitCode::from(2);
        }
    };
    let mut failed = 0;

    let mut gate: Vec<(&str, Box<dyn Fn() -> Result<String, String>>)> = vec![
        (
            "format",
            Box::new(move || {
                if fix {
                    cargo::cargo(&["fmt", "--all"])
                } else {
                    cargo::cargo(&["fmt", "--all", "--check"])
                }
            }),
        ),
        ("file sizes", Box::new(|| tree::file_sizes(LINE_LIMIT))),
        (
            "crates are tested",
            Box::new(|| tree::crates_are_tested(&UNTESTED)),
        ),
        (
            "READMEs warn",
            Box::new(|| tree::readmes_warn(&NOT_A_FRONT_PAGE)),
        ),
        (
            "prose is compiled",
            Box::new(|| prose::is_compiled(&NOT_COMPILED, &KNOWN_LANGUAGES)),
        ),
        ("documented paths resolve", Box::new(docs::doc_paths)),
        (
            "rustdoc links resolve",
            Box::new(|| cargo::rustdoc(&["--workspace"])),
        ),
        (
            "documented commands resolve",
            Box::new(|| commands::resolve(&cargo::packages(), &[])),
        ),
        (
            "the header's symbols are all defined",
            Box::new(boundary::symbols_agree),
        ),
        (
            "documented C++ compiles",
            Box::new(|| cpp::snippets_compile(boundary::snippet_includes(), true)),
        ),
        ("lint", Box::new(lint)),
        ("tests", Box::new(|| cargo::cargo(&["test", "--workspace"]))),
        (
            "doctests",
            Box::new(|| cargo::cargo(&["test", "--workspace", "--doc"])),
        ),
        ("the C++ host compiles", Box::new(boundary::host_compiles)),
        (
            "README sections",
            Box::new(|| {
                readme::readme_sections(
                    README_ORDER,
                    README_OPTIONAL,
                    NESTED_ORDER,
                    NESTED_OPTIONAL,
                    &NOT_A_FRONT_PAGE,
                )
            }),
        ),
        (
            "AGENTS.md",
            Box::new(|| agents::agents_file_exists(AGENTS_FILE)),
        ),
        (
            "published crates deny missing_docs",
            Box::new(|| tree::published_crates_deny_missing_docs(DOCUMENTED)),
        ),
        (
            "comment blocks",
            Box::new(|| comments::comment_blocks(COMMENT_CAPS, COMMENT_SCOPE)),
        ),
        (
            "comment narration",
            Box::new(|| comments::comment_narration(NARRATION_CHECKED, COMMENT_SCOPE)),
        ),
    ];

    // `all` is what a laptop runs before a board is flashed, and what CI runs
    // in a job provisioned for it. It is separate from `check` because these
    // stages need a toolchain or a build system that a quick run should not
    // demand.
    if everything {
        gate.extend::<Vec<(&str, Box<dyn Fn() -> Result<String, String>>)>>(vec![(
            "the C++ host builds, links and self-tests",
            Box::new(boundary::host_runs),
        )]);
    }

    gate.insert(1, ("C++ format", Box::new(move || cpp::format(fix))));

    // Last, after every insert and extend, owning the names: a closure in
    // the vector cannot borrow the vector.
    let names: Vec<String> = gate.iter().map(|(n, _)| n.to_string()).collect();
    gate.push((
        "the gate is documented",
        Box::new(move || agents::agents_documents_the_gate(&names)),
    ));

    for (name, check) in gate.drain(..) {
        println!("\n==> {name}");
        match check() {
            Ok(note) if note.is_empty() => println!("    ok"),
            Ok(note) => println!("    {}", note.replace('\n', "\n    ")),
            Err(why) => {
                println!("{why}");
                eprintln!("FAILED: {name}");
                failed += 1;
            }
        }
    }

    if failed == 0 {
        if everything {
            println!("\nEverything passed.");
        } else {
            println!("\nChecks passed. `./build-and-test.sh all` also links and runs it.");
        }
        ExitCode::SUCCESS
    } else {
        eprintln!("\n{failed} check(s) failed.");
        ExitCode::FAILURE
    }
}

/// Clippy on the host, with warnings as errors.
///
/// No bare-metal run: the Rust in this repository is a host library and an ABI
/// comparison. What crosses to a device is C++, and it is checked as C++.
fn lint() -> Result<String, String> {
    cargo::cargo(&[
        "clippy",
        "--workspace",
        "--all-targets",
        "--",
        "-D",
        "warnings",
    ])?;
    Ok("host".into())
}
