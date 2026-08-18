# An ESP32 firmware, built by PlatformIO

`examples/cpp_host` proves the C ABI on a desktop, with CMake. This proves the
thing a firmware author actually has to do: get Rust compiled by PlatformIO,
linked into an ESP32 image, and running beside the C++ that was already there.

```bash
export FREEINK_SDK_DIR=/path/to/freeink-sdk   # once

pio run -e simulator_x3    # a window, on this machine
pio run -e default         # Xteink X3, ESP32-C3
pio run -e sticky          # Seeed Sticky, ESP32-S3
```

All three build. The two device environments produce flashable images —
265 kB for the C3, 280 kB for the S3 — and the simulator one produces the same
desktop binary CMake does, self-test and all.

**Homebrew's `pio` may lack the `littlefs` module** the espressif32 builder
wants. If a device build stops with `ModuleNotFoundError: No module named
'littlefs'`, use `~/.platformio/penv/bin/pio`.

## What porting to a device actually cost

This is the result worth reading, and it is the reason the example exists.

| | |
|---|---|
| Rust changed | **nothing** |
| C++ shared with the desktop host | `ScreenHost`, `ScreenStack`, `host_screen.cpp`, `host_i18n.cpp`, and the shim |
| C++ written for the device | `main.cpp`, and four `host_*.cpp` |

The screens, the `Platform`, the `Navigator`, every `xpui_host_*` declaration
and the whole of `xpui` come from `examples/cpp_host` **as a dependency, not a
copy** — `scripts/build_rust.py` builds that same package. What a laptop and a
device genuinely disagree about is input, the panel, device identity, the heap
and where a panic goes, and those are exactly the files under `cpp/`.

If porting had meant forking the screens, the boundary would be in the wrong
place. It did not, and that is the claim.

## One finding about the layering

The spec asked for a Rust crate here, wrapping `xpui-cpp-host` to add the two
things a device needs: a global allocator over the firmware's heap, and a panic
handler. **That does not build**, and finding out why is worth writing down.

Cargo produces every crate-type a package declares. `xpui-cpp-host` declares
`staticlib`, and a staticlib is a *final artifact* — so building it for a
bare-metal target requires an allocator and a panic handler in that package,
not in a downstream one. The wrapper failed before it was reached.

So the runtime lives in `examples/cpp_host/src/runtime.rs`, gated on
`target_os = "none"`, and this directory has no Rust at all. The result is
better than the wrapper would have been: a firmware links the same archive a
desktop does.

## The heap figures become true here

On the desktop host, `xpui_host_heap_*` covers the C++ side only — Rust has its
own allocator there, and the About screen says so. On a device the Rust
allocator routes through the firmware's `malloc`, so there is one heap and the
four figures cover both languages. That is the only visibility into what Rust
costs at run time: a build-time size report measures static sections, where it
contributes almost nothing.

`largest_block` also stops being `-1`. A desktop cannot measure fragmentation;
ESP-IDF can, and on a device with no MMU the gap between "free" and "largest
block" is the figure that actually decides whether the next allocation fails.

## There is no panel driver

Same as [`examples/esp32`](../esp32/), and for the same reason: no published
driver exists for these panels that this repository can test. `flush()` in
`main.cpp` counts the ink and logs it, which is where a driver goes.

## Kept in step by hand

`scripts/build_rust.py` is adapted from CrossPoint's, and `platformio.ini`'s
environments mirror its. **A change to either repository's FFI or layering
should be made in the other**, and nothing enforces it — see
[`examples/cpp_host/README.md`](../cpp_host/README.md) for the file-by-file
map.
