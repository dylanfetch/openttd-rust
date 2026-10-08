#!/usr/bin/env python3
"""Preserve ignored files before removing a clean, merged linked worktree.

plan is read-only. archive requires the full expected commit; resume uses the same
arguments and journal. Renames require one filesystem. Symlinks are preserved,
never followed. Locked worktrees, the main checkout and pinned reference are
protected. Journals live separately from older ad hoc archive manifests.
"""

import argparse
import fcntl
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

VERSION = 1


class ArchiveError(Exception):
    pass


def git(repo, *args):
    result = subprocess.run(
        ["git", "-C", str(repo), *args], capture_output=True, text=True, check=False
    )
    if result.returncode:
        raise ArchiveError(result.stderr.strip() or f"git {' '.join(args)} failed")
    return result.stdout


def no_symlinks(path):
    for parent in [path, *path.parents]:
        if parent.is_symlink():
            raise ArchiveError(f"symlink path component: {parent}")


def normalized(value):
    path = Path(value).expanduser()
    if ".." in path.parts:
        raise ArchiveError(f"path traversal: {value}")
    path = Path(os.path.abspath(path))
    no_symlinks(path)
    return path


def child(base, value):
    rel = Path(value)
    if not rel.parts or rel.is_absolute() or ".." in rel.parts or ".git" in rel.parts:
        raise ArchiveError(f"unsafe preserved path: {value}")
    target = base / rel
    no_symlinks(target.parent)
    return target


def worktrees(repo):
    records = []
    for token in git(repo, "worktree", "list", "--porcelain", "-z").split("\0"):
        if token.startswith("worktree "):
            records.append({"path": token[9:]})
        elif token and records:
            key, _, value = token.partition(" ")
            records[-1][key] = value
    return records


def inventory(path):
    """Fingerprint contents without dereferencing symlinks, including empty dirs."""
    result = []
    pending = [(path, ".")]
    while pending:
        current, name = pending.pop()
        info = current.lstat()
        item = {"path": name, "mode": info.st_mode & 0o7777}
        if current.is_symlink():
            item.update(kind="symlink", target=os.readlink(current))
        elif current.is_dir():
            item["kind"] = "directory"
            for entry in sorted(current.iterdir(), reverse=True):
                pending.append((entry, str(Path(name) / entry.name)))
        elif current.is_file():
            digest = hashlib.sha256()
            with current.open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(block)
            item.update(kind="file", size=info.st_size, sha256=digest.hexdigest())
        else:
            raise ArchiveError(f"unsupported special file: {current}")
        result.append(item)
    return sorted(result, key=lambda item: item["path"])


def verified(path, expected):
    if inventory(path) != expected:
        raise ArchiveError(f"preserved contents changed: {path}")


def exists(path):
    return os.path.lexists(path)


def write_journal(path, data):
    temporary = path.with_suffix(".tmp")
    no_symlinks(temporary)
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(data, stream, indent=2)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)
    directory = os.open(path.parent, os.O_RDONLY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)


def merged(root, head, integration):
    commit = git(
        root, "rev-parse", "--verify", "--end-of-options", f"{integration}^{{commit}}"
    )
    result = subprocess.run(
        ["git", "-C", str(root), "merge-base", "--is-ancestor", head, commit.strip()],
        capture_output=True,
        check=False,
    )
    if result.returncode:
        raise ArchiveError(f"expected head is not merged into {integration}: {head}")


def clean(worktree, head):
    if git(worktree, "rev-parse", "HEAD").strip() != head:
        raise ArchiveError("worktree HEAD differs from --expected-head")
    if git(worktree, "status", "--porcelain=v1", "--untracked-files=all"):
        raise ArchiveError("worktree has tracked changes or untracked nonignored files")


def context(repo, worktree, head, integration):
    if not re.fullmatch(r"[0-9a-f]{40}", head):
        raise ArchiveError(
            "--expected-head must be a full lowercase 40-character commit"
        )
    records = worktrees(repo)
    if not records or "bare" in records[0]:
        raise ArchiveError("a non-bare clone is required")
    root = normalized(records[0]["path"])
    source = normalized(worktree)
    reference = root / ".local/reference/openttd"
    if (
        source == root
        or source in root.parents
        or source == reference
        or reference in source.parents
    ):
        raise ArchiveError("main checkout and pinned reference are protected")
    if source == root / ".local" or root / ".local" in source.parents:
        raise ArchiveError("worktrees inside the shared .local directory are protected")
    for record in records:
        if normalized(record["path"]) == source and "locked" in record:
            raise ArchiveError("locked/paused worktree is protected")
    integration = (
        git(
            root,
            "rev-parse",
            "--symbolic-full-name",
            "--verify",
            "--end-of-options",
            integration,
        ).strip()
        or git(
            root,
            "rev-parse",
            "--verify",
            "--end-of-options",
            f"{integration}^{{commit}}",
        ).strip()
    )
    merged(root, head, integration)
    identity = hashlib.sha256(f"{source}\0{head}".encode()).hexdigest()[:24]
    archive = root / ".local/archived-worktrees/v1" / identity
    no_symlinks(archive)
    return root, source, archive, records, integration


