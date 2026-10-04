#!/usr/bin/env python3
"""Compare accepted wide math templates with the pinned C++ on the native library."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import migration


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=migration.ROOT / "build-rust")
    parser.add_argument(
        "--output", type=Path, default=migration.LOCAL / "math-extension-comparison"
    )
    args = parser.parse_args()
    # macOS platform checkout is shallow; fetch only the immutable oracle object
    # when it is absent, without moving any candidate branch or reference tree.
    present = subprocess.run(
        ["git", "cat-file", "-e", migration.BASELINE["commit"] + "^{commit}"],
        cwd=migration.ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if present.returncode:
        subprocess.run(
            ["git", "fetch", "--no-tags", "origin", migration.BASELINE["commit"]],
            cwd=migration.ROOT,
            check=True,
        )
    migration.ensure_reference()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    configuration = migration.rust_configuration(args.build)
    archive = migration.rust_archive(args.build)
    cache = {}
    for line in (args.build / "CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    compiler = cache["CMAKE_CXX_COMPILER"]
    native_flags = [f"-DOPENTTD_MATH_POINTER_BYTES={configuration['pointer_bytes']}"]
    if configuration["pointer_bytes"] == 8:
        # Match CMake's definition; leave the original osx_stdafx.h guard intact.
        native_flags.append("-DPOINTER_IS_64BIT")
    if configuration["platform"] == "Darwin":
        architecture = cache.get("CMAKE_OSX_ARCHITECTURES")
        if (
            architecture != "arm64"
            or not configuration["sdk"]
            or not configuration["deployment_target"]
        ):
            raise RuntimeError(
                "Wide math probe needs the validated native macOS architecture, SDK and minimum"
            )
        native_flags.extend(
            [
                "-arch",
                architecture,
                "-isysroot",
                configuration["sdk"],
                "-mmacosx-version-min=" + configuration["deployment_target"],
            ]
        )
    env = migration.environment()
    fixture = migration.ROOT / "tools/migration/math-extension-comparison.cpp"
    report = {
        "baseline": migration.BASELINE,
        "candidate_commit": migration.git("rev-parse", "HEAD"),
        "candidate_status": migration.git("status", "--short"),
        "commands": [],
        "passed": False,
        "compiler": subprocess.check_output(
            [compiler, "--version"], env=env, text=True
        ),
        "native_configuration": configuration,
        "compiler_path": compiler,
        "native_flags": native_flags,
        "reference_header_sha256": hashlib.sha256(
            (migration.REFERENCE / "src/core/math_func.hpp").read_bytes()
        ).hexdigest(),
    }
    streams = {}
    try:
        for label, source in (
            ("reference", migration.REFERENCE),
            ("portable", migration.ROOT),
            ("candidate", migration.ROOT),
        ):
            binary = output / label
            command = [
                compiler,
                "-std=c++20",
                "-O2",
                *native_flags,
                "-I",
                str(source / "src"),
                str(fixture),
                "-o",
                str(binary),
            ]
            if label == "candidate":
                command.extend(
                    ["-DWITH_RUST", str(archive), *configuration["native_libraries"]]
                )
            for name, argv in ((f"compile-{label}", command), (label, [str(binary)])):
                result = subprocess.run(
                    argv,
                    env=env,
                    cwd=migration.ROOT,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                )
                (output / f"{name}.log").write_text(result.stdout)
                report["commands"].append(
                    {
                        "name": name,
                        "argv": list(map(str, argv)),
                        "exit_code": result.returncode,
                    }
                )
                if result.returncode:
                    raise RuntimeError(f"{name} failed: {result.stdout}")
                if name == label:
                    streams[label] = result.stdout
        if len(set(streams.values())) != 1:
            raise RuntimeError(f"Wide math outputs differ; inspect {output}")
        if migration.git(
            "status", "--porcelain", "--untracked-files=all", cwd=migration.REFERENCE
        ):
            raise RuntimeError("Pinned reference became dirty")
        report["output"] = streams["reference"]
        report["passed"] = True
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Wide math comparison passed: {output / 'report.json'}")


if __name__ == "__main__":
    main()
