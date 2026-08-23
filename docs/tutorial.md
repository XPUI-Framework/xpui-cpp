# Rust screens in a C++ firmware

You have a C++ firmware. You want to write its next screen in Rust, without
rewriting the firmware and without the two halves drifting apart.

This walks the whole boundary once. Every block below is compiled by
`./build-and-test.sh` — the Rust ones as doctests, the C++ ones each as its own
translation unit — so a snippet that stops being true fails the build rather
than quietly misleading whoever reads it next.

`cargo test --workspace` runs the Rust half by hand.

The finished thing is [`cpp_host`](../cpp_host), which runs in a window, and
[`firmware`](../firmware), which is the same code in an ESP32 image. Read
either beside this.

## The shape of it

Four sets of C symbols, two crossing each way. That is the whole design, and
everything below is one of these four boxes:

```text
   your C++                                          Rust
   ────────                                          ────
   main, screen stack, input, panel
        │
        ├── xpui_screen_*  drive a screen ────────►  xpui-fui defines these
        ├── xpui_app_*     make a screen ─────────►  your crate defines these
        │
        ◄── xpui_fui_*     draw ──────────────────── xpui-fui calls these
        ◄── xpui_host_*    input, i18n, device ───── your crate calls these
```

The two arrows pointing left are things **you** implement in C++. The two
pointing right are things Rust gives you. Nothing else crosses.

---

## 1. Write a screen

A screen is a struct with a `body()` and an `update()`. Nothing about it knows
it will be driven from C++ — this is the same code the desktop simulator runs.

```rust
use xpui::screen::Screen;
use xpui::{List, ListRow, NavigationScreen, View};

pub struct Settings {
    wifi: bool,
}

#[derive(Clone, Copy)]
pub enum Message {
    ToggleWifi,
}

impl Settings {
    pub fn new() -> Self {
        Settings { wifi: false }
    }
}

impl Screen for Settings {
    type Message = Message;

    fn body(&self) -> impl View<Message> {
        NavigationScreen::new(
            List::new().push(
                ListRow::new("Wi-Fi")
                    .value(if self.wifi { "On" } else { "Off" })
                    .on_tap(Message::ToggleWifi),
            ),
        )
        .title("Settings")
    }

    fn update(&mut self, message: Message) {
        match message {
            Message::ToggleWifi => self.wifi = !self.wifi,
        }
    }
}
```

If you have written a screen for the simulator before, this is that. Skip to
step 2.

## 2. The words on it come from your firmware

Nothing in the screen above is in English on purpose. `xpui` has no idea what
language its user reads, and neither does the backend — so every label a
person sees is looked up through the host, and the framework never holds a
word of its own.

That is one function, and you already have the table behind it:

```rust
use core::ffi::CStr;
use xpui::screen::Screen;
use xpui::{NavigationScreen, Text, View};

// Your firmware's lookup. `cpp_host`'s is `strings::tr`, which crosses the
// boundary to a table in C++; this stands in for it so the snippet compiles
// with no host linked.
fn tr(key: &'static CStr) -> &'static str {
    match key.to_bytes() {
        b"STR_BRIGHTNESS" => "Brightness",
        // An unknown key comes back as the key itself, which is the whole
        // trick: a missing translation reads as STR_BRIGHTNESS on the panel
        // rather than as a blank row nobody can explain.
        _ => key.to_str().unwrap_or(""),
    }
}

struct Brightness;

impl Screen for Brightness {
    type Message = ();

    fn body(&self) -> impl View<Self::Message> {
        NavigationScreen::new(Text::new(tr(c"STR_BRIGHTNESS"))).title(tr(c"STR_BRIGHTNESS"))
    }

    fn update(&mut self, _message: Self::Message) {}
}

assert_eq!(tr(c"STR_BRIGHTNESS"), "Brightness");
assert_eq!(tr(c"STR_NOT_ADDED_YET"), "STR_NOT_ADDED_YET");
```

**Add the key to your English table and stop.** Do not copy it into the other
thirty language files: a missing key falls back, and that absence *is* how a
translation backlog is tracked. Copying English into a French file makes the
row look finished.

Three things about the signature, each of which is a fault somebody has hit:

- **`&'static CStr`, so a `c"…"` literal allocates nothing.** `body()` runs on
  every paint and every input frame; a `CString` built there is an allocation
  per frame per row.
- **`'static` is a soundness bound, not a convenience.** An unknown key comes
  back as *the caller's own pointer*. A borrowed key would be handed back as a
  `&'static str` that dangles the moment it drops, so the bound makes that
  unwriteable.
