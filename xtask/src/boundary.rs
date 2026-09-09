//! The C ABI this repository owns.
//!
//! One header, `cpp_host/cpp/xpui_host.h`, answered by a desktop host and by a
//! firmware — so `symbols_agree` compares it against both. `host_compiles`
//! syntax-checks the desktop half, `snippet_includes` points the documented
//! C++ at the same headers, and `host_runs` — behind `all` — is the only
//! thing in the organisation that compiles, links and *runs* the boundary
//! rather than reading it.

use std::path::PathBuf;

use crate::cpp;

/// The backend's shim, as a sibling checkout.
///
/// Not a path inside the `xpui-fui` package: nothing here is published, so a
/// git dependency lands in a cargo checkout directory with no path a
/// `CMakeLists.txt` can name. The sibling is the arrangement the SDK has too.
pub fn backends_cpp() -> PathBuf {
    PathBuf::from(std::env::var("XPUI_BACKENDS_DIR").unwrap_or("../xpui-backends".into()))
        .join("fui/cpp")
}

/// Both roots, because a snippet here may include either side of the boundary.
pub fn snippet_includes() -> Result<Vec<String>, String> {
    let sdk = cpp::freeink_include()
        .ok_or("FreeInkUI headers not found. Set FREEINK_SDK_INCLUDE to run it.")?;
    let shim = backends_cpp();
    if !shim.is_dir() {
        return Err(
            "the backends repository is not beside this one. Set XPUI_BACKENDS_DIR.".into(),
        );
    }
    Ok(vec![
        format!("-I{}", sdk.display()),
        format!("-I{}", shim.display()),
        "-Icpp_host/cpp".to_string(),
    ])
}

/// The two boundaries this repository owns: one header, answered twice.
///
/// `abi/tests/abi.rs` compares *signatures* — two swapped parameters link
/// fine, because C has no mangling to disagree with, and the result is a
/// corrupt call frame. This compares *presence*, which that cannot read.
pub fn symbols_agree() -> Result<String, String> {
    let declared = cpp::symbols("xpui_host_", &[PathBuf::from("cpp_host/cpp/xpui_host.h")])?;
    if declared.is_empty() {
        return Err(
            "no xpui_host_ symbols in cpp_host/cpp/xpui_host.h — nothing was compared".into(),
        );
    }

    let desktop = cpp::symbols("xpui_host_", &glob("cpp_host/cpp", "host_", ".cpp"))?;
    let mut failures = Vec::new();
    if let Err(why) = cpp::symbols_agree(
        "xpui_host.h and the desktop host",
        "xpui_host.h",
        &declared,
        "cpp_host",
        &desktop,
    ) {
        failures.push(why);
    }

    // The firmware answers the same header with different files: four of its
    // own, plus the ones it compiles straight out of the desktop host because
    // nothing in them is platform-specific. That second set is read out of
    // `platformio.ini` rather than restated here, because two lists of the
    // same files drift.
    //
    // Worth checking at all because `pio run` needs an ESP-IDF toolchain, so
    // no gate ever reaches the link: a firmware missing one symbol would fail
    // on somebody's board rather than here.
    let shared = shared_with_firmware()?;
    let mut firmware_files = glob("firmware/cpp", "host_", ".cpp");
    firmware_files.extend(shared.iter().map(PathBuf::from));
    if let Err(why) = cpp::symbols_agree(
        "xpui_host.h and the firmware",
        "xpui_host.h",
        &declared,
        "firmware",
        &cpp::symbols("xpui_host_", &firmware_files)?,
    ) {
        failures.push(why);
    }

    if failures.is_empty() {
        Ok(format!("{} symbol(s), against two answers", declared.len()))
    } else {
        Err(format!(
            "{}\n\nA symbol in a header with no definition is a link error waiting\n\
             for whoever includes it.",
            failures.join("\n")
        ))
    }
}

/// The desktop host's files that the firmware compiles too, from
/// `build_src_filter` rather than a second list here.
fn shared_with_firmware() -> Result<Vec<String>, String> {
    let ini = std::fs::read_to_string("firmware/platformio.ini")
        .map_err(|e| format!("firmware/platformio.ini: {e}"))?;
    let mut found = Vec::new();
    let mut inside = false;
    for line in ini.lines() {
        if line.trim_start().starts_with("build_src_filter")
            && line
                .split('=')
                .next()
                .is_some_and(|k| k.trim() == "build_src_filter")
        {
            inside = true;
        } else if inside && line.trim().is_empty() {
            inside = false;
        }
        if !inside {
            continue;
        }
        for piece in line.split("+<").skip(1) {
            if let Some(path) = piece.split('>').next()
                && let Some(rest) = path.strip_prefix("../../")
                && rest.starts_with("cpp_host/cpp/")
                // A glob is not a filename: `[env:simulator_x3]` filters with
                // `+<../../cpp_host/cpp/*.cpp>`.
                && !rest.contains('*')
                && !rest.contains('?')
            {
                found.push(rest.to_string());
            }
        }
    }
    if found.is_empty() {
        // An empty list would compare against the firmware's own four files
        // alone, and pass for the wrong reason.
        return Err(
            "could not read build_src_filter out of firmware/platformio.ini.\n\
                    That list is what this check compares against; it cannot be empty."
                .into(),
        );
    }
    Ok(found)
}

