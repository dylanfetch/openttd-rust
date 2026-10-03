#!/usr/bin/env python3
"""Exercise scoped native MSVC refusal paths and effective archive invalidation."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

import migration


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "configuration-report.json").unlink(missing_ok=True)
    report = {"commit": migration.git("rev-parse", "HEAD"), "commands": [], "refusals": [], "freshness": [], "passed": False}
    env = os.environ.copy()
    common = ["cmake", "-S", str(migration.ROOT), "-G", "Ninja", "-DOPTION_RUST=ON", "-DOPTION_TOOLS_ONLY=ON",
              "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DOPTION_USE_ASSERTS=ON", "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"]

    def run(command, name, run_env=env):
        result = subprocess.run(list(map(str, command)), env=run_env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (output / f"{name}.log").write_bytes(result.stdout)
        report["commands"].append({"argv": list(map(str, command)), "exit_code": result.returncode,
                                   "environment": {key: run_env.get(key) for key in ("RUSTFLAGS", "CARGO_ENCODED_RUSTFLAGS", "CARGO_PROFILE_RELEASE_CODEGEN_UNITS")}})
        return result

    refusals = [
        ("debug", ["-DCMAKE_BUILD_TYPE=Debug"], {}, "RelWithDebInfo and OPTION_USE_ASSERTS=ON"),
        ("release", ["-DCMAKE_BUILD_TYPE=Release"], {}, "RelWithDebInfo and OPTION_USE_ASSERTS=ON"),
        ("assertions-off", ["-DOPTION_USE_ASSERTS=OFF"], {}, "RelWithDebInfo and OPTION_USE_ASSERTS=ON"),
        ("dynamic-crt", ["-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"], {}, "dynamic and debug CRTs are unsupported"),
        ("debug-crt", ["-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug"], {}, "dynamic and debug CRTs are unsupported"),
        ("conflicting-crt-flag", ["-DCMAKE_CXX_FLAGS=/MD"], {}, "conflicting CRT flags"),
        ("crt-mismatch-suppression", ["-DCMAKE_EXE_LINKER_FLAGS=/force:multiple"], {}, "CRT mismatch suppression"),
        ("external-host-tools", ["-DHOST_BINARY_DIR=" + str(output / "external")], {}, "external HOST_BINARY_DIR"),
        ("cross-configuration", ["-DCMAKE_SYSTEM_NAME=Windows"], {}, "cross-OS builds are unsupported"),
        ("contradictory-rust-crt", [], {"RUSTFLAGS": "-C target-feature=-crt-static"}, "contradictory or unvalidated flags"),
        ("encoded-target-flags", [], {"CARGO_ENCODED_RUSTFLAGS": "-C\x1ftarget-feature=-crt-static"}, "unvalidated CARGO_ENCODED_RUSTFLAGS"),
        ("unwind-profile", [], {"CARGO_PROFILE_RELEASE_PANIC": "unwind"}, "release panic=abort"),
        ("arm-macro-conflict", ["-DCMAKE_CXX_FLAGS=/D_M_ARM64"], {}, "Configured C++ architecture/platform/pointer width"),
    ]
    for name, flags, changes, diagnostic in refusals:
        candidate_env = env.copy()
        candidate_env.update(changes)
        result = run([*common, "-B", str(output / f"reject-{name}"), *flags], f"reject-{name}", candidate_env)
        if result.returncode == 0 or diagnostic.encode() not in result.stdout:
            raise RuntimeError(f"Expected specific refusal for {name}; see retained log")
        report["refusals"].append({"name": name, "diagnostic": diagnostic})
    result = run([*common[:3], "-G", "Ninja Multi-Config", *common[5:], "-B", str(output / "reject-multi-config")], "reject-multi-config")
    if result.returncode == 0 or b"single-config Ninja" not in result.stdout:
        raise RuntimeError("Multi-config Ninja refusal failed")
    report["refusals"].append({"name": "multi-config", "diagnostic": "single-config Ninja"})
    clang = shutil.which("clang-cl") or str(Path(r"C:\Program Files\LLVM\bin\clang-cl.exe"))
    if not Path(clang).is_file():
        raise RuntimeError("Native clang-cl is required to exercise the non-MSVC refusal")
    result = run([*common, "-B", str(output / "reject-clang"), f"-DCMAKE_CXX_COMPILER={clang}"], "reject-clang")
    if result.returncode == 0 or b"VS 2022 MSVC" not in result.stdout:
        raise RuntimeError("Non-MSVC compiler refusal failed")
    report["refusals"].append({"name": "clang-cl", "diagnostic": "VS 2022 MSVC"})

    tools = args.tools.resolve()
    configuration = migration.rust_configuration(tools)
    # Temporarily hide only std rlibs on this ephemeral CI toolchain, then restore
    # in finally. rustc itself remains usable; no shared reference/source changes.
    stdlib = Path(subprocess.check_output(["rustc", "--print", "target-libdir", "--target", configuration["target"]], text=True).strip())
    original_std = list(stdlib.glob("libstd-*.rlib"))
    if not original_std:
        raise RuntimeError("No installed target std rlib found for the missing-std refusal")
    hidden = []
    try:
        for path in original_std:
            backup = path.with_name(path.name + ".windows-rust-refusal")
            if backup.exists():
                raise RuntimeError("Unexpected prior hidden std artifact")
            path.rename(backup)
            hidden.append((path, backup))
        result = run([*common, "-B", str(output / "reject-missing-target-std")], "reject-missing-target-std")
        if result.returncode == 0 or b"Cannot obtain native static libraries" not in result.stdout:
            raise RuntimeError("Missing target standard library did not fail the native Rust query")
        (output / "missing-target-std-rust-query.log").write_bytes((output / "reject-missing-target-std/rust-native-libs.log").read_bytes())
    finally:
        for path, backup in hidden:
            backup.rename(path)
    report["refusals"].append({"name": "missing-target-std", "diagnostic": "Cannot obtain native static libraries", "restored": True})
    archive = migration.rust_archive(tools)
    stamp = tools / "rust-build-configuration.txt"
    original = hashlib.sha256(stamp.read_bytes()).hexdigest()
    previous = archive.stat().st_mtime_ns
    changed_env = env.copy()
    changed_env["CARGO_PROFILE_RELEASE_CODEGEN_UNITS"] = "2"
    for name, step_env in (("changed-codegen", changed_env), ("restored-codegen", env)):
        result = run(["cmake", "-S", migration.ROOT, "-B", tools], name + "-configure", step_env)
        if result.returncode:
            raise RuntimeError("Freshness reconfigure failed")
        current = hashlib.sha256(stamp.read_bytes()).hexdigest()
        if (name == "changed-codegen" and current == original) or (name == "restored-codegen" and current != original):
            raise RuntimeError("Effective configuration stamp did not track the profile change/restoration")
        result = run(["cmake", "--build", tools, "--target", "tools", "--parallel", "4", "--", "-d", "keeprsp"], name + "-build", step_env)
        if result.returncode or archive.stat().st_mtime_ns <= previous:
            raise RuntimeError("Changed effective configuration did not rebuild its actual archive")
        if (tools / "rust-build-configuration.txt.successful").read_text() != current:
            raise RuntimeError("Successful configuration digest does not describe the built archive")
        if migration.rust_configuration(tools) != configuration:
            raise RuntimeError("Freshness trial changed target/CRT/native-library metadata")
        report["freshness"].append({"name": name, "configuration_sha256": current, "archive_mtime_ns": archive.stat().st_mtime_ns})
        (output / (name + "-rust-build.log")).write_bytes((tools / "rust-build.log").read_bytes())
        previous = archive.stat().st_mtime_ns
    report["passed"] = True
    (output / "configuration-report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