- **Never hardcode a numeric string id.** Whatever generates your table
  renumbers when a key is inserted in the middle. Look up by name.

`cpp_host/cpp/host_i18n.cpp` is the table, built by hand because one built by
hand is enough to prove the path. A firmware generates it from translation
files, and nothing above changes.

---

## 3. Export it to C++

C++ cannot name a Rust type, so it never sees one. It gets an opaque handle.

```rust
use xpui::screen::Screen;
use xpui::{NavigationScreen, Text, View};
use xpui_fui::register_screen;

# pub struct Settings;
# impl Settings { pub fn new() -> Self { Settings } }
# impl Screen for Settings {
#     type Message = ();
#     fn body(&self) -> impl View<()> {
#         NavigationScreen::new(Text::new("Settings")).title("Settings")
#     }
#     fn update(&mut self, _message: ()) {}
# }
register_screen!(Settings, xpui_app_create_settings);
```

That macro is the whole of it. It exports one C function:

```text
void* xpui_app_create_settings(void);
```

Declare it in a header your C++ includes — [`cpp_host/cpp/xpui_app.h`](../cpp_host/cpp/xpui_app.h)
is the worked example — and you have a screen your firmware can hold.

> Fenced `text` rather than `cpp`, because it is a declaration of a symbol the
> Rust side defines: there is nothing here for a C++ compiler to check that the
> linker will not check better.

## 4. Drive it

