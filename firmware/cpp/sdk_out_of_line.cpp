// FreeInkUI's out-of-line definitions, compiled from inside this project.
//
// The SDK is header-only except for one source: `themeTokensForLineHeight`,
// `defaultButtonStyles`, `defaultListRowStyles`, `defaultPopupStyles`,
// `plainStyles` and `defaultThemeTokens` all live in `FreeInkUI.cpp`, and a
// build without it link-errors on names that look like constants.
//
// Included from here rather than named in `build_src_filter`, because that
// takes an absolute path when the SDK is a sibling checkout — and PlatformIO
// then writes the object file into a mirror of that absolute path **inside
// the project directory**, leaving a `crosspoint-reader/` tree beside the
// source. One `#include` keeps every output under `.pio/`.
//
// **Not named `freeinkui.cpp`.** macOS's filesystem is case-insensitive, so a
// file by that name in this directory is what `#include <FreeInkUI.cpp>`
// finds — the file including itself, reported as "#include nested too
// deeply", which names the symptom and not the cause.

#include <FreeInkUI.cpp>  // NOLINT(bugprone-suspicious-include)
