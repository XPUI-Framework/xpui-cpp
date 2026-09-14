# Documentation

[`../README.md`](../README.md) is the front page. Every document in this
repository, and what each is for:

| | |
|---|---|
| [tutorial.md](tutorial.md) | **Start here.** Eleven steps: write a screen, get its words from your string table, export it, drive it, open it from the menu you already have, answer what the framework asks, teach the firmware a new symbol, start it, build it, test it with no device, and flash it |
| [reference.md](reference.md) | the index of the reference: the host library and the C ABI |
| [reference/host.md](reference/host.md) | `xpui_app_install` and `xpui_app_create_menu`, checked against rustdoc; its Rust and C++ fences are compiled |
| [reference/abi.md](reference/abi.md) | the four headers and who defines each, every function `xpui_app.h` and `xpui_host.h` declare, and the rules for strings, handles, buttons and figures; written by hand, its C++ compiled |
| [boundary.md](boundary.md) | the four symbol sets as one picture, the double-boxed handle, the firmware this host was modelled on and the three deliberate differences, what the nine `ctest` cases prove, and what porting to a device cost |
| [contributing.md](contributing.md) | the requirements, the gate in both modes, the four-place rule for a C symbol, the five review steps, and how a commit is written |
