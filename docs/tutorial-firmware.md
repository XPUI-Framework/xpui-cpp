# Put the Rust screen on a board

Part three of three, continuing from [Run the Rust screen in your C++ host](tutorial-host.md),
where the screen was built and tested on a laptop. This page flashes the same
code to an ESP32, lists what a real firmware changes, measures what the screen
costs in memory, and ends on the one rule that keeps the boundary working.

## 11. Put it on a board

> **Nothing checks anything below this line, and nobody here has run it.** The
> gate builds and runs the desktop host; it never flashes a device, and it
> cannot. Neither ESP32 board in this organisation has ever been powered on —
> there is no panel driver for either, which is the whole reason. These are the
> ordinary [`esptool`](https://github.com/espressif/esptool) and [PlatformIO](https://platformio.org/) commands, transplanted from a firmware that
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
| [ESP32-C3](https://www.espressif.com/en/products/socs/esp32-c3) | [Xteink X3](https://www.xteink.com/products/xteink-x3) | `default` |
| [ESP32-S3](https://www.espressif.com/en/products/socs/esp32-s3) | [Seeed Sticky](https://www.seeedstudio.com/reTerminal-Sticky-p-6861.html) | `sticky` |

There is a third, `simulator_x3`, which builds the same code as a window on
this machine and flashes nothing.
[`firmware/README.md`](../firmware/README.md) records what each one produces —
and it is the page to trust on that, since this section is under a warning that
nothing here is checked.

Then build, flash, and watch it come up, from `firmware/`, where
`platformio.ini` is. [`firmware/README.md`](../firmware/README.md) has the
build commands, what the S3 needs installed first, and the one PlatformIO trap
worth knowing; flashing adds `-t upload` to them, and then a second command
reads the serial port at the console's 115200 baud:

```bash
cd firmware
pio run -e <environment> -t upload
pio device monitor -b 115200
```

`platformio.ini` runs `scripts/build_rust.py` before every compile, so `pio
run` builds the [Rust](https://rust-lang.org/) crates alongside the C++ and picks the Rust target from
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
crate, not a fork — and six C++ files:

| | |
|---|---|
| `main.cpp` | your firmware already has one |
| `host_input.cpp` | wire it to your input manager |
| `host_device.cpp` | your board configuration |
| `host_heap.cpp` | your RTOS |
| `host_panic.cpp` | your log |
| `sdk_out_of_line.cpp` | your build already compiles its SDK |

## What it costs in memory

Measure it, because the intuition is wrong in a specific and expensive way.

**Rust contributes almost nothing to static RAM, no matter how much it uses.**
A build-time size report measures `.data`, `.bss` and `.noinit`, and a Rust
screen allocates its strings and its widget tree through the heap — your heap,
once you give it a `#[global_allocator]` routing to your `malloc`, as
[`cpp_host/src/runtime.rs`](../cpp_host/src/runtime.rs) does. A report
crediting Rust with a few kilobytes of static RAM is telling you about its
statics and nothing about the screen.

So the figure that matters is the heap, over a screen's lifetime:

```rust
# #[derive(Copy, Clone)]
# struct Reading { free: i32, largest_block: i32 }
# // Two invented readings, from a host that can measure fragmentation,
# // either side of a screen that leaked.
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

---

That is the whole tutorial. [The reference](reference.md) describes every
symbol it used, and [`boundary.md`](boundary.md) says what proves the two
sides agree.
