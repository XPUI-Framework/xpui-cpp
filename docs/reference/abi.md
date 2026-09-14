# The C ABI

The C functions that cross between a C++ application and its Rust screens, as
this repository's two headers declare them:
[`cpp_host/cpp/xpui_app.h`](../../cpp_host/cpp/xpui_app.h), which is what the
application exports, and [`cpp_host/cpp/xpui_host.h`](../../cpp_host/cpp/xpui_host.h),
which is what the host answers.

**This page is written by hand against those two headers.** Change it in the
same commit as either of them, because no check compares its prose with them.
Three checks do guard the boundary itself:
- **the fences:** every `cpp` fence here is compiled against the real headers
  by `documented C++ compiles`, so a declaration whose return type no longer
  matches its header fails the gate;
- **the names:** `the header's symbols are all defined` checks that
  `xpui_host.h`'s names are defined by both C++ hosts;
- **the types:** [`abi/`](../../abi/) checks both headers against the Rust.

Each declaration below is shown with the `#include` that brings it, because
each fence is compiled on its own.

## The four headers

Every symbol that crosses is declared in exactly one header. Two headers belong
to `xpui-backends`; the other two are this repository's, and this page covers
both.

| Header | Declares | Defined in | Called from |
|---|---|---|---|
| `xpui-backends`' `fui/cpp/xpui_fui.h` | drawing | `xpui_fui.cpp` | Rust |
| `xpui-backends`' `fui/cpp/xpui_screen.h` | the screen lifecycle | `xpui-fui`'s `lifecycle.rs` | C++ |
| [`cpp_host/cpp/xpui_host.h`](../../cpp_host/cpp/xpui_host.h) | input, the screen stack, translations, device, heap, panics | `cpp/host_*.cpp` | Rust |
| [`cpp_host/cpp/xpui_app.h`](../../cpp_host/cpp/xpui_app.h) | install, and the root screen's factory | `src/lib.rs` | C++ |

## Topics

### What the application exports: `xpui_app.h`

