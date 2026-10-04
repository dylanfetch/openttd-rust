#!/usr/bin/env python3
"""Compare the bounded math corpus against unchanged pinned-reference sources."""

import argparse
import difflib
import hashlib
import json
import subprocess
from pathlib import Path

import migration


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, default=migration.REFERENCE)
    parser.add_argument(
        "--output", type=Path, default=migration.LOCAL / "math-comparison"
    )
    args = parser.parse_args()
    reference = args.reference.resolve()
    output = args.output.resolve()
    if (
        migration.git("rev-parse", "HEAD", cwd=reference)
        != migration.BASELINE["commit"]
    ):
        raise RuntimeError("Math oracle is not the pinned reference revision")
    if migration.git("status", "--porcelain", "--untracked-files=all", cwd=reference):
        raise RuntimeError("Math oracle source tree is dirty")
    output.mkdir(parents=True, exist_ok=True)
    env = migration.environment()
    env["CARGO_TARGET_DIR"] = str(output / "cargo")
    records = []

    def run(command, name, *, expected_failure=False):
        result = subprocess.run(
            command,
            cwd=migration.ROOT,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        (output / f"{name}.log").write_text(result.stdout)
        records.append(
            {
                "name": name,
                "argv": list(map(str, command)),
                "exit_code": result.returncode,
                "expected_failure": expected_failure,
            }
        )
        if bool(result.returncode) != expected_failure:
            raise RuntimeError(
                f"{name} had unexpected status {result.returncode}: {result.stdout}"
            )
        return result.stdout

    configuration = migration.rust_configuration(migration.ROOT / "build-rust")
    run(
        [
            "cargo",
            "build",
            "--release",
            "--locked",
            "--target",
            configuration["target"],
        ],
        "rust-build",
    )
    archive = migration.rust_archive(
        migration.ROOT / "build-rust", target_dir=output / "cargo"
    )
    fixture = migration.ROOT / "tools/migration/math-comparison.cpp"
    report = {
        "baseline": migration.BASELINE,
        "candidate_commit": migration.git("rev-parse", "HEAD"),
        "candidate_status": migration.git("status", "--short"),
        "commands": records,
        "reference_sources": {},
        "passed": False,
    }
    for name in (
        "src/core/math_func.cpp",
        "src/core/math_func.hpp",
        "src/core/overflowsafe_type.hpp",
        "src/core/strong_typedef_type.hpp",
        "src/landscape.cpp",
        "src/slope_func.h",
        "src/tile_type.h",
    ):
        report["reference_sources"][name] = hashlib.sha256(
            (reference / name).read_bytes()
        ).hexdigest()
    try:
        for mode in ("asserts", "ndebug"):
            streams = {}
            for label, source in (
                ("reference", reference),
                ("portable", migration.ROOT),
                ("candidate", migration.ROOT),
            ):
                # The simulation harness keeps coordinates inside a tile. Extract
                # the exact production body to exercise the defined wider domain.
                landscape = (source / "src/landscape.cpp").read_text()
                start = landscape.index(
                    "uint GetPartialPixelZ(int x, int y, Slope corners)"
                )
                end = landscape.index("\n}\n", start) + 3
                landscape_fixture = output / f"landscape-{label}.cpp"
                landscape_fixture.write_text(
                    '#include "stdafx.h"\n#include "landscape.h"\n'
                    '#include "slope_func.h"\n'
                    '#ifdef WITH_RUST\n#include "rust/ffi.h"\n#endif\n'
                    + landscape[start:end]
                )
                executable = output / f"{label}-{mode}"
                command = [
                    "c++",
                    "-std=c++20",
                    "-O2",
                    "-ffunction-sections",
                    "-fdata-sections",
                    "-DFMT_HEADER_ONLY",
                    "-fsanitize=undefined",
                    "-fno-sanitize-recover=undefined",
                    "-I",
                    str(source / "src"),
                    str(fixture),
                    str(source / "src/core/math_func.cpp"),
                    str(landscape_fixture),
                    "-Wl,--gc-sections",
                    "-o",
                    str(executable),
                ]
                if mode == "ndebug":
                    command.append("-DNDEBUG")
                if label == "candidate":
                    command.extend(
                        [
                            "-DWITH_RUST",
                            "-DMATH_ROUTE_PROBE",
                            str(archive),
                            "-ldl",
                            "-lpthread",
                            "-lm",
                            "-Wl,--wrap=openttd_rust_int_sqrt",
                            "-Wl,--wrap=openttd_rust_clamp_to",
                            "-Wl,--wrap=openttd_rust_soft_clamp",
                        ]
                    )
                run(command, f"compile-{label}-{mode}")
                stream = run([str(executable)], f"{label}-{mode}")
                streams[label] = "".join(
                    line + "\n"
                    for line in stream.splitlines()
                    if not line.startswith("Rust routing:")
                )
                if label == "candidate":
                    report[f"{mode}_routing"] = next(
                        line
                        for line in stream.splitlines()
                        if line.startswith("Rust routing:")
                    )
                    run(["nm", "-C", str(executable)], f"symbols-{mode}")
            for label in ("portable", "candidate"):
                if streams["reference"] != streams[label]:
                    diff = "".join(
                        difflib.unified_diff(
                            streams["reference"].splitlines(keepends=True),
                            streams[label].splitlines(keepends=True),
                            fromfile="reference",
                            tofile=label,
                        )
                    )
                    (output / f"difference-{label}-{mode}.diff").write_text(diff)
                    raise RuntimeError(
                        f"Math mismatch: {output / f'difference-{label}-{mode}.diff'}"
                    )
            report[f"{mode}_lines"] = len(streams["reference"].splitlines())
            report[f"{mode}_sha256"] = hashlib.sha256(
                streams["reference"].encode()
            ).hexdigest()
        # These strict-mode extended-type combinations were never accepted by
        # the pinned template; preserve failures while accepting unsigned widening.
        rejected = {
            "wide-signed-source": "ClampTo<unsigned __int128>(int64_t(-1))",
            "wide-signed-destination": "ClampTo<__int128>(uint64_t(7))",
            "wide-source": "ClampTo<uint64_t>(static_cast<unsigned __int128>(7))",
        }
        for name, expression in rejected.items():
            source_file = output / f"{name}.cpp"
            source_file.write_text(
                '#include "stdafx.h"\n#include "core/math_func.hpp"\nauto result = '
                + expression
                + ";\n"
            )
            for label, source in (
                ("reference", reference),
                ("portable", migration.ROOT),
                ("candidate", migration.ROOT),
            ):
                command = [
                    "c++",
                    "-std=c++20",
                    "-fsyntax-only",
                    "-I",
                    str(source / "src"),
                    str(source_file),
                ]
                if label == "candidate":
                    command.append("-DWITH_RUST")
                run(command, f"reject-{label}-{name}", expected_failure=True)
        report["preserved_rejections"] = list(rejected)
        if migration.git(
            "status", "--porcelain", "--untracked-files=all", cwd=reference
        ):
            raise RuntimeError("Reference became dirty during comparison")
        report["passed"] = True
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Math comparison passed: {output / 'report.json'}")


if __name__ == "__main__":
    main()
