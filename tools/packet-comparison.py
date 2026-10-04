#!/usr/bin/env python3
"""Compare bounded actual Packet framing, bytes and callback/allocation states."""

import hashlib
import json
import runpy
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = runpy.run_path(str(ROOT / "tools/migration.py"))
REFERENCE = MIGRATION["REFERENCE"].resolve()
OUT = ROOT / ".local/packet-comparison"


def main():
    MIGRATION["ensure_reference"]()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "report.json").unlink(missing_ok=True)
    env = MIGRATION["environment"]()
    archive = MIGRATION["rust_archive"](ROOT / "build-rust")
    fixture = ROOT / "tools/migration/packet-comparison.cpp"
    sources = (
        "src/network/core/packet.cpp",
        "src/network/core/packet.h",
        "src/string.cpp",
        "src/core/string_builder.cpp",
        "src/core/string_inplace.cpp",
        "src/core/utf8.cpp",
    )
    commands, hashes, line_counts = [], {}, {}
    for mode in ("O0", "O2"):
        outputs = {}
        for label, source in (
            ("reference", REFERENCE),
            ("candidate", ROOT),
            ("candidate-cpp", ROOT),
        ):
            binary = OUT / f"{label}-{mode}"
            command = [
                "g++",
                "-std=c++20",
                f"-{mode}",
                "-DUNIX",
                "-DFMT_HEADER_ONLY",
                "-ffunction-sections",
                "-fdata-sections",
                "-I",
                str(source / "src"),
                str(fixture),
                *[str(source / name) for name in sources if name.endswith(".cpp")],
                "-Wl,--gc-sections",
                "-o",
                str(binary),
            ]
            if label == "candidate":
                command.extend(
                    ["-DWITH_RUST", str(archive), "-ldl", "-lpthread", "-lm"]
                )
            commands.append(command)
            with (OUT / f"{label}-{mode}-compile.log").open("wb") as log:
                subprocess.run(
                    command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True
                )
            output = subprocess.check_output([str(binary)], env=env)
            (OUT / f"{label}-{mode}.txt").write_bytes(output)
            outputs[label] = output
            hashes[f"{label}-{mode}"] = hashlib.sha256(output).hexdigest()
        assert (
            outputs["reference"] == outputs["candidate"] == outputs["candidate-cpp"]
        ), f"Packet mismatch at {mode}"
        line_counts[mode] = len(outputs["reference"].splitlines())
    report = {
        "passed": True,
        "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"),
        "candidate_status": MIGRATION["git"]("status", "--short"),
        "reference": MIGRATION["BASELINE"],
        "commands": commands,
        "mode_lines": line_counts,
        "output_sha256": hashes,
        "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
        "source_sha256": {
            label: {
                name: hashlib.sha256((source / name).read_bytes()).hexdigest()
                for name in sources
            }
            for label, source in (("reference", REFERENCE), ("candidate", ROOT))
        },
        "limits": [
            "Bounded companion; unchanged network tests and authentication corpus remain primary.",
            "Send_string/Recv_string collection and sanitation remain C++.",
            "Nonempty-short Recv_bytes is undefined upstream (#41), excluded and unchanged.",
            "No real transport delivery, sockets or allocator-wide equivalence claim.",
        ],
        "agent": "/root/math_worker",
        "model": "gpt-6.1-sol",
        "reasoning_effort": "high",
    }
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Packet comparisons passed: {line_counts}; {OUT / 'report.json'}")


if __name__ == "__main__":
    main()
