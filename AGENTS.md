# `xpui-cpp`

## What this is, and what it may not become

The C++ side of the boundary: a desktop host that owns its screen stack and
drives Rust screens over the C ABI, the same C++ built for an ESP32 through
PlatformIO, and the signature checker for the two headers that cross into the
application. `cpp_host` is the only thing any gate in the organisation
compiles `xpui-backends`' `fui/cpp/xpui_fui.cpp` into, links and runs; design
and prove an ABI change here.

**Nothing here is a backend, a board, a screen library or a panel driver.**
The Rust is a host library and its three example screens; the drawing is
`xpui-fui`'s and the screens' components are `xpui`'s. There is no bare-metal
lint, because nothing here is a firmware crate — `firmware/` links the
`cpp_host` archive a desktop links. No Rust test harness: every `xpui_host_*`
symbol is defined by the C++, so the proof is `ctest`.

## The gate

```bash
./build-and-test.sh          # everything below
./build-and-test.sh all      # the above, plus the line marked `+`
./build-and-test.sh fix      # the same as check, formatting both languages in place first
```

```text
format · C++ format · file sizes · crates are tested · READMEs warn · prose is compiled · documented paths resolve · rustdoc links resolve · the reference mirrors rustdoc · documented commands resolve · the header's symbols are all defined · documented C++ compiles · lint · tests · doctests · the C++ host compiles · README sections · AGENTS.md · published crates deny missing_docs · comment blocks · comment narration
+ the C++ host builds, links and self-tests
```

A last stage, `the gate is documented`, compares this list to what ran. Run
it before saying a change is done, and read the real exit code; run `all`
before pushing anything that touches C++ or the boundary, because CI runs
`check`.

## What only this repository checks

- **`C++ format`**, through clang-format 21 or newer.
- **`documented C++ compiles`** (every `cpp` fence in the prose, compiled as
  a translation unit against the real headers) and **`the C++ host
  compiles`** run only when two roots are found: the FreeInkUI headers, where
  `freeink_include` in `xtask/src/cpp.rs` looks — the directory
  `FREEINK_SDK_INCLUDE` names and nowhere else when it is set, two sibling
  layouts when it is not — and `xpui-backends`' `fui/cpp/`, beside this
  checkout or under `XPUI_BACKENDS_DIR`. The second also needs SDL2's flags
  from `sdl2-config` or `pkg-config`. Short of any of these the stage prints
  `skipped:` and **passes**, having compiled nothing; read the note. With
  `CI` set it fails instead: the workflow fetches the SDK and `xpui-backends`
  at the revision `Cargo.lock` pins, so nothing it compiles can be skipped.
- **`the header's symbols are all defined`**: one header, `cpp_host/cpp/xpui_host.h`,
  against **two** answers — `cpp_host/cpp/host_*.cpp` and the device's, which
  is `firmware/cpp/host_*.cpp` plus the files `firmware/platformio.ini`
  shares. Names only, and `xpui_app.h` is not in it: that boundary is
  `abi/`'s, on types, under `tests`.
- **`the C++ host builds, links and self-tests`**, under `all` only, through
  CMake, Ninja where installed, and nine `ctest` cases. It skips only without
  `cmake`, and on CI that is a failure too. CI runs `all`, and a second job
  builds the firmware's three PlatformIO environments. It hands CMake the SDK `freeink_include` finds, or lets CMake fetch
  the pinned revision, and without `xpui-backends` it fails rather than skips.
- `published crates deny missing_docs` prints `no publishable crates`, this
  repository's permanent truth; both crates deny it anyway.

## Style that bites here

- **A C symbol exists in four places and all four move together**: the
  header (`cpp_host/cpp/xpui_host.h` or `xpui_app.h`), the Rust that calls or
  defines it (`cpp_host/src/raw.rs` or `lib.rs`), the desktop host's C++
  under `cpp_host/cpp/`, and the firmware's under `firmware/cpp/` or in a
  file `firmware/platformio.ini` shares. The backend's rule adds a stub
  double; this repository has none, on purpose. `docs/reference/abi.md`
  describes each symbol by hand, and changes with its header.
