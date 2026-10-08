#!/usr/bin/env python3
"""Check warning recognition, receipt selection and actual checker invocation."""

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import preflight


class PreflightTests(unittest.TestCase):
    def test_many_warnings_bound_terminal_output_and_retain_complete_receipt(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "candidate.log"
            log.write_text(
                "".join(
                    f"src/game.cpp:{line}: warning: example\n" for line in range(25)
                )
            )
            stderr = io.StringIO()
            with (
                patch.object(preflight.migration, "LOCAL", root / ".local"),
                patch.object(preflight, "hooks_checkout", return_value=root),
                patch.object(
                    preflight, "git", side_effect=["a" * 40] * 3 + ["src/game.cpp"]
                ),
                patch.object(preflight, "run_logged", return_value=0),
                patch.object(sys, "argv", ["preflight.py", "--build-log", str(log)]),
                contextlib.redirect_stdout(io.StringIO()),
                contextlib.redirect_stderr(stderr),
            ):
                self.assertEqual(preflight.main(), 1)
            receipt = json.loads(
                next((root / ".local/preflight").glob("*/report.json")).read_text()
            )
            self.assertEqual(len(receipt["compiler_logs"][0]["warnings"]), 25)
            self.assertEqual(stderr.getvalue().count("warning: example"), 10)
            self.assertIn("25 found; showing 10", stderr.getvalue())
            self.assertIn("report.json", stderr.getvalue())

    def test_compiler_warning_formats(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "build.log"
            path.write_text(
                "src/game.cpp:12:4: warning: unused variable [-Wunused-variable]\n"
                "C:\\src\\game.cpp(4): warning C4244: conversion\n"
                "warning: unused import\n"
                "cc1plus: warning: ignored option\n"
                "[1/3] Building warning:example.cpp.o\n"
                "0 warnings generated.\n"
            )
            self.assertEqual(
                [item["line"] for item in preflight.warnings(path)], [1, 2, 3, 4]
            )
            self.assertEqual(
                [item["line"] for item in preflight.warnings(path, ["src/game.cpp"])],
                [1, 2, 3, 4],
            )
            self.assertEqual(
                [item["line"] for item in preflight.warnings(path, [])], [3, 4]
            )

    def test_receipt_selects_candidate_only_and_rejects_partial_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            receipt = {
                "schema_version": 1,
                "candidate_root": directory,
                "commands": [
                    {"name": "reference-build", "log": "reference.log"},
                    {"name": "candidate-build", "log": "candidate.log"},
                ],
            }
            path.write_text(json.dumps(receipt))
            self.assertEqual(
                preflight.verification_logs(path), [Path(directory) / "candidate.log"]
            )
            receipt["commands"] = []
            path.write_text(json.dumps(receipt))
            with self.assertRaisesRegex(ValueError, "no candidate"):
                preflight.verification_logs(path)
            receipt["schema_version"] = 2
            path.write_text(json.dumps(receipt))
            with self.assertRaisesRegex(ValueError, "schema_version"):
                preflight.verification_logs(path)

    def test_supplied_checker_must_match_pin_and_be_clean(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(preflight.migration, "COMMON_LOCAL", root):
                with patch.object(preflight, "git", side_effect=["bad revision"]):
                    with self.assertRaisesRegex(ValueError, "pinned revision"):
                        preflight.hooks_checkout(root)
                with patch.object(
                    preflight,
                    "git",
                    side_effect=[preflight.HOOKS_REVISION, " M hooks/check-diff.py"],
                ):
                    with self.assertRaisesRegex(ValueError, "dirty"):
                        preflight.hooks_checkout(root)


if __name__ == "__main__":
    unittest.main()
