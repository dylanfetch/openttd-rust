#!/usr/bin/env python3
"""Check warning recognition, receipt selection and actual checker invocation."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import preflight


class PreflightTests(unittest.TestCase):
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
