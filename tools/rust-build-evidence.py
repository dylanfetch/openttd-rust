#!/usr/bin/env python3
"""Record native macOS Rust linkage and fresh generator execution evidence."""

import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

import migration


def archive_is_linked(link_script: Path, archive: Path) -> bool:
    """Resolve CMake Makefiles link tokens from the target's build directory."""
    # <target-directory>/CMakeFiles/<target>.dir/link.txt runs in target-directory.
    working_directory = link_script.parents[2]
    return any((working_directory / token).resolve() == archive.resolve()
               for token in shlex.split(link_script.read_text()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--tools", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = {"commit": migration.git("rev-parse", "HEAD"), "builds": {}, "passed": False}

    def run(argv, name):
        result = subprocess.run(list(map(str, argv)), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        (output / f"{name}.log").write_bytes(result.stdout)
        if result.returncode:
            raise RuntimeError(f"{name} failed: {result.stdout.decode(errors='replace')}")
        return result.stdout

    for label, build in (("game", args.build.resolve()), ("tools", args.tools.resolve())):
        configuration = migration.rust_configuration(build)
        archive = migration.rust_archive(build)
        if configuration["target"] != "aarch64-apple-darwin":
            raise RuntimeError("This evidence command requires native macOS arm64")
        (output / f"{label}-configuration.json").write_text(json.dumps(configuration, indent=2) + "\n")
        for name in ("CMakeCache.txt", "rust-toolchain.txt", "rust-native-libs.log", "rust-build.log", "rust-build-configuration.txt", "compile_commands.json"):
            (output / f"{label}-{name}").write_bytes((build / name).read_bytes())
        actual = re.findall(r"native-static-libs: ([^\r\n]+)", (build / "rust-build.log").read_text())
        if not actual or actual[-1].split() != configuration["native_libraries"]:
            raise RuntimeError("Actual archive native libraries differ from the configure-time query")
        if run(["lipo", "-archs", archive], f"{label}-archive-architecture").strip() != b"arm64":
            raise RuntimeError("Rust archive is not exclusively arm64")
        # Apple nm's LLVM reader can lag the pinned Rust compiler and reject
        # archive bitcode (Apple LLVM21 versus Rust LLVM23). Check the archive
        # architecture above; exact link paths and final Mach-O symbols below
        # remain mandatory, and their nm failures are never suppressed.
        commands = json.loads((build / "compile_commands.json").read_text())
        consumers = {"strgen": "/strgen/strgen.cpp", "settingsgen": "/settingsgen/settingsgen.cpp"}
        if label == "game":
            consumers.update({"game": "/openttd.cpp", "tests": "/tests/test_main.cpp"})
        expected_assertions = "ON" if configuration["build_type"] == "Debug" else "OFF"
        if configuration["assertions"] != expected_assertions:
            raise RuntimeError("Unexpected C++ assertion policy for this matrix mode")
        for name, suffix in consumers.items():
            matching = [entry for entry in commands if entry["file"].endswith(suffix)]
            if not matching or any("WITH_RUST" not in entry["command"] for entry in matching):
                raise RuntimeError(f"{label}/{name} lacks WITH_RUST compile propagation")
            for entry in matching:
                flags = entry["command"]
                if expected_assertions == "ON" and ("-DWITH_ASSERT" not in flags or "-DNDEBUG" in flags):
                    raise RuntimeError(f"{label}/{name} does not enable original C++ assertions")
                if expected_assertions == "OFF" and "-DNDEBUG" not in flags:
                    raise RuntimeError(f"{label}/{name} does not disable original C++ assertions")
        links = list(build.rglob("link.txt"))
        rust_links = [str(path.relative_to(build)) for path in links if archive_is_linked(path, archive)]
        for target in (["openttd", "openttd_test", "strgen", "settingsgen"] if label == "game" else ["strgen", "settingsgen"]):
            if not any(f"/{target}.dir/" in "/" + path for path in rust_links):
                raise RuntimeError(f"{label}/{target} lacks target-specific Rust archive linkage")
        for path in links:
            destination = output / label / path.relative_to(build)
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(path.read_bytes())
        for name, binary in (("strgen", build / "src/strgen/strgen"), ("settingsgen", build / "src/settingsgen/settingsgen")):
            symbols = run(["nm", "-g", binary], f"{label}-{name}-symbols")
            if not re.search(rb"\b[Tt] _openttd_rust_", symbols):
                raise RuntimeError(f"{name} has no defined Rust ABI symbol")
        if label == "game":
            for name in ("openttd", "openttd_test"):
                symbols = run(["nm", "-g", build / name], f"game-{name}-symbols")
                if not re.search(rb"\b[Tt] _openttd_rust_", symbols):
                    raise RuntimeError(f"{name} has no defined Rust ABI symbol")
        report["builds"][label] = configuration

    generated = output / "fresh-generated"
    generated.mkdir(exist_ok=True)
    names = ("strings.h", "english.lng", "french.lng", "settings.h")
    for name in names:
        (generated / name).unlink(missing_ok=True)
    lang = migration.ROOT / "src/lang"
    strgen = args.tools.resolve() / "src/strgen/strgen"
    for name, extra in (("header", []), ("english", [lang / "english.txt"]), ("french", [lang / "french.txt"])):
        run([strgen, "-s", lang, "-d", generated, *extra], f"fresh-strgen-{name}")
    settings = migration.ROOT / "src/table/settings"
    inputs = [settings / name for name in re.findall(r"\$\{CMAKE_CURRENT_SOURCE_DIR\}/(\w+\.ini)", (settings / "CMakeLists.txt").read_text())]
    run([args.tools.resolve() / "src/settingsgen/settingsgen", "-o", generated / "settings.h",
         "-b", migration.ROOT / "src/table/settings.h.preamble", "-a", migration.ROOT / "src/table/settings.h.postamble", *inputs], "fresh-settingsgen")
    if any(not (generated / name).is_file() or not (generated / name).stat().st_size for name in names):
        raise RuntimeError("Fresh generator outputs are missing or empty")
    report["passed"] = True
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
