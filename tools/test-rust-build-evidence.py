#!/usr/bin/env python3
"""Check archive evidence parsing for CMake's actual Makefiles path conventions."""

import json
import runpy
import shlex
import tempfile
import unittest
from pathlib import Path

evidence = runpy.run_path(str(Path(__file__).with_name("rust-build-evidence.py")))
archive_is_linked = evidence["archive_is_linked"]
consumer_assertion_policy = evidence["consumer_assertion_policy"]


class LinkEvidenceTests(unittest.TestCase):
    def test_target_working_directories_and_relative_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            archive = build / "cargo/aarch64-apple-darwin/release/libopenttd_kernels.a"
            for target, working_directory, token in (
                (
                    "openttd",
                    build,
                    "cargo/aarch64-apple-darwin/release/libopenttd_kernels.a",
                ),
                (
                    "openttd_test",
                    build,
                    "cargo/aarch64-apple-darwin/release/libopenttd_kernels.a",
                ),
                (
                    "strgen",
                    build / "src/strgen",
                    "../../cargo/aarch64-apple-darwin/release/libopenttd_kernels.a",
                ),
                (
                    "settingsgen",
                    build / "src/settingsgen",
                    "../../cargo/aarch64-apple-darwin/release/libopenttd_kernels.a",
                ),
            ):
                with self.subTest(target=target):
                    script = working_directory / f"CMakeFiles/{target}.dir/link.txt"
                    script.parent.mkdir(parents=True)
                    script.write_text(f"c++ object.o -o {target} {token} -lSystem\n")
                    self.assertTrue(archive_is_linked(script, archive))
                    self.assertFalse(
                        archive_is_linked(script, archive.with_name("other.a"))
                    )

    def test_absolute_and_quoted_paths(self):
        with tempfile.TemporaryDirectory(prefix="Rust evidence 'quoted' ") as directory:
            build = Path(directory)
            archive = build / "cargo/aarch64-apple-darwin/release/libopenttd_kernels.a"
            script = build / "CMakeFiles/openttd.dir/link.txt"
            script.parent.mkdir(parents=True)
            script.write_text(f"c++ {shlex.quote(str(archive))} -o openttd\n")
            self.assertTrue(archive_is_linked(script, archive))
            script.write_text(
                f"c++ {shlex.quote(str(archive) + '.unrelated')} -o openttd\n"
            )
            self.assertFalse(archive_is_linked(script, archive))


class AssertionEvidenceTests(unittest.TestCase):
    def test_recorded_native_debug_release_roles(self):
        fixtures = json.loads(
            (Path(__file__).parent / "migration/macos-assertion-flags.json").read_text()
        )
        for record in fixtures["records"]:
            with self.subTest(
                build_type=record["build_type"], consumer=record["consumer"]
            ):
                consumer_assertion_policy(
                    record["command"], record["consumer"], record["build_type"]
                )
                bad_flag = (
                    " -DNDEBUG" if record["build_type"] == "Debug" else " -DWITH_ASSERT"
                )
                with self.assertRaises(RuntimeError):
                    consumer_assertion_policy(
                        record["command"] + bad_flag,
                        record["consumer"],
                        record["build_type"],
                    )

    def test_incorrect_role_macros_are_rejected(self):
        for consumer in ("strgen", "settingsgen"):
            with self.assertRaises(RuntimeError):
                consumer_assertion_policy("c++ -DWITH_ASSERT -g", consumer, "Debug")
        for consumer in ("game", "tests"):
            with self.assertRaises(RuntimeError):
                consumer_assertion_policy("c++ -g", consumer, "Debug")
        with self.assertRaises(RuntimeError):
            consumer_assertion_policy("c++ -O2", "strgen", "RelWithDebInfo")


if __name__ == "__main__":
    unittest.main()
