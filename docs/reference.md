# Reference

Everything `xpui-cpp` exposes, in two halves: the [Rust](https://rust-lang.org/) library a C++ application
links, and the C functions that cross between the two languages.
[The tutorial](tutorial.md) walks both a step at a time.

## Topics

| Page | Holds |
|---|---|
| [The host library](reference/host.md) | `xpui_app_install` and `xpui_app_create_menu`, the public functions of `xpui-cpp-host`, checked against its rustdoc |
| [The C ABI](reference/abi.md) | the four headers and who defines each, every function `xpui_app.h` and `xpui_host.h` declare, and the rules for strings, handles, buttons and figures; written by hand, with every C++ fence compiled |

The host library's `screens` module is the worked example, meant to be read as
source: [`cpp_host/src/screens/`](../cpp_host/src/screens/).
