# Contributing to `xpui-cpp`

## Building it

Four things have to be findable, and `README.md`'s `## Requirements` says
where each may sit: `xpui-backends` beside this checkout (or
`XPUI_BACKENDS_DIR`), the FreeInk SDK (or `FREEINK_SDK_INCLUDE`, or the
revision `cpp_host/freeink-sdk.rev` pins, fetched by CMake), SDL2, and
clang-format 21 or newer. `rust-toolchain.toml` pins the rest.

```bash
cargo build -p xpui-cpp-host                     # the Rust archive alone
./build-and-test.sh                              # everything CI checks
./build-and-test.sh all                          # plus cmake, the link, nine ctest cases
```

`all` needs CMake and Ninja; CI runs `check`, so run `all` yourself before
pushing anything the linker or a `ctest` case could reject.

## The gate

A change is not finished until `./build-and-test.sh` passes. It is the same
command CI runs, so a green run locally means what a green tick means there.
The checks are listed in [`AGENTS.md`](../AGENTS.md) and implemented in
[`xtask/`](../xtask/); `./build-and-test.sh fix` formats both languages in
place first — never invoke `clang-format` directly, because an older binary
ignores options it does not know and formats differently with no warning.

Three things bite here more than anywhere else:

- **A C symbol exists in four places, and all four move together**: its
  declaration in `cpp_host/cpp/xpui_host.h` or `xpui_app.h`; the Rust that
  calls or defines it, in `cpp_host/src/raw.rs` or `lib.rs`; the desktop
  host's C++ under `cpp_host/cpp/`; and the firmware's under `firmware/cpp/`
  or in a file `firmware/platformio.ini` shares. `the header's symbols are
  all defined` catches a name in `xpui_host.h` that either C++ side does not
  define; `abi/` is what reaches `xpui_app.h`, and what catches two
  parameters swapped, which links fine and corrupts the call frame.
- **`raw.rs` marks a declaration `safe fn` only when no argument can reach
  undefined behaviour.** Anything taking or returning a pointer stays
  `unsafe`, with a `# Safety` section naming the rule.
- **No Rust test harness, on purpose.** Every `xpui_host_*` symbol the crate
  calls is defined by the C++, so a Rust test binary could only link doubles.
  What has to be proven is that the whole stack links and draws, and that is
  `ctest`; [`docs/boundary.md`](boundary.md) says what each case catches.

## The review

Five steps, in order, none skipped:

1. The gate passes, with the real exit code read — `all`, for anything that
   touches C++ or the boundary.
2. The [code-reviewer](../.claude/agents/code-reviewer.md) agent reviews the
   change — every finding resolved, not noted.
3. The [docs-reviewer](../.claude/agents/docs-reviewer.md) agent reviews the
   prose, last: it runs every command a document gives and resolves every
   snippet against the API.
4. The author runs the host, and a firmware where the change reaches one.
   That is their step; a window and hardware are not an agent's to sign off.
5. They say commit.

## Commits

The subject says what was done — imperative, under fifty characters, one
concern. The body says what changed and why, in under about ten lines,
carrying the fact that is not in the diff. Nothing about how the bug was
found. No self-attribution.

## Working across the repositories

This repository depends on `xpui` and `xpui-backends` through `git`
dependencies on `main`, and on `xpui-backends` a second time as the sibling
checkout whose C++ it compiles. Nothing depends on it. Before pushing a
change that reaches into either, run the umbrella:

```bash
for d in ../xpui*/; do git -C "$d" fetch --quiet --all; done
cd ../xpui-dev && ./build-and-test.sh cross
```

It builds every crate from the sibling checkouts on disk and says which one
broke. `cross` is that repository's gate, not this one's — run it from there,
not here. The fetch first, because its link check resolves every
`github.com/XPUI-Framework/…` URL against each sibling's `origin/main`, and a
stale remote is a stale answer. `xpui`'s [`docs/orientation.md`](https://github.com/XPUI-Framework/xpui-framework/blob/main/docs/orientation.md)
describes the layout it expects.
