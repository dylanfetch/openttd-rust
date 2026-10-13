#!/usr/bin/env python3
"""Check original retirement against conversion and ownership history (#223)."""

import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "port_metrics", Path(__file__).with_name("port-metrics.py")
)
METRICS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(METRICS)


class PortMetricsTests(unittest.TestCase):
    def test_shared_road_conversion_counts_original_and_removed_glue(self):
        result = METRICS.metrics("7349ec577a", "4b130309a5")
        self.assertEqual(result["retired"], 11)
        self.assertEqual(result["net_glue"], -387)
        self.assertEqual(result["glue_removed"], 1718)

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
            "src/pathfinder/yapf/yapf.hpp",
        ):
            self.assertTrue(METRICS.simulation_source(path), path)
        for path in (
            "src/town_gui.cpp",
            "src/3rdparty/foo.cpp",
            "src/os/unix/unix.cpp",
            "src/core/pool_type.hpp",
        ):
            self.assertFalse(METRICS.simulation_source(path), path)


if __name__ == "__main__":
    unittest.main()
