# Rust screens in a C++ firmware

You have a C++ firmware. You want to write its next screen in [Rust](https://rust-lang.org/), without
rewriting the firmware and without the two halves drifting apart.

This walks the whole boundary once, over three pages. This one writes a screen
and hands it to your C++; [the second](tutorial-host.md) links it into a host
and runs it; [the third](tutorial-firmware.md) takes it to a board. Every Rust
block on the three is a doctest, which `./build-and-test.sh` compiles on every
run. Every C++ block is compiled as its own translation unit by the same gate
**only when it finds the [FreeInkUI](https://github.com/Free-Ink/freeink-sdk/tree/main/libs/ui/FreeInkUI) headers and `xpui-backends`**; without
either, that stage prints `skipped:` and the gate still passes, so read its
note. CI has both, and fails rather than skips. [`README.md`](../README.md)'s
`## Requirements` says where each may sit. A snippet that stops being true then
fails the build rather than quietly misleading whoever reads it next.

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
[The C ABI reference](reference/abi.md) is the reference for all four: who
defines each, and every function in the two this repository owns.
[`boundary.md`](boundary.md) says what proves the two sides agree.

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

**Next: [Run the Rust screen in your C++ host](tutorial-host.md).** The screen
is exported and your C++ can drive it; part two answers what the framework
asks of your firmware, starts the UI, builds the archive, and tests the screen
with no device.
