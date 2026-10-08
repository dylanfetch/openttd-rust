#!/usr/bin/env python3
"""Run the inherited commit checker and scan explicit candidate compiler logs."""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from datetime import UTC, datetime
from pathlib import Path

from validation_execution import file_lock, run_logged

import migration

HOOKS_REVISION = "eeb3791aadf0aded5a7cd634c80823f17e87af9c"
HOOKS_URL = "https://github.com/OpenTTD/OpenTTD-git-hooks.git"
MAX_WARNING_OUTPUT = 10
WARNING = re.compile(
    r"(?:^warning:|^.+(?::\d+(?::\d+)?|\(\d+(?:,\d+)?\))\s*:\s*warning(?:\s+[A-Z]\d+)?:"
    r"|^(?:cc1(?:plus)?|clang(?:\+\+)?): warning:)",
    re.IGNORECASE,
)


def git(repo, *arguments):
    return subprocess.check_output(
        ["git", "-C", str(repo), *arguments], text=True
    ).strip()


def hooks_checkout(supplied=None):
    destination = supplied or (
        migration.COMMON_LOCAL / "tooling/OpenTTD-git-hooks" / HOOKS_REVISION
    )
    with file_lock(
        migration.COMMON_LOCAL / "tooling/hooks.lock", label="commit-checker checkout"
    ):
        if not destination.exists() and supplied is None:
            destination.mkdir(parents=True)
            subprocess.run(["git", "init", "--quiet", str(destination)], check=True)
        if supplied is None and not (destination / "hooks/check-commits.sh").exists():
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(destination),
                    "fetch",
                    "--depth=1",
                    HOOKS_URL,
                    HOOKS_REVISION,
                ],
                check=True,
            )
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(destination),
                    "checkout",
                    "--quiet",
                    "--detach",
                    HOOKS_REVISION,
                ],
                check=True,
            )
        if git(destination, "rev-parse", "HEAD") != HOOKS_REVISION:
            raise ValueError("commit checker is not at the pinned revision")
        if git(destination, "status", "--porcelain", "--untracked-files=all"):
            raise ValueError("commit checker checkout is dirty")
    return destination


def warnings(path):
    return [
        {"line": number, "text": line}
        for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1)
        if WARNING.search(line)
    ]


def verification_logs(path):
    report = json.loads(path.read_text())
    if report.get("schema_version") != 1:
        raise ValueError("--verification requires a schema_version=1 report")
    root = Path(report["candidate_root"])
    commands = report["commands"]
    logs = [
        root / command["log"]
        for command in commands
        if command["name"] in {"candidate-build", "tools-build"}
    ]
    if not logs:
        raise ValueError("verification report contains no candidate build log")
    return logs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="origin/rust-migration")
    parser.add_argument("--head", default="HEAD")
    parser.add_argument(
        "--hooks", type=Path, help="existing clean pinned hooks checkout (offline)"
    )
    parser.add_argument("--build-log", type=Path, action="append", default=[])
    parser.add_argument("--verification", type=Path)
    arguments = parser.parse_args()
    evidence = (
        migration.LOCAL / "preflight" / datetime.now(UTC).strftime("%Y%m%dT%H%M%S.%fZ")
    )
    evidence.mkdir(parents=True)
    report = {"schema_version": 1, "scope": "commit style and supplied compiler logs"}
    code = 1
    try:
        base = git(
            migration.ROOT,
            "rev-parse",
            "--verify",
            "--end-of-options",
            f"{arguments.base}^{{commit}}",
        )
        head = git(
            migration.ROOT,
            "rev-parse",
            "--verify",
            "--end-of-options",
            f"{arguments.head}^{{commit}}",
        )
        hooks = hooks_checkout(arguments.hooks.resolve() if arguments.hooks else None)
        env = os.environ.copy()
        env.update(
            GIT_DIR=git(migration.ROOT, "rev-parse", "--absolute-git-dir"),
            HOOKS_DIR=str(hooks / "hooks"),
        )
        check = run_logged(
            ["sh", str(hooks / "hooks/check-commits.sh"), f"{base}..{head}"],
            cwd=migration.ROOT,
            env=env,
            log=evidence / "commits.log",
            phase="inherited commit checker",
            stream=True,
        )
        report.update(
            base=base,
            head=head,
            hooks_revision=HOOKS_REVISION,
            commit_checker_exit=check,
        )
        logs = arguments.build_log + (
            verification_logs(arguments.verification) if arguments.verification else []
        )
        report["compiler_logs"] = []
        total_warnings = 0
        displayed_warnings = 0
        for path in dict.fromkeys(path.resolve() for path in logs):
            found = warnings(path)
            report["compiler_logs"].append(
                {
                    "path": str(path),
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "warnings": found,
                }
            )
            total_warnings += len(found)
            displayed = found[: MAX_WARNING_OUTPUT - displayed_warnings]
            for item in displayed:
                print(f"{path}:{item['line']}: {item['text']}", file=sys.stderr)
            displayed_warnings += len(displayed)
        if total_warnings:
            print(
                f"Compiler warnings: {total_warnings} found; showing {displayed_warnings}. "
                f"All warnings retained in {evidence / 'report.json'}",
                file=sys.stderr,
            )
        code = int(
            bool(check or any(item["warnings"] for item in report["compiler_logs"]))
        )
        if not logs:
            print(
                "Compiler warnings: not checked (no explicit candidate log supplied)."
            )
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        subprocess.CalledProcessError,
    ) as error:
        report["error"] = str(error)
        print(error, file=sys.stderr)
    report["exit_code"] = code
    (evidence / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Preflight report: {evidence / 'report.json'}")
    return code


if __name__ == "__main__":
    sys.exit(main())
