#!/usr/bin/env bash

# Everything CI checks in this repository, in one command.
#
#   ./build-and-test.sh          format, lint, test, snippets, and the C++
#   ./build-and-test.sh check    the same thing; the name CI uses
#   ./build-and-test.sh all      the above, plus cmake, the link, and ctest
#   ./build-and-test.sh fix      format Rust and C++ in place first
#
# **Half of what runs is in `bin/gate-common.sh`**, of which every repository
# in the organisation carries a byte-identical copy. This file is what this
# repository configures, what only it checks, and the order they run in.
# `xpui-dev` compares the nine copies and runs all nine gates.
#
# **This is where the C ABI is settled**, because `cpp_host` is the only thing
# a gate anywhere builds, links and *runs* the FreeInkUI shim through. Design
# an ABI change here. `firmware/` compiles the same shim through PlatformIO,
# which nothing below invokes — so a break there surfaces when somebody builds
# a firmware, not when this is run.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${PROJECT_DIR}"

# `firmware/` holds no Rust of its own: it links the staticlib `cpp_host`
# builds, so its sources are C++ and its manifest is a platformio.ini.
SOURCE_ROOTS=(abi cpp_host)

# The Rust half is an ordinary host library and its tests need no feature.
TEST_FEATURES=""

# Host only. What runs on a device here is C++, compiled by PlatformIO.
HOST_WORKSPACE=1

. bin/gate-common.sh

# ---------------------------------------------------------------------------
# What only this repository checks.
# ---------------------------------------------------------------------------

# Where the FreeInkUI headers are, or nothing. Neither candidate is a submodule
# and neither is required: a missing SDK skips the C++ stages with a note
# rather than failing a gate somebody cannot fix without a download.
# `FREEINK_SDK_INCLUDE` overrides both, and CMake falls back to fetching the
# revision `cpp_host/freeink-sdk.rev` pins.
freeink_include() {
  if [[ -n "${FREEINK_SDK_INCLUDE:-}" ]]; then
    printf '%s' "${FREEINK_SDK_INCLUDE}"
    return
  fi
  for candidate in \
    "../../Freeink/freeink-sdk/libs/ui/FreeInkUI/include" \
    "../crosspoint-reader/freeink-sdk/libs/ui/FreeInkUI/include"; do
    if [[ -d "${candidate}" ]]; then
      printf '%s' "${candidate}"
      return
    fi
  done
}

# The backend's shim, as a sibling checkout.
#
# Not a path inside the `xpui-fui` package: `include = [...]` ships `cpp/**` in
# a *published* package and nothing here is published, so a git dependency
# lands in a cargo checkout directory with no path a CMakeLists.txt can name.
# The sibling is the same arrangement the SDK already has.
backends_cpp() {
  printf '%s' "${XPUI_BACKENDS_DIR:-../xpui-backends}/fui/cpp"
}

# Where a documented C++ snippet's headers are, or the reason there are none.
# Both roots, because a snippet here may include either side of the boundary.
cpp_snippet_includes() {
  local sdk shim
  sdk="$(freeink_include)"
  shim="$(backends_cpp)"
  if [[ -z "${sdk}" || ! -d "${sdk}" ]]; then
    printf 'FreeInkUI headers not found. Set FREEINK_SDK_INCLUDE to run it.'
    return 1
  fi
  if [[ ! -d "${shim}" ]]; then
    printf 'the backends repository is not beside this one. Set XPUI_BACKENDS_DIR.'
    return 1
  fi
  printf -- '-I %s -I %s -I cpp_host/cpp' "${sdk}" "${shim}"
}

