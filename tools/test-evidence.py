#!/usr/bin/env python3
"""Check evidence joins, partial scopes and malformed receipt diagnostics."""

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import evidence

HEAD = "a" * 40
BINARY = "b" * 64
REFERENCE = "c" * 64
METRICS = {"base": "d" * 40, "rust": 1, "tooling": 2, "glue": 3, "retired": 4}


def build_identity():
    return {
        "schema_version": 1,
        "source_commit": HEAD,
        "source_status": "",
        "source_digest": "e" * 64,
        "configuration_digest": "f" * 64,
        "configuration_cache_sha256": "0" * 64,
        "binary_sha256": BINARY,
        "source_stable": True,
    }


def verification(action="verify"):
    names = sorted(evidence.RUST_CHECKS)
    if action == "verify":
        names += [
            "reference-build",
            "candidate-build",
            "reference-tests",
            "candidate-tests",
        ]
    elif action in {"build", "tools"}:
        names = ["candidate-build"]
    result = {
        "schema_version": 1,
        "action": action,
        "candidate_commit": HEAD,
        "candidate_status": "",
        "passed": True,
        "commands": [
            {
                "name": name,
                "argv": [name, "--literal"],
                "exit_code": 0,
                "cwd": "/source",
            }
            for name in names
        ],
        "candidate_rust_enabled": action == "verify",
        "reference_test_count": 10,
        "candidate_test_count": 10,
    }
    if action in {"verify", "build"}:
        result.update(
            candidate_binary_sha256=BINARY,
            reference_binary_sha256=REFERENCE,
            candidate_build_identity=build_identity(),
        )
    return result


def simulation(repetitions=0):
    return {
        "schema_version": 1,
        "mode": "reference-vs-candidate",
        "passed": True,
        "benchmark_repetitions": repetitions,
        "binaries": {
            "candidate": {"sha256": BINARY, "build_identity": build_identity()},
            "reference": {"sha256": REFERENCE},
        },
        "results": [
            {
                "scenario": "selected-game",
                "passed": True,
                "differences": [],
                "known_failures": [],
                "problems": [],
                "plain_speed": {
                    "samples": [
                        {"same_end": True, "candidate_reference_ratio": 2.0}
                        for _ in range(repetitions)
                    ],
                    "median_candidate_reference_ratio": 2.0 if repetitions else None,
                },
            }
        ],
    }