Six calls, declared in [`cpp/xpui_screen.h`](https://github.com/XPUI-Framework/xpui-backends/blob/main/fui/cpp/xpui_screen.h) and
**defined in Rust**. Your C++ calls them; you implement none of them.

```cpp
#include <xpui_screen.h>

// The smallest thing that can run a screen. `cpp_host`'s ScreenHost
// is this plus a title and the globals; a firmware's activity class is this
// plus whatever it already had.
class Screen {
 public:
  explicit Screen(void* handle) : handle_(handle) { xpui_screen_on_enter(handle_); }

  ~Screen() {
    xpui_screen_on_exit(handle_);
    xpui_screen_destroy(handle_);
  }

  // One frame. Input first, then paint — a screen asks for its repaint from
  // inside loop(), so painting first shows the frame before the one it wanted.
  void frame(bool paint) {
    xpui_screen_loop(handle_);
    if (paint) {
      xpui_screen_render(handle_);
    }
  }

 private:
  void* handle_;
};
```

The handle is a `void*` and must stay one. It is a double-boxed trait object on
the Rust side, so unwrapping it once too few is a wild pointer rather than a
type error — which is why nothing on this side ever looks inside it.

## 5. Open it from a menu you already have

A wrapper standing on its own proves the boundary works. It is not how anybody
ships one — the screen has to be reachable from the menu your firmware already
draws. Which menu that is decides which of two paths you take, and they are
genuinely different.

### If the menu is C++

This is the usual case in a firmware being converted a screen at a time. Three
edits, beside the rows that are already there: an entry in whatever enumerates
them, a row, and the case that opens it.

```cpp
#include <stdint.h>
#include <vector>

#include <xpui_app.h>

// Stand-ins for what your firmware already has.
enum class SettingAction { About, Controls, Brightness };
struct SettingInfo {
  const char* key;
  SettingAction action;
};

// The factory `register_screen!` exported in step 3.
extern "C" void* xpui_app_create_brightness(void);
void start_screen(void* handle);  // your firmware's, whatever it is called

void rebuild(std::vector<SettingInfo>& rows) {
  // A key, not a word. Step 2 is why: the menu is C++ and the screen is Rust,
  // and both read the same table.
  rows.push_back({"STR_BRIGHTNESS", SettingAction::Brightness});
}

void open(SettingAction action) {
  switch (action) {
    case SettingAction::Brightness:
      start_screen(xpui_app_create_brightness());
      break;
    default:
      break;
  }
}
```

Only the *entry point* needs a `register_screen!` factory. A screen reached
solely from another Rust screen needs none, which is why `cpp_host` exports
exactly one — `xpui_app_create_menu`.

### If the menu is already a Rust screen

Then it is one call, and the stack is still C++'s:

```rust
use xpui::screen::Screen;
use xpui::{List, ListRow, NavigationScreen, Text, View, present};

# struct Brightness;
# impl Brightness { fn new() -> Self { Brightness } }
# impl Screen for Brightness {
#     type Message = ();
#     fn body(&self) -> impl View<()> { NavigationScreen::new(Text::new("Brightness")) }
#     fn update(&mut self, _: ()) {}
# }
struct Menu;

#[derive(Clone, Copy)]
enum Open {
    Brightness,
}

impl Screen for Menu {
    type Message = Open;

    fn body(&self) -> impl View<Self::Message> {
        NavigationScreen::new(
            List::new().push(ListRow::new("Brightness").on_tap(Open::Brightness)),
        )
    }

    fn update(&mut self, message: Self::Message) {
        match message {
            // `present` answers whether the host took it. A firmware that
            // can refuse — a stack at its depth limit — checks; this one is
            // showing the shape.
            Open::Brightness => {
                present(Brightness::new());
            }
        }
    }
}
```

`present` is not a push onto a Rust stack — there isn't one. It goes out
through `xpui_host_screen_present` as an opaque handle, onto the same C++ stack
the rest of your firmware uses. That is the whole reason forward navigation
works across this boundary at all.

**What you do not do is give the Rust side a stack of its own.** A firmware
with two has two ideas about what Back means, and the one the user is looking
at is whichever drew last.

---

## 6. Answer what the framework asks

Two headers' worth, and this is the part that is genuinely yours.

`xpui_fui.h` is drawing: 30 functions, and
[`cpp/xpui_fui.cpp`](https://github.com/XPUI-Framework/xpui-backends/blob/main/fui/cpp/xpui_fui.cpp) already implements every one of them
against FreeInkUI. **Add that file to your build and you are done with it.** A
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
| CMake, on a desktop | [`cpp_host/CMakeLists.txt`](../cpp_host/CMakeLists.txt) |
| PlatformIO, on an ESP32 | [`firmware/scripts/build_rust.py`](../firmware/scripts/build_rust.py) |

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

## 11. Put it on a board

> **Nothing checks anything below this line, and nobody here has run it.** The
> gate builds and runs the desktop host; it never flashes a device, and it
> cannot. Neither ESP32 board in this organisation has ever been powered on —
> there is no panel driver for either, which is the whole reason. These are the
> ordinary `esptool` and PlatformIO commands, transplanted from a firmware that
> does flash these chips; treat them as a starting point and read your board's
> own documentation beside them.

Plug the device in over USB-C. **Find out which chip it is first**, because
the firmware differs and the wrong image simply will not boot:

```bash
esptool --port /dev/cu.usbmodem2101 chip-id
```

If `esptool` is not on your path, PlatformIO ships one under
`~/.platformio/packages/tool-esptoolpy/`. Its subcommands were spelled with
underscores before esptool 5 — `chip_id`, `read_flash` — and still work, with a
deprecation warning naming the hyphenated form.

Read the `Chip type` line and pick the environment that matches:

| Chip | Board | PlatformIO environment |
|---|---|---|
| ESP32-C3 | Xteink X3 | `default` |
| ESP32-S3 | Seeed Sticky | `sticky` |

There is a third, `simulator_x3`, which builds the same code as a window on
this machine and flashes nothing.
[`firmware/README.md`](../firmware/README.md) records what each one produces —
and it is the page to trust on that, since this section is under a warning that
nothing here is checked.

Then build, flash, and watch it come up.
[`firmware/README.md`](../firmware/README.md) has the build commands and the
one PlatformIO trap worth knowing; flashing adds `-t upload` to them, and then
a second command to read the serial port:

```bash
pio run -e <environment> -t upload
pio device monitor
```

`platformio.ini` runs `scripts/build_rust.py` before every compile, so `pio
run` builds the Rust crates alongside the C++ and picks the Rust target from
the environment's MCU. There is no separate Rust step and no order to remember.

**You cannot brick it this way.** The first-stage bootloader lives in mask ROM
and cannot be overwritten, so the worst case is a device that does not boot:
hold **BOOT**, tap **RESET**, and flash again. If you want a restore point
before you start, take one first — **and read the size off the chip rather than
copying one**, because the two boards do not have the same flash:

```bash
esptool --port /dev/cu.usbmodem2101 flash-id      # prints "Detected flash size"
esptool --port /dev/cu.usbmodem2101 read-flash 0x0 ALL backup.bin
```

`ALL` reads whatever is there. Spelled out, the `default` environment's
`esp32-c3-devkitm-1` is 4 MB (`0x400000`) and `sticky`'s
`esp32-s3-devkitc1-n16r8` is 16 MB (`0x1000000`) — a number pasted from the
wrong board reads past the end or leaves most of it out.

If the build fails, read the **first** error rather than the last. Rust errors
cascade, and the twentieth is usually a consequence of the first.

**And one more warning about this path than the rest of the page needs.**
`firmware/` compiles the same shim through PlatformIO, and no gate in this
organisation ever invokes PlatformIO. A break there surfaces when somebody
builds a firmware — which may be you, now — rather than in any check.

---

## What changes for a real firmware

Not much, which is the point. [`firmware`](../firmware) is the same Rust — the same
crate, not a fork — and five C++ files:

| | |
|---|---|
| `main.cpp` | your firmware already has one |
| `host_input.cpp` | wire it to your input manager |
| `host_device.cpp` | your board configuration |
| `host_heap.cpp` | your RTOS |
| `host_panic.cpp` | your log |

Two things do change, and both are improvements:

**Rust allocates from your heap.** Give it a `#[global_allocator]` that calls
your `malloc`, as
[`cpp_host/src/runtime.rs`](../cpp_host/src/runtime.rs)
does, and `xpui_host_heap_*` then reports figures covering both languages. It
is the only visibility you get into what Rust costs at run time — a build-time
size report measures static sections, where it contributes almost nothing.

**Fragmentation becomes measurable.** On a desktop `largest_block` answers "I
cannot tell you". Your RTOS can, and on a device with no MMU the gap between
"free" and "largest block" is what actually decides whether the next allocation
fails.

## What it costs in memory

Measure it, because the intuition is wrong in a specific and expensive way.

**Rust contributes almost nothing to static RAM, no matter how much it uses.**
A build-time size report measures `.data`, `.bss` and `.noinit`, and a Rust
screen allocates its strings and its widget tree through the heap — your heap,
once you give it a `#[global_allocator]` routing to your `malloc`. A report
saying Rust costs 26 KB of static RAM is telling you about its statics and
nothing about the screen.

So the figure that matters is the heap, over a screen's lifetime:

```rust
# #[derive(Copy, Clone)]
# struct Reading { free: i32, largest_block: i32 }
# // Two readings a host actually gave, either side of a screen that leaked.
# fn heap_on_entry() -> Reading { Reading { free: 32_768, largest_block: 20_480 } }
# fn heap_on_exit() -> Reading { Reading { free: 32_720, largest_block: 18_944 } }
let before = heap_on_entry();
// ... open the screen, use it, close it ...
let after = heap_on_exit();

// A screen that does not return to its entry figure is leaking, and this one
// did: forty-eight bytes that never came back.
assert_eq!(before.free - after.free, 48);

// The gap between free and largest block is fragmentation, and on a device
// with no MMU it is what actually decides whether the next allocation fails.
// Plenty free and nowhere to put anything is a real failure.
assert!(after.largest_block < after.free);
assert_eq!(after.free - after.largest_block, 13_776);
```

`xpui_host_heap_*` is four functions — total, free, largest block, and the
lowest free has ever been — and your RTOS already answers all four. The last
is the only one that says whether you ever came *close*, and the third is the
one that catches fragmentation, which is the failure that looks like plenty of
memory right up until it doesn't.

On a desktop `largest_block` answers "I cannot tell you", and that is honest
rather than broken: a system allocator with virtual memory has no such number.

**There is no memory gate here.** A firmware should have one — a per-build
report checked against a budget file, failing the build when it grows — but
this repository ships a desktop example and a PlatformIO skeleton, and
inventing a limit for those would be inventing a number.

---

## The one rule that keeps it working

**Every C symbol lives in more than one place, and they move together.** The
drawing ABI is a header, a Rust declaration, a C++ definition and a host double
for tests; the host ABI is a header, a Rust declaration and your C++.

Miss one and you get a link error, which is loud. Get one *wrong* — a parameter
reordered, an `int32_t` narrowed — and it links perfectly and corrupts the call
frame, because C has no mangling to disagree with. The symptom is a rendering
fault somewhere unrelated.

Two checks stand behind that, and they are worth copying if you fork this:

- [`fui/tests/abi.rs`](https://github.com/XPUI-Framework/xpui-backends/blob/main/fui/tests/abi.rs)
  and [`abi/tests/abi.rs`](../abi/tests/abi.rs) parse each header and the Rust
  beside it and compare **signatures**, across five boundaries between them. An
  unrecognised C type fails the run rather than being skipped.
- `symbols_agree` in each repository's `xtask/` covers the half
  those cannot read: a header against the C++ that defines it.
