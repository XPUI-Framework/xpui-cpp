# `xpui-cpp`

> ⚠️ **Under heavy development.** Not production-ready. The API can break
> without notice. Use at your own risk.

The C++ side of the boundary: an application that already owns its screen
stack, hosting `xpui` screens over a C ABI.

This is the repository to read if you have a firmware written in C++ and want
one screen of it in Rust. Not a rewrite — a screen at a time, called through
six lifecycle entry points and an opaque `void*`.

## Which directory you want

| | |
|---|---|
| [`docs/tutorial.md`](docs/tutorial.md) | **Start here.** Write a screen, export it, drive it from C++, answer what the framework asks, start it |
| [`cpp_host`](cpp_host/) | The worked example, and the only thing any gate in this organisation builds, links and *runs* the FreeInkUI shim through. Design an ABI change here |
| [`firmware`](firmware/) | The same C++ through PlatformIO, for a real ESP32. No gate invokes it, so a break surfaces when somebody builds a firmware |
| [`abi`](abi/) | Two of the five ABI boundaries, checked: `xpui_app.h` and `xpui_screen.h` against the Rust that defines them |

## The boundary runs both ways

Four sets of symbols, two crossing each way, and every one declared in exactly
one header:

```text
C++ (main, ScreenStack, Display, Input)
  |  xpui_screen_*   lifecycle, defined in xpui-fui   ->
  |  xpui_app_*      your screens                     ->
  <- xpui_host_*     answered by cpp/host_*.cpp
  <- xpui_fui_*      drawing, answered by cpp/xpui_fui.cpp
```

The handle is double-boxed — `Box<Box<dyn Driver>>` — because `dyn Driver` is a
fat pointer and cannot cross as one word. Unwrap it once too few and it is a
wild pointer, not a type error.

## What it depends on

[`xpui`](https://github.com/XPUI-Framework/xpui-framework) and the FreeInkUI
backend from
[`xpui-backends`](https://github.com/XPUI-Framework/xpui-backends), whose
`fui/cpp/` this compiles and links. That one is a **sibling checkout**, not a
package path: nothing is published, and a git dependency lands in a cargo
checkout directory with no path a `CMakeLists.txt` can name. Clone it beside
this repository, or set `XPUI_BACKENDS_DIR`.

The FreeInk SDK is the same arrangement — beside the checkout, or named by
`FREEINK_SDK_INCLUDE`, or fetched by CMake at the revision
[`cpp_host/freeink-sdk.rev`](cpp_host/freeink-sdk.rev) pins.

Nothing depends on this repository. It is the far end.

## Checking it

```bash
./build-and-test.sh          # format, lint, test, and every snippet
./build-and-test.sh all      # plus cmake, the link, and eight ctest cases
```

**The window has never been looked at by a person.** The loop is tested
headlessly, which is not the same thing.
