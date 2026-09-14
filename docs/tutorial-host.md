# Run the Rust screen in your C++ host

Part two of three, continuing from [Rust screens in a C++ firmware](tutorial.md),
where a [Rust](https://rust-lang.org/) screen was written, exported and driven from C++. This page
implements what the framework asks of your firmware, teaches the firmware a
new symbol, starts the UI, builds the Rust half into your link, and proves the
screen with no device. As on part one, every Rust block is a doctest and every
C++ block is compiled by `./build-and-test.sh`.

## 6. Answer what the framework asks

Two headers' worth, and this is the part that is genuinely yours.

`xpui_fui.h` is drawing: 30 functions, and
[`cpp/xpui_fui.cpp`](https://github.com/XPUI-Framework/xpui-backends/blob/main/fui/cpp/xpui_fui.cpp) already implements every one of them
against [FreeInkUI](https://github.com/Free-Ink/freeink-sdk/tree/main/libs/ui/FreeInkUI). **Add that file to your build and you are done with it.** A
firmware with its own themed renderer can implement the same header instead —
that is a supported path, not a fork.

`xpui_host.h` is everything else: input, translations, device identity, the
heap. Nobody can implement those for you, because they are your firmware. They
are small:

```cpp
#include <stdint.h>

#include <xpui_host.h>

// Your input manager already knows this. `button` is xpui::Button's own
// discriminant — 0 Back, 1 Confirm, 2 Left, 3 Right, 4 Up, 5 Down, and so on.
uint8_t xpui_host_was_pressed(uint8_t button) {
  // return my_input.wasPressed(my_button_for(button));
  (void)button;
  return 0;
}

// The only clock the framework has. It times key auto-repeat, and its contract
// is that it may wrap — what reads it measures short intervals.
uint32_t xpui_host_millis(void) {
  // return millis();
  return 0;
}
```

## 7. Teach the firmware something new

Sooner or later a screen wants something no header offers — the uptime, the
Wi-Fi state, the page count. That is **three edits, and a fourth if you want to
test on a laptop**. Missing the fourth is the most common mistake at this
boundary, and the one that fails days later.

Say the screen wants to show how long the device has been awake.

**7a. Write the C++ function.** Beside the other device facts, in whichever
file answers them:

```cpp
#include <stdint.h>

// Your firmware's clock.
static uint32_t millis(void) { return 0; }

extern "C" int32_t xpui_host_uptime_seconds(void) {
  return static_cast<int32_t>(millis() / 1000);
}
```

**7b. Declare it in the header**, so both languages read one description of
the symbol rather than two:

```text
int32_t xpui_host_uptime_seconds(void);
```

**7c. Declare it to Rust**, beside the other `xpui_host_*` entries in
`raw.rs`:

```rust
unsafe extern "C" {
    pub fn xpui_host_uptime_seconds() -> i32;
}
```

That is Rust promising the linker such a symbol exists. **Nothing checks the
signature for you at this point** — a swapped parameter or a narrowed
`int32_t` links perfectly, because C has no mangling to disagree with, and
corrupts the call frame instead.

**7d. Give your tests something to link against**, if your crate has tests.
`cargo test` runs on a laptop, where the firmware does not exist, so the symbol
is missing and the test binary does not link at all:

```rust
/// Stands in for the firmware, so a test binary links on a laptop.
#[unsafe(no_mangle)]
extern "C" fn xpui_host_uptime_seconds() -> i32 {
    4_321
}
```

`unsafe(no_mangle)` is not a typo. Exporting a symbol under a fixed name is
unsafe — nothing checks that the C++ side declares it the same way — and Rust
2024 makes you say so.

Then wrap it once, so no screen ever writes `unsafe`:

```rust
# // Stands in for 7c's `unsafe extern "C"` block, so this snippet links with
# // no firmware behind it. In your crate `raw` is that block, and the call
# // below is genuinely unsafe; here it is a constant wearing the shape.
# mod raw {
#     pub unsafe fn xpui_host_uptime_seconds() -> i32 {
#         4_321
#     }
# }
/// Seconds since the device booted.
pub fn uptime_seconds() -> i32 {
    // Safety: an integer in, an integer out; the host promises no more.
    unsafe { raw::xpui_host_uptime_seconds() }
}

assert_eq!(uptime_seconds(), 4_321);
```

**The double is the one that rots quietly**, because nothing calls it in a
firmware build. A missing `cpp_scrim` sat unnoticed in a firmware until a test
finally exercised an overlay.

Two things about where it goes. `xpui-fui` keeps *its* doubles — for the
drawing symbols, not these — in `src/testing/stubs.rs` behind a `testing`
feature, and that feature is **never** on in a staticlib you ship: it swaps 28
of the 30 drawing symbols for a recorder, so the archive links cleanly into
your C++ and draws nothing at all.

And **`cpp_host` deliberately has no `xpui_host_*` doubles**, which is why 7d
names no file in this repository. Its `Cargo.toml` says why: every wrapper here
is one `unsafe` call and nothing else, so a test binary would be testing a
double against a wrapper with no logic in it — a fourth place for the ABI to
rot, proving nothing. Your crate is different the moment a screen has state
worth asserting on, and then 7d is where the double goes.

---

## 8. Start it

Three calls, in this order, once:

```cpp
#include <stdint.h>

#include <xpui_app.h>
#include <xpui_fui.h>

// 1 bit per pixel, MSB first, (width + 7) / 8 bytes per row, and **a set bit
// is white** — FreeInkUI's convention, the inverse of the usual one.
static uint8_t framebuffer[(480 + 7) / 8 * 800];

void start_the_ui(void) {
  // Where the panel is. The shim draws into this and nothing else.
  xpui_fui_attach(framebuffer, 480, 800);

  // How a finished frame reaches the glass. Prefer this to overriding the weak
  // `xpui_fui_present`: whether an override wins depends on the object format,
  // and the failure is silent — a panel that never updates.
  xpui_fui_set_present([]() { /* mark dirty; blit after rendering, not here */ });

  // What paints, and what owns the screen stack. Before the first screen.
  xpui_app_install();
}
```

The ordering trap is worth stating plainly, because nothing checks it:
`xpui_fui_attach` tells the **shim** where the panel is, and `xpui_app_install`
tells the **framework** where the shim is. Neither works without the other, and
missing one is a blank screen with no error.

## 9. Build it

The Rust half is a `staticlib`. Your C++ links the archive:

```text
cargo build --release -p your-crate --target <your-triple>
# -> target/<your-triple>/release/libyour_crate.a
```

Two worked examples, and they differ only in the build system:

| | |
|---|---|
| [CMake](https://cmake.org/), on a desktop | [`cpp_host/CMakeLists.txt`](../cpp_host/CMakeLists.txt) |
| [PlatformIO](https://platformio.org/), on an ESP32 | [`firmware/scripts/build_rust.py`](../firmware/scripts/build_rust.py) |

Both call `cargo` in plain sight rather than through a helper, because forty
lines you can read transplant into another build system and a dependency that
hides the mechanism does not.

**Pass `--target` explicitly even for the host**, or cargo writes to
`target/release/` instead of `target/<triple>/release/` and your link path is
wrong. And note the trap beside it: the `dev` profile's directory is `debug`,
not `dev`.

## 10. Prove it without a device

`update()` is an ordinary function and `body()` is a pure description, so a
screen is testable on a laptop with no panel, no firmware and no window.

```rust
use xpui::screen::Screen;
use xpui::{NavigationScreen, Slider, View, testing};

struct Brightness {
    level: i32,
}

#[derive(Clone, Copy)]
enum Msg {
    Level(i32),
}

impl Screen for Brightness {
    type Message = Msg;

    fn body(&self) -> impl View<Self::Message> {
        NavigationScreen::new(Slider::new(self.level, 100).on_change(Msg::Level))
            .title("Brightness")
    }

    fn update(&mut self, message: Self::Message) {
        match message {
            Msg::Level(value) => self.level = value,
        }
    }
}

// State, with no host in sight.
let mut screen = Brightness { level: 40 };
screen.update(Msg::Level(75));
assert_eq!(screen.level, 75);

// And what a backend would be asked to paint. The fake host records every
// call, so this asserts the slider is drawn where the level says.
//
// `install` is explicit rather than required: a build with `xpui/testing` on
// installs the fake the first time anything asks for a host, so deleting this
// line changes nothing here. In a crate without that feature it is the
// difference between a test and a panic. `reset` is not optional — the call
// log is process-wide, and a test that does not clear it reads the previous
// one's frame.
testing::install();
testing::reset();
xpui::App::new(Brightness { level: 75 }).render();

let sliders = testing::drawn_sliders();
assert_eq!(sliders.len(), 1, "one slider");
let (_rect, value, max) = sliders[0];
assert_eq!((value, max), (75, 100));
```

Two things this does not prove, and both need the C++:

- **That the archive links.** A missing `xpui_host_*` definition is invisible
  until something links it, which is what `cpp_host`'s `ctest` cases are for.
- **That anything reached the glass.** `testing` records draw *calls*. Pixels
  are `xpui-screenshot` on a desktop, and eyes on a board.

The doctests on this page are subject to the same limit, and it is worth
knowing why: a snippet here is compiled as its own binary, so anything that
calls across the FFI would need the C++ half linked into it. That is why these
examples stand up their own `tr` and their own `raw` rather than calling
`cpp_host`'s.

---

**Next: [Put the Rust screen on a board](tutorial-firmware.md).** The screen
builds and runs on a laptop; part three flashes it to an ESP32, says what a
real firmware changes, and what the screen costs in memory.