cpp_compiles() {
  say "The C++ host compiles"
  local sdk shim
  sdk="$(freeink_include)"
  shim="$(backends_cpp)"
  if [[ -z "${sdk}" || ! -d "${sdk}" ]]; then
    echo "    skipped: FreeInkUI headers not found."
    echo "    Set FREEINK_SDK_INCLUDE to <sdk>/libs/ui/FreeInkUI/include to run it."
    return 0
  fi
  if [[ ! -d "${shim}" ]]; then
    echo "    skipped: the backends repository is not checked out beside this one."
    echo "    Clone github.com/XPUI-Framework/xpui-backends, or set XPUI_BACKENDS_DIR."
    return 0
  fi
  local sdl
  sdl="$(sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2 2>/dev/null || true)"
  if [[ -z "${sdl}" ]]; then
    echo "    skipped: SDL2 headers not found (sdl2-config, pkg-config sdl2)."
    return 0
  fi
  # Exceptions stay on: this is ordinary desktop C++ and its replacement
  # operator new throws, which is what the standard says it must do.
  # shellcheck disable=SC2086
  clang++ -std=c++17 -fsyntax-only -Wall -Wextra \
    -I "${sdk}" -I "${shim}" -I cpp_host/cpp ${sdl} \
    -DXPUI_HOST_VERSION='"gate"' -DXPUI_HOST_DEVICE_NAME='"gate"' \
    cpp_host/cpp/*.cpp
  echo "    clean"
}

# The symbols a pattern finds, out of the files given.
ffi_symbols() {
  local pattern="$1"
  shift
  grep -hoE "${pattern}" "$@" | sort -u
}

# Two symbol lists, and what each side is called when they disagree.
same_symbols() {
  local what="$1" left_name="$2" right_name="$3" left="$4" right="$5"
  local only_left only_right
  only_left="$(comm -23 <(printf '%s\n' "${left}") <(printf '%s\n' "${right}"))"
  only_right="$(comm -13 <(printf '%s\n' "${left}") <(printf '%s\n' "${right}"))"
  if [[ -z "${only_left}" && -z "${only_right}" ]]; then
    return 0
  fi
  echo "  ${what}:" >&2
  [[ -n "${only_left}" ]] && printf '    only in %s: %s\n' "${left_name}" "$(echo ${only_left})" >&2
  [[ -n "${only_right}" ]] && printf '    only in %s: %s\n' "${right_name}" "$(echo ${only_right})" >&2
  return 1
}

# The two boundaries this repository owns: one header, answered twice.
#
# `abi/tests/abi.rs` compares *signatures* — two swapped parameters link fine,
# because C has no mangling to disagree with, and the result is a corrupt call
# frame. This compares *presence*, which that cannot read: a symbol declared in
# a header with nothing defining it is a link error waiting for whoever
# includes it.
ffi_symbols_agree() {
  say "Every C header's symbols are defined by the C++ that answers them"
  local failures=0
  local host="xpui_host_[a-z0-9_]+"
  local declared
  declared="$(ffi_symbols "${host}" cpp_host/cpp/xpui_host.h)"

  same_symbols "xpui_host.h and the desktop host" xpui_host.h cpp_host \
    "${declared}" \
    "$(ffi_symbols "${host}" cpp_host/cpp/host_*.cpp)" || failures=$((failures + 1))

  # The firmware answers the same header with different files: four of its own,
  # plus the ones it compiles straight out of the desktop host because nothing
  # in them is platform-specific.
  #
  # **That second set is read out of `platformio.ini` rather than restated
  # here.** Two lists of the same files drift, so the gate parses the
  # `+<../../cpp_host/cpp/…>` lines of the device `build_src_filter` and checks
  # exactly what PlatformIO compiles.
  #
  # Worth checking at all because `pio run` needs an ESP-IDF toolchain, so no
  # gate ever reaches the link: a firmware missing one symbol would fail on
  # somebody's board rather than here.
  local shared_with_firmware
  shared_with_firmware="$(sed -n '/^build_src_filter =/,/^$/p' firmware/platformio.ini \
    | grep -oE '\+<\.\./\.\./cpp_host/cpp/[a-zA-Z0-9_]+\.cpp>' \
    | sed -e 's|^+<\.\./\.\./|| ' -e 's|>$||' || true)"

  # An empty list would mean the parse broke and the check below would compare
  # against the firmware's own four files alone — passing for the wrong reason.
  if [[ -z "${shared_with_firmware}" ]]; then
    echo "ERROR: could not read build_src_filter out of firmware/platformio.ini." >&2
    echo "       That list is what this check compares against; it cannot be empty." >&2
    return 1
  fi

  # shellcheck disable=SC2086
  same_symbols "xpui_host.h and the firmware" xpui_host.h firmware \
    "${declared}" \
    "$(ffi_symbols "${host}" firmware/cpp/host_*.cpp ${shared_with_firmware})" \
    || failures=$((failures + 1))

  if [[ "${failures}" -gt 0 ]]; then
    echo "ERROR: ${failures} pair(s) above disagree. A symbol in a header with no" >&2
    echo "       definition is a link error waiting for whoever includes it." >&2
    return 1
  fi
  echo "    agreed"
}

# The one thing in this organisation that links Rust into C++ and runs it.
cpp_host_runs() {
  say "The C++ host builds, links and passes its own self-test"
  if ! command -v cmake >/dev/null 2>&1; then
    echo "    skipped: cmake not installed."
    return 0
  fi

  # `if` rather than `cmd && args+=(...)`: an AND-OR list that fails is the
  # function's exit status when it lands last, and `set -e` then takes the
  # whole gate down for want of Ninja.
  local args=(-S cpp_host -B target/cpp_host -DCMAKE_BUILD_TYPE=Release)
  if command -v ninja >/dev/null 2>&1; then
    args+=(-G Ninja)
  fi

  # Hand over the SDK when this machine has one, so the build needs no network.
  # Without it CMake fetches the revision cpp_host/freeink-sdk.rev pins.
  local sdk root
  sdk="$(freeink_include)"
  if [[ -n "${sdk}" ]]; then
    root="$(cd "${sdk}/../../../.." 2>/dev/null && pwd || true)"
    # Only when the include directory really sits where the SDK's layout says.
    # A FREEINK_SDK_INCLUDE pointing elsewhere is still fine — CMake then
    # fetches the pinned revision instead of being handed a wrong root.
    if [[ -n "${root}" && -d "${root}/libs/ui/FreeInkUI/include" ]]; then
      args+=(-D "FREEINK_SDK_DIR=${root}")
    fi
  fi

  cmake "${args[@]}" >/dev/null
  cmake --build target/cpp_host
  ctest --test-dir target/cpp_host --output-on-failure
}

# ---------------------------------------------------------------------------

gates() {
  file_sizes
  every_check_runs
  readmes_warn
  prose_is_compiled
  doc_paths
  commands_resolve
  ffi_symbols_agree
  cpp_snippets_compile
  lint
  test_suite
  doc_tests
  doc_links
  cpp_compiles
}

case "${1:-check}" in
  check)
    run_all "${FORMAT_CHECK[@]}"
    gates
    printf '\nChecks passed. "./build-and-test.sh all" also links and runs it.\n'
    ;;
  fix)
    run_all "${FORMAT_FIX[@]}"
    gates
    printf '\nFormatted and checked.\n'
    ;;
  all)
    run_all "${FORMAT_CHECK[@]}"
    gates
    cpp_host_runs
    printf '\nEverything passed.\n'
    ;;
  *)
    echo "usage: ./build-and-test.sh [check|fix|all]" >&2
    exit 2
    ;;
esac
