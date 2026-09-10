# An ESP32 firmware, built by PlatformIO

> ⚠️ **Under heavy development.** Not production-ready. The API can break
> without notice. Use at your own risk.

`cpp_host` proves the C ABI on a desktop, with CMake. This proves the thing a
firmware author actually has to do: get Rust compiled by PlatformIO, linked
into an ESP32 image, and running beside the C++ that was already there. The
port changed no Rust at all; what a laptop and a device disagree about is the
six files under `cpp/` — `main.cpp`, four `host_*.cpp`, and one translation
unit that gives the header-only SDK a home.

## Using it

```bash
export FREEINK_SDK_DIR=/path/to/freeink-sdk   # once

pio run -e simulator_x3    # a window, on this machine
pio run -e default         # Xteink X3, ESP32-C3
pio run -e sticky          # Seeed Sticky, ESP32-S3
```

All three build. The two device environments produce flashable images —
265 kB for the C3, 280 kB for the S3 — and the simulator one produces the same
desktop binary CMake does, self-test and all. There is no panel driver:
`flush()` in `cpp/main.cpp` counts the ink and logs it, which is where one
goes.

**Homebrew's `pio` may lack the `littlefs` module** the espressif32 builder
wants. If a device build stops with `ModuleNotFoundError: No module named
'littlefs'`, use `~/.platformio/penv/bin/pio`.

## Checking it

No gate invokes this directory: `./build-and-test.sh` builds the desktop host
through CMake, so a break here surfaces when somebody runs `pio run`. Build
all three environments before pushing a change to `cpp_host` or to the
boundary.

## Where next

| | |
|---|---|
| [`docs/boundary.md`](../docs/boundary.md) | what porting to a device cost, why the runtime lives in `cpp_host`, the heap figures that become true on a device, the panel-driver seam, and what is kept in step with CrossPoint by hand |

## License

MIT — see [LICENSE](../LICENSE). Copyright (c) 2026 Thiago Holanda.
