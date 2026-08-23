# `xpui-cpp-abi`

> ⚠️ **Under heavy development.** Not production-ready. The API can break
> without notice. Use at your own risk.

The two ABI boundaries this repository owns, checked on every run.

Five boundaries cross between Rust and C++ in this organisation. Three belong
to the backend and are checked in
[`xpui-backends`](https://github.com/XPUI-Framework/xpui-backends); these two
cross into the **application**, so they are checked here:

| Header | Defined by | Declared by | Who calls |
|---|---|---|---|
| [`cpp_host/cpp/xpui_host.h`](../cpp_host/cpp/xpui_host.h) | `cpp/host_*.cpp` | `cpp_host/src/raw.rs` | Rust calls C++ |
| [`cpp_host/cpp/xpui_app.h`](../cpp_host/cpp/xpui_app.h) | `cpp_host/src/lib.rs`, and what `register_screen!` generates | the header itself | C++ calls Rust |

The second is the awkward one. No Rust file spells those signatures out — the
only place they exist is the macro's own body, in another repository — so
`xpui-fui` exports its source as a `&'static str` and this crate parses that.
A git dependency lands in a cargo checkout directory with no path a sibling can
name, which is why a string and not a file.

## What it catches that a link does not

C has no mangling, so two swapped parameters resolve perfectly and corrupt the
call frame. `symbols_agree` in `../xtask/src/boundary.rs` compares which
symbols *exist*; this compares what they *are*. Neither subsumes the other, and
a symbol wrong in the second way is a rendering fault somewhere unrelated.

```bash
cargo test -p xpui-cpp-abi
```

## License

MIT — see [LICENSE](../LICENSE). Copyright (c) 2026 Thiago Holanda.
