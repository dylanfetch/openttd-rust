#!/usr/bin/env python3
"""Export explicit validation receipts and port metrics as compact PR evidence."""

import argparse
import json
import math
import re
import shlex
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUST_CHECKS = {"rust-fmt", "rust-check", "rust-clippy", "rust-tests"}
SHA256 = re.compile(r"[0-9a-f]{64}\Z")


class EvidenceError(ValueError):
    """A receipt cannot support the requested evidence summary."""


def require(condition, message):
    if not condition:
        raise EvidenceError(message)


def field(record, name, kind):
    require(isinstance(record, dict), "expected a JSON object")
    require(name in record, f"missing field: {name}")
    value = record[name]
    require(type(value) is kind, f"invalid field: {name} (expected {kind.__name__})")
    return value


def version(record):
    require(isinstance(record, dict), "expected a JSON object")
    if "schema_version" not in record:
        return "legacy unversioned"
    require(field(record, "schema_version", int) == 1, "unsupported schema_version")
    return "schema 1"


def digest(value, name):
    require(isinstance(value, str) and SHA256.fullmatch(value), f"invalid {name}")
    return value


def code(value):
    # Keep receipt-provided values from inserting Markdown or extra lines.
    return (
        "`" + str(value).replace("`", "'").replace("\n", " ").replace("\r", " ") + "`"
    )


def identity(record, binary_hash):
    require(version(record) == "schema 1", "build identity requires schema_version 1")
    require(
        digest(field(record, "binary_sha256", str), "identity binary_sha256")
        == binary_hash,
        "build identity binary hash mismatch",
    )
    for name in ("source_digest", "configuration_digest", "configuration_cache_sha256"):
        digest(field(record, name, str), name)
    require(
        field(record, "source_stable", bool), "build source changed during compilation"
    )
    require(field(record, "source_commit", str), "empty build source_commit")
    field(record, "source_status", str)
    return record


def source_label(commit, status, head):
    label = "dirty source" if status else "clean source"
    if commit != head:
        label += f"; prepared at {code(commit)}, differs from metrics head"
    return label


def verification(record, head):
    schema = version(record)
    require(field(record, "passed", bool), "verification failed or incomplete")
    action = field(record, "action", str)
    require(
        action in {"verify", "rust-checks", "build", "tools"},
        f"unsupported action: {action}",
    )
    commit = field(record, "candidate_commit", str)
    require(commit, "empty candidate_commit")
    status = field(record, "candidate_status", str)
    commands = field(record, "commands", list)
    require(commands, "empty verification commands")
    names = set()
    for command in commands:
        name = field(command, "name", str)
        require(name not in names, f"duplicate command: {name}")
        names.add(name)
        require(field(command, "exit_code", int) == 0, f"failed command: {name}")
        argv = field(command, "argv", list)
        require(
            argv and all(isinstance(arg, str) for arg in argv), f"invalid argv: {name}"
        )
    required = set(RUST_CHECKS) if action in {"verify", "rust-checks"} else set()
    if action == "verify":
        required |= {
            "reference-build",
            "candidate-build",
            "reference-tests",
            "candidate-tests",
        }
        for role in ("reference", "candidate"):
            require(
                field(record, f"{role}_test_count", int) > 0,
                f"empty {role} test inventory",
            )
            digest(field(record, f"{role}_binary_sha256", str), f"{role} binary hash")
        require(
            field(record, "candidate_rust_enabled", bool),
            "candidate Rust linkage not checked",
        )
    elif action in {"build", "tools"}:
        required.add("candidate-build")
    require(
        required <= names,
        f"incomplete {action}: missing commands {sorted(required - names)}",
    )
    binary_hash = None
    build_identity = None
    if action in {"verify", "build"}:
        binary_hash = digest(
            field(record, "candidate_binary_sha256", str), "candidate binary hash"
        )
        if record.get("candidate_build_identity") is not None:
            build_identity = identity(record["candidate_build_identity"], binary_hash)
        elif schema == "schema 1":
            raise EvidenceError("missing field: candidate_build_identity")
    scope = {
        "verify": "Rust checks, native builds and both CTest suites",
        "rust-checks": "Rust checks only; no native build, CTest or simulation",
        "build": "prepared game binaries only; no tests or simulation",
        "tools": "prepared generators only; no tests or simulation",
    }[action]
    origin = source_label(commit, status, head) + " (invocation checkout)"
    if build_identity:
        origin = source_label(
            build_identity["source_commit"], build_identity["source_status"], head
        )
    elif binary_hash:
        origin += "; executable source provenance unknown"
    return scope, origin, schema, binary_hash, build_identity


