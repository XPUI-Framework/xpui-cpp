# A C++ host

**This exists to prove the C ABI, not to develop screens in.** Screen work
happens in `cargo run -p xpui-gallery`, where an iteration costs a Rust rebuild
rather than a CMake one. Everything here is kept as small as it can be while
still being a real application.

Until this example existed, `crates/backend/fui/cpp/xpui_fui.cpp` — 882 lines
that a firmware is told to add to its build — had never been compiled to an
object, never linked, and never run. Only syntax-checked. This is the thing
that runs it.

```bash
cmake -S examples/cpp_host -B target/cpp_host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build target/cpp_host
ctest --test-dir target/cpp_host --output-on-failure

./target/cpp_host/xpui-host                      # a window
./target/cpp_host/xpui-host --headless --frames 30 --selftest
```

Arrows move focus, Enter opens, Backspace goes back, H is the home gesture, Q
or Escape quits — the same keys `xpui-simulator` uses, deliberately. The two
would otherwise disagree about what Enter does, and both are on this machine.

`-D FREEINK_SDK_DIR=<path>` uses an SDK you already have. Without it CMake
fetches the revision pinned in [`freeink-sdk.rev`](freeink-sdk.rev), which CI
reads too.

## The boundary

Four sets of C symbols, two crossing each way, each declared in exactly one
header:

| Header | Declares | Defined in | Called from |
|---|---|---|---|
| `crates/backend/fui/cpp/xpui_fui.h` | drawing | `xpui_fui.cpp` | Rust |
| `crates/backend/fui/cpp/xpui_screen.h` | the screen lifecycle | `xpui-fui`'s `lifecycle.rs` | C++ |
| `cpp/xpui_host.h` | input, i18n, device, heap, navigation | `cpp/host_*.cpp` | Rust |
| `cpp/xpui_app.h` | install, and the root screen's factory | `src/lib.rs` | C++ |

`ffi_symbols_agree()` in `build-and-test.sh` fails when any of those pairs
stops agreeing about *names*. It cannot see types — two parameters swapped
still links, and still corrupts the call frame — which is what
[spec 07](../../docs/specs/07-ffi-checker.md) is for.

## What this was modelled on

This host mirrors the layering of the e-reader firmware it was modelled on,
which is why the map below is worth reading before moving a file. **It is a
reference, not an obligation**: this host is what the gate builds and runs, and
what an ABI change is designed and proven against. A firmware consuming the ABI
is on its own cadence, and nothing links the two.

The right-hand column is that firmware's paths, kept so the shapes can be
compared.

| Here | The firmware it was modelled on |
|---|---|
| `cpp/main.cpp` | `src/main.cpp`, and the Arduino loop |
| `cpp/ScreenHost.{h,cpp}` | `src/activities/ActivityRs.{h,cpp}` |
| `cpp/ScreenStack.{h,cpp}` | `src/activities/ActivityManager.{h,cpp}` |
| `cpp/Display.{h,cpp}` | `GfxRenderer` and the panel driver |
| `cpp/Input.{h,cpp}` | `src/MappedInputManager.cpp` |
| `cpp/internal.h` | `src/rust_ffi/internal.h` |
| `cpp/host_input.cpp` | `src/rust_ffi/input.cpp` |
| `cpp/host_screen.cpp` | `src/rust_ffi/activity.cpp` |
| `cpp/host_i18n.cpp` | `src/rust_ffi/i18n.cpp` |
| `cpp/host_device.cpp` | `src/rust_ffi/device.cpp` |
| `cpp/host_heap.cpp` | `src/rust_ffi/heap.cpp` |
| `src/raw.rs` | `lib/backend_rs/src/raw.rs` |
| `src/platform.rs` | `lib/backend_rs/src/input.rs` |
| `src/shell.rs` | the `Navigator` half of `lib/backend_rs/src/firmware.rs` |
| `src/strings.rs` | `lib/backend_rs/src/i18n.rs` |
| `src/device.rs` | `lib/backend_rs/src/device.rs` |
| `src/screens/` | `lib/crosspoint_rs/src/activities/` |
| `crates/backend/fui/src/lifecycle.rs` | `lib/backend_rs/src/lifecycle.rs` |

CrossPoint's `renderer.rs`, `theme.rs`, `font.rs`, `icon.rs` and `cells.rs`
have no counterpart because `crates/backend/fui` **is** their counterpart:
that firmware wrote its own backend against its own renderer, and this example
uses the one in this repository. Its `runtime.rs` — the global allocator and
the panic handler — has none either, because those are `no_std` device
concerns and belong with
[spec 17](../../docs/specs/17-platformio-example.md).

## Three differences from the firmware, on purpose

Each is documented at its site so it is not "fixed" back:

