#!/usr/bin/env python3
"""Bounded checks for MSVC role flags and retained Ninja linker response files."""

from pathlib import Path
import runpy
import tempfile
import unittest

import migration

evidence = runpy.run_path(str(Path(__file__).with_name("windows-rust-evidence.py")))


class WindowsEvidenceTests(unittest.TestCase):
    def test_both_target_archive_metadata_and_refusals(self):
        with tempfile.TemporaryDirectory() as directory:
            # Windows TEMP can use an 8.3 alias; the locator returns canonical paths.
            build = Path(directory).resolve()
            for target, width in (("i686-pc-windows-msvc", 4), ("x86_64-pc-windows-msvc", 8)):
                with self.subTest(target=target):
                    archive = build / "cargo" / target / "release/openttd_kernels.lib"
                    archive.parent.mkdir(parents=True)
                    archive.write_bytes(b"fixture archive")
                    cache = {"OPTION_RUST": "ON", "RUST_TARGET": target, "RUST_HOST": "x86_64-pc-windows-msvc",
                             "RUST_PLATFORM": "Windows", "RUST_POINTER_WIDTH": str(width),
                             "RUST_CRT": "static-release", "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded",
                             "CMAKE_BUILD_TYPE": "RelWithDebInfo", "OPTION_USE_ASSERTS": "ON",
                             "RUST_EFFECTIVE_FLAGS": "-C;target-feature=+crt-static",
                             "RUST_TARGET_DIR": str(build / "cargo"), "RUST_ARCHIVE": str(archive)}

                    def write(values):
                        (build / "CMakeCache.txt").write_text("".join(f"{key}:STRING={value}\n" for key, value in values.items()))

                    write(cache)
                    self.assertEqual(migration.rust_archive(build), archive)
                    self.assertEqual(migration.rust_configuration(build)["pointer_bytes"], width)
                    for key, value in (("RUST_POINTER_WIDTH", "16"), ("RUST_CRT", "dynamic"),
                                       ("CMAKE_MSVC_RUNTIME_LIBRARY", "MultiThreadedDLL"),
                                       ("CMAKE_BUILD_TYPE", "Debug"), ("OPTION_USE_ASSERTS", "OFF"),
                                       ("RUST_ARCHIVE", str(archive.with_name("libopenttd_kernels.a"))),
                                       ("RUST_EFFECTIVE_FLAGS", "-C;target-feature=-crt-static")):
                        write(cache | {key: value})
                        with self.assertRaises(RuntimeError):
                            migration.rust_archive(build)
                    write(cache)

    def test_original_role_flags_and_static_release_runtime(self):
        for role in ("game", "tests", "abi", "strgen", "settingsgen"):
            with self.subTest(role=role):
                flags = "/DNDEBUG /DWITH_RUST /MT"
                if role in ("game", "tests", "abi"):
                    flags += " /DWITH_ASSERT"
                evidence["assertion_policy"]("cl.exe " + flags, role)
                for bad in (flags.replace("/MT", "/MD"), flags.replace("/MT", "/MTd"),
                            flags.replace("/DNDEBUG", ""), flags + " /MD"):
                    with self.assertRaises(RuntimeError):
                        evidence["assertion_policy"]("cl.exe " + bad, role)
        with self.assertRaises(RuntimeError):
            evidence["assertion_policy"]("cl /DNDEBUG /DWITH_ASSERT /MT", "strgen")
        with self.assertRaises(RuntimeError):
            evidence["assertion_policy"]("cl /DNDEBUG /MT", "game")

    def test_nested_quoted_response_and_exact_archive(self):
        with tempfile.TemporaryDirectory(prefix="windows evidence ") as directory:
            build = Path(directory)
            archive = build / "cargo/i686-pc-windows-msvc/release/openttd_kernels.lib"
            (build / "inner response.rsp").write_text(f'"{archive}" kernel32.lib\n')
            (build / "link.rsp").write_text('/out:src/strgen/strgen.exe @"inner response.rsp"\n', encoding="utf-8-sig")
            command = 'cmake -E vs_link_exe -- link.exe @link.rsp'
            original, expanded, responses = evidence["select_link"](
                ['cl.exe /c src/strgen.cpp', command, 'link.exe /out:openttd.exe'], build, "src/strgen/strgen.exe")
            self.assertEqual(original, command)
            self.assertEqual(len(responses), 2)
            self.assertTrue(evidence["linked_archive"](expanded, build, archive))
            self.assertFalse(evidence["linked_archive"](expanded, build, archive.with_name("other.lib")))
            self.assertFalse(evidence["linked_archive"](f'"{archive}.other"', build, archive))
            with self.assertRaises(RuntimeError):
                evidence["select_link"]([command, command], build, "src/strgen/strgen.exe")
            with self.assertRaises(RuntimeError):
                evidence["select_link"]([command], build, "strgen.exe")
            with self.assertRaises(FileNotFoundError):
                evidence["expand_responses"]("link.exe @missing.rsp", build)


if __name__ == "__main__":
    unittest.main()