class EvidenceTests(unittest.TestCase):
    def render(self, record=None, sims=None):
        return evidence.render(
            Path("verify.json"),
            record or verification(),
            [(Path(f"simulation-{i}.json"), item) for i, item in enumerate(sims or [])],
            METRICS,
            HEAD,
            "/root/test",
            "gpt-6.1-sol",
            "high",
        )

    def test_valid_joins_and_compact_attributed_output(self):
        text = self.render(sims=[simulation(), simulation(3)])
        self.assertIn("2.000x (3 repetitions)", text)
        self.assertIn("Rust checks, native builds and both CTest suites", text)
        self.assertIn("selected semantic scenarios", text)
        self.assertIn("Reasoning effort: `high`", text)
        self.assertIn("Rust 1 / tooling 2 / C++ glue 3 / C++ retired 4", text)
        self.assertLessEqual(len(text.splitlines()), 55)

    def test_candidate_and_reference_hash_mismatches(self):
        for role in ("candidate", "reference"):
            with self.subTest(role=role):
                sim = simulation()
                sim["binaries"][role]["sha256"] = "1" * 64
                with self.assertRaisesRegex(
                    evidence.EvidenceError, f"{role} binary hash mismatch"
                ):
                    self.render(sims=[sim])

    def test_identity_mismatches(self):
        sim = simulation()
        sim["binaries"]["candidate"]["build_identity"]["source_digest"] = "1" * 64
        with self.assertRaisesRegex(evidence.EvidenceError, "build identity mismatch"):
            self.render(sims=[sim])
        report = verification()
        report["candidate_build_identity"]["binary_sha256"] = "1" * 64
        with self.assertRaisesRegex(
            evidence.EvidenceError, "identity binary hash mismatch"
        ):
            self.render(report)

    def test_required_fields_and_empty_receipts(self):
        for field in (
            "passed",
            "commands",
            "candidate_commit",
            "candidate_status",
            "candidate_build_identity",
        ):
            with self.subTest(field=field):
                record = verification()
                del record[field]
                with self.assertRaisesRegex(evidence.EvidenceError, "missing field"):
                    self.render(record)
        sim = simulation()
        sim["results"] = []
        with self.assertRaisesRegex(evidence.EvidenceError, "empty simulation results"):
            self.render(sims=[sim])

    def test_failed_and_incomplete_receipts(self):
        changes = [
            ("failed", lambda r: r.update(passed=False)),
            ("failed command", lambda r: r["commands"][0].update(exit_code=1)),
            ("missing commands", lambda r: r["commands"].pop()),
            (
                "empty reference test inventory",
                lambda r: r.update(reference_test_count=0),
            ),
            (
                "source changed",
                lambda r: r["candidate_build_identity"].update(source_stable=False),
            ),
        ]
        for error, change in changes:
            with self.subTest(error=error):
                record = verification()
                change(record)
                with self.assertRaisesRegex(evidence.EvidenceError, error):
                    self.render(record)
        sim = simulation()
        sim["results"][0]["differences"] = [{"unexpected": True}]
        with self.assertRaisesRegex(evidence.EvidenceError, "unresolved differences"):
            self.render(sims=[sim])

    def test_unsupported_versions_include_build_identity(self):
        for location in ("verification", "simulation", "identity"):
            record, sim = verification(), simulation()
            target = {
                "verification": record,
                "simulation": sim,
                "identity": record["candidate_build_identity"],
            }[location]
            target["schema_version"] = 2
            with (
                self.subTest(location=location),
                self.assertRaisesRegex(
                    evidence.EvidenceError, "unsupported schema_version"
                ),
            ):
                self.render(record, [sim])

    def test_legacy_receipts_explicitly_have_unknown_origin(self):
        record, sim = verification(), simulation()
        del record["schema_version"]
        del record["candidate_build_identity"]
        del sim["schema_version"]
        del sim["binaries"]["candidate"]["build_identity"]
        text = self.render(record, [sim])
        self.assertIn("legacy unversioned", text)
        self.assertIn("executable source provenance unknown", text)

    def test_rust_checks_are_narrow(self):
        text = self.render(verification("rust-checks"))
        self.assertIn("Rust checks only; no native build, CTest or simulation", text)
        self.assertNotIn("Candidate executable SHA-256", text)
        self.assertIn("no simulation or benchmark receipts selected", text)

    def test_dirty_and_prepared_source_are_not_metrics_head_evidence(self):
        record = verification()
        record["candidate_build_identity"].update(
            source_status=" M source.cpp", source_commit="1" * 40
        )
        text = self.render(record)
        self.assertIn("dirty source; prepared at", text)
        self.assertIn("differs from metrics head", text)
        self.assertIn("committed sources", text)
        self.assertIn("prepared game binaries only", self.render(verification("build")))

    def test_partial_invalid_and_unknown_origin_benchmarks(self):
        sim = simulation(3)
        sim["results"][0]["plain_speed"]["samples"].pop()
        with self.assertRaisesRegex(
            evidence.EvidenceError, "incomplete benchmark samples"
        ):
            self.render(sims=[sim])
        for bad_sample in (
            None,
            {"same_end": False},
            {"same_end": True, "candidate_reference_ratio": float("nan")},
        ):
            sim = simulation(1)
            sim["results"][0]["plain_speed"]["samples"][0] = bad_sample
            with self.assertRaises(evidence.EvidenceError):
                self.render(sims=[sim])
        sim = simulation(3)
        sim["results"][0]["plain_speed"]["median_candidate_reference_ratio"] = 1.0
        with self.assertRaisesRegex(
            evidence.EvidenceError, "inconsistent benchmark median"
        ):
            self.render(sims=[sim])
        sim = simulation(3)
        del sim["binaries"]["candidate"]["build_identity"]
        text = self.render(sims=[sim])
        self.assertIn("executable source provenance unknown", text)
        self.assertIn("2.000x", text)

    def test_self_comparison_is_explicit(self):
        sim = simulation()
        sim["mode"] = "reference-vs-reference"
        sim["binaries"]["candidate"] = copy.deepcopy(sim["binaries"]["reference"])
        self.assertIn("self comparison only", self.render(sims=[sim]))

    def test_cli_exports_valid_explicit_receipts_without_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "verify.json"
            payload = json.dumps(verification())
            path.write_text(payload)
            result = subprocess.run(
                [
                    sys.executable,
                    evidence.__file__,
                    "--verification",
                    str(path),
                    "--base",
                    "HEAD",
                    "--head",
                    "HEAD",
                    "--agent",
                    "/root/test",
                    "--model",
                    "gpt-6.1-sol",
                    "--effort",
                    "high",
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(path.read_text(), payload)
            self.assertEqual(list(Path(directory).iterdir()), [path])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Port metrics", result.stdout)
        self.assertIn("Rust 0 / tooling 0 / C++ glue 0 / C++ retired 0", result.stdout)
        self.assertIn("/root/test", result.stdout)

    def test_cli_requires_explicit_selection_and_diagnoses_bad_receipt(self):
        script = Path(evidence.__file__)
        result = subprocess.run(
            [sys.executable, str(script)], capture_output=True, text=True
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("--verification", result.stderr)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "broken.json"
            path.write_text(json.dumps({"passed": False}))
            result = subprocess.run(
                [
                    sys.executable,
                    str(script),
                    "--verification",
                    str(path),
                    "--base",
                    "HEAD",
                    "--head",
                    "HEAD",
                    "--agent",
                    "/root/test",
                    "--model",
                    "gpt-6.1-sol",
                    "--effort",
                    "high",
                ],
                capture_output=True,
                text=True,
            )
        self.assertEqual(result.returncode, 1)
        self.assertIn("verification failed or incomplete", result.stderr)
        self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
