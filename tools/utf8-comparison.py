#!/usr/bin/env python3
"""Compare the scoped UTF-8 corpus against unchanged pinned-reference sources."""

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess

import migration


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, default=migration.REFERENCE)
    parser.add_argument("--output", type=Path, default=migration.LOCAL / "utf8-comparison")
    args = parser.parse_args()
    reference = args.reference.resolve()
    output = args.output.resolve()
    if migration.git("rev-parse", "HEAD", cwd=reference) != migration.BASELINE["commit"]:
        raise RuntimeError("UTF-8 oracle is not the pinned reference revision")
    if migration.git("status", "--porcelain", "--untracked-files=all", cwd=reference):
        raise RuntimeError("UTF-8 oracle source tree is dirty")
    output.mkdir(parents=True, exist_ok=True)
    env = migration.environment()
    env["CARGO_TARGET_DIR"] = str(output / "cargo")
    records = []

    def run(command, name):
        result = subprocess.run(command, cwd=migration.ROOT, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (output / f"{name}.log").write_text(result.stdout)
        records.append({"name": name, "argv": list(map(str, command)), "exit_code": result.returncode})
        if result.returncode:
            raise RuntimeError(f"{name} failed: {result.stdout}")
        return result.stdout

    configuration = migration.rust_configuration(migration.ROOT / "build-rust")
    run(["cargo", "build", "--release", "--locked", "--target", configuration["target"]], "rust-build")
    archive = migration.rust_archive(migration.ROOT / "build-rust", target_dir=output / "cargo")
    fixture = migration.ROOT / "tools/migration/utf8-comparison.cpp"
    report = {"baseline": migration.BASELINE, "candidate_commit": migration.git("rev-parse", "HEAD"),
              "candidate_status": migration.git("status", "--short"), "commands": records,
              "reference_sources": {}, "passed": False}
    for name in ("src/core/utf8.cpp", "src/core/utf8.hpp", "src/core/string_consumer.cpp", "src/core/string_consumer.hpp", "src/string.cpp", "src/core/string_inplace.cpp", "src/core/string_inplace.hpp", "src/core/string_builder.cpp", "src/string_type.h", "src/table/control_codes.h"):
        report["reference_sources"][name] = hashlib.sha256((reference / name).read_bytes()).hexdigest()
    try:
        for mode in ("asserts", "ndebug"):
            streams = {}
            for label, source in (("reference", reference), ("candidate", migration.ROOT), ("candidate-cpp", migration.ROOT)):
                executable = output / f"{label}-{mode}"
                command = ["c++", "-std=c++20", "-O2", "-ffunction-sections", "-fdata-sections", "-DFMT_HEADER_ONLY",
                           "-I", str(source / "src"), str(fixture),
                           str(source / "src/core/utf8.cpp"), str(source / "src/core/string_consumer.cpp"),
                           str(source / "src/core/string_builder.cpp"), str(source / "src/core/string_inplace.cpp"),
                           "-Wl,--gc-sections", "-o", str(executable)]
                if mode == "ndebug":
                    command.append("-DNDEBUG")
                if label == "candidate":
                    command.extend(["-DWITH_RUST", str(archive), "-ldl", "-lpthread", "-lm"])
                run(command, f"compile-{label}-{mode}")
                streams[label] = run([str(executable)], f"{label}-{mode}")
            if not streams["reference"] == streams["candidate"] == streams["candidate-cpp"]:
                diff = "".join(difflib.unified_diff(streams["reference"].splitlines(keepends=True),
                                                   streams["candidate"].splitlines(keepends=True),
                                                   fromfile="reference", tofile="candidate"))
                (output / f"difference-{mode}.diff").write_text(diff)
                raise RuntimeError(f"UTF-8 mismatch: {output / f'difference-{mode}.diff'}")
            report[f"{mode}_lines"] = len(streams["reference"].splitlines())
            report[f"{mode}_sha256"] = hashlib.sha256(streams["reference"].encode()).hexdigest()
        if migration.git("status", "--porcelain", "--untracked-files=all", cwd=reference):
            raise RuntimeError("Reference became dirty during comparison")
        report["validation_limits"] = ["Historical byte codec; all 16 flag combinations and ignored unknown bits.", "In-place copy only original defined disjoint/left-overlap domain; exact/right destination-inside-input overlap excluded.", "Bounded ordinary C++ append failure and live-consumer/copy observations, not arbitrary allocator failures.", "Packet text loops and other string algorithms remain unchanged."]
        report["passed"] = True
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"UTF-8 comparison passed: {output / 'report.json'}")


if __name__ == "__main__":
    main()
