[![CI](https://github.com/XPUI-Framework/xpui-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/XPUI-Framework/xpui-cpp/actions/workflows/ci.yml) [![MIT](https://img.shields.io/badge/license-MIT-blue.svg)](../LICENSE)

# `xpui-cpp-host`

The worked example of a C++ application hosting `xpui` screens: a desktop
window over SDL2, a screen stack the C++ owns, and three Rust screens on it.
**It exists to prove the C ABI, not to develop screens in.** Screen work
happens in `cargo run -p xpui-gallery`, where an iteration costs a Rust
rebuild rather than a CMake one; everything here is kept as small as it can
be while still being a real application. It is the only thing **any gate in this
organisation** compiles `xpui-backends`' `fui/cpp/xpui_fui.cpp` into, links
and runs; [`firmware/`](../firmware/)'s `simulator_x3` environment builds the
same binary through PlatformIO, which no gate invokes.

## Using it

From the repository root:

```bash
cmake -S cpp_host -B target/cpp_host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build target/cpp_host

./target/cpp_host/xpui-host                      # a window
./target/cpp_host/xpui-host --headless --frames 30 --selftest
```

Arrows move focus, Enter opens, Backspace goes back, H is the home gesture, Q
or Escape quits — the same keys `xpui-simulator` uses, deliberately. The two
would otherwise disagree about what Enter does, and both are on this machine.

`-D FREEINK_SDK_DIR=<path>` uses an SDK you already have. Without it CMake
fetches the revision pinned in [`freeink-sdk.rev`](freeink-sdk.rev), which CI
reads too.

What it deliberately does not do:

- **No touch.** Every touch method of `Platform` keeps its default: they are
  a platform concern rather than an ABI one, and `xpui-simulator` already
  drives them with a mouse.
- **No overlay screens.** A dialog here is an overlay inside a screen's own
  body. A *stack* of them would need `Driver::is_overlay` across the boundary,
  and `xpui_screen.h` is six functions on purpose.
- **No Rust test harness.** Every `xpui_host_*` symbol this crate calls is
  defined by the C++, so a Rust test binary could only link doubles — a
  fourth place for the ABI to rot, testing wrappers with no logic. What has
  to be proven is that the whole stack links and draws, and that is `ctest`.
- **The heap figures cover C++ only.** Rust on a desktop has its own
  allocator. Joining them is a `#[global_allocator]` routing to the host's
  `malloc`, which is a firmware concern, and [`firmware/`](../firmware/) is
  where it is joined.

## Checking it

From the repository root, once the host is built:

```bash
ctest --test-dir target/cpp_host --output-on-failure
```

Nine cases, all headless, every assertion an exit code; `./build-and-test.sh
all` from the root builds and runs them, and what each catches is in
[`docs/boundary.md`](../docs/boundary.md). Every snippet in this file is
fenced `bash` or `text`: the gate requires each Rust fence in the repository
to be compiled by something, and a doctest for this crate would have to link
the C++ half.

## Where next

| | |
|---|---|
| [`docs/boundary.md`](../docs/boundary.md) | the four symbol sets, the firmware this host was modelled on and the three deliberate differences, and what the nine `ctest` cases prove |

## License

MIT — see [LICENSE](../LICENSE). Copyright (c) 2026 Thiago Holanda.
