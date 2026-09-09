// FreeInkUI's out-of-line definitions, compiled from inside this project.
//
// The SDK is header-only except for `FreeInkUI.cpp`, and a build without it
// link-errors on names that look like constants. Included from here rather
// than named in `build_src_filter`: that takes an absolute path when the SDK
// is a sibling checkout, and PlatformIO then writes the object file into a
// mirror of that path inside the project directory. One `#include` keeps
// every output under `.pio/`.
//
// **Not named `freeinkui.cpp`.** macOS's filesystem is case-insensitive, so a
// file by that name here is what `#include <FreeInkUI.cpp>` finds — the file
// including itself, reported as "#include nested too deeply".

#include <FreeInkUI.cpp>  // NOLINT(bugprone-suspicious-include)
