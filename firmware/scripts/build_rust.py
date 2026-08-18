"""Build the Rust static library and link it into the firmware.

PlatformIO runs this as a `pre:` script. It compiles `xpui-cpp-host` for the
target matching the current environment and appends the archive to the link
line.

**It is the same package the desktop C++ host links** — `xpui-cpp-host`, not
a firmware fork of it. That archive statically contains `xpui` and `xpui-fui`,
so one library is built and one is linked, and porting the screens to a device
is not a line of Rust.

Targets, keyed off the environment's MCU:

    <host>      simulator envs, std, the pinned stable toolchain
    esp32c3     riscv32imc-unknown-none-elf, no_std, stable
    esp32s3     xtensa-esp32s3-none-elf, no_std, the `esp` fork, build-std

Xtensa is tier 3 and ships no prebuilt core/alloc, so it is built from source
with `-Z build-std`. That needs the esp-rs toolchain (`espup install`) and its
`rust-src` component.

Adapted from CrossPoint's `scripts/build_rust.py`. **An FFI or layout change
in either repository should be mirrored in the other**, and nothing enforces
that — see `examples/cpp_host/README.md` for the file-by-file map.
"""

import os
import platform
import subprocess
import sys

Import("env")  # noqa: F821 - injected by PlatformIO's SConscript runner

PACKAGE = "xpui-cpp-host"
STATICLIB = "xpui_cpp_host"
BUILD_TIMEOUT_SECONDS = 900


def workspace_root(project_dir):
    """The cargo workspace this example is a member of.

    `examples/firmware` -> the repository root. Derived rather than assumed, so
    moving the example is one edit here and not a silent wrong path.
    """
    root = os.path.abspath(os.path.join(project_dir, "..", ".."))
    if not os.path.exists(os.path.join(root, "Cargo.toml")):
        raise RuntimeError("cargo workspace manifest not found at {}".format(root))
    return root


def host_triple():
    """Rust triple for the machine running the build (simulator envs)."""
    machine = platform.machine()
    system = platform.system()

    if system == "Darwin":
        return "aarch64-apple-darwin" if machine == "arm64" else "x86_64-apple-darwin"
    if system == "Linux":
        return "aarch64-unknown-linux-gnu" if machine == "aarch64" else "x86_64-unknown-linux-gnu"
    if system == "Windows":
        return "x86_64-pc-windows-msvc"

    raise RuntimeError("Unsupported host for the Rust build: {} {}".format(system, machine))


def target_for_env(env):
    """(triple, toolchain, build_std) for the active PlatformIO environment.

    `toolchain` is None for the pinned default; `build_std` is True when
    core/alloc have to be compiled from source.
    """
    if "native" in env.get("PIOPLATFORM", ""):
        return host_triple(), None, False

    mcu = env.BoardConfig().get("build.mcu", "")

    if mcu == "esp32s3":
        return "xtensa-esp32s3-none-elf", "esp", True
    if mcu == "esp32c3":
        return "riscv32imc-unknown-none-elf", None, False

    raise RuntimeError(
        "No Rust target mapped for MCU '{}'. Add it to scripts/build_rust.py "
        "before building this environment.".format(mcu)
    )


def setup_help(toolchain):
    """What to actually type when the toolchain is missing.

    Worth the lines: this runs for every environment, so somebody cloning the
    firmware without Rust hits it on their first `pio run`. A bare Python
    traceback there reads as "the project is broken" rather than "install this".
    """
    steps = [
        "",
        "This firmware links a Rust static library, so the build needs a Rust",
        "toolchain. To install it:",
        "",
        "    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh",
        "",
    ]
    if toolchain == "esp":
        steps += [
            "The ESP32-S3 is Xtensa, which no stable Rust targets, so it also",
            "needs the esp-rs fork:",
            "",
            "    cargo install espup --locked && espup install",
            "",
            "espup ships the Xtensa linker inside the toolchain without putting",
            "it on PATH. Source ~/export-esp.sh, or add:",
            "",
            "    $RUSTUP_HOME/toolchains/esp/xtensa-esp-elf/*/xtensa-esp-elf/bin",
            "",
        ]
    else:
        steps += [
            "rust-toolchain.toml pins the version and the targets, so cargo",
            "installs both on the first build. Nothing else to do.",
            "",
        ]
    steps += ["See examples/firmware/README.md for how the Rust build fits in.", ""]
    return "\n".join(steps)


def build(root, triple, toolchain, build_std):
    """Run cargo and return the directory holding the archive."""
    command = ["cargo"]
    if toolchain:
        command.append("+" + toolchain)
    command += ["build", "--release", "--package", PACKAGE, "--target", triple]
    if build_std:
        command += ["-Z", "build-std=core,alloc"]

    print("[rust] {}".format(" ".join(command)))

    try:
        result = subprocess.run(
            command,
            cwd=root,
            timeout=BUILD_TIMEOUT_SECONDS,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError:
        # cargo is not on PATH at all.
        sys.stderr.write(setup_help(toolchain))
        raise RuntimeError("cargo was not found on PATH")

    if result.returncode != 0:
        # cargo writes diagnostics to stderr; surface them or the failure is
        # opaque.
        sys.stderr.write(result.stderr)
        # cargo is present but the toolchain it was asked for is not, which is
        # a setup problem wearing a compiler error's clothes.
        if toolchain and "not installed" in result.stderr:
            sys.stderr.write(setup_help(toolchain))
        raise RuntimeError("cargo build failed for target {}".format(triple))

    # Warnings still matter on success.
    if result.stderr.strip():
        print(result.stderr.strip())

    # **`--target` is passed even for the host**, or cargo writes to
    # `target/release/` and this path is wrong. Note the trap that goes with
    # it: the `dev` profile's directory is `debug`, not `dev`.
    return os.path.join(root, "target", triple, "release")


def main(env):
    project_dir = env.subst("$PROJECT_DIR")
    root = workspace_root(project_dir)

    triple, toolchain, build_std = target_for_env(env)
    artifact_dir = build(root, triple, toolchain, build_std)

    archive = os.path.join(artifact_dir, "lib{}.a".format(STATICLIB))
    if not os.path.exists(archive):
        raise RuntimeError("Expected Rust archive not produced: {}".format(archive))

    env.Append(LIBPATH=[artifact_dir], LIBS=[STATICLIB])

    # Relink whenever the archive changes, or SCons caches a stale binary and
    # a Rust edit appears to do nothing.
    env.Depends("$BUILD_DIR/${PROGNAME}$PROGSUFFIX", archive)

    print("[rust] linking {} ({})".format(archive, triple))


main(env)  # noqa: F821
