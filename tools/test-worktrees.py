#!/usr/bin/env python3
"""Exercise recoverable archives using disposable clones, never real worktrees."""

import importlib.util
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

spec = importlib.util.spec_from_file_location(
    "worktrees", Path(__file__).with_name("worktrees.py")
)
archive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(archive)


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "clone"
        self.root.mkdir()
        self.git("init", "-b", "rust-migration")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        (self.root / ".gitignore").write_text(".local/\ncache/\nasset-link\n")
        (self.root / "tracked").write_text("source\n")
        self.git("add", ".")
        self.git("commit", "-m", "Add: Fixture")
        self.head = self.git("rev-parse", "HEAD").strip()
        self.source = self.base / "task"
        self.git("worktree", "add", "-b", "task", str(self.source))
        (self.source / ".local/evidence").mkdir(parents=True)
        (self.source / ".local/evidence/report.json").write_text('{"result":"pass"}\n')
        (self.source / "cache/empty").mkdir(parents=True)
        (self.source / "cache/blob").write_bytes(b"\x00\xffpayload")
        (self.source / "cache/blob").chmod(0o750)
        self.external = self.base / "external"
        self.external.write_text("outside\n")
        (self.source / "asset-link").symlink_to(self.external)
        (self.source / "cache/broken").symlink_to("missing")

    def git(self, *args, repo=None):
        return subprocess.check_output(
            ["git", "-C", str(repo or self.root), *args],
            text=True,
            stderr=subprocess.DEVNULL,
        )

    def plan(self, source=None):
        return archive.plan(
            self.root, source or self.source, self.head, "rust-migration"
        )

    def apply(self, resume=False, source=None):
        return archive.apply(
            self.root, source or self.source, self.head, "rust-migration", resume=resume
        )

    def test_plan_read_only_and_archive_repeat(self):
        planned = self.plan()
        saved = Path(planned["archive"])
        self.assertFalse(saved.exists())
        result = self.apply()
        self.assertEqual(result["stage"], "removed")
        self.assertFalse(self.source.exists())
        self.assertEqual(self.apply(), result)
        self.assertEqual(self.apply(resume=True), result)
        self.assertEqual((saved / "files/cache/blob").read_bytes(), b"\x00\xffpayload")
        self.assertEqual((saved / "files/cache/blob").stat().st_mode & 0o777, 0o750)
        self.assertTrue((saved / "files/cache/empty").is_dir())
        self.assertEqual(os.readlink(saved / "files/asset-link"), str(self.external))
        self.assertEqual(os.readlink(saved / "files/cache/broken"), "missing")
        self.assertEqual(self.external.read_text(), "outside\n")

    def test_interrupted_move_resumes(self):
        rename = archive.os.rename
        calls = 0

        def interrupted(source, target):
            nonlocal calls
            rename(source, target)
            calls += 1
            if calls == 1:
                raise OSError("interrupted after rename")

        planned = self.plan()
        with mock.patch.object(archive.os, "rename", side_effect=interrupted):
            with self.assertRaisesRegex(OSError, "interrupted"):
                self.apply()
        journal = json.loads((Path(planned["archive"]) / "journal.json").read_text())
        self.assertEqual(journal["stage"], "preserving")
        self.assertFalse(journal["paths"][0]["moved"])
        self.assertEqual(self.apply(resume=True)["stage"], "removed")

    def test_interrupted_removal_and_finalization(self):
        for point in ("before_remove", "after_remove", "finalization"):
            with self.subTest(point=point):
                source = self.base / point
                self.git("worktree", "add", "--detach", str(source), self.head)
                (source / "cache").mkdir()
                (source / "cache/log").write_text("retained")
                git = archive.git
                write = archive.write_journal

                def interrupted_git(repo, *args, point=point, git=git):
                    if args[:2] == ("worktree", "remove"):
                        if point == "before_remove":
                            raise OSError("interrupted")
                        result = git(repo, *args)
                        if point == "after_remove":
                            raise OSError("interrupted")
                        return result
                    return git(repo, *args)

                def interrupted_write(path, data, point=point, write=write):
                    if point == "finalization" and data["stage"] == "removed":
                        raise OSError("interrupted")
                    write(path, data)

                with (
                    mock.patch.object(archive, "git", side_effect=interrupted_git),
                    mock.patch.object(
                        archive, "write_journal", side_effect=interrupted_write
                    ),
                ):
                    with self.assertRaisesRegex(OSError, "interrupted"):
                        self.apply(source=source)
                result = self.apply(resume=True, source=source)
                self.assertEqual(result["stage"], "removed")
                self.assertEqual(
                    (Path(result["archive"]) / "files/cache/log").read_text(),
                    "retained",
                )

    def test_dirty_untracked_unmerged_and_expected_head_refused(self):
        for filename in ("tracked", "untracked"):
            (self.source / filename).write_text("changed")
            with self.assertRaisesRegex(archive.ArchiveError, "tracked changes"):
                self.apply()
            if filename == "tracked":
                self.git("restore", filename, repo=self.source)
            else:
                (self.source / filename).unlink()
        self.git("commit", "--allow-empty", "-m", "Add: Unmerged", repo=self.source)
        unmerged = self.git("rev-parse", "HEAD", repo=self.source).strip()
        with self.assertRaisesRegex(archive.ArchiveError, "not merged"):
            archive.apply(self.root, self.source, unmerged, "rust-migration")
        with self.assertRaisesRegex(archive.ArchiveError, "HEAD differs"):
            self.apply()
        self.assertTrue((self.source / "cache/blob").exists())
        self.assertFalse((self.root / ".local/archived-worktrees").exists())

    def test_protected_locked_traversal_and_symlink_paths(self):
        with self.assertRaisesRegex(archive.ArchiveError, "protected"):
            self.plan(self.root)
        reference = self.root / ".local/reference/openttd"
        self.git("worktree", "add", "--detach", str(reference), self.head)
        with self.assertRaisesRegex(archive.ArchiveError, "protected"):
            self.plan(reference)
        self.git("worktree", "lock", "--reason", "paused", str(self.source))
        with self.assertRaisesRegex(archive.ArchiveError, "locked/paused"):
            self.apply()
        self.git("worktree", "unlock", str(self.source))
        with self.assertRaisesRegex(archive.ArchiveError, "traversal"):
            self.plan(self.base / "other/../task")
        alias = self.base / "alias"
        alias.symlink_to(self.source, target_is_directory=True)
        with self.assertRaisesRegex(archive.ArchiveError, "symlink path"):
            self.plan(alias)

    def test_same_head_different_locations_and_legacy_manifest(self):
        legacy = self.root / ".local/archived-worktrees/manifest.json"
        legacy.parent.mkdir(parents=True)
        legacy.write_text('{"archives": [{"old": true}]}\n')
        original = legacy.read_bytes()
        other = self.base / "other"
        self.git("worktree", "add", "--force", str(other), "task")
        self.assertEqual(self.plan()["branch"], self.plan(other)["branch"])
        self.assertNotEqual(self.plan()["archive"], self.plan(other)["archive"])
        self.apply()
        self.apply(source=other)
        self.assertEqual(legacy.read_bytes(), original)

    def test_modified_saved_files_refused(self):
        result = self.apply()
        (Path(result["archive"]) / "files/cache/blob").write_bytes(b"corrupted")
        with self.assertRaisesRegex(archive.ArchiveError, "contents changed"):
            self.apply(resume=True)

    def test_source_path_reuse_refused(self):
        self.apply()
        self.git("worktree", "add", "--detach", str(self.source), self.head)
        with self.assertRaisesRegex(archive.ArchiveError, "reused"):
            self.apply()

    def test_new_ignored_files_not_deleted(self):
        git = archive.git

        def create_ignored(repo, *args):
            if args[:2] == ("worktree", "list") and (self.source / "cache").exists():
                (self.source / "cache/new").write_text("arrived after planning")
            return git(repo, *args)

        # Introduce a file after the plan inventory, before the preserving loop.
        planned = self.plan()
        with mock.patch.object(archive, "plan", return_value=planned):
            with mock.patch.object(archive, "git", side_effect=create_ignored):
                with self.assertRaisesRegex(archive.ArchiveError, "contents changed"):
                    self.apply()
        self.assertEqual(
            (self.source / "cache/new").read_text(), "arrived after planning"
        )

    def test_late_ignored_root_refused_before_removal(self):
        write = archive.write_journal

        def introduce_file(path, data):
            if data["stage"] == "removing":
                (self.source / ".local").mkdir(exist_ok=True)
                (self.source / ".local/late-log").write_text("preserve me")
            write(path, data)

        with mock.patch.object(archive, "write_journal", side_effect=introduce_file):
            with self.assertRaisesRegex(archive.ArchiveError, "new ignored files"):
                self.apply()
        self.assertEqual((self.source / ".local/late-log").read_text(), "preserve me")
        self.assertIn(str(self.source), self.git("worktree", "list"))

    def test_cross_filesystem_refused_without_mutation(self):
        lstat = Path.lstat

        def different_device(path):
            info = lstat(path)
            if path == self.source / ".local":
                fields = list(info)
                fields[2] += 1
                return os.stat_result(fields)
            return info

        with mock.patch.object(Path, "lstat", different_device):
            with self.assertRaisesRegex(archive.ArchiveError, "one filesystem"):
                self.apply()
        self.assertFalse((self.root / ".local/archived-worktrees").exists())
        self.assertTrue((self.source / ".local/evidence/report.json").exists())

    def test_journal_path_traversal_refused(self):
        git = archive.git

        def stop(repo, *args):
            if args[:2] == ("worktree", "remove"):
                raise OSError("interrupted")
            return git(repo, *args)

        planned = self.plan()
        with mock.patch.object(archive, "git", side_effect=stop):
            with self.assertRaisesRegex(OSError, "interrupted"):
                self.apply()
        journal = Path(planned["archive"]) / "journal.json"
        data = json.loads(journal.read_text())
        data["paths"][0]["path"] = "../../external"
        journal.write_text(json.dumps(data))
        with self.assertRaisesRegex(archive.ArchiveError, "unsafe preserved path"):
            self.apply(resume=True)
        self.assertEqual(self.external.read_text(), "outside\n")

    def test_integration_alias_and_symlinked_journal(self):
        planned = self.plan()
        canonical = archive.plan(
            self.root, self.source, self.head, "refs/heads/rust-migration"
        )
        self.assertEqual(planned, canonical)
        saved = Path(planned["archive"])
        saved.mkdir(parents=True)
        (saved / "journal.json").symlink_to(self.external)
        with self.assertRaisesRegex(archive.ArchiveError, "symlink path"):
            self.apply()
        self.assertEqual(self.external.read_text(), "outside\n")

    def test_no_journal_resume_refused(self):
        with self.assertRaisesRegex(archive.ArchiveError, "no archive journal"):
            self.apply(resume=True)


if __name__ == "__main__":
    unittest.main()