/// Files in `dir` whose name starts with `prefix` and ends with `suffix`.
fn glob(dir: &str, prefix: &str, suffix: &str) -> Vec<PathBuf> {
    let mut out: Vec<PathBuf> = std::fs::read_dir(dir)
        .into_iter()
        .flatten()
        .filter_map(Result::ok)
        .map(|e| e.path())
        .filter(|p| {
            p.file_name().is_some_and(|n| {
                let n = n.to_string_lossy();
                n.starts_with(prefix) && n.ends_with(suffix)
            })
        })
        .collect();
    out.sort();
    out
}

/// The desktop host compiles.
///
/// Exceptions stay on, unlike the shim's own compile: this is ordinary desktop
/// C++ and its replacement `operator new` throws, which is what the standard
/// says it must do.
pub fn host_compiles() -> Result<String, String> {
    let Some(sdk) = cpp::freeink_include() else {
        return Ok("skipped: FreeInkUI headers not found. Set FREEINK_SDK_INCLUDE.".into());
    };
    let shim = backends_cpp();
    if !shim.is_dir() {
        return Ok("skipped: the backends repository is not beside this one. \
                   Clone XPUI-Framework/xpui-backends, or set XPUI_BACKENDS_DIR."
            .into());
    }
    let Some(sdl) = sdl_cflags() else {
        return Ok("skipped: SDL2 headers not found (sdl2-config, pkg-config sdl2).".into());
    };
    let mut command = std::process::Command::new("clang++");
    command
        .args(["-std=c++17", "-fsyntax-only", "-Wall", "-Wextra"])
        .arg(format!("-I{}", sdk.display()))
        .arg(format!("-I{}", shim.display()))
        .arg("-Icpp_host/cpp")
        .args(sdl)
        .args([
            "-DXPUI_HOST_VERSION=\"gate\"",
            "-DXPUI_HOST_DEVICE_NAME=\"gate\"",
        ])
        .args(glob("cpp_host/cpp", "", ".cpp"));
    if command
        .status()
        .map_err(|e| format!("clang++: {e}"))?
        .success()
    {
        Ok("clean".into())
    } else {
        Err("cpp_host/cpp does not compile".into())
    }
}

/// SDL2's compile flags, from whichever of the two tools is installed.
fn sdl_cflags() -> Option<Vec<String>> {
    for (program, arguments) in [
        ("sdl2-config", vec!["--cflags"]),
        ("pkg-config", vec!["--cflags", "sdl2"]),
    ] {
        if let Ok(out) = std::process::Command::new(program)
            .args(&arguments)
            .output()
            && out.status.success()
        {
            let text = String::from_utf8_lossy(&out.stdout);
            let flags: Vec<String> = text.split_whitespace().map(str::to_string).collect();
            if !flags.is_empty() {
                return Some(flags);
            }
        }
    }
    None
}

/// The C++ host builds, links and passes its own self-test.
///
/// The only place in the organisation where the C ABI is compiled, linked and
/// *executed* rather than syntax-checked. Not part of `check`: it needs CMake
/// and, without a local SDK, a network fetch of the pinned revision.
pub fn host_runs() -> Result<String, String> {
    if std::process::Command::new("cmake")
        .arg("--version")
        .output()
        .is_err()
    {
        return Ok("skipped: cmake not installed.".into());
    }
    let mut configure = vec![
        "-S".to_string(),
        "cpp_host".into(),
        "-B".into(),
        "target/cpp_host".into(),
        "-DCMAKE_BUILD_TYPE=Release".into(),
    ];
    if std::process::Command::new("ninja")
        .arg("--version")
        .output()
        .is_ok()
    {
        configure.extend(["-G".to_string(), "Ninja".into()]);
    }
    // Hand over the SDK when this machine has one, so the build needs no
    // network. Only when the include directory really sits where the SDK's
    // layout says: a FREEINK_SDK_INCLUDE pointing elsewhere is still fine,
    // because CMake then fetches the pinned revision rather than being handed
    // a wrong root.
    if let Some(include) = cpp::freeink_include()
        && let Ok(root) = include.join("../../../..").canonicalize()
        && root.join("libs/ui/FreeInkUI/include").is_dir()
    {
        configure.push(format!("-DFREEINK_SDK_DIR={}", root.display()));
    }
    run("cmake", &configure)?;
    run("cmake", &["--build".to_string(), "target/cpp_host".into()])?;
    run(
        "ctest",
        &[
            "--test-dir".to_string(),
            "target/cpp_host".into(),
            "--output-on-failure".into(),
        ],
    )?;
    Ok("built, linked, and its ctest cases passed".into())
}

/// One external command, failing with its own name.
fn run(program: &str, arguments: &[String]) -> Result<(), String> {
    let status = std::process::Command::new(program)
        .args(arguments)
        .status()
        .map_err(|e| format!("{program}: {e}"))?;
    if status.success() {
        Ok(())
    } else {
        Err(format!("{program} {} failed", arguments.join(" ")))
    }
}
