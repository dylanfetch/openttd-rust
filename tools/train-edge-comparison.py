#!/usr/bin/env python3
"""Compare NewGRF-only variable-length curve and reversal inputs with pinned bodies.

The semantic rail scenarios use stock engines (length eight, no articulated parts).
This narrow copied-world probe calls production Rust entries, retaining the exact
original curve/reversal bodies as the oracle; it does not emulate a controller.
"""

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
        "--output", type=Path, default=migration.LOCAL / "train-edge-comparison"
    )
    args = parser.parse_args()
    migration.ensure_reference()
    if (
        migration.git("rev-parse", "HEAD", cwd=migration.REFERENCE)
        != migration.BASELINE["commit"]
    ):
        raise RuntimeError("Train oracle is not pinned")
    if migration.git("status", "--porcelain", cwd=migration.REFERENCE):
        raise RuntimeError("Train oracle source is dirty")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = (migration.REFERENCE / "src/train_cmd.cpp").read_text()
    bodies = []
    for signature in (
        "uint16_t Train::GetCurveSpeedLimit() const",
        "static void SwapTrainFlags(uint16_t *swap_flag1, uint16_t *swap_flag2)",
        "static void UpdateStatusAfterSwap(Train *v)",
        "void ReverseTrainSwapVeh(Train *v, int l, int r)",
    ):
        start = source.index(signature)
        end = source.index("\n}\n", start) + 3
        bodies.append(source[start:end])
    (output / "train-reference.inc").write_text("\n".join(bodies))
    configuration = migration.rust_configuration(args.build)
    cache = dict(
        (line.split("=", 1)[0].split(":", 1)[0], line.split("=", 1)[1])
        for line in (args.build / "CMakeCache.txt").read_text().splitlines()
        if line and not line.startswith(("#", "//")) and "=" in line
    )
    flags = []
    if configuration["pointer_bytes"] == 8:
        flags.append("-DPOINTER_IS_64BIT")
    if configuration["platform"] == "Darwin":
        flags += [
            "-arch",
            "arm64",
            "-isysroot",
            configuration["sdk"],
            "-mmacosx-version-min=" + configuration["deployment_target"],
        ]
    command = [
        cache["CMAKE_CXX_COMPILER"],
        "-std=c++20",
        "-O2",
        *flags,
        "-I",
        str(migration.ROOT / "src"),
        "-I",
        str(output),
        str(migration.ROOT / "tools/migration/train-edge-comparison.cpp"),
        str(migration.rust_archive(args.build)),
        *configuration["native_libraries"],
        "-o",
        str(output / "train-edge"),
    ]
    records = []
    report = {
        "baseline": migration.BASELINE,
        "candidate_commit": migration.git("rev-parse", "HEAD"),
        "reference_body_sha256": hashlib.sha256("\n".join(bodies).encode()).hexdigest(),
        "commands": records,
        "passed": False,
    }
    try:
        for name, argv in (("compile", command), ("run", [str(output / "train-edge")])):
            result = subprocess.run(
                argv,
                env=migration.environment(),
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            (output / f"{name}.log").write_text(result.stdout)
            records.append({"name": name, "argv": argv, "exit_code": result.returncode})
            if result.returncode:
                raise RuntimeError(f"Train edge {name} failed: {result.stdout}")
        report["output"] = result.stdout
        report["passed"] = True
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Train edge comparison passed: {output / 'report.json'}")


if __name__ == "__main__":
    main()