def simulation(record, verified_hash, verified_identity, head, verified_reference=None):
    schema = version(record)
    require(field(record, "passed", bool), "simulation failed or incomplete")
    mode = field(record, "mode", str)
    require(
        mode in {"reference-vs-candidate", "reference-vs-reference"},
        "unknown simulation mode",
    )
    binaries = field(record, "binaries", dict)
    candidate_hash = digest(
        field(binaries.get("candidate"), "sha256", str), "simulation candidate hash"
    )
    reference_hash = digest(
        field(binaries.get("reference"), "sha256", str), "simulation reference hash"
    )
    if verified_reference:
        require(
            reference_hash == verified_reference,
            "verification/simulation reference binary hash mismatch",
        )
    if mode == "reference-vs-candidate" and verified_hash:
        require(
            candidate_hash == verified_hash,
            "verification/simulation candidate binary hash mismatch",
        )
    if mode == "reference-vs-reference":
        require(
            reference_hash == candidate_hash, "self comparison binary hash mismatch"
        )
    build_identity = binaries["candidate"].get("build_identity")
    if build_identity is not None:
        identity(build_identity, candidate_hash)
        if verified_identity and mode == "reference-vs-candidate":
            require(
                build_identity == verified_identity,
                "verification/simulation build identity mismatch",
            )
    repetitions = field(record, "benchmark_repetitions", int)
    require(repetitions >= 0, "invalid benchmark repetitions")
    results = field(record, "results", list)
    require(results, "empty simulation results")
    names, speeds, known = [], [], 0
    for result in results:
        name = field(result, "scenario", str)
        require(name and name not in names, "empty or duplicate scenario")
        names.append(name)
        require(field(result, "passed", bool), f"scenario failed: {name}")
        for key in ("differences", "problems"):
            require(not field(result, key, list), f"{name}: unresolved {key}")
        known += len(field(result, "known_failures", list))
        speed = field(result, "plain_speed", dict)
        samples = field(speed, "samples", list)
        ratio = speed.get("median_candidate_reference_ratio")
        if repetitions:
            require(
                len(samples) == repetitions, f"{name}: incomplete benchmark samples"
            )
            require(
                all(isinstance(sample, dict) for sample in samples),
                f"{name}: invalid benchmark sample",
            )
            valid = all(
                sample.get("same_end") is True
                and type(sample.get("candidate_reference_ratio")) in (int, float)
                and math.isfinite(sample["candidate_reference_ratio"])
                and sample["candidate_reference_ratio"] > 0
                for sample in samples
            )
            require(
                valid and not result["known_failures"],
                f"{name}: benchmark lacks semantic-valid samples",
            )
            require(
                type(ratio) in (int, float) and math.isfinite(ratio) and ratio > 0,
                f"{name}: missing benchmark ratio",
            )
            require(
                math.isclose(
                    ratio,
                    statistics.median(
                        sample["candidate_reference_ratio"] for sample in samples
                    ),
                ),
                f"{name}: inconsistent benchmark median",
            )
            speeds.append(f"{code(name)} {ratio:.3f}x")
    scope = (
        "self comparison only"
        if mode == "reference-vs-reference"
        else "selected semantic scenarios"
    )
    entries = speeds if repetitions else [code(name) for name in names]
    summary = ", ".join(entries[:8])
    if len(entries) > 8:
        summary += f"; {len(entries)} scenarios total (full list in receipt)"
    if repetitions:
        summary += f" ({repetitions} repetitions)"
    if known:
        summary += f"; {known} known differences retained"
    origin = "executable source provenance unknown"
    if build_identity:
        origin = source_label(
            build_identity["source_commit"], build_identity["source_status"], head
        )
    return f"{scope}: {summary}; {schema}; {origin}"