def plan(repo, worktree, head, integration):
    root, source, archive, records, integration = context(
        repo, worktree, head, integration
    )
    if not any(record["path"] == str(source) for record in records):
        raise ArchiveError("source is not a registered worktree")
    clean(source, head)
    paths = git(
        source,
        "ls-files",
        "--others",
        "--ignored",
        "--exclude-standard",
        "--directory",
        "-z",
    ).split("\0")
    storage = archive.parent
    while not storage.exists():
        storage = storage.parent
    device = storage.stat().st_dev
    selected = []
    for name in sorted(
        filter(None, paths), key=lambda name: (len(Path(name).parts), name)
    ):
        rel = Path(name)
        if any(parent == rel or parent in rel.parents for parent in selected):
            continue
        original = child(source, name)
        # Check before even creating an archive directory or journal.
        if original.lstat().st_dev != device:
            raise ArchiveError(
                "archive requires source and destination on one filesystem"
            )
        selected.append(rel)
    return {
        "version": VERSION,
        "root": str(root),
        "source": str(source),
        "head": head,
        "integration_ref": integration,
        "branch": git(source, "branch", "--show-current").strip(),
        "archive": str(archive),
        "stage": "planned",
        "paths": [
            {
                "path": str(rel),
                "inventory": inventory(child(source, str(rel))),
                "moved": False,
            }
            for rel in selected
        ],
    }


def validate_journal(data, root, source, archive, head, integration):
    expected = {
        "version": VERSION,
        "root": str(root),
        "source": str(source),
        "head": head,
        "integration_ref": integration,
        "archive": str(archive),
    }
    if any(data.get(key) != value for key, value in expected.items()):
        raise ArchiveError("journal version or archive identity does not match request")
    if data.get("stage") not in {"planned", "preserving", "removing", "removed"}:
        raise ArchiveError("unknown journal stage")
    selected = []
    for item in data["paths"]:
        rel = Path(item["path"])
        child(source, str(rel))
        child(archive / "files", str(rel))
        if any(
            parent == rel or parent in rel.parents or rel in parent.parents
            for parent in selected
        ):
            raise ArchiveError("overlapping journal paths")
        selected.append(rel)


def apply(repo, worktree, head, integration, resume=False):
    root, source, archive, _, integration = context(repo, worktree, head, integration)
    journal = archive / "journal.json"
    no_symlinks(journal)
    if resume and not journal.exists():
        raise ArchiveError("no archive journal to resume")
    initial = None if journal.exists() else plan(repo, worktree, head, integration)
    archive.parent.mkdir(parents=True, exist_ok=True)
    lock = archive.parent / ".lock"
    no_symlinks(lock)
    with lock.open("a") as stream:
        fcntl.flock(stream, fcntl.LOCK_EX)
        # Reload after acquiring the clone-wide archive lock.
        if journal.exists():
            data = json.loads(journal.read_text())
            validate_journal(data, root, source, archive, head, integration)
        else:
            archive.mkdir()
            data = initial
            write_journal(journal, data)
        registered = any(record["path"] == str(source) for record in worktrees(root))
        if data["stage"] == "removed":
            if registered or source.exists():
                raise ArchiveError("removed source path has been reused")
        elif data["stage"] != "removing":
            if not registered:
                raise ArchiveError("source disappeared before removal stage")
            clean(source, head)
            data["stage"] = "preserving"
            write_journal(journal, data)
            for item in data["paths"]:
                original = child(source, item["path"])
                saved = child(archive / "files", item["path"])
                if exists(original) and exists(saved):
                    raise ArchiveError(f"both source and archive exist: {item['path']}")
                if exists(original):
                    verified(original, item["inventory"])
                    saved.parent.mkdir(parents=True, exist_ok=True)
                    os.rename(original, saved)
                verified(saved, item["inventory"])
                item["moved"] = True
                write_journal(journal, data)
            data["stage"] = "removing"
            write_journal(journal, data)
        for item in data["paths"]:
            verified(child(archive / "files", item["path"]), item["inventory"])
        if data["stage"] != "removed":
            if registered:
                clean(source, head)
                remaining = git(
                    source,
                    "ls-files",
                    "--others",
                    "--ignored",
                    "--exclude-standard",
                    "--directory",
                    "-z",
                )
                if remaining:
                    raise ArchiveError("new ignored files remain; refusing removal")
                git(root, "worktree", "remove", str(source))
            elif source.exists():
                raise ArchiveError(
                    "unregistered source still exists; manual inspection required"
                )
            data["stage"] = "removed"
            write_journal(journal, data)
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["plan", "archive", "resume"])
    parser.add_argument("worktree", help="linked worktree path, including for resume")
    parser.add_argument(
        "--repo", default=".", help="any surviving checkout in the clone"
    )
    parser.add_argument("--expected-head", required=True, help="full commit ID")
    parser.add_argument("--integration-ref", default="rust-migration")
    args = parser.parse_args()
    try:
        if args.action == "plan":
            result = plan(
                args.repo, args.worktree, args.expected_head, args.integration_ref
            )
        else:
            result = apply(
                args.repo,
                args.worktree,
                args.expected_head,
                args.integration_ref,
                resume=args.action == "resume",
            )
        print(json.dumps(result, indent=2))
    except (ArchiveError, OSError, ValueError, KeyError, TypeError) as error:
        print(f"worktree archive: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
