#!/usr/bin/env python3
"""Check conservative cache provenance and explicit return to ordinary CMake mode."""

from pathlib import Path
import tempfile
import unittest

import migration


class CompilerCacheTests(unittest.TestCase):
    def test_provenance_and_ambient_policy_overrides(self):
        original = {"PATH": "/bin", "CCACHE_SLOPPINESS": "time_macros", "CCACHE_BASEDIR": "/tmp"}
        reference = migration.compiler_cache_environment(original, "reference")
        candidate = migration.compiler_cache_environment(original, "candidate")
        self.assertNotEqual(reference["CCACHE_DIR"], candidate["CCACHE_DIR"])
        self.assertEqual(reference["PATH"], "/bin")
        self.assertNotIn("CCACHE_SLOPPINESS", reference)
        self.assertNotIn("CCACHE_BASEDIR", reference)
        self.assertEqual(original["CCACHE_SLOPPINESS"], "time_macros")
        self.assertEqual(migration.compiler_cache_environment(original, "candidate", bypass=True)["CCACHE_DISABLE"], "1")
        with self.assertRaises(ValueError):
            migration.compiler_cache_environment(original, "../reference")

    def test_disabled_mode_clears_launchers_and_pch_override(self):
        options = migration.compiler_cache_options(None)
        self.assertIn("-DCMAKE_C_COMPILER_LAUNCHER=", options)
        self.assertIn("-DCMAKE_CXX_COMPILER_LAUNCHER=", options)
        self.assertIn("-DCMAKE_DISABLE_PRECOMPILE_HEADERS=OFF", options)
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "CMakeCache.txt").write_text("CMAKE_C_COMPILER_LAUNCHER:STRING=/usr/bin/ccache\nCMAKE_CXX_COMPILER_LAUNCHER:STRING=/usr/bin/ccache\nCMAKE_DISABLE_PRECOMPILE_HEADERS:BOOL=ON\n")
            with self.assertRaises(RuntimeError):
                migration.verify_compiler_cache_options(build, None)
            migration.verify_compiler_cache_options(build, "/usr/bin/ccache")


if __name__ == "__main__":
    unittest.main()