def load_receipt(path):
    try:
        record = json.loads(path.read_text())
        require(isinstance(record, dict), "expected a JSON object")
        return record
    except (OSError, ValueError) as error:
        raise EvidenceError(f"{path}: {error}") from error


def render(
    verification_path,
    verification_record,
    simulations,
    metrics,
    head,
    agent,
    model,
    effort,
):
    scope, origin, schema, binary_hash, build_identity = verification(
        verification_record, head
    )
    lines = [
        f"Agent: {code(agent)} | Model: {code(model)} | Reasoning effort: {code(effort)}",
        "",
        f"- Validation: {scope}; {schema}; {origin}. Receipt: {code(verification_path)}.",
    ]
    if binary_hash:
        lines.append(f"- Candidate executable SHA-256: {code(binary_hash)}.")
    for path, record in simulations:
        try:
            summary = simulation(
                record,
                binary_hash,
                build_identity,
                head,
                verification_record.get("reference_binary_sha256"),
            )
        except EvidenceError as error:
            raise EvidenceError(f"{path}: {error}") from error
        lines.append(f"- Simulation: {summary}. Receipt: {code(path)}.")
    if not simulations:
        lines.append("- Simulation: no simulation or benchmark receipts selected.")
    for key in ("rust", "tooling", "glue", "retired"):
        require(
            type(metrics.get(key)) is int and metrics[key] >= 0,
            f"invalid port metrics: {key}",
        )
    require(
        isinstance(metrics.get("base"), str) and metrics["base"],
        "missing port metrics base",
    )
    lines.append(
        f"- Port metrics ({code(metrics['base'])} to {code(head)}, committed sources): "
        f"Rust {metrics['rust']} / tooling {metrics['tooling']} / C++ glue {metrics['glue']} / C++ retired {metrics['retired']}."
    )
    lines += ["", "Exact validation commands recorded in the receipt:"]
    for command in verification_record["commands"]:
        lines.append(
            f"- {code(shlex.join(command['argv']))} (cwd {code(command.get('cwd', 'unknown'))})."
        )
    require(
        len(lines) <= 55,
        "too many receipts/commands for the PR evidence budget; export a smaller selection",
    )
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verification", type=Path, required=True)
    parser.add_argument("--simulation", type=Path, action="append", default=[])
    parser.add_argument(
        "--base", required=True, help="explicit metrics target revision"
    )
    parser.add_argument("--head", required=True, help="explicit metrics head revision")
    parser.add_argument("--agent", required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--effort", required=True)
    args = parser.parse_args()
    try:
        head = subprocess.check_output(
            ["git", "rev-parse", "--verify", args.head + "^{commit}"],
            cwd=ROOT,
            text=True,
        ).strip()
        metrics = json.loads(
            subprocess.check_output(
                [
                    sys.executable,
                    str(ROOT / "tools/port-metrics.py"),
                    "--target",
                    args.base,
                    "--head",
                    head,
                    "--json",
                ],
                cwd=ROOT,
                text=True,
            )
        )
        print(
            render(
                args.verification,
                load_receipt(args.verification),
                [(path, load_receipt(path)) for path in args.simulation],
                metrics,
                head,
                args.agent,
                args.model,
                args.effort,
            ),
            end="",
        )
    except (EvidenceError, OSError, subprocess.CalledProcessError, ValueError) as error:
        parser.exit(1, f"evidence: {error}\n")


if __name__ == "__main__":
    main()
