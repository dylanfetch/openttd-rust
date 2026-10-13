#!/usr/bin/env python3
"""Check original retirement against conversion and ownership history (#223)."""

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SPEC = importlib.util.spec_from_file_location(
    "port_metrics", Path(__file__).with_name("port-metrics.py")
)
METRICS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(METRICS)


class PortMetricsTests(unittest.TestCase):
    def test_original_restoration_and_fork_glue_transitions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(METRICS, "ROOT", root):
                METRICS.git("init", "-q")
                METRICS.git("config", "user.name", "Metric regression")
                METRICS.git("config", "user.email", "metrics@example.invalid")

                def commit():
                    METRICS.git("add", "-A")
                    METRICS.git("commit", "-qm", "Change: Metric fixture")
                    return METRICS.git("rev-parse", "HEAD").strip()

                (root / "src").mkdir()
                original = root / "src/town_cmd.cpp"
                glue = root / "src/rust_glue.cpp"
                original.write_text("int original = 1;\n")
                baseline = commit()
                with mock.patch.object(METRICS, "BASELINE", baseline):
                    original.unlink()
                    glue.write_text("int glue = 2;\n")
                    deleted = commit()
                    result = METRICS.metrics(baseline, deleted)
                    self.assertEqual(
                        tuple(
                            result[k]
                            for k in ("retired", "glue", "glue_removed", "net_glue")
                        ),
                        (1, 1, 0, 1),
                    )
                    original.write_text("int original = 1;\n")
                    restored = commit()
                    result = METRICS.metrics(deleted, restored)
                    self.assertEqual(
                        tuple(
                            result[k]
                            for k in ("retired", "glue", "glue_removed", "net_glue")
                        ),
                        (-1, 0, 0, 0),
                    )
                    renamed = root / "src/renamed_glue.cpp"
                    glue.rename(renamed)
                    moved = commit()
                    result = METRICS.metrics(restored, moved)
                    self.assertEqual((result["glue"], result["glue_removed"]), (0, 0))
                    renamed.write_text("#ifndef WITH_RUST\nint glue = 2;\n#endif\n")
                    portable = commit()
                    result = METRICS.metrics(moved, portable)
                    self.assertEqual(
                        (result["glue"], result["glue_removed"], result["net_glue"]),
                        (2, 1, 1),
                    )
                    result = METRICS.metrics(portable, moved)
                    self.assertEqual(
                        (result["glue"], result["glue_removed"], result["net_glue"]),
                        (1, 2, -1),
                    )
                    renamed.unlink()
                    removed = commit()
                    result = METRICS.metrics(portable, removed)
                    self.assertEqual(
                        (result["glue"], result["glue_removed"], result["net_glue"]),
                        (0, 2, -2),
                    )

    def test_shared_road_conversion_counts_original_and_removed_glue(self):
        result = METRICS.metrics("7349ec577a", "4b130309a5")
        self.assertEqual(result["retired"], 11)
        self.assertEqual(result["net_glue"], -387)
        # Four unchanged fork blank lines become compiled as guards change.
        self.assertEqual(result["glue"], 1335)
        self.assertEqual(result["glue_removed"], 1722)

    def test_fleet_ownership_equals_whole_fork_retirement_delta(self):
        head = "a7a6e6650a"
        base = METRICS.git("rev-parse", head + "^1").strip()
        before = METRICS.source_metrics(METRICS.BASELINE, base)
        after = METRICS.source_metrics(METRICS.BASELINE, head)
        result = METRICS.metrics(base, head)
        self.assertEqual(result["retired"], after["retired"] - before["retired"])
        self.assertGreater(result["retired"], 1000)
        reversed_result = METRICS.metrics(head, base)
        self.assertEqual(reversed_result["retired"], -result["retired"])
        self.assertEqual(reversed_result["net_glue"], -result["net_glue"])

    def test_whole_fork_share_has_one_simulation_inventory(self):
        result = json.loads(
            subprocess.check_output(
                [
                    sys.executable,
                    str(Path(METRICS.__file__)),
                    "--whole-fork",
                    "--head",
                    "4b130309a5",
                    "--json",
                ],
                text=True,
            )
        )
        self.assertEqual(result["base"], METRICS.BASELINE)
        self.assertEqual(result["retired"], 34429)
        self.assertGreater(result["simulation_total"], result["simulation_retired"])
        self.assertGreater(result["simulation_retired"], 0)
        self.assertAlmostEqual(
            result["simulation_percent"],
            100 * result["simulation_retired"] / result["simulation_total"],
        )
        for path in (
            "src/town_cmd.cpp",
            "src/map_func.h",
            "src/openttd.cpp",
            "src/pathfinder/yapf/yapf.hpp",
        ):
            self.assertTrue(METRICS.simulation_source(path), path)
        for path in (
            "src/town_gui.cpp",
            "src/linkgraph/linkgraph_gui.cpp",
            "src/linkgraph/linkgraph_gui.h",
            "src/3rdparty/foo.cpp",
            "src/os/unix/unix.cpp",
            "src/core/pool_type.hpp",
        ):
            self.assertFalse(METRICS.simulation_source(path), path)


if __name__ == "__main__":
    unittest.main()
