#!/usr/bin/env python3
"""Print the roadmap progress metric for the current branch.

Counts, from the merge base with the target branch to HEAD (comments and
blank lines included):

  rust     lines added under rust/
  tooling  lines added under tools/
  glue     lines added under src/ that are compiled when WITH_RUST is defined
  retired  net increase in src/ lines compiled only without WITH_RUST (the
           original C++ kept as the portable fallback), plus deleted src/ files

Both guard forms are understood: `#ifdef WITH_RUST ... #else <original>
#endif` and `#ifndef WITH_RUST <original> #endif` (and the `defined()` forms).
"""

import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIRECTIVE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)")
POSITIVE = re.compile(r"^\s*(?:defined\s*\(?\s*WITH_RUST\s*\)?|WITH_RUST)\s*$")
NEGATIVE = re.compile(r"^\s*!\s*defined\s*\(?\s*WITH_RUST\s*\)?\s*$")


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, errors="replace")


def classify(text):
    """Tag every line 'cpp' (only without WITH_RUST), 'rust' or 'common'."""
    stack, tags = [], []
    for line in text.splitlines():
        match = DIRECTIVE.match(line)
        kind, rest = (match.group(1), match.group(2).split("//")[0].strip()) if match else (None, "")
        if kind in ("if", "ifdef", "ifndef"):
            if (kind == "ifdef" and rest == "WITH_RUST") or (kind == "if" and POSITIVE.match(rest)):
                frame = "rust"
            elif (kind == "ifndef" and rest == "WITH_RUST") or (kind == "if" and NEGATIVE.match(rest)):
                frame = "cpp"
            else:
                frame = None
            stack.append([frame, False])
            tags.append("rust" if frame else state(stack[:-1]))
            continue
        if kind in ("elif", "else") and stack:
            frame, switched = stack[-1]
            tags.append("rust" if frame else state(stack[:-1]))
            if frame and not switched:
                # Every later branch of a WITH_RUST conditional is the other side.
                stack[-1] = ["cpp" if frame == "rust" else "rust", True]
            continue
        if kind == "endif" and stack:
            frame, _ = stack.pop()
            tags.append("rust" if frame else state(stack))
            continue
        tags.append(state(stack))
    return tags


def state(stack):
    frames = [frame for frame, _ in stack]
    if "cpp" in frames:
        return "cpp"
    return "rust" if "rust" in frames else "common"


def show(rev, path):
    try:
        return git("show", f"{rev}:{path}")
    except subprocess.CalledProcessError:
        return ""


def added_lines(base, head, path):
    """Return 1-based HEAD line numbers added relative to base."""
    numbers = []
    for line in git("diff", "-U0", base, head, "--", path).splitlines():
        hunk = re.match(r"^@@ -\S+ \+(\d+)(?:,(\d+))? @@", line)
        if hunk:
            start, count = int(hunk.group(1)), int(hunk.group(2) or "1")
            numbers.extend(range(start, start + count))
    return numbers


def metrics(base, head="HEAD"):
    result = {"rust": 0, "tooling": 0, "glue": 0, "retired": 0}
    for row in git("diff", "--numstat", base, head).splitlines():
        added, _, path = row.split("\t", 2)
        if added == "-":
            continue
        if path.startswith("rust/"):
            result["rust"] += int(added)
        elif path.startswith("tools/"):
            result["tooling"] += int(added)
    for row in git("diff", "--name-status", "--no-renames", base, head, "--", "src").splitlines():
        status, path = row.split("\t", 1)
        if not path.endswith((".cpp", ".h", ".hpp", ".c")):
            continue
        before = show(base, path)
        if status == "D":
            result["retired"] += len(before.splitlines())
            continue
        head_tags = classify(show(head, path))
        base_tags = classify(before)
        result["retired"] += max(0, head_tags.count("cpp") - base_tags.count("cpp"))
        result["glue"] += sum(1 for n in added_lines(base, head, path) if n <= len(head_tags) and head_tags[n - 1] != "cpp")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--target", default="origin/rust-migration", help="branch the PR targets (default: origin/rust-migration)")
    parser.add_argument("--head", default="HEAD", help="revision to measure (default: HEAD)")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    base = git("merge-base", args.target, args.head).strip()
    result = metrics(base, args.head)
    if args.json:
        print(json.dumps(dict(result, base=base)))
    else:
        print(f"Rust added: {result['rust']} | tooling added: {result['tooling']} | "
              f"C++ glue added: {result['glue']} | C++ retired: {result['retired']}")


if __name__ == "__main__":
    main()
