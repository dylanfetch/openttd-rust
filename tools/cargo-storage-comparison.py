#!/usr/bin/env python3
"""Compare the unreachable pool-limit Split failure with the unchanged reference."""

import hashlib
import json
import runpy
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = runpy.run_path(str(ROOT / "tools/migration.py"))
OUT = ROOT / ".local/cargo-storage-comparison"


def main():
    MIGRATION["ensure_reference"]()
    reference = MIGRATION["REFERENCE"] / "src/cargopacket.cpp"
    original = reference.read_text()
    start = original.index("CargoPacket *CargoPacket::Split(uint new_size)")
    end = original.index("\n}", start) + 2
    body = original[start:end]
    fixture = ROOT / "tools/simulation/cargo-split-reference.cpp"
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "split.cpp"
    source.write_text(fixture.read_text().replace("@UNCHANGED_SPLIT@", body))
    env = MIGRATION["environment"]()
    commands, outputs = [], {}
    for candidate in (False, True):
        label = "candidate" if candidate else "reference"
        binary = OUT / label
        command = [
            "g++",
            "-std=c++20",
            "-O2",
            "-include",
            "cstdlib",
            "-include",
            "initializer_list",
            "-I",
            str(ROOT / "src"),
            str(source),
            "-o",
            str(binary),
        ]
        if candidate:
            command += [
                "-DWITH_RUST",
                str(MIGRATION["rust_archive"](ROOT / "build-rust")),
                "-ldl",
                "-lpthread",
                "-lm",
            ]
        subprocess.run(command, env=env, check=True)
        outputs[label] = subprocess.check_output([str(binary)], env=env)
        commands.append(command)
    if outputs["reference"] != outputs["candidate"]:
        raise RuntimeError("pool-limit Split failure differs")
    report = {
        "passed": True,
        "gap": "pool-limit Split failure: early return and no mutation or allocation",
        "cases": len(outputs["reference"].splitlines()),
        "reference_sha256": hashlib.sha256(reference.read_bytes()).hexdigest(),
        "unchanged_body_sha256": hashlib.sha256(body.encode()).hexdigest(),
        "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
        "commands": commands,
        "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"),
        "candidate_status": MIGRATION["git"]("status", "--short"),
    }
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Cargo pool-limit Split comparison passed: {report['cases']} cases")


if __name__ == "__main__":
    main()