- **No `renderer` argument on render.** CrossPoint's own comment says its one
  is unused; a parameter nothing reads can drift from the header unnoticed.
- **One install, not two.** That firmware installs the host from `on_enter`
  *and* `render` because two FreeRTOS tasks race for the first frame. A host
  with one thread installs once, before the first screen exists.
- **`Navigator::present` crosses the FFI.** The firmware cannot push a Rust
  screen from Rust; here it can, because the handle is opaque and thin. A host
  that declines the push has not taken ownership, and the screen is reclaimed
  rather than leaked.

## What it deliberately does not do

- **No touch.** Every touch method of `Platform` keeps its default, because
  they are a platform concern rather than an ABI one and `xpui-simulator`
  already drives them with a mouse.
- **No overlay screens.** A dialog here is an overlay inside a screen's own
  body. A *stack* of them would need `Driver::is_overlay` across the boundary,
  and `xpui_screen.h` is six functions on purpose.
- **No Rust test harness.** Every `xpui_host_*` symbol this crate calls is
  defined by the C++, so a Rust test binary could only link doubles — a fourth
  place for the ABI to rot, testing wrappers with no logic. What has to be
  proven is that the whole stack links and draws, and that is `ctest`.
- **The heap figures cover C++ only.** Rust on a desktop has its own
  allocator. Joining them is a `#[global_allocator]` routing to the host's
  `malloc`, which is a firmware concern — spec 17 again.

## What proves it

Eight `ctest` cases, all headless. **Every assertion is an exit code**, never a
pattern matched against the summary line: ctest ignores a test's exit status
entirely once `PASS_REGULAR_EXPRESSION` is set, so a run whose `--selftest`
printed a failure and exited 1 was still reported as passing. That mistake was
in here and is the reason for the `--expect-*` flags.

`--selftest` exits non-zero unless all three of these hold. They fail
independently, and each has been broken on purpose to check that it notices:

| | Catches |
|---|---|
| something asked for the panel to update | the present hook never fired |
| a frame was blitted | the loop never painted |
| the framebuffer holds both ink and paper | **a build that linked `xpui-fui/testing`** — every C symbol replaced by a host double, links cleanly, draws nothing |

The last one is the reason this is a self-test and not just an exit code. An
all-white panel is its only symptom anywhere.

The cases:

| Test | What would otherwise go unnoticed |
|---|---|
| `selftest` | the three above |
| `selftest_weak_present` | the weak-symbol override losing to the shim's own no-op — a panel that never updates, with nothing to point at |
| `navigates_into_rust` | `Navigator::present` crossing the FFI: a screen leaves Rust as an opaque handle and arrives on the C++ stack |
| `back_pops_to_the_root` | the pop half of the same thing; a stack that never pushed and one that never popped are each half right |
| `draws_an_overlay` | the scrim — the call that rots quietly, because only an overlay reaches it |
| `a_plain_screen_is_mostly_paper` | an **inverted panel**. Flip the framebuffer's polarity and every other case still passes |
| `a_focused_value_control_differs_from_an_idle_one` | a `Chrome::draw_slider` that ignores the state it is handed |
| `an_open_value_control_differs_from_a_focused_one` | the two states drawn *identically* — which is what "paper over paper" did here, and what an ink percentage cannot see |

The last two run the binary twice and fail unless the two frames differ,
because that is the only shape the question fits: a state you cannot see is a
refresh spent saying nothing, and an outline is too little ink to move a
percentage. They compare a **crop** of the control band rather than the whole
panel — the hint bar changes whenever the keys change meaning, so two whole
frames always differ somewhere and a full-frame comparison would pass without
ever looking at the control. They also pass `--no-pair`, since a keyboard has
arrow keys and the framework never opens an edit where the pair exists.

`draws_an_overlay` and `a_plain_screen_is_mostly_paper` are a pair on purpose. `--expect-ink` asserts a *relationship* —
a scrim is ink on one checkerboard parity of everything behind the dialog, so
the panel goes from a few percent ink to about half — and it is bounded on both
sides, because a dialog whose body never paints leaves the scrim covering what
the popup would have cleared and pushes the figure *up*. A one-sided assertion
would get greener as the dialog disappeared.

`--weak-present` runs the same thing through the weak `xpui_fui_present`
symbol this binary overrides, instead of the `xpui_fui_set_present` hook. The
hook is what a firmware should copy — whether an override beats a weak
definition is the linker's business and the failure is silent — and the flag
exists so that path is proven here rather than assumed.

**What none of this covers**: the window, as a person sees it. The loop is
tested headlessly; that is not the same thing. Run `./target/cpp_host/xpui-host`
and drive it.

## A note on the code blocks

Every snippet in this file is fenced `bash` or `text`. `./build-and-test.sh`
requires each ```rust block in the repository to be compiled by something, and
a doctest for this crate would have to link the C++ half.
