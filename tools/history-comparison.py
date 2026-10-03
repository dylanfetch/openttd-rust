#!/usr/bin/env python3
"""Compare demonstrated generic-history gaps through unchanged public interfaces."""

from collections import Counter
import hashlib
import json
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = runpy.run_path(str(ROOT / "tools/migration.py"))
REFERENCE = MIGRATION["REFERENCE"].resolve()
OUT = ROOT / ".local/history-comparison"
REDUCERS = {
    "src/industry_cmd.cpp": (
        "Industry::ProducedHistory SumHistory(std::span<const Industry::ProducedHistory> history)",
        "Industry::AcceptedHistory SumHistory(std::span<const Industry::AcceptedHistory> history)",
    ),
    "src/town_cmd.cpp": (
        "Town::SuppliedHistory SumHistory(std::span<const Town::SuppliedHistory> history)",
    ),
}
STRUCTURE = ("src/misc/history.cpp", "src/misc/history_func.hpp", "src/misc/history_type.hpp")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def reducers():
    """Extract actual unchanged production specializations, including int accumulators."""
    pieces, hashes = [], {}
    for filename, signatures in REDUCERS.items():
        source = (REFERENCE / filename).read_text()
        candidate = (ROOT / filename).read_text()
        for signature in signatures:
            start = source.index("template <>\n" + signature + "\n{")
            opened = source.index("{", start)
            depth, end = 1, opened + 1
            # These three functions have balanced braces, including their lambdas.
            while depth:
                depth += (source[end] == "{") - (source[end] == "}")
                end += 1
            text = source[start:end] + "\n"
            if text not in candidate:
                raise RuntimeError(f"Production reducer changed: {signature}")
            pieces.append(text)
            hashes[signature] = sha256(text.encode())
    path = OUT / "production-reducers.cpp"
    path.write_text('#include "stdafx.h"\n#include "misc/history_func.hpp"\n#include "industry.h"\n#include "town.h"\n' + "\n".join(pieces))
    return path, hashes


def run(command, env, name):
    with (OUT / f"{name}-compile.log").open("wb") as log:
        subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    result = subprocess.run([str(OUT / name)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (OUT / f"{name}.out").write_bytes(result.stdout)
    (OUT / f"{name}.err").write_bytes(result.stderr)
    if result.returncode or result.stderr:
        raise RuntimeError(f"{name} failed: exit {result.returncode}; see retained output")
    return result.stdout


def main():
    MIGRATION["ensure_reference"]()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "report.json").unlink(missing_ok=True)
    env = MIGRATION["environment"]()
    archive = MIGRATION["rust_archive"](ROOT / "build-rust")
    production, hashes = reducers()
    fixture = ROOT / "tools/migration/history-comparison.cpp"
    commands, outputs, failures = [], {}, []
    for optimization in ("O0", "O2"):
        for label, source in (("reference", REFERENCE), ("candidate", ROOT), ("candidate-cpp", ROOT)):
            name = f"{label}-{optimization}"
            command = ["g++", "-std=c++20", f"-{optimization}", "-DFMT_HEADER_ONLY", "-ffunction-sections", "-fdata-sections",
                       "-I", str(source / "src"), str(fixture), str(production), str(source / "src/misc/history.cpp"),
                       "-Wl,--gc-sections", "-o", str(OUT / name)]
            if label == "candidate":
                command.extend(["-DWITH_RUST", str(archive), "-ldl", "-lpthread", "-lm"])
            commands.append(command)
            output = run(command, env, name)
            outputs[label, optimization] = output
            if label != "reference" and output != outputs["reference", optimization]:
                expected = outputs["reference", optimization].splitlines()
                actual = output.splitlines()
                for index in range(max(len(expected), len(actual))):
                    a = expected[index] if index < len(expected) else b"<missing>"
                    b = actual[index] if index < len(actual) else b"<missing>"
                    if a != b:
                        failures.append({"candidate": label, "optimization": optimization, "line": index + 1,
                                         "expected": a.decode(), "actual": b.decode()})
    (OUT / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
    if failures:
        raise RuntimeError(f"{len(failures)} history discrepancies; see {OUT / 'failures.json'}")
    original = outputs["reference", "O0"].splitlines()
    counts = Counter(line.split()[0].decode() for line in original)
    # These concrete gaps must actually occur in the pristine output.
    if b"grouping 0 1" not in original:
        raise RuntimeError("Nested production rounding fixture no longer demonstrates grouping")
    validity_disagreement = [line.decode() for line in original if line.startswith(b"query ")
                             and b" fatal " not in line and line.split()[5] != line.split()[6]]
    if not validity_disagreement:
        raise RuntimeError("First-child validity versus OR validity gap not exercised")
    sanitizer_command = next(list(command) for command in commands if str(OUT / "candidate-O0") in command)
    sanitizer_command[sanitizer_command.index("-o") + 1] = str(OUT / "candidate-sanitized")
    sanitizer_command.extend(["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g", "-no-pie"])
    sanitized_env = env.copy()
    sanitized_env["ASAN_OPTIONS"] = "detect_leaks=1:abort_on_error=1"
    sanitized_env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    commands.append(sanitizer_command)
    sanitized = run(sanitizer_command, sanitized_env, "candidate-sanitized")
    if sanitized != outputs["candidate", "O0"]:
        raise RuntimeError("Sanitizer corpus output changed")
    MIGRATION["ensure_reference"]()
    report = {
        "baseline": MIGRATION["BASELINE"], "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"),
        "candidate_status": MIGRATION["git"]("status", "--porcelain"), "rust_archive": str(archive),
        "commands": commands, "records_per_optimization": len(original), "record_counts": dict(counts),
        "optimization_modes": ["O0", "O2"], "typed_exception_records": sum(b" throw " in line for line in original if line.startswith(b"typed ")),
        "validity_disagreement_records": len(validity_disagreement), "validity_disagreement_example": validity_disagreement[0],
        "production_grouping_example": "grouping 0 1", "production_reducers_sha256": hashes,
        "reference_source_sha256": {name: sha256((REFERENCE / name).read_bytes()) for name in STRUCTURE + tuple(REDUCERS)},
        "fixture_sha256": sha256(fixture.read_bytes()),
        "outputs_sha256": {f"{label}-{mode}": sha256(output) for (label, mode), output in outputs.items()},
        "sanitizer_scope": {"cpp_fixture_and_adapters_instrumented": True, "rust_memory_accesses_instrumented": False,
                            "rust_allocator_leaks_checked": True, "passed": True},
        "fatal_limit": "NOT_REACHED/AssertFailedError stubs check dispatch and C++ unwinding, not game fatal text or source locations",
        "passed": True,
    }
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"History comparisons passed: {len(original)} records at O0 and O2 through Rust and portable C++; {OUT / 'report.json'}")


if __name__ == "__main__":
    main()
