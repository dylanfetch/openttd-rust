#!/usr/bin/env python3
"""Exercise simulation identity and runtime isolation across a concurrent rebuild."""

import contextlib
import hashlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import simulate


class SimulationProvenanceTests(unittest.TestCase):
    def exercise(self, *, custom=False, self_compare=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tools").mkdir()
            (root / "head").write_text("initial-checkout")
            (root / "status").write_text(" M tracked-file")
            # A tiny driver fixture supplies paths/identity; actual simulation
            # main(), runtime copying and executable launches stay exercised.
            (root / "tools/migration.py").write_text(
                "from contextlib import nullcontext\n"
                "from pathlib import Path\n"
                "ROOT = Path(__file__).resolve().parents[1]\n"
                "LOCAL = COMMON_LOCAL = ROOT / '.local'\n"
                "REFERENCE_BUILD = ROOT / 'reference'\n"
                "def reference_lock(shared=False): return nullcontext()\n"
                "def environment(): return {}\n"
                "def git(*args):\n"
                "    return (ROOT / ('head' if args[0] == 'rev-parse' else 'status')).read_text()\n"
            )
            ai_initial = {}
            for folder in ("town-name-observer", "tree-scenario-ai"):
                ai = root / "tools" / folder
                ai.mkdir()
                (ai / "main.nut").write_text(f"{folder}-before")
                ai_initial[folder] = hashlib.sha256(
                    (ai / "main.nut").read_bytes()
                ).hexdigest()
            originals, initial, assets = {}, {}, {}
            for role in ("reference", "candidate"):
                if role == "candidate" and self_compare:
                    continue
                build = root / (
                    "reference"
                    if role == "reference"
                    else "custom-build"
                    if custom
                    else "build-rust"
                )
                (build / "lang").mkdir(parents=True)
                asset = root / f"{role}-asset"
                asset.write_text(f"{role}-data-before")
                (build / "lang/linked-data").symlink_to(Path("../..") / asset.name)
                (build / "lang/missing-data").symlink_to("absent-relative-target")
                binary = build / ("openttd" if role == "reference" else "openttd-rust")
                binary.write_text(f"print('{role}-before')\n")
                originals[role], assets[role] = binary, asset
                initial[role] = hashlib.sha256(binary.read_bytes()).hexdigest()

            invocations = []

            def run(scenario, binaries, builds, *args):
                for role in ("reference", "candidate"):
                    original_role = "reference" if self_compare else role
                    output = subprocess.check_output(
                        [sys.executable, binaries[role]], text=True
                    ).strip()
                    self.assertEqual(output, f"{original_role}-before")
                    self.assertEqual(
                        (builds[role] / "lang/linked-data").read_text(),
                        f"{original_role}-data-before",
                    )
                    self.assertFalse((builds[role] / "lang/missing-data").exists())
                folder = (
                    "town-name-observer"
                    if "town_style" in scenario
                    else "tree-scenario-ai"
                )
                self.assertEqual(
                    (Path(scenario["scenario_ai"]) / "main.nut").read_text(),
                    f"{folder}-before",
                )
                invocations.append(scenario["name"])
                if len(invocations) == 1:
                    for role, binary in originals.items():
                        replacement = binary.with_suffix(".new")
                        replacement.write_text(f"print('{role}-after')\n")
                        replacement.replace(binary)
                        assets[role].write_text(f"{role}-data-after")
                    for folder in ai_initial:
                        (root / "tools" / folder / "main.nut").write_text(
                            f"{folder}-after"
                        )
                    (root / "head").write_text("later-checkout")
                    (root / "status").write_text("")
                return {
                    "scenario": scenario["name"],
                    "passed": True,
                    "snapshots": 1,
                    "stats": {"chunks": 1, "elements": 1, "masked": {}},
                    "differences": [],
                    "known_failures": [],
                    "problems": [],
                }

            argv = ["simulate.py", "--jobs", "1"]
            if self_compare:
                argv += ["--self", "--candidate", str(root / "missing")]
            elif custom:
                argv += ["--candidate", str(originals["candidate"])]
            with (
                patch.object(simulate, "ROOT", root),
                patch.object(simulate, "MACHINE"),
                patch.object(
                    simulate,
                    "scenario_list",
                    return_value=[
                        {"name": "a", "town_style": 0},
                        {"name": "b", "trees": "commands"},
                    ],
                ),
                patch.object(simulate, "run_scenario", side_effect=run),
                patch.object(sys, "argv", argv),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(simulate.main(), 0)
            report = json.loads(
                next((root / ".local/simulation").glob("*/report.json")).read_text()
            )
            self.assertEqual(invocations, ["a", "b"])
            self.assertEqual(report["candidate_commit"], "initial-checkout")
            self.assertEqual(report["candidate_status"], " M tracked-file")
            for folder, digest in ai_initial.items():
                evidence = report["scenario_ai"][folder]
                self.assertEqual(evidence["files"], {"main.nut": digest})
                self.assertEqual(
                    (Path(evidence["path"]) / "main.nut").read_text(),
                    f"{folder}-before",
                )
            for role, evidence in report["binaries"].items():
                original_role = "reference" if self_compare else role
                self.assertEqual(evidence["sha256"], initial[original_role])
                self.assertEqual(
                    Path(evidence["source_path"]), originals[original_role]
                )
                self.assertFalse(Path(evidence["path"]).parent.exists())

    def test_default_candidate_survives_rebuild(self):
        self.exercise()

    def test_custom_candidate_uses_its_own_frozen_runtime(self):
        self.exercise(custom=True)

    def test_reference_self_needs_no_candidate(self):
        self.exercise(self_compare=True)


if __name__ == "__main__":
    unittest.main()
