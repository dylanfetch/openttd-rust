#!/usr/bin/env python3
"""Check archive evidence parsing for CMake's actual Makefiles path conventions."""

from pathlib import Path
import runpy
import shlex
import tempfile
import unittest

archive_is_linked = runpy.run_path(str(Path(__file__).with_name("rust-build-evidence.py")))["archive_is_linked"]


class LinkEvidenceTests(unittest.TestCase):
    def test_target_working_directories_and_relative_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            archive = build / "cargo/aarch64-apple-darwin/release/libopenttd_kernels.a"
            for target, working_directory, token in (
                ("openttd", build, "cargo/aarch64-apple-darwin/release/libopenttd_kernels.a"),
                ("openttd_test", build, "cargo/aarch64-apple-darwin/release/libopenttd_kernels.a"),
                ("strgen", build / "src/strgen", "../../cargo/aarch64-apple-darwin/release/libopenttd_kernels.a"),
                ("settingsgen", build / "src/settingsgen", "../../cargo/aarch64-apple-darwin/release/libopenttd_kernels.a"),
            ):
                with self.subTest(target=target):
                    script = working_directory / f"CMakeFiles/{target}.dir/link.txt"
                    script.parent.mkdir(parents=True)
                    script.write_text(f"c++ object.o -o {target} {token} -lSystem\n")
                    self.assertTrue(archive_is_linked(script, archive))
                    self.assertFalse(archive_is_linked(script, archive.with_name("other.a")))

    def test_absolute_and_quoted_paths(self):
        with tempfile.TemporaryDirectory(prefix="Rust evidence 'quoted' ") as directory:
            build = Path(directory)
            archive = build / "cargo/aarch64-apple-darwin/release/libopenttd_kernels.a"
            script = build / "CMakeFiles/openttd.dir/link.txt"
            script.parent.mkdir(parents=True)
            script.write_text(f"c++ {shlex.quote(str(archive))} -o openttd\n")
            self.assertTrue(archive_is_linked(script, archive))
            script.write_text(f"c++ {shlex.quote(str(archive) + '.unrelated')} -o openttd\n")
            self.assertFalse(archive_is_linked(script, archive))


if __name__ == "__main__":
    unittest.main()