- **`raw.rs` marks a declaration `safe fn` only when no argument can reach
  undefined behaviour.** A pointer in or out keeps `unsafe` and a `# Safety`
  section naming the rule.
- **The handle is double-boxed** — `Box<Box<dyn Driver>>` — because a fat
  pointer cannot cross as one word. One unwrap too few is a wild pointer.
- **The FreeInk SDK revision lives in `cpp_host/freeink-sdk.rev` and nowhere
  else**; CMake and CI both read it. It is compared with `xpui-backends`'
  copy by `xpui-dev`, so it is not this repository's to change alone.
- **`xtask/src/cpp.rs` is shared with `xpui-backends`** and compared
  byte-for-byte; a fix goes to both.
- **`cpp_host/Cargo.toml` has no `doctest` key, and must not gain one.**
  Cargo's default runs the tutorial's Rust fences as doctests; a
  `doctest = false` would silence them and `doctests` would still pass.
- **No per-frame allocation in a screen.** A label is formatted when its
  value changes, never in `body()`.
- **Every `pub` item is documented.** `#![deny(missing_docs)]` is on in both
  crates; the one macro-generated factory carries a scoped `allow`.
- **A file under `src/` is at most 400 lines.**

## Where the documentation lives, and what proves each piece

| Document | Proven by |
|---|---|
| [`README.md`](README.md) and the three nested READMEs | their paths and commands resolve; none carries a `rust` fence |
| [`docs/README.md`](docs/README.md) | its paths resolve; the README-heading check exempts it, because it is the index of `docs/`, not a front page |
| [`docs/tutorial.md`](docs/tutorial.md), [`docs/tutorial-host.md`](docs/tutorial-host.md) and [`docs/tutorial-firmware.md`](docs/tutorial-firmware.md) | the tutorial in three parts; every `rust` fence is a doctest of `xpui-cpp-host`, mounted by `cpp_host/src/lib.rs`; every `cpp` fence is compiled by the gate |
| [`docs/reference.md`](docs/reference.md) | its paths resolve; it links every page under `docs/reference/` |
| [`docs/reference/host.md`](docs/reference/host.md) | `the reference mirrors rustdoc`, for `xpui_cpp_host`, with the `screens` items exempt as the worked example; its `rust` fences are doctests mounted by `cpp_host/src/lib.rs`, and its `cpp` fences are compiled |
| [`docs/reference/abi.md`](docs/reference/abi.md) | written by hand against `xpui_app.h` and `xpui_host.h`, and changed with them. Every `cpp` fence is compiled against the real headers, which catches a declaration whose return type drifted; nothing compares its prose. Its headings are plain text, because `the reference mirrors rustdoc` reads this directory and a heading that is one code span names a Rust item |
| [`docs/boundary.md`](docs/boundary.md) | its paths resolve; the symbol sets are what `the header's symbols are all defined` and `abi/` check; the nine cases are the ones `CMakeLists.txt` registers |
| [`docs/contributing.md`](docs/contributing.md) | every path and command it gives resolves; the umbrella command is `xpui-dev`'s |
| `AGENTS.md` | the stage list above is compared to what the gate runs, in both modes |
| every `///`, `//!` and C++ `//` | `rustdoc links resolve`, and the two comment checks |

## Git

Never stage, never commit, never push without being asked, each time. The
index is the reviewer's queue; leave new work unstaged. No self-attribution
in a commit message. Never rewrite a commit that exists; a correction is a new
commit. The rules that apply to all ten repositories, and the five review
steps, are in [`xpui`'s `docs/orientation.md`](https://github.com/XPUI-Framework/xpui-framework/blob/main/docs/orientation.md);
how a change is built and reviewed here is in
[`docs/contributing.md`](docs/contributing.md).
