# The C++ host: FreeInkUI, the xpui shim, and a Rust staticlib.
#
#   cmake -S examples/cpp_host -B target/cpp_host -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build target/cpp_host
#   ctest --test-dir target/cpp_host --output-on-failure
#
# The cargo call below is a plain `add_custom_target`, not corrosion, and that
# is deliberate. This example exists to be READ by someone wiring Rust into a
# firmware build: forty lines of visible CMake transplant into a PlatformIO
# extra-script, and a four-thousand-line dependency that hides the mechanism
# does not.

cmake_minimum_required(VERSION 3.20)
project(xpui_cpp_host VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(XPUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(XPUI_FUI_DIR "${XPUI_ROOT}/crates/backend/fui")

# -- FreeInkUI ----------------------------------------------------------------
#
# Pinned to a revision rather than tracking `main`: the SDK has no tags, and an
# unpinned clone means an upstream commit can break this build with no change
# here to point at.
#
# `-D FREEINK_SDK_DIR=<path>` short-circuits the download, which is what a
# developer with the firmware checked out next door wants, and what the gate
# uses so it needs no network.

# One file, read by this and by CI, because a revision written down twice is a
# revision that will disagree with itself.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/freeink-sdk.rev" FREEINK_SDK_REVISION)
string(STRIP "${FREEINK_SDK_REVISION}" FREEINK_SDK_REVISION)

if(NOT FREEINK_SDK_DIR)
  include(FetchContent)
  FetchContent_Declare(
    freeink_sdk
    GIT_REPOSITORY https://github.com/Free-Ink/freeink-sdk.git
    GIT_TAG ${FREEINK_SDK_REVISION}
  )
  # Populate only. The SDK ships no CMakeLists.txt, so nothing is added as a
  # subproject and the two files below are picked up by hand.
  FetchContent_MakeAvailable(freeink_sdk)
  set(FREEINK_SDK_DIR "${freeink_sdk_SOURCE_DIR}")
endif()

set(FREEINK_UI_DIR "${FREEINK_SDK_DIR}/libs/ui/FreeInkUI")
if(NOT EXISTS "${FREEINK_UI_DIR}/include/FreeInkUI.h")
  message(FATAL_ERROR "FreeInkUI not found under ${FREEINK_SDK_DIR}. Pass -D FREEINK_SDK_DIR=<sdk>.")
endif()

# Header-only except for these: themeTokensForLineHeight and the default style
# tables are out of line, so a build without this source link-errors on names
# that look like constants.
add_library(freeink_ui STATIC "${FREEINK_UI_DIR}/src/FreeInkUI.cpp")
target_include_directories(freeink_ui PUBLIC "${FREEINK_UI_DIR}/include")

# -- the Rust staticlib -------------------------------------------------------
#
# `--target <triple>` is passed explicitly, or cargo writes to `target/<profile>`
# and this path is wrong. Note the trap: the `dev` profile's directory is
# `debug`, not `dev`.

execute_process(
  COMMAND ${CMAKE_COMMAND} -E env rustc -vV
  OUTPUT_VARIABLE RUSTC_VERSION_VERBOSE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)
string(REGEX MATCH "host: ([^\n]*)" _ "${RUSTC_VERSION_VERBOSE}")
set(RUST_TARGET "${CMAKE_MATCH_1}")

if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
  set(CARGO_FLAGS --release)
  set(CARGO_DIR release)
else()
  set(CARGO_FLAGS "")
  set(CARGO_DIR debug)
endif()

# The workspace's own target/, shared with `cargo test`, so the two can never
# disagree about what was built.
set(RUST_STATICLIB "${XPUI_ROOT}/target/${RUST_TARGET}/${CARGO_DIR}/libxpui_cpp_host.a")

# Run unconditionally and let cargo decide whether anything changed — it is the
# incremental build system here, and teaching CMake the dependency graph of a
# Rust workspace would be a second, wrong copy of it.
add_custom_target(
  xpui_rust_staticlib ALL
  COMMAND cargo build -p xpui-cpp-host --target ${RUST_TARGET} ${CARGO_FLAGS}
  BYPRODUCTS "${RUST_STATICLIB}"
  WORKING_DIRECTORY "${XPUI_ROOT}"
  COMMENT "cargo build -p xpui-cpp-host --target ${RUST_TARGET} ${CARGO_FLAGS}"
  VERBATIM
)

# -- SDL2 ---------------------------------------------------------------------

find_package(SDL2 REQUIRED)

# -- the host -----------------------------------------------------------------

add_executable(xpui-host
  cpp/main.cpp
  cpp/Display.cpp
  cpp/Input.cpp
  cpp/ScreenHost.cpp
  cpp/ScreenStack.cpp
  cpp/host_device.cpp
  cpp/host_heap.cpp
  cpp/host_i18n.cpp
  cpp/host_input.cpp
  cpp/host_screen.cpp
  # The shim itself, from the backend crate. Compiled, linked and run here for
  # the first time — everywhere else it is only ever syntax-checked.
  "${XPUI_FUI_DIR}/cpp/xpui_fui.cpp"
)

target_include_directories(xpui-host PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/cpp"
  "${XPUI_FUI_DIR}/cpp"
)

# The quotes are escaped so the value reaches the compiler as a string literal.
# Written unescaped, CMake reads `NAME="Desktop (Darwin)"` as a function-style
# macro and silently drops it — the About screen would then show the built-in
# fallback and nothing would say why.
target_compile_definitions(xpui-host PRIVATE
  "XPUI_HOST_VERSION=\"${PROJECT_VERSION}\""
  "XPUI_HOST_DEVICE_NAME=\"Desktop ${CMAKE_SYSTEM_NAME}\""
)

target_compile_options(xpui-host PRIVATE -Wall -Wextra)

add_dependencies(xpui-host xpui_rust_staticlib)

target_link_libraries(xpui-host PRIVATE freeink_ui SDL2::SDL2 "${RUST_STATICLIB}")

if(APPLE)
  # A Rust std staticlib needs no frameworks of its own on Darwin beyond what
  # libSystem already gives it; SDL2 brings Cocoa itself.
  target_link_libraries(xpui-host PRIVATE "-framework CoreFoundation")
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(xpui-host PRIVATE pthread dl m)
endif()

# -- tests --------------------------------------------------------------------
#
# Headless throughout: `--headless` never calls SDL_Init(SDL_INIT_VIDEO), so
# these need no display and no dummy driver.

enable_testing()

# **Nothing here uses PASS_REGULAR_EXPRESSION.** ctest ignores a test's exit
# code entirely once one is set, so a run whose `--selftest` failed still
# passes. Every assertion is a flag the binary turns into an exit code.

# The gate that matters. Fails if nothing asked for a frame, if nothing was
# blitted, or if the framebuffer came out one flat colour.
add_test(NAME selftest COMMAND xpui-host --headless --frames 30 --selftest
                               --out "${CMAKE_CURRENT_BINARY_DIR}/frame.bmp")

# The same run, pushed through the weak `xpui_fui_present` instead of the hook.
# Whether an override beats a weak definition is the linker's business and
# differs by object format; this is the only thing that proves it here.
add_test(NAME selftest_weak_present COMMAND xpui-host --headless --frames 30 --selftest --weak-present)

# Confirm on the first row asks the framework to present a screen, which leaves
# Rust as an opaque handle and comes back as a second entry on the C++ stack.
# Without this the stack — and the globals it makes load-bearing — are untested.
add_test(NAME navigates_into_rust
         COMMAND xpui-host --headless --frames 12 --keys confirm --selftest
                 --expect-depth 2 --expect-max-depth 2)

# And back out again: Back on the pushed screen pops it, leaving the root. Both
# halves are asserted — a stack that never pushed and one that never popped are
# each half right, and a depth of 1 alone cannot tell them apart.
add_test(NAME back_pops_to_the_root
         COMMAND xpui-host --headless --frames 12 --keys confirm,back --selftest
                 --expect-depth 1 --expect-max-depth 2)

# Opens the refresh-mode dialog, which is the only thing here that reaches
# `xpui_fui_scrim`, `xpui_fui_draw_option_popup` and
# `xpui_fui_option_popup_row_rect`. An overlay is the path that rots quietly:
# nothing else exercises it, and a host missing the scrim went unnoticed in the
# sibling firmware until a test finally opened one.
#
# The keys walk to the Refresh-mode row: the focus order puts the slider first,
# so `down,confirm,confirm` opens nothing at all and leaves a frame identical to
# the run without the third key.
#
# `--expect-ink` is what makes this a test rather than a click. A scrim is ink
# on one checkerboard parity of everything behind the dialog, so the panel goes
# from a few percent ink to about half — the assertion is on that relationship.
#
# **Bounded on both sides, and the upper bound is not padding.** A dialog whose
# body never paints leaves the scrim covering the area the popup's own panel
# would have cleared, so the figure goes *up*, to 51%. A one-sided assertion
# gets greener as the dialog disappears. 30..48 fails that, and fails every
# off-by-one in the key sequence below (2%, 11%, 11%).
#
# Measured at the default 480x800. Changing that default means re-measuring.
add_test(NAME draws_an_overlay
         COMMAND xpui-host --headless --frames 20 --keys down,confirm,down,down,confirm --selftest
                 --expect-depth 2 --expect-ink 30 --expect-ink-at-most 48)

# The control for that band: the same screen with no dialog on it. Without this
# pair, "at least 30% ink" is a number rather than a relationship — and the two
# together fail if the overlay ever stops darkening what is behind it.
#
# It is also the only thing in the suite that catches an inverted panel: flip
# the framebuffer's polarity and every other case still passes, while this one
# sees 97%.
add_test(NAME a_plain_screen_is_mostly_paper
         COMMAND xpui-host --headless --frames 20 --keys down,confirm --selftest
                 --expect-depth 2 --expect-ink-at-most 20)
