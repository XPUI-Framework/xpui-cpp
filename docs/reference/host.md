# The host library

`xpui-cpp-host` is the [Rust](https://rust-lang.org/) half of a C++ application that hosts `xpui` screens.
It compiles to a `staticlib` that the C++ links, and it has two public
functions: one wires the framework up, and one builds the screen the host
starts on. [The C ABI](abi.md) is every symbol on both sides of the boundary;
[the tutorial](../tutorial.md) builds the same thing a step at a time.

Its `screens` module is public too, because it is the half a firmware would
reuse. `Menu`, `About`, `Controls` and `Example` are the worked example, meant
to be read as source, and have no sections here.

## Topics

| | |
|---|---|
| [`xpui_app_install`](#xpui_app_install) | Installs what paints and what navigates. |
| [`xpui_app_create_menu`](#xpui_app_create_menu) | The root screen's factory, as `cpp/xpui_app.h` declares it. |

## `xpui_app_install`

Installs what paints and what navigates.

```text
pub unsafe extern "C" fn xpui_app_install()
```

**Two installs, not one.** `xpui` keeps them apart because they answer to
different owners, and a host that supplies only the first gets a screen that
draws perfectly and whose Back does nothing:

| Installs | Through | Without it |
|---|---|---|
| the host: the [FreeInkUI](https://github.com/Free-Ink/freeink-sdk/tree/main/libs/ui/FreeInkUI) backend, which asks the C++ for input, words and device figures through `xpui_host_*` | `xpui::host::install` | nothing paints |
| the navigator: the shell that turns `present` and a finished screen into `xpui_host_screen_present` and `xpui_host_screen_finish` | `xpui::host::install_navigator` | Back and `present` silently do nothing |

Each install is skipped when one is already in place, so a second call does
nothing. A firmware whose render task could run first calls it from each entry
point that could, as `xpui::host::install_navigator` asks, provided those calls
can never run at the same time as each other or as a frame. Call it before the
first screen is created.

> [!WARNING]
> **Safety.** No frame in flight on any task, and no other call to it running
> at the same time: from the activity's entry point before the render task
> starts, or between frames on the loop's own thread. The installs, and the
> checks that skip them, are plain statics with no locking, so overlapping one
> with a frame or with another call is a data race, not a stale pointer.

> [!NOTE]
> Nothing checks that `xpui_fui_attach` has run as well. Attaching tells the
> **shim** where the panel is; this tells the **framework** where the shim is.
> Missing either is a blank panel with no error.

**Example — starting the UI**

```cpp
#include <stdint.h>

#include <xpui_app.h>
#include <xpui_fui.h>

// 1 bit per pixel, (width + 7) / 8 bytes per row, and a set bit is white.
static uint8_t framebuffer[(480 + 7) / 8 * 800];

void* start_the_ui(void) {
  xpui_fui_attach(framebuffer, 480, 800);  // where the panel is
  xpui_app_install();                      // what paints, and what navigates
  return xpui_app_create_menu();           // only now is there a screen
}
```

**See also:** [`xpui_app_create_menu`](#xpui_app_create_menu), [its C declaration](abi.md#xpui_app_install)

## `xpui_app_create_menu`

The root screen's factory, as `cpp/xpui_app.h` declares it.

```text
pub extern "C" fn xpui_app_create_menu() -> *mut c_void
```

`register_screen!`, from `xpui-fui`, writes the function: the source holds
`register_screen!(screens::Menu, xpui_app_create_menu)`, and the declaration
above, marked `#[unsafe(no_mangle)]`, is what that expands to.

It builds a `screens::Menu` with `Menu::new` and returns it as an opaque handle.
The host calls it once, and owns the handle until it passes it to
`xpui_screen_destroy`. Between the two it drives the screen through
`xpui_screen.h`: `xpui_screen_on_enter` once, then `xpui_screen_loop` and
`xpui_screen_render` every frame, then `xpui_screen_on_exit`.
[The screen handle](abi.md#the-screen-handle) says what the pointer is and why
nothing on the C++ side looks inside it.

Only a screen the C++ opens directly needs a factory. A screen opened from
another Rust screen goes out through `present` and needs none, which is why
this crate exports exactly one.

**Example — running the root screen**

```cpp
#include <xpui_app.h>
#include <xpui_screen.h>

// After `xpui_app_install`.
void run_the_root(bool (*running)(void)) {
  void* menu = xpui_app_create_menu();
  xpui_screen_on_enter(menu);
  while (running()) {
    xpui_screen_loop(menu);  // input first: a screen asks for its repaint here
    xpui_screen_render(menu);
  }
  xpui_screen_on_exit(menu);
  xpui_screen_destroy(menu);  // the handle dangles from here on
}
```

**Example — a second factory beside it**

```rust
use xpui::screen::Screen;
use xpui::{NavigationScreen, Text, View};
use xpui_fui::register_screen;

pub struct Brightness;

impl Brightness {
    // `register_screen!` calls `new` on the type it is handed.
    pub fn new() -> Self {
        Brightness
    }
}

impl Screen for Brightness {
    type Message = ();

    fn body(&self) -> impl View<()> {
        NavigationScreen::new(Text::new("Brightness")).title("Brightness")
    }

    fn update(&mut self, _message: ()) {}
}

register_screen!(Brightness, xpui_app_create_brightness);
```

The C++ then needs one declaration, `void* xpui_app_create_brightness(void);`,
beside the menu's in `cpp_host/cpp/xpui_app.h`.

**See also:** [`xpui_app_install`](#xpui_app_install), [its C declaration](abi.md#xpui_app_create_menu)