| | |
|---|---|
| [`xpui_app_install`](#xpui_app_install) | Installs what paints and what navigates. |
| [`xpui_app_create_menu`](#xpui_app_create_menu) | Builds the root screen. |

### What the host answers: `xpui_host.h`

| | |
|---|---|
| [`xpui_host_was_pressed`](#xpui_host_was_pressed) | Whether `button` went down this frame. |
| [`xpui_host_is_pressed`](#xpui_host_is_pressed) | Whether `button` is held. |
| [`xpui_host_was_released`](#xpui_host_was_released) | Whether `button` came up this frame. |
| [`xpui_host_was_home_gesture`](#xpui_host_was_home_gesture) | Whether the system-level "go home" gesture was made. |
| [`xpui_host_has_left_right_keys`](#xpui_host_has_left_right_keys) | Whether this device has a Left/Right pair. |
| [`xpui_host_millis`](#xpui_host_millis) | Milliseconds since the host started. |
| [`xpui_host_screen_title`](#xpui_host_screen_title) | The running screen's title. |
| [`xpui_host_screen_finish`](#xpui_host_screen_finish) | Pops the running screen. |
| [`xpui_host_screen_present`](#xpui_host_screen_present) | Pushes a screen the Rust side built, with the title to show for it. |
| [`xpui_host_tr`](#xpui_host_tr) | Looks `key` up in the host's string table. |
| [`xpui_host_device_name`](#xpui_host_device_name) | The device's name. |
| [`xpui_host_firmware_version`](#xpui_host_firmware_version) | The firmware's version. |
| [`xpui_host_battery_percent`](#xpui_host_battery_percent) | The battery's charge, from 0 to 100. |
| [`xpui_host_heap_total`](#xpui_host_heap_total) | The heap's size, in bytes. |
| [`xpui_host_heap_free`](#xpui_host_heap_free) | The bytes free on the heap now. |
| [`xpui_host_heap_largest_block`](#xpui_host_heap_largest_block) | The largest single block the heap could hand out now, in bytes. |
| [`xpui_host_heap_min_free`](#xpui_host_heap_min_free) | The fewest bytes the heap has ever had free. |
| [`xpui_host_panic`](#xpui_host_panic) | Where a Rust panic lands on a device. |

### Conventions

| | |
|---|---|
| [Strings](#strings) | NUL-terminated `const uint8_t*`, immortal or borrowed for the call. |
| [The screen handle](#the-screen-handle) | An opaque `void*`, never looked inside. |
| [Button numbers](#button-numbers) | `xpui::Button`'s own discriminants, 0 to 14. |
| [Negative figures](#negative-figures) | A figure the host cannot measure. |

## Conventions

### Strings

Strings cross as NUL-terminated `const uint8_t*` rather than `const char*`,
because `char`'s signedness is implementation-defined. Every pointer carries one
of two promises, and each function says which:

| Promise | Means | Carried by |
|---|---|---|
| **Immortal** | Valid for the rest of the program. Rust reads it as a `&'static str`, which no compiler checks, so a pointer into anything later freed hands the framework a dangling reference. `cpp_host` keeps `intern()` for exactly this. | what `xpui_host_screen_title`, `xpui_host_device_name` and `xpui_host_firmware_version` return, and what `xpui_host_tr` returns for a known key |
| **Borrowed for the call** | Valid until the function returns. Copy what you need. | `title` in `xpui_host_screen_present`, `message` in `xpui_host_panic` |

### The screen handle

A screen is a `void*`: what a factory such as
[`xpui_app_create_menu`](#xpui_app_create_menu) returns, and what
[`xpui_host_screen_present`](#xpui_host_screen_present) hands the host. The
C++ drives it through `xpui-backends`' `xpui_screen.h` and frees it, once, with
`xpui_screen_destroy`.

On the Rust side it is a double-boxed `Box<Box<dyn Driver>>`, because
`dyn Driver` is a fat pointer and cannot cross as one word. Nothing on the C++
side ever looks inside it: unwrapped once too few, it is a wild pointer rather
than a type error.

### Button numbers

Every `button` parameter is `xpui::Button`'s own discriminant. A number outside
the range answers 0 rather than reading off the end of a table.

| Number | Button | Number | Button | Number | Button |
|---|---|---|---|---|---|
| 0 | `Back` | 5 | `Down` | 10 | `NavPrevious` |
| 1 | `Confirm` | 6 | `Power` | 11 | `ScreenLeft` |
| 2 | `Left` | 7 | `PageBack` | 12 | `ScreenRight` |
| 3 | `Right` | 8 | `PageForward` | 13 | `ScreenUp` |
| 4 | `Up` | 9 | `NavNext` | 14 | `ScreenDown` |

### Negative figures

The battery and the four heap figures are signed so that a host can say "I
cannot measure that": **any negative answer**. It is a different answer from 0,
and must never be reported as one. A host with no battery is not a flat
battery, and a heap whose fragmentation is invisible has not run out.

## xpui_app_install

Installs what paints and what navigates.

```cpp
#include <xpui_app.h>

void xpui_app_install(void);
```

Call it from the thread that runs the frame loop, before creating the first
screen; a second call is a no-op. Both installs are plain statics with no
locking, so installing while a frame is in flight is a data race rather than a
stale pointer.

Order matters with `xpui_fui_attach`: attaching tells the **shim** where the
panel is, and this tells the **framework** where the shim is. Neither works
without the other, and nothing checks that both happened.

Defined in Rust: [`xpui_app_install`](host.md#xpui_app_install).

## xpui_app_create_menu

Builds the root screen.

```cpp
#include <xpui_app.h>

void* xpui_app_create_menu(void);
```

The handle belongs to the caller from here on, as
[the screen handle](#the-screen-handle) describes. `register_screen!` defines
it in Rust, as [`xpui_app_create_menu`](host.md#xpui_app_create_menu).

Adding a screen the host can open directly is one `register_screen!` line in
`cpp_host/src/lib.rs` and one declaration in `xpui_app.h`.

## xpui_host_was_pressed

Whether `button` went down this frame.

```cpp
#include <xpui_host.h>

uint8_t xpui_host_was_pressed(uint8_t button);
```

| Parameter | Meaning |
|---|---|
| `button` | A [button number](#button-numbers). Out of range answers 0. |

Non-zero for exactly one frame. Reading it does not consume it: the framework
asks the same question more than once per frame, and must get the same answer
each time.

**Example — answering from a key table**

```cpp
#include <stdint.h>

#include <xpui_host.h>

// Stand-ins for the firmware's input manager, sampled once per frame.
static bool down_now[15];
static bool down_last_frame[15];

uint8_t xpui_host_was_pressed(uint8_t button) {
  if (button >= 15) return 0;  // out of range answers 0
  return down_now[button] && !down_last_frame[button];
}

uint8_t xpui_host_is_pressed(uint8_t button) { return button < 15 && down_now[button]; }

uint8_t xpui_host_was_released(uint8_t button) {
  if (button >= 15) return 0;
  return !down_now[button] && down_last_frame[button];
}
```

## xpui_host_is_pressed

Whether `button` is held.

```cpp
#include <xpui_host.h>

uint8_t xpui_host_is_pressed(uint8_t button);
```

A level rather than an edge, for as long as the key is down. Key auto-repeat is
timed from it against [`xpui_host_millis`](#xpui_host_millis).

## xpui_host_was_released

Whether `button` came up this frame.

```cpp
#include <xpui_host.h>

uint8_t xpui_host_was_released(uint8_t button);
```

The other edge of [`xpui_host_was_pressed`](#xpui_host_was_pressed), with the
same rules: one frame, not consumed by reading, 0 out of range.

## xpui_host_was_home_gesture

Whether the system-level "go home" gesture was made.

```cpp
#include <xpui_host.h>

uint8_t xpui_host_was_home_gesture(void);
```

The gesture is offered to the running screen first. A host applies its own
meaning only when the screen declines it, which `xpui_screen_home_gesture`
reports by returning 0.

## xpui_host_has_left_right_keys

Whether this device has a Left/Right pair.

```cpp
#include <xpui_host.h>

uint8_t xpui_host_has_left_right_keys(void);
```

**The host answers, because only the host knows.** One binary is built for a
keyboard, which has the pair, and for boards that do not, and the framework
branches on the answer. A device without the pair opens a value control on
Confirm, and takes over four keys while it is open. A constant compiled into the
Rust side would be wrong for half the devices that link it. `cpp_host`'s
`--no-pair` flag is how a headless run exercises the other half.

## xpui_host_millis

Milliseconds since the host started.

```cpp
#include <xpui_host.h>

uint32_t xpui_host_millis(void);
```

The only clock the framework has, and what key auto-repeat is timed against. It
may wrap: what reads it measures short intervals.

**Example — a steady clock**

```cpp
#include <chrono>
#include <cstdint>

#include <xpui_host.h>

uint32_t xpui_host_millis(void) {
  using namespace std::chrono;
  static const steady_clock::time_point start = steady_clock::now();
  return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now() - start).count());
}
```

## xpui_host_screen_title

The running screen's title.

```cpp
#include <xpui_host.h>

const uint8_t* xpui_host_screen_title(void);
```

Never null: a host with no screen bound answers with an empty string.
[Immortal](#strings), which a title handed over by
[`xpui_host_screen_present`](#xpui_host_screen_present) is not, so a host keeps
its own copy for as long as it might return it.

## xpui_host_screen_finish

Pops the running screen.

```cpp
#include <xpui_host.h>

void xpui_host_screen_finish(void);
```

> [!WARNING]
> It is called from inside that screen's own frame, so an implementation must
> **record** the request and act on it once the frame is over. Popping here
> frees the screen that is running.

## xpui_host_screen_present

Pushes a screen the Rust side built, with the title to show for it.

```cpp
#include <xpui_host.h>

uint8_t xpui_host_screen_present(void* screen, const uint8_t* title);
```

| Parameter | Meaning |
|---|---|
| `screen` | An opaque [handle](#the-screen-handle) from `xpui_fui::lifecycle::handle_for`. |
| `title` | The title to show for it, [borrowed for the call](#strings). |

Returns non-zero when the host **took ownership**. Zero means it did not, and
the caller reclaims the screen, so a host that declines must not have destroyed,
entered or kept the handle. A host with a depth limit declines this way.

The re-entrancy rule is [`xpui_host_screen_finish`](#xpui_host_screen_finish)'s:
record the push, then act on it after the frame.

**Example — recording requests until the frame is over**

```cpp
#include <stdint.h>

#include <string>
#include <vector>

#include <xpui_host.h>
#include <xpui_screen.h>

struct Pending {
  void* screen;
  std::string title;  // copied: `title` is borrowed for the call
};

static std::vector<Pending> g_pushes;
static bool g_pop = false;

uint8_t xpui_host_screen_present(void* screen, const uint8_t* title) {
  if (g_pushes.size() >= 4) return 0;  // declined: the caller reclaims `screen`
  g_pushes.push_back({screen, title ? reinterpret_cast<const char*>(title) : ""});
  return 1;  // taken: this host owns the handle now
}

void xpui_host_screen_finish(void) { g_pop = true; }

// Called by the host's loop between frames, never from inside one.
void apply_requests(std::vector<void*>& stack) {
  if (g_pop && !stack.empty()) {
    xpui_screen_on_exit(stack.back());
    xpui_screen_destroy(stack.back());
    stack.pop_back();
  }
  g_pop = false;
  for (const Pending& push : g_pushes) {
    xpui_screen_on_enter(push.screen);
    stack.push_back(push.screen);
  }
  g_pushes.clear();
}
```

## xpui_host_tr

Looks `key` up in the host's string table.

```cpp
#include <xpui_host.h>

const uint8_t* xpui_host_tr(const uint8_t* key);
```

| `key` is | Returns |
|---|---|
| in the table | its text, [immortal](#strings) |
| not in the table | `key` itself, so a typo shows on the panel rather than as a blank row |
| null | an empty string |

An unknown key comes back as the caller's own pointer, so pass an immortal key.
A `c"…"` literal on the Rust side is one.

**Example — a table built by hand**

```cpp
#include <stdint.h>
#include <string.h>

#include <xpui_host.h>

struct Entry {
  const char* key;
  const char* text;
};

static const Entry kTable[] = {
    {"STR_BRIGHTNESS", "Brightness"},
};

const uint8_t* xpui_host_tr(const uint8_t* key) {
  if (!key) return reinterpret_cast<const uint8_t*>("");
  for (const Entry& entry : kTable) {
    if (strcmp(entry.key, reinterpret_cast<const char*>(key)) == 0) {
      return reinterpret_cast<const uint8_t*>(entry.text);  // a literal: immortal
    }
  }
  return key;  // the caller's own pointer
}
```

## xpui_host_device_name

The device's name.

```cpp
#include <xpui_host.h>

const uint8_t* xpui_host_device_name(void);
```

[Immortal](#strings).

## xpui_host_firmware_version

The firmware's version.

```cpp
#include <xpui_host.h>

const uint8_t* xpui_host_firmware_version(void);
```

[Immortal](#strings).

## xpui_host_battery_percent

The battery's charge, from 0 to 100.

```cpp
#include <xpui_host.h>

int32_t xpui_host_battery_percent(void);
```

[Negative](#negative-figures) when this host has no battery to report, which is
not the same as a flat one and must not be reported as 0.

**Example — a host with no battery**

```cpp
#include <stdint.h>

#include <xpui_host.h>

const uint8_t* xpui_host_device_name(void) { return reinterpret_cast<const uint8_t*>("Desktop"); }

const uint8_t* xpui_host_firmware_version(void) { return reinterpret_cast<const uint8_t*>("0.1.0"); }

// Negative: there is no battery here. 0 would report a flat one.
int32_t xpui_host_battery_percent(void) { return -1; }
```

## xpui_host_heap_total

The heap's size, in bytes.

```cpp
#include <xpui_host.h>

int32_t xpui_host_heap_total(void);
```

On a firmware, Rust allocates from this same heap, and the four heap figures
cover both languages. On a desktop they cover the C++ side only, and the About
screen that shows them says so. Each is [negative](#negative-figures) when the
host cannot measure it.

**Example — a heap that can count but not see fragmentation**

```cpp
#include <stdint.h>

#include <xpui_host.h>

// Stand-ins for what the allocator counts.
static const int32_t kBudget = 8 * 1024 * 1024;
static int32_t g_live = 0;
static int32_t g_peak = 0;

int32_t xpui_host_heap_total(void) { return kBudget; }

int32_t xpui_host_heap_free(void) { return kBudget - g_live; }

int32_t xpui_host_heap_min_free(void) { return kBudget - g_peak; }

// Not measurable here. `free` in its place would pass off the wrong figure as
// the one that matters.
int32_t xpui_host_heap_largest_block(void) { return -1; }
```

## xpui_host_heap_free

The bytes free on the heap now.

```cpp
#include <xpui_host.h>

int32_t xpui_host_heap_free(void);
```

## xpui_host_heap_largest_block

The largest single block the heap could hand out now, in bytes.

```cpp
#include <xpui_host.h>

int32_t xpui_host_heap_largest_block(void);
```

The figure that usually cannot be measured, and the one that matters most.
Fragmentation is what a device with no MMU actually runs out of, and a host that
reported `free` in its place would turn that figure into decoration. On a device
with no MMU, the gap between this and
[`xpui_host_heap_free`](#xpui_host_heap_free) decides whether the next
allocation fails.

## xpui_host_heap_min_free

The fewest bytes the heap has ever had free.

```cpp
#include <xpui_host.h>

int32_t xpui_host_heap_min_free(void);
```

The low-water mark since the host started, which says how close the busiest
moment came to the ceiling.

## xpui_host_panic

Where a Rust panic lands on a device.

```cpp
#include <xpui_host.h>

void xpui_host_panic(const uint8_t* message);
```

| Parameter | Meaning |
|---|---|
| `message` | What panicked, [borrowed for the call](#strings), and never null. |

Only a firmware build reaches it. On a desktop the standard library has its own
handler. Nothing can be recovered, because the framework builds with
`panic = "abort"`, so say so somewhere a person will see; the Rust side spins
after this returns.

It is declared without `__attribute__((noreturn))`, because `abi/` reads
declarations rather than parsing C, and an attribute after the parameter list
defeats it.

**Example — saying why, somewhere a person looks**

```cpp
#include <stdint.h>
#include <stdio.h>

#include <xpui_host.h>

void xpui_host_panic(const uint8_t* message) {
  fprintf(stderr, "rust panic: %s\n", reinterpret_cast<const char*>(message));
}
```
