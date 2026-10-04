#!/usr/bin/env python3
"""Compare scoped widget-parser gaps against unchanged original control bodies."""

import hashlib
import json
import re
import runpy
import shlex
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = runpy.run_path(str(ROOT / "tools/migration.py"))
REFERENCE = MIGRATION["REFERENCE"].resolve()
OUT = ROOT / ".local/widget-parser-comparison"
BUILD = ROOT / "build-rust"
SIGNATURES = (
    "static bool IsAttributeWidgetPartType(",
    "bool IsContainerWidgetType(",
    "static std::unique_ptr<NWidgetBase> MakeNWidget(const NWidgetPart &nwid)",
    "static std::span<const NWidgetPart>::iterator MakeNWidget(",
    "static std::span<const NWidgetPart>::iterator MakeWidgetTree(",
    "std::unique_ptr<NWidgetBase> MakeNWidgets(",
    "std::unique_ptr<NWidgetBase> MakeWindowNWidgetTree(",
)


def extract(text, signature):
    start = text.index(signature)
    opened = text.index("{", start)
    depth, end = 1, opened + 1
    # These bounded original bodies have balanced braces, including comments.
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    MIGRATION["ensure_reference"]()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "report.json").unlink(missing_ok=True)
    original_source = (REFERENCE / "src/widget.cpp").read_text()
    current_source = (ROOT / "src/widget.cpp").read_text()
    bodies = {
        signature: extract(original_source, signature) for signature in SIGNATURES
    }
    primitive_hashes = {}
    for signature in (
        "void ApplyNWidgetPartAttribute(",
        SIGNATURES[2],
        "class NWidgetLayer : public NWidgetContainer {",
    ):
        original = extract(original_source, signature)
        if original != extract(current_source, signature):
            raise RuntimeError(f"Retained typed primitive changed: {signature}")
        primitive_hashes[signature] = hashlib.sha256(original.encode()).hexdigest()
    renamed = "\n\n".join(bodies.values())
    for name in (
        "IsAttributeWidgetPartType",
        "IsContainerWidgetType",
        "MakeNWidget",
        "MakeWidgetTree",
        "MakeNWidgets",
        "MakeWindowNWidgetTree",
    ):
        # Replace longest matching identifiers rather than prefixes of MakeNWidgets.
        renamed = re.sub(r"\b" + name + r"\b", "Original" + name, renamed)
    oracle = OUT / "original.cpp"
    oracle.write_text(
        '#include "stdafx.h"\n#include "window_gui.h"\n#include "table/strings.h"\n'
        + "void ApplyNWidgetPartAttribute(const NWidgetPart &, NWidgetBase *);\n"
        + extract(original_source, "class NWidgetLayer : public NWidgetContainer {")
        + ";\n"
        + renamed
        + "\n"
    )
    env = MIGRATION["environment"]()
    config = MIGRATION["rust_configuration"](BUILD)
    if config["platform"] != "Linux":
        raise RuntimeError(
            "This production-object semantic fixture currently requires native Linux"
        )
    compiler = None
    for line in (BUILD / "CMakeCache.txt").read_text().splitlines():
        if line.startswith("CMAKE_CXX_COMPILER:FILEPATH="):
            compiler = line.split("=", 1)[1]
    if compiler is None:
        raise RuntimeError("Configured native compiler missing")
    flags = [
        compiler,
        "-std=c++20",
        "-DWITH_RUST",
        "-DUNIX",
        "-DPOINTER_IS_64BIT",
        "-DFMT_HEADER_ONLY",
        "-I",
        str(ROOT / "src"),
        "-I",
        str(BUILD / "generated"),
        "-I",
        str(ROOT / "src/script/api"),
        "-I",
        str(BUILD / "generated/script/api"),
        "-I",
        str(ROOT / "src/3rdparty/squirrel/include"),
    ]
    # Reuse the actual game object/library inputs, replacing only widget.cpp and
    # test entry points. This preserves registered descriptors and real primitives.
    raw = (
        subprocess.check_output(
            ["ninja", "-C", str(BUILD), "-t", "commands", "openttd_test"], env=env
        )
        .decode()
        .splitlines()[-1]
    )
    link = shlex.split(raw.removeprefix(": && "))
    if "&&" in link:
        link = link[: link.index("&&")]
    if link[0] != compiler:
        raise RuntimeError("Unexpected native test link compiler")
    inputs, cursor = [], 1
    while cursor < len(link):
        token = link[cursor]
        cursor += 1
        if token == "-o":
            cursor += 1
            continue
        if token.startswith("-Wl,--dependency-file="):
            continue
        if token.endswith(".o"):
            if "CMakeFiles/openttd_test.dir/" in token and not token.endswith(
                "/mock_spritecache.cpp.o"
            ):
                continue
            if token.endswith("/src/widget.cpp.o"):
                continue
            token = str(BUILD / token)
        elif token.endswith((".a", ".so")) and not token.startswith("/"):
            token = str(BUILD / token)
        inputs.append(token)
    commands, results, failures = [], {}, []

    def command(argv, label):
        commands.append(argv)
        with (OUT / f"{label}.log").open("wb") as log:
            subprocess.run(
                argv, env=env, stdout=log, stderr=subprocess.STDOUT, check=True
            )

    modes = {
        "custom-assert": ["-O2", "-DNDEBUG", "-DWITH_ASSERT"],
        "standard-assert": ["-O0"],
        "release": ["-O2", "-DNDEBUG"],
    }
    for mode, policy in modes.items():
        objects = []
        for label, source in (
            ("widget", ROOT / "src/widget.cpp"),
            ("oracle", oracle),
            ("fixture", ROOT / "tools/migration/widget-parser-comparison.cpp"),
        ):
            obj = OUT / f"{mode}-{label}.o"
            command(
                [*flags, *policy, "-c", str(source), "-o", str(obj)],
                f"{mode}-{label}-compile",
            )
            objects.append(str(obj))
        binary = OUT / mode
        command([compiler, *objects, *inputs, "-o", str(binary)], f"{mode}-link")
        completed = subprocess.run(
            [str(binary)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        (OUT / f"{mode}.out").write_bytes(completed.stdout)
        (OUT / f"{mode}.err").write_bytes(completed.stderr)
        if completed.returncode or completed.stderr:
            raise RuntimeError(f"{mode} semantic fixture failed; see retained output")
        null_records = []
        for version in ("original", "candidate"):
            result = subprocess.run(
                [str(binary), "--null", version],
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            (OUT / f"{mode}-null-{version}.out").write_bytes(result.stdout)
            (OUT / f"{mode}-null-{version}.err").write_bytes(result.stderr)
            null_records.append(
                {"exit": result.returncode, "stdout": result.stdout.decode()}
            )
        if mode == "release":
            if null_records[0] != null_records[1] or null_records[0]["exit"] != 0:
                failures.append({"mode": mode, "null_records": null_records})
        elif (
            any(
                record["exit"] == 0 or "null-callback" not in record["stdout"]
                for record in null_records
            )
            or null_records[0]["exit"] != null_records[1]["exit"]
        ):
            failures.append({"mode": mode, "null_records": null_records})
        results[mode] = {
            "records": len(completed.stdout.splitlines()),
            "output_sha256": hashlib.sha256(completed.stdout).hexdigest(),
            "null_records": null_records,
        }
    (OUT / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
    if failures:
        raise RuntimeError("Null-generator disposition changed; see failures.json")
    MIGRATION["ensure_reference"]()
    registered = int((OUT / "custom-assert.out").read_text().splitlines()[0].split()[1])
    report = {
        "baseline": MIGRATION["BASELINE"],
        "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"),
        "candidate_status": MIGRATION["git"]("status", "--porcelain"),
        "commands": commands,
        "registered_descriptors": registered,
        "modes": results,
        "original_body_sha256": {
            name: hashlib.sha256(body.encode()).hexdigest()
            for name, body in bodies.items()
        },
        "retained_primitive_sha256": primitive_hashes,
        "reference_widget_sha256": hashlib.sha256(
            (REFERENCE / "src/widget.cpp").read_bytes()
        ).hexdigest(),
        "fixture_sha256": hashlib.sha256(
            (ROOT / "tools/migration/widget-parser-comparison.cpp").read_bytes()
        ).hexdigest(),
        "limits": "Native Linux. Only parser/widget.cpp and oracle/fixture assertion flags vary; other production objects retain verified native flags. Fatal null paths compare termination category and callback entry, not changed assertion expression/location text. Background shapes use existing default vertical children. No rendering or complete GUI equivalence.",
        "passed": True,
    }
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        f"Widget parser comparisons passed; {registered} registered descriptors; {OUT / 'report.json'}"
    )


if __name__ == "__main__":
    main()
