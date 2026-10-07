#!/usr/bin/env python3
"""Exercise bounded validation, lock coordination, and executable identity."""

import contextlib
import importlib.util
import io
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

from build_identity import IDENTITY_NAME, read_identity, source_identity, write_identity
from simulation import core
from validation_execution import file_lock, run_logged

import migration

TOOLS = Path(__file__).resolve().parent


class ExecutionTests(unittest.TestCase):
    def test_quiet_child_reports_heartbeat_and_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "quiet.log"
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                code = run_logged(
                    [
                        sys.executable,
                        "-c",
                        "import time; time.sleep(.12); print('retained'); raise SystemExit(7)",
                    ],
                    cwd=directory,
                    env=None,
                    log=log,
                    phase="quiet",
                    heartbeat=0.03,
                )
            self.assertEqual(code, 7)
            self.assertEqual(log.read_text(), "retained\n")
            self.assertIn("quiet: running", output.getvalue())
            self.assertIn(str(log), output.getvalue())
            self.assertIn("exit 7", output.getvalue())

    def test_streaming_preserves_split_utf8_and_complete_log(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "stream.log"
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                code = run_logged(
                    [
                        sys.executable,
                        "-c",
                        "import os,time; os.write(1,b'\\xe2'); time.sleep(.08); os.write(1,b'\\x82\\xac\\n')",
                    ],
                    cwd=directory,
                    env=None,
                    log=log,
                    phase="stream",
                    heartbeat=0.02,
                    stream=True,
                )
            self.assertEqual(code, 0)
            self.assertEqual(log.read_text(), "€\n")
            self.assertIn("€\n", output.getvalue())
            self.assertNotIn("�", output.getvalue())

    def test_timeout_reaps_child_and_retains_output(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "timeout.log"
            with contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(subprocess.TimeoutExpired):
                    run_logged(
                        [
                            sys.executable,
                            "-c",
                            "import time; print('started', flush=True); time.sleep(20)",
                        ],
                        cwd=directory,
                        env=None,
                        log=log,
                        phase="timeout",
                        heartbeat=0.03,
                        timeout=0.09,
                    )
            self.assertEqual(log.read_text(), "started\n")

    @unittest.skipUnless(sys.platform == "linux", "Linux descendant process state")
    def test_timeout_kills_descendant_even_when_parent_exits_on_term(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "descendant.log"
            child = (
                "import os,signal,time; "
                "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                "print(os.getpid(), flush=True); time.sleep(60)"
            )
            parent = (
                "import subprocess,sys,time; "
                f"subprocess.Popen([sys.executable, '-c', {child!r}]); time.sleep(60)"
            )
            descendant = None
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    with self.assertRaises(subprocess.TimeoutExpired):
                        run_logged(
                            [sys.executable, "-c", parent],
                            cwd=directory,
                            env=None,
                            log=log,
                            phase="descendant",
                            heartbeat=0.03,
                            timeout=0.3,
                        )
                descendant = int(log.read_text().strip())
                # An orphan may remain a zombie until the host reaps it; it must
                # no longer execute or retain the build's file descriptors.
                state = Path(f"/proc/{descendant}/stat")
                deadline = time.monotonic() + 1
                while state.exists():
                    if state.read_text().split(") ", 1)[1][0] == "Z":
                        break
                    if time.monotonic() >= deadline:
                        self.fail("descendant survived timeout cleanup")
                    time.sleep(0.01)
            finally:
                if descendant is not None:
                    try:
                        os.kill(descendant, signal.SIGKILL)
                    except ProcessLookupError:
                        pass

    @unittest.skipUnless(os.name == "posix", "POSIX cross-process locks")
    def test_wait_reports_holder_and_releases(self):
        with tempfile.TemporaryDirectory() as directory:
            lock = Path(directory) / "held.lock"
            script = (
                "from validation_execution import file_lock; import sys; "
                "\nwith file_lock(sys.argv[1], heartbeat=.03, label='child'): print('entered', flush=True)"
            )
            with file_lock(lock, label="parent"):
                child = subprocess.Popen(
                    [sys.executable, "-c", script, str(lock)],
                    cwd=TOOLS,
                    stdout=subprocess.PIPE,
                    text=True,
                )
                line = child.stdout.readline()
                self.assertIn("waiting", line)
                self.assertIn(f'"pid": {os.getpid()}', line)
                self.assertIn('"label": "parent"', line)
                self.assertIsNone(child.poll())
            with child:
                remaining = child.stdout.read()
                self.assertEqual(child.wait(timeout=5), 0)
            self.assertIn("acquired", remaining)
            self.assertIn("entered", remaining)
            self.assertEqual(
                list(lock.with_name(lock.name + ".holders").glob("*.json")), []
            )


class IdentityTests(unittest.TestCase):
    def initialize_repo(self, root):
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        subprocess.run(
            ["git", "-C", str(root), "config", "user.email", "test@example.invalid"],
            check=True,
        )
        subprocess.run(
            ["git", "-C", str(root), "config", "user.name", "Fixture"], check=True
        )
        (root / ".gitignore").write_text(".local/\nbuild-rust/\nreference-build/\n")
        (root / "source").write_text("initial")
        subprocess.run(
            ["git", "-C", str(root), "add", "source", ".gitignore"], check=True
        )
        subprocess.run(
            ["git", "-C", str(root), "commit", "-qm", "Add: Fixture"], check=True
        )

    def test_dirty_source_and_during_build_changes_are_recorded(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.initialize_repo(root)
            clean = source_identity(root)
            (root / "source").write_text("changed")
            dirty = source_identity(root)
            self.assertNotEqual(clean["source_digest"], dirty["source_digest"])
            self.assertIn("source", dirty["source_status"])
            build = root / "build"
            build.mkdir()
            binary = build / "openttd-rust"
            binary.write_text("executable")
            (build / "CMakeCache.txt").write_text("configuration")
            manifest = write_identity(build, binary, clean, dirty, {"flag": "value"})
            self.assertFalse(manifest["source_stable"])
            self.assertEqual(read_identity(build, binary), manifest)
            binary.write_text("replaced")
            self.assertIsNone(read_identity(build, binary))

    def test_capture_keeps_matching_identity_and_rejects_changed_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            binary = build / "openttd-rust"
            binary.write_text("first")
            (build / "CMakeCache.txt").write_text("configuration")
            source = {
                "source_commit": "older-build-head",
                "source_status": "",
                "source_digest": "1" * 64,
            }
            manifest = write_identity(build, binary, source, source, {})
            frozen = core.copy_runtime(build, binary, root / "frozen", candidate=True)
            self.assertEqual(frozen.read_text(), "first")
            self.assertEqual(
                json.loads((frozen.parent / IDENTITY_NAME).read_text()), manifest
            )
            (build / "CMakeCache.txt").write_text("other configuration")
            unknown = core.copy_runtime(build, binary, root / "unknown", candidate=True)
            self.assertFalse((unknown.parent / IDENTITY_NAME).exists())
            (build / IDENTITY_NAME).unlink()
            self.assertIsNone(read_identity(build, binary))

    @unittest.skipUnless(os.name == "posix", "POSIX cross-process locks")
    def test_capture_waits_for_build_and_freezes_completed_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            binary = build / "openttd-rust"
            binary.write_text("old")
            (build / "CMakeCache.txt").write_text("configuration")
            script = (
                "from simulation.core import copy_runtime; from pathlib import Path; import sys; "
                "copy_runtime(Path(sys.argv[1]),Path(sys.argv[2]),Path(sys.argv[3]),candidate=True)"
            )
            with migration.candidate_lock(build):
                child = subprocess.Popen(
                    [
                        sys.executable,
                        "-c",
                        script,
                        str(build),
                        str(binary),
                        str(root / "frozen"),
                    ],
                    cwd=TOOLS,
                    stdout=subprocess.PIPE,
                    text=True,
                )
                self.assertIn("candidate capture: waiting", child.stdout.readline())
                self.assertFalse((root / "frozen").exists())
                binary.write_text("new")
                source = {
                    "source_commit": "build-head",
                    "source_status": "",
                    "source_digest": "2" * 64,
                }
                identity = write_identity(build, binary, source, source, {})
            with child:
                child.stdout.read()
                self.assertEqual(child.wait(timeout=5), 0)
            self.assertEqual((root / "frozen/openttd-rust").read_text(), "new")
            self.assertEqual(
                json.loads((root / "frozen" / IDENTITY_NAME).read_text()), identity
            )

    def test_direct_game_call_initializes_clone_lock(self):
        with tempfile.TemporaryDirectory() as directory:
            with (
                patch.object(migration, "COMMON_LOCAL", Path(directory)),
                patch.object(core, "GAME_LOCK", None),
            ):
                with core.game_slot(False):
                    self.assertTrue((Path(directory) / "simulation-game.lock").exists())


class NarrowChecksTests(unittest.TestCase):
    initialize_repo = IdentityTests.initialize_repo

    def exercise_checks(self, *, failing=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.initialize_repo(root)
            bin_dir = root / "bin"
            bin_dir.mkdir()
            cargo = bin_dir / "cargo"
            cargo.write_text(
                f"#!{sys.executable}\nimport os,sys,json\n"
                "print(json.dumps({'args':sys.argv[1:],'jobs':os.environ.get('CARGO_BUILD_JOBS')}))\n"
                + ("sys.exit(3 if sys.argv[1]=='clippy' else 0)\n" if failing else "")
            )
            cargo.chmod(0o755)
            git = migration.git
            env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"])
            with (
                patch.object(migration, "ROOT", root),
                patch.object(migration, "LOCAL", root / ".local"),
                patch.object(
                    migration, "git", side_effect=lambda *args: git(*args, cwd=root)
                ),
                patch.object(migration, "environment", return_value=env),
                patch.object(
                    migration,
                    "ensure_reference",
                    side_effect=AssertionError("reference preparation"),
                ),
                patch.object(
                    migration,
                    "reference_lock",
                    side_effect=AssertionError("reference lock"),
                ),
                patch.object(
                    migration.shutil,
                    "which",
                    side_effect=AssertionError("cache preparation"),
                ),
                patch.object(
                    sys,
                    "argv",
                    ["migration.py", "rust-checks"]
                    + ([] if failing else ["--jobs", "2"]),
                ),
                contextlib.redirect_stdout(io.StringIO()),
                contextlib.redirect_stderr(io.StringIO()),
            ):
                if failing:
                    with self.assertRaises(RuntimeError):
                        migration.main()
                else:
                    migration.main()
            report = json.loads(
                next((root / ".local/verification").glob("*/report.json")).read_text()
            )
            self.assertEqual(report["schema_version"], 1)
            self.assertEqual(report["action"], "rust-checks")
            self.assertFalse(report["candidate_rust_enabled"])
            self.assertNotIn("candidate_build_identity", report)
            self.assertFalse(report["compiler_cache"]["enabled"])
            self.assertEqual(report["passed"], not failing)
            names = [c["name"] for c in report["commands"]]
            self.assertEqual(
                names,
                ["rust-fmt", "rust-check", "rust-clippy"]
                + ([] if failing else ["rust-tests"]),
            )
            for command in report["commands"]:
                log = json.loads((root / command["log"]).read_text())
                self.assertEqual(log["jobs"], "2")

    def test_rust_checks_never_prepares_native_or_reference_build(self):
        self.exercise_checks()

    def test_rust_checks_retains_failed_receipt(self):
        self.exercise_checks(failing=True)


class NativeJobLimitTests(unittest.TestCase):
    initialize_repo = IdentityTests.initialize_repo

    def test_cmake_child_cargo_inherits_driver_job_budget(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.initialize_repo(root)
            bin_dir = root / "bin"
            bin_dir.mkdir()
            cargo = bin_dir / "cargo"
            cargo.write_text(
                f"#!{sys.executable}\nimport os\nprint('nested cargo jobs='+os.environ['CARGO_BUILD_JOBS'])\n"
            )
            cargo.chmod(0o755)
            cmake = bin_dir / "cmake"
            cmake.write_text(
                f"#!{sys.executable}\nimport sys,subprocess\nfrom pathlib import Path\n"
                "args=sys.argv[1:]\n"
                "if '--build' in args:\n"
                "    build=Path(args[args.index('--build')+1])\n"
                "    subprocess.run(['cargo'],check=True)\n"
                "    (build/('openttd-rust' if build.name=='build-rust' else 'openttd')).write_text('built')\n"
                "else:\n"
                "    build=Path(args[args.index('-B')+1]); build.mkdir(parents=True,exist_ok=True)\n"
                "    (build/'CMakeCache.txt').write_text('fixture configuration')\n"
            )
            cmake.chmod(0o755)
            git = migration.git
            env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"])
            with (
                patch.object(migration, "ROOT", root),
                patch.object(migration, "LOCAL", root / ".local"),
                patch.object(migration, "REFERENCE_BUILD", root / "reference-build"),
                patch.object(
                    migration, "git", side_effect=lambda *args: git(*args, cwd=root)
                ),
                patch.object(migration, "environment", return_value=env),
                patch.object(migration, "ensure_reference"),
                patch.object(
                    migration, "reference_lock", return_value=contextlib.nullcontext()
                ),
                patch.object(
                    migration, "verify_compiler_cache_options", return_value={}
                ),
                patch.object(migration, "rust_configuration", return_value={}),
                patch.object(migration, "cmake_cache", return_value={}),
                patch.object(migration, "supply_graphics"),
                patch.object(
                    sys, "argv", ["migration.py", "build", "--jobs", "2", "--no-ccache"]
                ),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                migration.main()
            report = json.loads(
                next((root / ".local/verification").glob("*/report.json")).read_text()
            )
            self.assertTrue(report["passed"])
            self.assertTrue(report["candidate_build_identity"]["source_stable"])
            self.assertEqual(
                read_identity(root / "build-rust", root / "build-rust/openttd-rust"),
                report["candidate_build_identity"],
            )
            for role in ("reference", "candidate"):
                command = next(
                    c for c in report["commands"] if c["name"] == f"{role}-build"
                )
                self.assertEqual(command["argv"][-2:], ["--parallel", "2"])
                self.assertIn(
                    "nested cargo jobs=2", (root / command["log"]).read_text()
                )


class ComparisonTests(unittest.TestCase):
    def test_fast_completion_is_reported_before_earlier_slow_tool(self):
        spec = importlib.util.spec_from_file_location(
            "comparisons", TOOLS / "run-comparisons.py"
        )
        comparisons = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(comparisons)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tools").mkdir()
            for name, delay in (("a-slow", 0.16), ("b-fast", 0.01)):
                (root / "tools" / f"{name}-comparison.py").write_text(
                    f"import time; time.sleep({delay})\n"
                )
            output = io.StringIO()
            with (
                patch.object(comparisons, "ROOT", root),
                patch.object(comparisons, "LOGS", root / "logs"),
                patch.object(sys, "argv", ["run-comparisons.py", "--jobs", "2"]),
                contextlib.redirect_stdout(output),
            ):
                comparisons.main()
            lines = [
                line
                for line in output.getvalue().splitlines()
                if line.startswith("ok  ")
            ]
            self.assertIn("b-fast", lines[0])
            self.assertIn("a-slow", lines[1])


if __name__ == "__main__":
    unittest.main()
